/*******************************************************************************
 * JNotify - Allow java applications to register to File system events.
 * 
 * Copyright (C) 2005 - Content Objects
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 ******************************************************************************
 *
 * Content Objects, Inc., hereby disclaims all copyright interest in the
 * library `JNotify' (a Java library for file system events). 
 * 
 * Yahali Sherman, 21 November 2005
 *    Content Objects, VP R&D.
 *    
 ******************************************************************************
 * Author : Omry Yadan
 ******************************************************************************/


#include "Win32FSHook.h"
#include <stdio.h>
#include <windows.h>
#include <winbase.h>
#include <winnt.h>
#include <time.h>
#include <stdio.h>
//#include "Lock.h"
#include "WatchData.h"
//#include "Logger.h"

Win32FSHook *Win32FSHook::instance = 0;

Win32FSHook::Win32FSHook() 
{
	_callback = 0;
	_mainLoopThreadHandle = INVALID_HANDLE_VALUE;
	_eventsThread = INVALID_HANDLE_VALUE;
	_isRunning = false;
	InitializeCriticalSection(&_cSection);
	InitializeCriticalSection(&_eventQueueLock);
	_eventQueueEvent = CreateEvent(NULL, FALSE,FALSE, NULL);
}

void Win32FSHook::init(ChangeCallback callback)
{
	instance = this;
	_callback = callback;
	if (!_isRunning)
	{
		_completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
		if (_completionPort == NULL)
		{
			throw GetLastError();
		}
		_isRunning = true;
		
	    DWORD dwThreadId;
	    LPVOID dwThrdParam = (LPVOID)this; 
	    _mainLoopThreadHandle = CreateThread( 
	        NULL,                        // default security attributes 
	        0,                           // use default stack size  
	        Win32FSHook::mainLoop,       // thread function 
	        dwThrdParam,                // argument to thread function 
	        0,                           // use default creation flags 
	        &dwThreadId);                // returns the thread identifier 
	 
		if (_mainLoopThreadHandle == NULL) 
		{
			throw ERR_INIT_THREAD;
		}

		SetThreadPriority(_mainLoopThreadHandle, THREAD_PRIORITY_TIME_CRITICAL);
		_eventsThread = CreateThread(
	        NULL,                        // default security attributes
	        0,                           // use default stack size
	        Win32FSHook::eventLoop,       // thread function
	        dwThrdParam,                // argument to thread function
	        0,                           // use default creation flags
	        &dwThreadId);                // returns the thread identifier

		if (_eventsThread == NULL)
		{
			CloseHandle(_mainLoopThreadHandle);
			throw ERR_INIT_THREAD;
		}
	}
}

Win32FSHook::~Win32FSHook()
{
//	debug("+Win32FSHook destructor");
	// terminate thread.
	_isRunning = false;
	SetEvent(_eventQueueEvent);
	PostQueuedCompletionStatus(_completionPort, EXIT_SIGNAL, (ULONG_PTR)NULL, NULL);

	// cleanup
	if (INVALID_HANDLE_VALUE != _mainLoopThreadHandle) CloseHandle(_mainLoopThreadHandle);
	if (INVALID_HANDLE_VALUE != _eventsThread) CloseHandle(_eventsThread);
	CloseHandle(_completionPort);
	DeleteCriticalSection(&_cSection);
//	debug("-Win32FSHook destructor");
}

void Win32FSHook::remove_watch(int wd)
{
//	debug("+remove_watch(%d)", wd);
	EnterCriticalSection(&_cSection);
	map <int, WatchData*>::const_iterator i = _wid2WatchData.find(wd);
	if (i == _wid2WatchData.end())
	{
//		debug("remove_watch: watch id %d not found", wd);
	}
	else
	{
		WatchData *wd = i->second;
		int res = wd->unwatchDirectory();
		if (res != 0)
        {
//			log("Error canceling watch on dir %ls : %d",wd->getPath(), res);
        }
		int res2 = _wid2WatchData.erase(wd->getId());
		if (res2 != 1)
        {
//			log("Error deleting watch %d from map, res=%d",wd->getId(), res2);
        }
		PostQueuedCompletionStatus(_completionPort, DELETE_WD_SIGNAL, (ULONG_PTR)wd, NULL);
	}
	LeaveCriticalSection(&_cSection);
//	debug("-remove_watch(%d)", wd);
}

int Win32FSHook::add_watch(const WCHAR* path, long notifyFilter, bool watchSubdirs, DWORD &error)
{
//	debug("+add_watch(%ls)", path);
	// synchronize access by multiple threads
	EnterCriticalSection(&_cSection);
	try
	{
		WatchData *watchData = new WatchData(path, notifyFilter, watchSubdirs, _completionPort);
		if (watchData == 0) throw (DWORD)8; //ERROR_NOT_ENOUGH_MEMORY
		int watchId = watchData->getId();
		_wid2WatchData[watchId] = watchData;
		watchDirectory(watchData);
		LeaveCriticalSection(&_cSection);
		return watchId;
	}
	catch (DWORD err)
	{
		error = err;
		LeaveCriticalSection(&_cSection);
		return 0;	
	}
}

WatchData* Win32FSHook::find(int wd)
{
	WatchData *watchData = 0;
	EnterCriticalSection(&_cSection);
	map <int, WatchData*>::const_iterator it = _wid2WatchData.find(wd);
	if (it != _wid2WatchData.end())
	{
		watchData = it->second;
	}
	LeaveCriticalSection(&_cSection);
	return watchData;
}

DWORD WINAPI Win32FSHook::mainLoop( LPVOID lpParam )
{
//	debug("mainLoop starts");
	Win32FSHook* _this = (Win32FSHook*)lpParam;

	HANDLE hPort = _this->_completionPort;
	DWORD dwNoOfBytes = 0;
	ULONG_PTR ulKey = 0;
	OVERLAPPED* pov = NULL;
	WCHAR name[4096];

	while (_this->_isRunning)
	{
	    pov = NULL;
	    BOOL fSuccess = GetQueuedCompletionStatus(
	                    hPort,         // Completion port handle
	                    &dwNoOfBytes,  // Bytes transferred
	                    &ulKey,
	                    &pov,          // OVERLAPPED structure
	                    INFINITE       // Notification time-out interval
	                    );
	    if (fSuccess)
	    {
	    	if (dwNoOfBytes == 0) continue; // happens when removing a watch some times.
	    	if (dwNoOfBytes == EXIT_SIGNAL)
	    		continue;
	    	if (dwNoOfBytes == DELETE_WD_SIGNAL)
	    	{
	    		WatchData *wd = (WatchData*)ulKey;
	    		delete wd;
	    		continue;
	    	}

	    	int wd = (int)ulKey;
//	    	EnterCriticalSection(&_this->_cSection);
	    	WatchData *watchData = _this->find(wd);
	    	if (!watchData)
	    	{
//	    		log("mainLoop : ignoring event for watch id %d, no longer in wid2WatchData map", wd);
	    		continue;
	    	}

	    	const char* buffer = watchData->getBuffer();
//	    	char buffer[watchData->getBufferSize()];
//	    	memcpy(buffer, watchData->getBuffer(), dwNoOfBytes);
//			LeaveCriticalSection(&_this->_cSection);
	    	FILE_NOTIFY_INFORMATION *event;
	    	DWORD offset = 0;
	    	do
	    	{
	    		event = (FILE_NOTIFY_INFORMATION*)(buffer+offset);
	    		int action = event->Action;
	    		DWORD len = event->FileNameLength / sizeof(WCHAR);
	    		for (DWORD k=0;k<len && k < (sizeof(name)-sizeof(WCHAR))/sizeof(WCHAR);k++)
	    		{
	    			name[k] = event->FileName[k];
	    		}
	    		name[len] = 0;

				_this->postEvent(new Event(watchData->getId(), action, watchData->getPath(), name));
	    		offset += event->NextEntryOffset;
	    	}
	    	while (event->NextEntryOffset != 0);

	    	int res = watchData->watchDirectory();
	    	if (res != 0)
	    	{
//	    		log("Error watching dir %s : %d",watchData->getPath(), res);
	    	}
	    }
	    else
	    {
//	    	log("GetQueuedCompletionStatus returned an error");
	    }
	}
//	debug("mainLoop exits");
	return 0;
}


void Win32FSHook::watchDirectory(WatchData* wd)
{
//	debug("Watching %ls", wd->getPath());
	int res = wd->watchDirectory();
	if (res != 0)
	{
//		log("Error watching dir %ls : %d",wd->getPath(), res);
	}
}

void Win32FSHook::postEvent(Event *event)
{
	EnterCriticalSection(&_eventQueueLock);
	_eventQueue.push(event);
	LeaveCriticalSection(&_eventQueueLock);
	SetEvent(_eventQueueEvent);
}

DWORD WINAPI Win32FSHook::eventLoop( LPVOID lpParam )
{
//	debug("eventLoop starts");
	Win32FSHook* _this = (Win32FSHook*)lpParam;
	queue<Event*> local;
	while (1)
	{
		// quickly copy to local queue, minimizing the time holding the lock
		EnterCriticalSection(&_this->_eventQueueLock);
		while(_this->_eventQueue.size() > 0)
		{
			Event *e = _this->_eventQueue.front();
			local.push(e);
			_this->_eventQueue.pop();
		}
		LeaveCriticalSection(&_this->_eventQueueLock);

		while(local.size() > 0)
		{
			Event *e = local.front();
			local.pop();
			if (_this->_isRunning)
			{
				_this->_callback(e->_watchID, e->_action, e->_rootPath, e->_filePath);
			}
			delete e;
		}

		if (_this->_isRunning)
		{
			WaitForSingleObjectEx(_this->_eventQueueEvent, INFINITE, TRUE);
		}
		else
			break;
	}
//	debug("eventLoop exits");
	return 0;
}
