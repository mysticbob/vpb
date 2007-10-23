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

#include <vpb/MachinePool>
#include <vpb/Task>

#include <osg/GraphicsThread>

#include <iostream>

using namespace vpb;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  MachineOperation
//
MachineOperation::MachineOperation(Task* Task):
    osg::Operation(Task->getFileName(), false),
    _Task(Task)
{
}

void MachineOperation::operator () (osg::Object* object)
{
    Machine* machine = dynamic_cast<Machine*>(object);
    if (machine)
    {

        std::string application;
        if (_Task->getProperty("application",application))
        {
            _Task->setProperty("hostname",machine->getHostName());
            
            _Task->write();

            std::string executionString = machine->getCommandPrefix() + std::string(" ") + application;

            std::string arguments;
            if (_Task->getProperty("arguments",arguments))
            {
                executionString += std::string(" ") + arguments;
            }

            if (machine->getCommandPrefix().empty())
            {
                executionString += std::string(" ") + machine->getCommandPrefix();
            }

            system(executionString.c_str());
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  BlockOperation
//
BlockOperation::BlockOperation():
    osg::Operation("Block", false)
{
}

void BlockOperation::release()
{
    Block::release();
}

void BlockOperation::operator () (osg::Object* object)
{
    Block::release();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Machine
//
Machine::Machine()
{
}

Machine::Machine(const Machine& m, const osg::CopyOp& copyop):
    osg::Object(m, copyop),
    _hostname(m._hostname),
    _commandPrefix(m._commandPrefix),
    _commandPostfix(m._commandPostfix)
{
}

Machine::Machine(const std::string& hostname,const std::string& commandPrefix, const std::string& commandPostfix, int numThreads):
    _hostname(hostname),
    _commandPrefix(commandPrefix),
    _commandPostfix(commandPostfix)
{
    if (numThreads<0)
    {
        // autodetect
        numThreads = 1;
    }
    
    for(int i=0; i<numThreads; ++i)
    {
        osg::OperationThread* thread = new osg::OperationThread;
        thread->setParent(this);
        _threads.push_back(thread);
    }
}

Machine::~Machine()
{
}

void Machine::setOperationQueue(osg::OperationQueue* queue)
{
    for(Threads::iterator itr = _threads.begin();
        itr != _threads.end();
        ++itr)
    {
        (*itr)->setOperationQueue(queue);
    }
}

unsigned int Machine::getNumThreadsActive() const
{
    unsigned int numThreadsActive = 0;
    for(Threads::const_iterator itr = _threads.begin();
        itr != _threads.end();
        ++itr)
    {
        if ((*itr)->getCurrentOperation().valid())
        {        
            ++numThreadsActive;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  MachinePool
//

MachinePool::MachinePool()
{
    _operationQueue = new osg::OperationQueue;
}

MachinePool::~MachinePool()
{
}

void MachinePool::addMachine(const std::string& hostname,const std::string& commandPrefix, const std::string& commandPostfix, int numThreads)
{
    addMachine(new Machine(hostname, commandPrefix, commandPostfix, numThreads));
}

void MachinePool::addMachine(Machine* machine)
{
    machine->setOperationQueue(_operationQueue.get());
    
    _machines.push_back(machine);
}

void MachinePool::run(Task* task)
{
    _operationQueue->add(new MachineOperation(task));
}

void MachinePool::waitForCompletion()
{
    osg::ref_ptr<BlockOperation> block = new BlockOperation;
    
    // wait till the operaion queu has been flushed.
    _operationQueue->add(block.get());
    

    // there can still be operations running though so need to double check.
    while(getNumThreadsActive()>0)
    {
        OpenThreads::Thread::YieldCurrentThread();
    }
}

unsigned int MachinePool::getNumThreads() const
{
    unsigned int numThreads = 0;
    for(Machines::const_iterator itr = _machines.begin();
        itr != _machines.end();
        ++itr)
    {
        numThreads += (*itr)->getNumThreads();
    }
    return numThreads;
}

unsigned int MachinePool::getNumThreadsActive() const
{
    unsigned int numThreadsActive = 0;
    for(Machines::const_iterator itr = _machines.begin();
        itr != _machines.end();
        ++itr)
    {
        numThreadsActive += (*itr)->getNumThreadsActive();
    }
    return numThreadsActive;
}

bool MachinePool::read(const std::string& filename)
{
    std::cout<<"MachinePool::read() still in developement."<<std::endl;
    return false;
}

bool MachinePool::write(const std::string& filename)
{
    std::cout<<"MachinePool::write() still in developement."<<std::endl;
    return false;
}
