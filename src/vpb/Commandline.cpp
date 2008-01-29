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

#include <vpb/Commandline>
#include <vpb/Source>
#include <vpb/BuildOptions>
#include <vpb/DatabaseBuilder>

#include <osg/Notify>
#include <osg/io_utils>

#include <osgSim/ShapeAttribute>

#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgDB/FileNameUtils>

using namespace vpb;

Commandline::Commandline()
{
    init();
}

void Commandline::init()
{
    maximumPossibleLevel = MAXIMUM_NUMBER_OF_LEVELS;
    typeAttributeName = "NAME";
    heightAttributeName = "HGT";
    terrainmask = 0xffffffff;

    reset();
}

void Commandline::reset()
{
    dataType = vpb::SpatialProperties::RASTER;

    minmaxLevelSet = false;
    min_level=0; 
    max_level=maximumPossibleLevel;
    layerNum = 0;

    currentCS = "";
    geoTransformSet = false;
    geoTransformScale = false;
    geoTransform.makeIdentity();            
    
    mask = 0xffffffff;
    
    heightAttribute = -1.0; // negative signifies that no height has been defined.
    typeAttribute = ""; // empty signifies no type attribute has been defined.
    
    
}

void Commandline::computeGeoTransForRange(double xMin, double xMax, double yMin, double yMax)
{
    geoTransformSet = true;
    geoTransformScale = true;

    geoTransform.makeIdentity();

    geoTransform(0,0) = xMax-xMin;
    geoTransform(3,0) = xMin;
    geoTransform(1,1) = yMax-yMin;
    geoTransform(3,1) = yMin;
}


void Commandline::processFile(vpb::Source::Type type, const std::string& filename)
{
    if (filename.empty()) return;

    if (osgDB::fileType(filename) == osgDB::REGULAR_FILE)
    {
        switch(type)
        {
            case(vpb::Source::IMAGE):           processImageOrHeightField(type,filename); break;
            case(vpb::Source::HEIGHT_FIELD):    processImageOrHeightField(type,filename); break;
            case(vpb::Source::MODEL):           processModel(filename); break;
            case(vpb::Source::SHAPEFILE):       processShapeFile(type, filename); break;
        }
    }
    else
    {
        processDirectory(type, filename);
    }
}

void Commandline::processImageOrHeightField(vpb::Source::Type type, const std::string& filename)
{
    osgTerrain::Layer* existingLayer = 0;
    osgTerrain::CompositeLayer* compositeLayer = 0;

    if (type==vpb::Source::IMAGE)
    {
        existingLayer = (layerNum < terrain->getNumColorLayers()) ? terrain->getColorLayer(layerNum) : 0;
        compositeLayer = dynamic_cast<osgTerrain::CompositeLayer*>(existingLayer);

        if (!compositeLayer)
        {
            compositeLayer = new osgTerrain::CompositeLayer;
            if (existingLayer) compositeLayer->addLayer(existingLayer);

            terrain->setColorLayer(layerNum, compositeLayer);
        }
    }
    else if (type==vpb::Source::HEIGHT_FIELD)
    {
        existingLayer = terrain->getElevationLayer();
        compositeLayer = dynamic_cast<osgTerrain::CompositeLayer*>(existingLayer);

        if (!compositeLayer)
        {
            compositeLayer = new osgTerrain::CompositeLayer;
            if (existingLayer) compositeLayer->addLayer(existingLayer);

            terrain->setElevationLayer(compositeLayer);
        }
    }

    if (!currentCS.empty() || geoTransformSet)
    {
        osg::ref_ptr<osgTerrain::ProxyLayer> loadedLayer = new osgTerrain::ProxyLayer;
        loadedLayer->setFileName(filename);

        if (loadedLayer.valid())
        {
            osgTerrain::Locator* locator = loadedLayer->getLocator();

            if (!loadedLayer->getLocator())
            {
                locator = new osgTerrain::Locator;
                loadedLayer->setLocator(locator);
            }

            if (!currentCS.empty())
            {
                osg::notify(osg::INFO)<<"locator->setCoordateSystem "<<currentCS<<std::endl;
                locator->setFormat("WKT");
                locator->setCoordinateSystem(currentCS);
                locator->setDefinedInFile(false);
                locator->setTransformScaledByResolution(!geoTransformScale);
            } 

            if (geoTransformSet)
            {
                osg::notify(osg::INFO)<<"locator->setTransform "<<geoTransform<<std::endl;
                locator->setTransform(geoTransform);
                locator->setDefinedInFile(false);
                locator->setTransformScaledByResolution(!geoTransformScale);
            }

            if (min_level!=0) loadedLayer->setMinLevel(min_level);
            if (max_level!=maximumPossibleLevel) loadedLayer->setMaxLevel(max_level);

            log(osg::NOTICE,"loaded layer %s",filename.c_str());

            compositeLayer->addLayer(loadedLayer.get());
        }
        else
        {
            log(osg::NOTICE,"Error: unable to load %s",filename.c_str());
        } 
    }
    else
    {
        if (min_level!=0 || max_level!=maximumPossibleLevel)
        {
            osgTerrain::ImageLayer* layer = new osgTerrain::ImageLayer;
            layer->setFileName(filename);
            
            if (min_level!=0) layer->setMinLevel(min_level);
            if (max_level!=maximumPossibleLevel) layer->setMaxLevel(max_level);

            compositeLayer->addLayer(layer);
        }
        else
        {
            compositeLayer->addLayer(filename);
        }
    }
}

class ApplyUserDataToDrawables : public osg::NodeVisitor
{
    public:
    
        ApplyUserDataToDrawables(osgSim::ShapeAttributeList* sal, bool replace):
            osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
            _sal(sal),
            _replace(replace)
        {
        }
            
            
        void apply(osg::Geode& geode)
        {
            osg::notify(osg::NOTICE)<<"Applying UserData "<<std::endl;

            for(unsigned int i=0; i<geode.getNumDrawables(); ++i)
            {
                osg::Drawable* drawable = geode.getDrawable(i);
                if (drawable)
                {
                    osgSim::ShapeAttributeList* user_sal = dynamic_cast<osgSim::ShapeAttributeList*>(drawable->getUserData());
                    if (user_sal) _shapeAttributeSet.insert(user_sal);
                    else drawable->setUserData(_sal.get());
                }
            }
            
            merge();
        }
        
        void merge()
        {
            for(ShapeAttributesSet::iterator itr = _shapeAttributeSet.begin();
                itr != _shapeAttributeSet.end();
                ++itr)
            {
                osgSim::ShapeAttributeList* user_sal = const_cast<osgSim::ShapeAttributeList*>(*itr);
                for(osgSim::ShapeAttributeList::iterator sitr = _sal->begin();
                    sitr != _sal->end();
                    ++sitr)
                {
                    merge(user_sal, *sitr);
                }
            }
        }
        
        void merge(osgSim::ShapeAttributeList* user_sal, osgSim::ShapeAttribute& sa)
        {
            for(osgSim::ShapeAttributeList::iterator sitr = user_sal->begin();
                    sitr != user_sal->end();
                    ++sitr)
            {
                if (sitr->getName()==sa.getName())
                {
                    if (_replace)
                    {
                        *sitr = sa;
                    }
                    return;
                }
            }
            
            user_sal->push_back(sa);
        }
        
        typedef std::set<osgSim::ShapeAttributeList*> ShapeAttributesSet;
            
        osg::ref_ptr<osgSim::ShapeAttributeList>    _sal;
        ShapeAttributesSet                          _shapeAttributeSet;
        bool                                        _replace;      
};

void Commandline::processShapeFile(vpb::Source::Type type, const std::string& filename)
{
    osg::ref_ptr<osgDB::ReaderWriter::Options> options = new osgDB::ReaderWriter::Options;
    options->setOptionString("double");

    osg::ref_ptr<osg::Node> model = osgDB::readNodeFile(filename, options.get());

    if (model.valid())
    {
        if (!currentCS.empty() || geoTransformSet)
        {
            osgTerrain::Locator* locator = new osgTerrain::Locator;
            model->setUserData(locator);

            if (!currentCS.empty())
            {
                osg::notify(osg::NOTICE)<<"locator->setCoordateSystem "<<currentCS<<std::endl;
                locator->setFormat("WKT");
                locator->setCoordinateSystem(currentCS);
                locator->setDefinedInFile(false);
                locator->setTransformScaledByResolution(false);
            } 

            if (geoTransformSet)
            {
                osg::notify(osg::NOTICE)<<"locator->setTransform "<<geoTransform<<std::endl;
                locator->setTransform(geoTransform);
                locator->setDefinedInFile(false);
                locator->setTransformScaledByResolution(false);
            }
        }
        
        osg::ref_ptr<osgSim::ShapeAttributeList> sal = new osgSim::ShapeAttributeList;

        if (!typeAttribute.empty())
        {
            sal->push_back(osgSim::ShapeAttribute(typeAttributeName.c_str(), typeAttribute.c_str()));
        }

        if (heightAttribute>=0.0)
        {
            sal->push_back(osgSim::ShapeAttribute(heightAttributeName.c_str(), heightAttribute));
        }
        
        if (!sal->empty())
        {
            ApplyUserDataToDrawables audtd(sal.get(), true);
            model->accept(audtd);
        }

        model->setNodeMask(mask);

        model->addDescription(std::string("SHAPEFILE"));

        if (!heightAttributeName.empty())
        {
            model->addDescription(std::string("HeightAttributeName ")+heightAttributeName);
        }

        if (!typeAttributeName.empty())
        {
            model->addDescription(std::string("TypeAttributeName ")+typeAttributeName);
        }

        if (min_level!=0)
        {
            std::stringstream ostr;
            ostr<<"MinLevel "<<min_level;
            model->addDescription(ostr.str());
        }
        if (max_level!=maximumPossibleLevel)
        {
            std::stringstream ostr;
            ostr<<"MaxLevel "<<max_level;
            model->addDescription(ostr.str());
        }


        terrain->addChild(model.get());
    }
    else
    {
        log(osg::NOTICE,"Error: unable to load file %s", filename.c_str());
    }
}

void Commandline::processModel(const std::string& filename)
{
    osg::ref_ptr<osg::Node> model = osgDB::readNodeFile(filename);

    if (model.valid())
    {
        if (!currentCS.empty() || geoTransformSet)
        {
            osgTerrain::Locator* locator = new osgTerrain::Locator;
            model->setUserData(locator);

            if (!currentCS.empty())
            {
                osg::notify(osg::INFO)<<"locator->setCoordateSystem "<<currentCS<<std::endl;
                locator->setFormat("WKT");
                locator->setCoordinateSystem(currentCS);
                locator->setDefinedInFile(false);
            } 

            if (geoTransformSet)
            {
                osg::notify(osg::INFO)<<"locator->setTransform "<<geoTransform<<std::endl;
                locator->setTransform(geoTransform);
                locator->setDefinedInFile(false);
            }
        }

        model->setName(filename);
        model->addDescription("MODEL");

        terrain->addChild(model.get());
    }
    else
    {
        log(osg::NOTICE,"Error: unable to load file %s", filename.c_str());
    }
}

void Commandline::processDirectory(vpb::Source::Type type, const std::string& filename)
{
    osgDB::DirectoryContents dirContents= osgDB::getDirectoryContents(filename);

    // loop through directory contents and call processFile
    std::vector<std::string>::iterator i;
    std::string fullfilename;
    for(i = dirContents.begin(); i != dirContents.end(); ++i)
    {
        if((*i != ".") && (*i != ".."))
        {
            fullfilename = filename + '/' + *i;
            processFile(type, fullfilename);
        }
    }
}

unsigned int Commandline::readMask(const std::string& maskstring)
{
    mask = strtoul(maskstring.c_str(),NULL,0);
    return mask;
}

void Commandline::getUsage(osg::ApplicationUsage& usage)
{
    usage.addCommandLineOption("-d <filename>","Specify the digital elevation map input file to process");
    usage.addCommandLineOption("-t <filename>","Specify the texture map input file to process");
    usage.addCommandLineOption("--building <filename>","Specify building outlines using shapefiles.");
    usage.addCommandLineOption("--forest <filename>","Specify forest outlines using shapefiles");
    usage.addCommandLineOption("-a <archivename>","Specify the archive to place the generated database");
    usage.addCommandLineOption("--ibn <buildname>","Specify the intermediate build file name");
    usage.addCommandLineOption("-o <outputfile>","Specify the output master file to generate");
    usage.addCommandLineOption("-l <numOfLevels>","Specify the number of PagedLOD levels to generate");
    usage.addCommandLineOption("--image-ext <ext>","Specify the Image format to output to via its plugin name, i.e. rgb, dds, jp2, jpeg.");
    usage.addCommandLineOption("--levels <begin_level> <end_level>","Specify the range of lavels that the next source Texture or DEM will contribute to.");
    usage.addCommandLineOption("--layer <layer_num>","Specify the layer that the next source Texture will contribute to..");
    usage.addCommandLineOption("-e <x> <y> <w> <h>","Extents of the model to generate");
    usage.addCommandLineOption("--cs <coordinates system string>","Set the coordinates system of source imagery, DEM or destination database. The string may be any of the usual GDAL/OGR forms, complete WKT, PROJ.4, EPS");     
    usage.addCommandLineOption("--wkt <WKT string>","Set the coordinates system of source imagery, DEM or destination database in WellKownText form.");     
    usage.addCommandLineOption("--wkt-file <WKT file>","Set the coordinates system of source imagery, DEM or destination database by as file containing WellKownText definition.");     
    usage.addCommandLineOption("--skirt-ratio <float>","Set the ratio of skirt height to tile size");     
    usage.addCommandLineOption("--HEIGHT_FIELD","Create a height field database");     
    usage.addCommandLineOption("--POLYGONAL","Create a height field database");     
    usage.addCommandLineOption("--TERRAIN","Create a osgTerrain::Terrain database");     
    usage.addCommandLineOption("--LOD","Create a LOD'd database");     
    usage.addCommandLineOption("--PagedLOD","Create a PagedLOD'd database");     
    usage.addCommandLineOption("-v","Set the vertical multiplier");     
    usage.addCommandLineOption("--compressed","Use OpenGL compression on RGB destination imagery");     
    usage.addCommandLineOption("--RGBA-compressed","Use OpenGL compression on RGBA destination imagery");     
    usage.addCommandLineOption("--RGB-16","Use 16bit RGB destination imagery");     
    usage.addCommandLineOption("--RGBA-24","Use 24bit RGB destination imagery");     
    usage.addCommandLineOption("--RGB-16","Use 16bit RGBA destination imagery");     
    usage.addCommandLineOption("--RGBA","Use 32bit RGBA destination imagery");     
    usage.addCommandLineOption("--vector","Interpret input as a vector data set");
    usage.addCommandLineOption("--raster","Interpret input as a raster data set (default)");
    usage.addCommandLineOption("--max-visible-distance-of-top-level","Set the maximum visible distance that the top most tile can be viewed at");     
    usage.addCommandLineOption("--no-terrain-simplification","Switch off terrain simplification.");
    usage.addCommandLineOption("--default-color <r,g,b,a>","Sets the default color of the terrain.");
    usage.addCommandLineOption("--radius-to-max-visible-distance-ratio","Set the maximum visible distance ratio for all tiles apart from the top most tile. The maximum visuble distance is computed from the ratio * tile radius.");     
    usage.addCommandLineOption("--no-mip-mapping","Disable mip mapping of textures");     
    usage.addCommandLineOption("--mip-mapping-hardware","Use mip mapped textures, and generate the mipmaps in hardware when available.");     
    usage.addCommandLineOption("--mip-mapping-imagery","Use mip mapped textures, and generate the mipmaps in imagery.");     
    usage.addCommandLineOption("--max-anisotropy","Max anisotropy level to use when texturing, defaults to 1.0.");
    usage.addCommandLineOption("--bluemarble-east","Set the coordinates system for next texture or dem to represent the eastern hemisphere of the earth.");     
    usage.addCommandLineOption("--bluemarble-west","Set the coordinates system for next texture or dem to represent the western hemisphere of the earth.");     
    usage.addCommandLineOption("--whole-globe","Set the coordinates system for next texture or dem to represent the whole hemisphere of the earth.");
    usage.addCommandLineOption("--geocentric","Build a database in geocentric (i.e. whole earth) database.");
    usage.addCommandLineOption("--radius-polar","Set the polar radius of the ellipsoid model when building a geocentric database.");
    usage.addCommandLineOption("--radius-equator","Set the polar radius of the ellipsoid model when building a geocentric database.");
    usage.addCommandLineOption("--spherical","Set the polar and equator radius both to the average of the two.");
    usage.addCommandLineOption("--range","");     
    usage.addCommandLineOption("--xx","");     
    usage.addCommandLineOption("--xt","");     
    usage.addCommandLineOption("--yy","");     
    usage.addCommandLineOption("--yt","");     
    usage.addCommandLineOption("--zz","");     
    usage.addCommandLineOption("--zt","");
    usage.addCommandLineOption("--BuildOverlays [True/False]","Switch on/off the building of overlay within the source imagery. Overlays can help reduce texture aliasing artificats.");   
    usage.addCommandLineOption("--ReprojectSources [True/False]","Switch on/off the reprojection of any source imagery that aren't in the correct projection for the database build.");   
    usage.addCommandLineOption("--GenerateTiles [True/False]","Switch on/off the generation of the output database tiles.");
    usage.addCommandLineOption("--version","Print out version");     
    usage.addCommandLineOption("--version-number","Print out version number only.");     
    usage.addCommandLineOption("--tile-image-size","Set the tile maximum image size");
    usage.addCommandLineOption("--tile-terrain-size","Set the tile maximum terrain size");
    usage.addCommandLineOption("--comment","Added a comment/description string to the top most node in the dataset");     
    usage.addCommandLineOption("-O","string option to pass to write plugins, use \" \" for multiple options");    
    usage.addCommandLineOption("--subtile <LOD> <X> <Y>","Set the subtile to begin the build from.");
    usage.addCommandLineOption("--record-subtile-on-leaf-tiles","Enable the setting of the subtile file name of the leaf tiles.");
    usage.addCommandLineOption("--split","Set the distributed build split level.");
    usage.addCommandLineOption("--splits","Set the distributed build primary and secondary split levels.");
    usage.addCommandLineOption("--run-path","Set the path that the build should be run from.");
    usage.addCommandLineOption("--notify-level","Set the notify level when logging messages.");
    usage.addCommandLineOption("--type-attribute","Set the type name which specify how the shapes should be interpreted in shapefile/dbase files.");
    usage.addCommandLineOption("--height-attribute","Set the attribute name for height attributes used in shapefile/dbase files.");
    usage.addCommandLineOption("--height","Set the height to use for asscociated shapefiles.");
    usage.addCommandLineOption("--mask","Set the mask to assign indivual shapefile/model.");
    usage.addCommandLineOption("--terrain-mask","Set the overall mask to assign terrain.");
    usage.addCommandLineOption("--read-threads-ratio <ratio>","Set the ratio number of read threads relative to number of cores to use.");
    usage.addCommandLineOption("--write-threads-ratio <ratio>","Set the ratio number of write threads relative to number of cores to use.");
    usage.addCommandLineOption("--build-options <string>","Set build options string.");
    usage.addCommandLineOption("--interpolate-terrain","Enable the use of interpolation when sampling data from source DEMs.");
    usage.addCommandLineOption("--no-interpolate-terrain","Disable the use of interpolation when sampling data from source DEMs.");
    usage.addCommandLineOption("--interpolate-imagery","Enable the use of interpolation when sampling data from source imagery.");
    usage.addCommandLineOption("--no-interpolate-imagery","Disable the use of interpolation when sampling data from source imagery.");
}

int Commandline::read(std::ostream& fout, osg::ArgumentParser& arguments, osgTerrain::Terrain* terrainInput)
{
    terrain = terrainInput;

    vpb::DatabaseBuilder* databaseBuilder = dynamic_cast<vpb::DatabaseBuilder*>(terrain->getTerrainTechnique());
    if (!databaseBuilder) 
    {
        databaseBuilder = new vpb::DatabaseBuilder;
        terrain->setTerrainTechnique(databaseBuilder);
    }
    
    buildOptions = databaseBuilder->getBuildOptions();
    if (!buildOptions)
    {
        buildOptions = new vpb::BuildOptions;
        databaseBuilder->setBuildOptions(buildOptions.get());
    }

    std::string logFilename;
    while(arguments.read("--log",logFilename)) { buildOptions->setLogFileName(logFilename); }


    float x,y,w,h;
    while (arguments.read("-e",x,y,w,h))
    {
        buildOptions->setDestinationExtents(vpb::GeospatialExtents(x,y,x+w,y+h,false)); // FIXME - need to check whether we a geographic extents of not
    }

    bool flag;
    while(arguments.read("--BuildOverlays",flag)) { buildOptions->setBuildOverlays(flag); }
    while(arguments.read("--BuildOverlays")) { buildOptions->setBuildOverlays(true); }

    while(arguments.read("--ReprojectSources",flag)) { buildOptions->setReprojectSources(flag); }
    while(arguments.read("--ReprojectSources")) { buildOptions->setReprojectSources(true); }

    while(arguments.read("--GenerateTiles",flag)) { buildOptions->setGenerateTiles(flag); }
    while(arguments.read("--GenerateTiles")) { buildOptions->setGenerateTiles(true); }

    unsigned int subtileLevel, subtileX, subtileY;
    while(arguments.read("--subtile",subtileLevel, subtileX, subtileY))
    {
        buildOptions->setGenerateSubtile(true);
        buildOptions->setSubtileLevel(subtileLevel);
        buildOptions->setSubtileX(subtileX);
        buildOptions->setSubtileY(subtileY);
    }

    unsigned int splitLevel;
    while(arguments.read("--split",splitLevel))
    {
        buildOptions->setDistributedBuildSplitLevel(splitLevel);
    }

    unsigned int secondarySplitLevel;
    while(arguments.read("--splits",splitLevel, secondarySplitLevel))
    {
        buildOptions->setDistributedBuildSplitLevel(splitLevel);
        buildOptions->setDistributedBuildSecondarySplitLevel(secondarySplitLevel);
    }

    while(arguments.read("--record-subtile-on-leaf-tiles"))
    {
        buildOptions->setRecordSubtileFileNamesOnLeafTile(true);
    }

    std::string notifyLevel;
    while(arguments.read("--notify-level", notifyLevel))
    {
        buildOptions->setNotifyLevel(notifyLevel);
    }

    while(arguments.read("--disable-writes"))
    {
        buildOptions->setDisableWrites(true);
    }

    while(arguments.read("--interpolate-terrain"))
    {
        buildOptions->setUseInterpolatedTerrainSampling(true);
    }

    while(arguments.read("--interpolate-imagery"))
    {
        buildOptions->setUseInterpolatedImagerySampling(true);
    }

    while(arguments.read("--no-interpolate-terrain"))
    {
        buildOptions->setUseInterpolatedTerrainSampling(false);
    }

    while(arguments.read("--no-interpolate-imagery"))
    {
        buildOptions->setUseInterpolatedImagerySampling(false);
    }

    std::string buildname;    
    while (arguments.read("--ibn",buildname))
    {
        buildOptions->setIntermediateBuildName(buildname);
    }

    std::string maskstring;
    while (arguments.read("--terrain-mask",maskstring))
    {
        terrainmask = readMask(maskstring);
    }

    while (arguments.read("--HEIGHT_FIELD") || arguments.read("--height-field") )
    {
        buildOptions->setGeometryType(vpb::BuildOptions::HEIGHT_FIELD);
    }

    while (arguments.read("--POLYGONAL") || arguments.read("--polygonal") )
    {
        buildOptions->setGeometryType(vpb::BuildOptions::POLYGONAL);
    }

    while (arguments.read("--TERRAIN") || arguments.read("--terrain"))
    {
        buildOptions->setGeometryType(vpb::BuildOptions::TERRAIN);
    }

    while (arguments.read("--LOD"))
    {
        buildOptions->setDatabaseType(vpb::BuildOptions::LOD_DATABASE);
    }
    
    while (arguments.read("--PagedLOD"))
    {
        buildOptions->setDatabaseType(vpb::BuildOptions::PagedLOD_DATABASE);
    }

    while (arguments.read("--compressed")) { buildOptions->setTextureType(vpb::BuildOptions::COMPRESSED_TEXTURE); }
    while (arguments.read("--RGBA-compressed")) { buildOptions->setTextureType(vpb::BuildOptions::COMPRESSED_RGBA_TEXTURE); }
    while (arguments.read("--RGB_16") || arguments.read("--RGB-16") ) { buildOptions->setTextureType(vpb::BuildOptions::RGB_16); }
    while (arguments.read("--RGBA_16") || arguments.read("--RGBA-16") ) { buildOptions->setTextureType(vpb::BuildOptions::RGBA_16); }
    while (arguments.read("--RGB_24") || arguments.read("--RGB-24") ) { buildOptions->setTextureType(vpb::BuildOptions::RGB_24); }
    while (arguments.read("--RGBA") || arguments.read("--RGBA") ) { buildOptions->setTextureType(vpb::BuildOptions::RGBA); }

    while (arguments.read("--no_mip_mapping") || arguments.read("--no-mip-mapping")) { buildOptions->setMipMappingMode(vpb::BuildOptions::NO_MIP_MAPPING); }
    while (arguments.read("--mip_mapping_hardware") || arguments.read("--mip-mapping-hardware")) { buildOptions->setMipMappingMode(vpb::BuildOptions::MIP_MAPPING_HARDWARE); }
    while (arguments.read("--mip_mapping_imagery") || arguments.read("--mip-mapping-imagery")) { buildOptions->setMipMappingMode(vpb::BuildOptions::MIP_MAPPING_IMAGERY); }

    float maxAnisotropy;
    while (arguments.read("--max_anisotropy",maxAnisotropy) || arguments.read("--max-anisotropy",maxAnisotropy))
    {
        buildOptions->setMaxAnisotropy(maxAnisotropy);
    }

    unsigned int image_size;
    while (arguments.read("--tile-image-size",image_size)) { buildOptions->setMaximumTileImageSize(image_size); }

    unsigned int terrain_size;
    while (arguments.read("--tile-terrain-size",terrain_size)) { buildOptions->setMaximumTileTerrainSize(terrain_size); }

    std::string comment;
    while (arguments.read("--comment",comment)) { buildOptions->setCommentString(comment); }


    std::string archiveName;
    while (arguments.read("-a",archiveName)) { buildOptions->setArchiveName(archiveName); }

    unsigned int numLevels = 10;
    while (arguments.read("-l",numLevels)) { buildOptions->setMaximumNumOfLevels(numLevels); }

    while (arguments.read("--otd") || 
           arguments.read("--output-task-directories") ||
           arguments.read("--subtile-directories"))
    {
        buildOptions->setOutputTaskDirectories(true);
    }


    float verticalScale;
    while (arguments.read("-v",verticalScale))
    {
        buildOptions->setVerticalScale(verticalScale);
    }

    float skirtRatio;
    while (arguments.read("--skirt-ratio",skirtRatio))
    {
        buildOptions->setSkirtRatio(skirtRatio);
    }

    float maxVisibleDistanceOfTopLevel;
    while (arguments.read("--max_visible_distance_of_top_level",maxVisibleDistanceOfTopLevel) ||
          arguments.read("--max-visible-distance-of-top-level",maxVisibleDistanceOfTopLevel) )
    {
        buildOptions->setMaximumVisibleDistanceOfTopLevel(maxVisibleDistanceOfTopLevel);
    }

    float radiusToMaxVisibleDistanceRatio;
    while (arguments.read("--radius_to_max_visible_distance_ratio",radiusToMaxVisibleDistanceRatio) ||
           arguments.read("--radius-to-max-visible-distance-ratio",radiusToMaxVisibleDistanceRatio))
    {
        buildOptions->setRadiusToMaxVisibleDistanceRatio(radiusToMaxVisibleDistanceRatio);
    }

    while (arguments.read("--no_terrain_simplification") ||
           arguments.read("--no-terrain-simplification"))
    {
        buildOptions->setSimplifyTerrain(false);
    }

    while (arguments.read("--geocentric"))
    {
        buildOptions->setConvertFromGeographicToGeocentric(true);
        fout<<"--geocentric "<<std::endl;
    }

    double radius;
    while (arguments.read("--radius-polar", radius))
    {
        osg::EllipsoidModel* ellipsoid = buildOptions->getEllipsoidModel();
        ellipsoid->setRadiusPolar(radius);
    }
    
    while (arguments.read("--radius-equator", radius))
    {
        osg::EllipsoidModel* ellipsoid = buildOptions->getEllipsoidModel();
        ellipsoid->setRadiusEquator(radius);
    }
    
    while (arguments.read("--spherical"))
    {
        osg::EllipsoidModel* ellipsoid = buildOptions->getEllipsoidModel();
        double radius = (ellipsoid->getRadiusPolar() + ellipsoid->getRadiusEquator())*0.5;
        ellipsoid->setRadiusPolar(radius);
        ellipsoid->setRadiusEquator(radius);
        fout<<"--spherical, new radius set to "<<radius<<std::endl;
    }


    std::string str;
    while (arguments.read("--default-color",str) ||
           arguments.read("--default_color",str))
    {
        osg::Vec4 defaultColor;
        if( sscanf( str.c_str(), "%f,%f,%f,%f",
                &defaultColor[0], &defaultColor[1], &defaultColor[2], &defaultColor[3] ) != 4 )
        {
            fout<<"Color argument format incorrect."<<std::endl;
            return 1;
        }
        buildOptions->setDefaultColor(defaultColor);
    }

    std::string image_ext;
    while (arguments.read("--image-ext",image_ext))
    {
        std::string::size_type dot = image_ext.find_last_of('.');
        if (dot!=std::string::npos) image_ext.erase(0,dot+1);

        osgDB::ReaderWriter* rw = osgDB::Registry::instance()->getReaderWriterForExtension(image_ext);
        if (rw) 
        {
            image_ext.insert(0,".");
            buildOptions->setDestinationImageExtension(image_ext);
        }
        else
        {
            fout<<"Error: can not find plugin to write out image with extension '"<<image_ext<<"'"<<std::endl;
            return 1;
        }
    }

    while (arguments.read("--height-attribute",heightAttributeName)) {}

    while (arguments.read("--type-attribute",typeAttributeName)) {}

    float ratio=0.0f;
    while(arguments.read("--read-threads-ratio",ratio)) { buildOptions->setNumReadThreadsToCoresRatio(ratio); }
    while(arguments.read("--write-threads-ratio",ratio)) { buildOptions->setNumWriteThreadsToCoresRatio(ratio); }


    while (arguments.read("--build-options",str) || arguments.read("--bo",str))
    {
        osg::notify(osg::NOTICE)<<"Setting --bo "<<str<<std::endl;
        buildOptions->setBuildOptionsString(str);
        osg::notify(osg::NOTICE)<<"Set --bo "<<buildOptions->getBuildOptionsString()<<std::endl;
    }

    if (arguments.read("-O",str))
    {
        osgDB::ReaderWriter::Options* options = new osgDB::ReaderWriter::Options;
        options->setOptionString(str);
        osgDB::Registry::instance()->setOptions(options);
    }

    std::string filename;
    double xMin, xMax, yMin, yMax;
    
    int pos = 1;
    while(pos<arguments.argc())
    {
        std::string def;

        if (arguments.read(pos, "--height",heightAttribute)) 
        {            
        }
        else if (arguments.read(pos, "--type",typeAttribute)) 
        {            
        }
        else if (arguments.read(pos, "--mask",def)) 
        {
            mask = readMask(def);
        }
        else if (arguments.read(pos, "--cs",def))
        {
            currentCS = !def.empty() ? vpb::coordinateSystemStringToWTK(def) : "";
            fout<<"--cs \""<<def<<"\" converted to "<<currentCS<<std::endl;
        }
        else if (arguments.read(pos, "--wkt",def))
        {
            currentCS = def;
            fout<<"--wkt "<<currentCS<<std::endl;
        }
        else if (arguments.read(pos, "--wkt-file",def))
        {
            std::ifstream in(def.c_str());
            if (in)
            {   
                currentCS = "";
                while (!in.eof())
                {
                    std::string line;
                    in >> line;
                    currentCS += line;
                }
                fout<<"--wkt-file "<<currentCS<<std::endl;
            }
        }
        else if (arguments.read(pos, "--bluemarble-east"))
        {
            currentCS = vpb::coordinateSystemStringToWTK("WGS84");
            computeGeoTransForRange(0.0, 180.0, -90.0, 90.0);
            
            fout<<"--bluemarble-east "<<currentCS<<" matrix="<<geoTransform<<std::endl;
        }

        else if (arguments.read(pos, "--bluemarble-west"))
        {
            currentCS = vpb::coordinateSystemStringToWTK("WGS84");
            computeGeoTransForRange(-180.0, 0.0, -90.0, 90.0);
            
            fout<<"--bluemarble-west "<<currentCS<<" matrix="<<geoTransform<<std::endl;
        }

        else if (arguments.read(pos, "--whole-globe"))
        {
            currentCS = vpb::coordinateSystemStringToWTK("WGS84");
            computeGeoTransForRange(-180.0, 180.0, -90.0, 90.0);
            
            fout<<"--whole-globe "<<currentCS<<" matrix="<<geoTransform<<std::endl;
        }

        else if (arguments.read(pos, "--range", xMin, xMax, yMin, yMax))
        {
            computeGeoTransForRange( xMin, xMax, yMin, yMax);
            
            fout<<"--range, matrix="<<geoTransform<<std::endl;
        }

        else if (arguments.read(pos, "--identity"))
        {
            geoTransformSet = false;
            geoTransform.makeIdentity();            
        }

        // x vector
        else if (arguments.read(pos, "--xx",geoTransform(0,0)))
        {
           geoTransformSet = true;
           geoTransformScale = false;
           fout<<"--xx "<<geoTransform(0,0)<<std::endl;
        }
        else if (arguments.read(pos, "--xy",geoTransform(1,0)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--xy "<<geoTransform(1,0)<<std::endl;
        }
        else if (arguments.read(pos, "--xz",geoTransform(2,0)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--xz "<<geoTransform(2,0)<<std::endl;
        }
        else if (arguments.read(pos, "--xt",geoTransform(3,0)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--xo "<<geoTransform(3,0)<<std::endl;
        }

        // y vector
        else if (arguments.read(pos, "--yx",geoTransform(0,1)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--yx "<<geoTransform(0,1)<<std::endl;
        }
        else if (arguments.read(pos, "--yy",geoTransform(1,1)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--yy "<<geoTransform(1,1)<<std::endl;
        }
        else if (arguments.read(pos, "--yz",geoTransform(2,1)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--yz "<<geoTransform(2,1)<<std::endl;
        }
        else if (arguments.read(pos, "--yt",geoTransform(3,1)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--yt "<<geoTransform(3,1)<<std::endl;
        }

        // z vector
        else if (arguments.read(pos, "--zx",geoTransform(0,2)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--zx "<<geoTransform(0,2)<<std::endl;
        }
        else if (arguments.read(pos, "--zy",geoTransform(1,2)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--zy "<<geoTransform(1,2)<<std::endl;
        }
        else if (arguments.read(pos, "--zz",geoTransform(2,2)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--zz "<<geoTransform(2,2)<<std::endl;
        }
        else if (arguments.read(pos, "--zt",geoTransform(3,2)))
        {
            geoTransformSet = true;
            geoTransformScale = false;
            fout<<"--zt "<<geoTransform(3,2)<<std::endl;
        }

        else if (arguments.read(pos, "--levels", min_level, max_level))
        {
            minmaxLevelSet = true;
            fout<<"--levels, min_level="<<min_level<<"  max_level="<<max_level<<std::endl;
        }
        
        else if (arguments.read(pos, "--layer", layerNum))
        {
            fout<<"--layer layeNumber="<<layerNum<<std::endl;
        }

        else if (arguments.read(pos, "--vector"))
        {
            dataType = vpb::SpatialProperties::VECTOR;
            fout<<"--vector input data"<<std::endl;
        }

        else if (arguments.read(pos, "--raster"))
        {
            dataType = vpb::SpatialProperties::RASTER;
            fout<<"--raster input data"<<std::endl;
        }

        else if (arguments.read(pos, "-d",filename))
        {
            fout<<"-d "<<filename<<std::endl;

            processFile(vpb::Source::HEIGHT_FIELD, filename);
            reset();
        }
        else if (arguments.read(pos, "-t",filename))
        {
            fout<<"-t "<<filename<<std::endl;

            processFile(vpb::Source::IMAGE, filename);
            reset();
        }
        else if (arguments.read(pos, "-m",filename))
        {
            fout<<"-m "<<filename<<std::endl;

            processFile(vpb::Source::MODEL, filename);
            reset();
        }
        else if (arguments.read(pos, "--buildings",filename) || arguments.read(pos, "--building",filename) || arguments.read(pos, "-b",filename))
        {
            fout<<"--buildings "<<filename<<std::endl;

            typeAttribute = "Building";
            processFile(vpb::Source::SHAPEFILE, filename);
            reset();
        }
        else if (arguments.read(pos, "--forest",filename) || arguments.read(pos, "-f",filename))
        {
            fout<<"--forest "<<filename<<std::endl;
            typeAttribute = "Forest";
            processFile(vpb::Source::SHAPEFILE, filename);
            reset();
        }
        else if (arguments.read(pos, "--sf",filename))
        {
            fout<<"--sf "<<filename<<std::endl;
            processFile(vpb::Source::SHAPEFILE, filename);
            reset();
        }
        else if (arguments.read(pos, "-o",filename)) 
        {
            fout<<"-o "<<filename<<std::endl;
            buildOptions->setDestinationName(filename);
            
            if (!currentCS.empty()) buildOptions->setDestinationCoordinateSystem(currentCS);

            reset();
        }
        else
        {
            // if no argument read advance to next argument.
            ++pos;
        }
    }
    return 0;    
}
