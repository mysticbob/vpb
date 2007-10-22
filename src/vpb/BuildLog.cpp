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

#include <vpb/BuildLog>
#include <vpb/BuildOperation>

#include <iostream>

using namespace vpb;

struct ThreadLog
{
    typedef std::list<osg::ref_ptr<OperationLog> > OperationLogStack;
    
    void push(OperationLog* log) { _logStack.push_back(log); }
    void pop() { if (!_logStack.empty()) _logStack.pop_back(); }
    
    void log(osg::NotifySeverity level, const char* message, va_list args)
    { 
        if (!_logStack.empty()) _logStack.back()->log(level, message, args);
        else if (level<=osg::getNotifyLevel()) { vprintf(message, args); printf("\n"); }
    }

    OperationLogStack _logStack;
    
};

typedef std::map<OpenThreads::Thread*, ThreadLog > OperationLogMap;
static OpenThreads::Mutex s_opertionLogMapMutex;
static OperationLogMap s_opertionLogMap;

void vpb::log(osg::NotifySeverity level, const char* format, ...)
{
    OpenThreads::Thread* thread = OpenThreads::Thread::CurrentThread();

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_opertionLogMapMutex);
    
    ThreadLog& tl = s_opertionLogMap[thread];
    
    va_list args; va_start(args, format);
    tl.log(level, format, args);
    va_end(args);
}

void vpb::log(osg::NotifySeverity level, const char* message, va_list args)
{
    OpenThreads::Thread* thread = OpenThreads::Thread::CurrentThread();

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_opertionLogMapMutex);
    
    ThreadLog& tl = s_opertionLogMap[thread];
    return tl.log(level, message, args);
}

void vpb::pushOperationLog(OperationLog* operationLog)
{
    OpenThreads::Thread* thread = OpenThreads::Thread::CurrentThread();

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_opertionLogMapMutex);
    s_opertionLogMap[thread].push(operationLog);
}

void vpb::popOperationLog()
{
    OpenThreads::Thread* thread = OpenThreads::Thread::CurrentThread();

    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(s_opertionLogMapMutex);
    s_opertionLogMap[thread].pop();
}

//////////////////////////////////////////////////////////////////////////////////////////////
//
//
//  LogFile


LogFile::LogFile(const std::string& filename)
{
    _fout.open(filename.c_str());
}

void LogFile::write(Message* message)
{
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_mutex);
    _fout<<message->time<<"\t:"<<message->message<<std::endl;
    
    if (_taskFile.valid())
    {
        _taskFile->setProperty("last message time",message->time);
        _taskFile->setProperty("last message",message->message);
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////
//
//
//  OperationLog

OperationLog::OperationLog():
    Object(true),
    _startPendingTime(-1.0),
    _startRunningTime(-1.0),
    _endRunningTime(-1.0)
{
}

OperationLog::OperationLog(const std::string& name):
    Object(true),
    _startPendingTime(-1.0),
    _startRunningTime(-1.0),
    _endRunningTime(-1.0)
{
    setName(name);
}


OperationLog::OperationLog(const OperationLog& log, const osg::CopyOp& copyop):
    osg::Object(log,copyop),
    _startPendingTime(log._startPendingTime),
    _startRunningTime(log._startRunningTime),
    _endRunningTime(log._endRunningTime)
{
}

OperationLog::~OperationLog()
{
}

void OperationLog::log(osg::NotifySeverity level, const char* format, ...)
{
    char str[1024];
    va_list args; va_start(args, format);
    vsprintf(str, format, args);
    va_end(args);
    
    Message* message = new Message(osg::Timer::instance()->time_s(), level, str);
    _messages.push_back(message);
    if (_logFile.valid()) _logFile->write(message);
}

void OperationLog::log(osg::NotifySeverity level, const char* format, va_list args)
{
    char str[1024];
    vsprintf(str, format, args);

    Message* message = new Message(osg::Timer::instance()->time_s(), level, str);
    _messages.push_back(message);
    if (_logFile.valid()) _logFile->write(message);
}

void OperationLog::report(std::ostream& out)
{
    out<<getName()<<":: waiting time: "<<getWaitingTime()<<" running time: "<<getRunningTime()<<std::endl;
    for(Messages::iterator itr = _messages.begin();
        itr != _messages.end();
        ++itr)
    {
        out<<"    "<<(*itr)->time<<" : "<<(*itr)->message<<std::endl;
    }
    out<<std::endl;
}


//////////////////////////////////////////////////////////////////////////////////////////////
//
//
//  BuildLog

BuildLog::BuildLog():
    OperationLog()
{
}

BuildLog::BuildLog(const std::string& name):
    OperationLog(name)
{
}

BuildLog::BuildLog(const BuildLog& bl, const osg::CopyOp& copyop):
    OperationLog(bl,copyop)
{
}


void BuildLog::initStartTime()
{
    osg::Timer::instance()->setStartTick(osg::Timer::instance()->tick());
}

void BuildLog::pendingOperation(BuildOperation* operation)
{
    OperationLog* log = operation->getOperationLog();
    
    if (log)
    {
        log->setStartPendingTime(getCurrentTime());

        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_pendingOperationsMutex);
        _pendingOperations.push_back(log);
    }
}

void BuildLog::runningOperation(BuildOperation* operation)
{
    OperationLog* log = operation->getOperationLog();
    
    if (log)
    {
        log->setStartRunningTime(getCurrentTime());

        {
            OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_runningOperationsMutex);
            _runningOperations.push_back(log);
        }
        
        {
            OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_pendingOperationsMutex);
            remove(_pendingOperations, log);
        }
        
    }
}


void BuildLog::completedOperation(BuildOperation* operation)
{
    OperationLog* log = operation->getOperationLog();
    
    if (log)
    {
        log->setEndRunningTime(getCurrentTime());

        {
            OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_completedOperationsMutex);
            _completedOperations.push_back(log);
        }
        
        {
            OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_runningOperationsMutex);
            remove(_runningOperations, log);
        }
    }
}


void BuildLog::remove(OperationLogs& logs, OperationLog* log)
{
    OperationLogs::iterator itr = std::find(logs.begin(), logs.end(), log);
    if (itr != logs.end())
    {
        logs.erase(itr);
    }
}

bool BuildLog::isComplete() const
{
    unsigned int numOutstandingOperations = 0;

    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_pendingOperationsMutex);
        numOutstandingOperations += _pendingOperations.size();
    }

    {
        OpenThreads::ScopedLock<OpenThreads::Mutex> lock(_runningOperationsMutex);
        numOutstandingOperations += _runningOperations.size();
    }

    return numOutstandingOperations==0;
}

void BuildLog::waitForCompletion() const
{
    while(!isComplete())
    {
        OpenThreads::Thread::YieldCurrentThread();
    }
}


void BuildLog::report(std::ostream& out)
{
    out<<"BuildLog::report"<<std::endl;
    out<<"================"<<std::endl;
    
    OperationLog::report(out);
    
    out<<std::endl<<"Pending Operations   "<<_pendingOperations.size()<<std::endl;

    for(OperationLogs::iterator itr = _pendingOperations.begin();
        itr != _pendingOperations.end();
        ++itr)
    {
        (*itr)->report(out);
    }
    
    out<<std::endl<<"Runnning Operations  "<<_runningOperations.size()<<std::endl;
    for(OperationLogs::iterator itr = _runningOperations.begin();
        itr != _runningOperations.end();
        ++itr)
    {
        (*itr)->report(out);
    }
    
    out<<std::endl<<"Completed Operations "<<_completedOperations.size()<<std::endl;
    for(OperationLogs::iterator itr = _completedOperations.begin();
        itr != _completedOperations.end();
        ++itr)
    {
        (*itr)->report(out);
    }

}

