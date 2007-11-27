/* -*-c++-*- VirtualPlanetBuilder - Copyright (C) 1998-2007 Robert Osfield 
 *
 * This library is open source and may be redistributed and/or modified under  
 * the terms of the OpenSceneGraph Public License (OSGPL) version 0.0 or 
 * (at your option) any later version.  The full license is in LICENSE file
 * included with this distribution, and on the openscenegraph.org website.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * OpenSceneGraph Public License for more details.
*/

#include <vpb/System>
#include <vpb/BuildLog>
#include <vpb/Date>

#include <map>

using namespace vpb;
 

std::string vpb::getLocalHostName()
{
    char hostname[1024];
    if (gethostname(hostname, sizeof(hostname))==0)
    {
        return std::string(hostname);
    }
    else
    {
        return std::string();
    }
}

int vpb::getProcessID()
{
    return getpid();
}


// convience methods for accessing System singletons variables.
osgDB::FilePathList& vpb::getSourcePaths() { return System::instance()->getSourcePaths(); }
std::string& vpb::getDestinationDirectory() { return System::instance()->getDestinationDirectory(); }
std::string& vpb::getIntermediateDirectory() { return System::instance()->getIntermediateDirectory(); }
std::string& vpb::getLogDirectory() { return System::instance()->getLogDirectory(); }
std::string& vpb::getTaskDirectory() { return System::instance()->getTaskDirectory(); }
std::string& vpb::getMachineFileName() { return System::instance()->getMachineFileName(); }

///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  FileSytem singleton

osg::ref_ptr<System>& System::instance()
{
    static osg::ref_ptr<System> s_System = new System;
    return s_System;
}

System::System()
{
    _trimOldestTiles = true;
    _numUnusedDatasetsToTrimFromCache = 10;
    _maxNumDatasets = (unsigned int)(double(getdtablesize()) * 0.8);
    
    readEnvironmentVariables();
}

System::~System()
{
}

void System::readEnvironmentVariables()
{
    const char* str = getenv("VPB_SOURCE_PATHS");
    if (str) 
    {
        osgDB::convertStringPathIntoFilePathList(std::string(str),_sourcePaths);
    }

    str = getenv("VPB_DESTINATION_DIR");
    if (str) 
    {
        _destinationDirectory = str;
    }

    str = getenv("VPB_INTERMEDIATE_DIR");
    if (str) 
    {
        _intermediateDirectory = str;
    }

    str = getenv("VPB_LOG_DIR");
    if (str) 
    {
        _logDirectory = str;
    }

    str = getenv("VPB_TASK_DIR");
    if (str) 
    {
        _taskDirectory = str;
    }

    str = getenv("VPB_MACHINE_FILE");
    if (str) 
    {
        _machineFileName = str;
    }
    
    str = getenv("VPB_TRIM_TILES_SCHEME");
    if (str) 
    {
        if (strcmp(str,"OLDEST")==0 || strcmp(str,"oldest")==0 ||  strcmp(str,"Oldest")==0)
        {
            _trimOldestTiles = true;
        }
        else
        {
            _trimOldestTiles = false;
        }
    }

    str = getenv("VPB_NUM_UNUSED_DATASETS_TO_TRIM_FROM_CACHE");
    if (str) 
    {
        _numUnusedDatasetsToTrimFromCache = atoi(str);
    }

    str = getenv("VPB_MAXIMUM_NUM_OPEN_DATASETS");
    if (str) 
    {
        _maxNumDatasets = atoi(str);
    }


    str = getenv("VPB_CACHE_FILE");
    if (str) 
    {
        osg::notify(osg::NOTICE)<<"VPB_CACHE_FILE = "<<str<<std::endl;
        openFileCache(str);
    }
    
}

void System::clearDatasetCache()
{
    _datasetMap.clear();
}


class TrimN
{
public:

    TrimN(unsigned int n, bool oldest):
        _oldest(oldest),
        _num(n) {}
    
    
    inline void add(System::DatasetMap::iterator itr)
    {
        if (itr->second->referenceCount()!=1) return;
    
        double t = itr->second->getTimeStamp();
        if (_timeIteratorMap.size() < _num) 
        {
            _timeIteratorMap.insert(TimeIteratorMap::value_type(t,itr));
        }
        else if (_oldest)
        {
            if (t < _timeIteratorMap.rbegin()->first)
            {
                // erase the end entry
                _timeIteratorMap.erase(_timeIteratorMap.rbegin()->first);
                _timeIteratorMap.insert(TimeIteratorMap::value_type(t,itr));
            }            
        }
        else
        {
            if (t > _timeIteratorMap.begin()->first)
            {
                // erase the first entry
                _timeIteratorMap.erase(_timeIteratorMap.begin()->first);
                _timeIteratorMap.insert(TimeIteratorMap::value_type(t,itr));
            }
        }
    }
    
    void add(System::DatasetMap& datasetMap)
    {
        for(System::DatasetMap::iterator itr = datasetMap.begin();
            itr != datasetMap.end();
            ++itr)
        {
            add(itr);
        }
    }
    
    void eraseFrom(System::DatasetMap& datasetMap)
    {
        for(TimeIteratorMap::iterator itr = _timeIteratorMap.begin();
            itr != _timeIteratorMap.end();
            ++itr)
        {
            datasetMap.erase(itr->second);
        }
    }
    
    typedef std::multimap<double, System::DatasetMap::iterator> TimeIteratorMap;
    
    bool            _oldest;
    unsigned int    _num;
    TimeIteratorMap _timeIteratorMap;    
};

void System::clearUnusedDatasets(unsigned int numToClear)
{
    TrimN lowerN(numToClear, _trimOldestTiles);

    lowerN.add(_datasetMap);    
    lowerN.eraseFrom(_datasetMap);
    
    _datasetMap.clear();
}

GeospatialDataset* System::openGeospatialDataset(const std::string& filename)
{
    // first check to see if dataset already exists in cache, if so return it.
    DatasetMap::iterator itr = _datasetMap.find(filename);
    if (itr != _datasetMap.end()) 
    {
        //osg::notify(osg::NOTICE)<<"System::openGeospatialDataset("<<filename<<") returning existing entry, ref count "<<itr->second->referenceCount()<<std::endl;
        return itr->second.get();
    }

    // make sure there is room available for this new Dataset
    if (_datasetMap.size()>=_maxNumDatasets) clearUnusedDatasets(_numUnusedDatasetsToTrimFromCache);
    
    // double check to make sure there is room to open a new dataset
    if (_datasetMap.size()>=_maxNumDatasets)
    {
        log(osg::NOTICE,"Error: System::GDALOpen(%s) unable to open file as unsufficient file handles available.",filename.c_str());
        return 0;
    }
    
    //osg::notify(osg::NOTICE)<<"System::openGeospatialDataset("<<filename<<") requires new entry "<<std::endl;

    // open the new dataset.
    GeospatialDataset* dataset = new GeospatialDataset(filename);

    // insert it into the cache
    _datasetMap[filename] = dataset;
    
    // return it.
    return dataset;
}

GeospatialDataset* System::openOptimumGeospatialDataset(const std::string& filename, const SpatialProperties& sp)
{
    if (_fileCache.valid())
    {
        std::string optimumFile = _fileCache->getOptimimumFile(filename, sp);
        return (optimumFile.empty()) ? 0 : openGeospatialDataset(optimumFile);
    }
    else
    {
        return openGeospatialDataset(filename);
    }
}

bool System::readFileCache(const std::string& filename)
{
    std::string foundFile = osgDB::findDataFile(filename);
    if (foundFile.empty())
    {
        log(osg::WARN,"Error: could not find cache file '%s'",filename.c_str());
        return false;
    }
    
    _fileCache = new FileCache;
    return _fileCache->read(foundFile);
}

bool System::openFileCache(const std::string& filename)
{
    _fileCache = new FileCache;
    return _fileCache->open(filename);
}

bool System::getDateOfLastModification(osgTerrain::Terrain* source, Date& date)
{
    typedef std::list<osgTerrain::Layer*> Layers;
    Layers layers;

    if (source->getElevationLayer())
    {
        layers.push_back(source->getElevationLayer());
    }

    for(unsigned int i=0; i<source->getNumColorLayers();++i)
    {
        osgTerrain::Layer* layer = source->getColorLayer(i);
        if (layer) 
        {
            layers.push_back(layer);
        }
    }

    typedef std::list<std::string> Filenames;
    Filenames filenames;

    for(Layers::iterator itr = layers.begin();
        itr != layers.end();
        ++itr)
    {
        osgTerrain::CompositeLayer* compositeLayer = dynamic_cast<osgTerrain::CompositeLayer*>(*itr);
        if (compositeLayer)
        {
            for(unsigned int i=0; i<compositeLayer->getNumLayers();++i)
            {
                filenames.push_back(compositeLayer->getFileName(i));
            }
        }
        else
        {
            filenames.push_back((*itr)->getFileName());
        }
    }
    
    bool modified = false;
    for(Filenames::iterator itr = filenames.begin();
        itr != filenames.end();
        ++itr)
    {
        Date lastModification;
        if (lastModification.setWithDateOfLastModification(*itr))
        {
            if (lastModification > date)
            {
                date = lastModification;
                modified = true;
            }
        }
    }
    
    return modified;


}
