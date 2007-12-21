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

#include <vpb/BuildOperation>
#include <vpb/ThreadPool>

using namespace vpb;

BuildOperation::BuildOperation(BuildLog* buildLog, const std::string& name, bool keep):
    osg::Operation(name,keep),
    _buildLog(buildLog)
{
    _log = new OperationLog(name);
    
    if (_buildLog.valid())
    {
        _buildLog->pendingOperation(this);
    }
}

void BuildOperation::operator () (osg::Object* object)
{
    ThreadPool* threadPool = dynamic_cast<ThreadPool*>(object);

    if (threadPool) threadPool->runningOperation(this);

    pushOperationLog(_log.get());

    if (_buildLog.valid())
    {
        _buildLog->runningOperation(this);
        _log->setLogFile(_buildLog->getLogFile());
    }
    
    build();
    
    if (_buildLog.valid()) _buildLog->completedOperation(this);
    
    popOperationLog();

    if (threadPool) threadPool->completedOperation(this);

}

