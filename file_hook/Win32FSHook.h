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


 

#ifndef WIN32FSHOOK_H_
#define WIN32FSHOOK_H_

#include <windows.h>
#include <string>
#include <map>
#include <queue>
#include <utility>


#include "WatchData.h"

using namespace std;

class Event
{
public:
	int _watchID;
	int _action;
	WCHAR* _rootPath;
	WCHAR* _filePath;
	Event(int wd, int action, const WCHAR* rootPath, const WCHAR* filePath)
	{
		_watchID = wd;
		_action = action;
		size_t len1 = wcslen(rootPath);
		size_t len2 = wcslen(filePath);
		_rootPath = new WCHAR[len1 + 1];
		_filePath = new WCHAR[len2 + 1];
		wcsncpy(_rootPath, rootPath, len1);
		_rootPath[len1] = 0;
		wcsncpy(_filePath, filePath, len2);
		_filePath[len2] = 0;
	}

	~Event()
	{
		delete [] _rootPath;
		delete [] _filePath;
	}
};
class Win32FSHook
{
private:
	static const DWORD DELETE_WD_SIGNAL = 99999997;
	static const DWORD EXIT_SIGNAL = 99999998;
	static Win32FSHook *instance;

	// running flag
	bool _isRunning;
	
	// thread handle
	HANDLE _mainLoopThreadHandle;
	
	HANDLE _completionPort;

	// critical seaction
	CRITICAL_SECTION _cSection;
	
	// watch id 2 watch map
	map<int, WatchData*> _wid2WatchData;

	// Thread function
	static DWORD WINAPI mainLoop( LPVOID lpParam );

	ChangeCallback _callback;
	
	void watchDirectory(WatchData* wd);
	
	CRITICAL_SECTION _eventQueueLock;
	HANDLE _eventQueueEvent;
	void postEvent(Event *event);
	HANDLE _eventsThread;
	static DWORD WINAPI eventLoop( LPVOID lpParam );
	queue<Event*> _eventQueue;
	WatchData* find(int wd);
public:
	static const int ERR_INIT_THREAD = 1;
	
	Win32FSHook();
	virtual ~Win32FSHook();
	
	void init(ChangeCallback callback);
	
	int add_watch(const WCHAR* path, long notifyFilter, bool watchSubdirs, DWORD &error);
	void remove_watch(int watchId);
};

#endif /*WIN32FSHOOK_H_*/
