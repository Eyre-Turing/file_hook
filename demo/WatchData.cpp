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

 

#include "WatchData.h"
//#include "Logger.h"

int WatchData::_counter = 0;

WatchData::WatchData()
{
}

WatchData::~WatchData()
{
	if (_path != NULL) free(_path);
	_hDir = 0;
}

WatchData::WatchData(const WCHAR* path, int mask, bool watchSubtree, HANDLE completionPort)
	:
	_watchId(++_counter), 
	_mask(mask), 
	_watchSubtree(watchSubtree),
	_byteReturned(0),
	_completionPort(completionPort)
{
	_path = _wcsdup(path); 
	_hDir = CreateFileW(_path,
						 FILE_LIST_DIRECTORY | GENERIC_READ,
						 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
						 NULL, //security attributes
						 OPEN_EXISTING,
						 FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
	if(_hDir == INVALID_HANDLE_VALUE )	
	{
		throw GetLastError();
	}
	
	if (NULL == CreateIoCompletionPort(_hDir, _completionPort, (ULONG_PTR)_watchId, 0))
	{
		throw GetLastError();
	}
}

int WatchData::unwatchDirectory()
{
	int c = CancelIo(_hDir);
	if (_hDir != INVALID_HANDLE_VALUE) CloseHandle(_hDir);
	if (c == 0)
	{
		return GetLastError();
	}
	else
	{
		return 0;
	}
}

int WatchData::watchDirectory()
{
	memset(_buffer, 0, sizeof(_buffer));
	memset(&_overLapped, 0, sizeof(_overLapped));
	if( !ReadDirectoryChangesW( _hDir,
		 					    _buffer,//<--FILE_NOTIFY_INFORMATION records are put into this buffer
								sizeof(_buffer),
								_watchSubtree,
								_mask,
								&_byteReturned,
								&_overLapped,
								NULL))



	{
		return GetLastError();
	}
	else
	{
		return 0;
	}
}
