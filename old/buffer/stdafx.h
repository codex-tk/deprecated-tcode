// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//
#if defined( _WIN32 )
#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
#include <WinSock2.h>
#include <MSWSOCK.h>
#include <WS2tcpip.h>
#include <windows.h>
#include <winbase.h>
#include <tchar.h>
#include <dbghelp.h>
#include <process.h>
#endif

#include <stdint.h>
#include <iostream>
#include <cassert>
#include <cstring>
#include <functional>
#include <algorithm>
#include <vector>
#include <map>
#include <string>
#include <iterator>
#include <ctime>
#include <system_error>
#include <common/define.hpp>
