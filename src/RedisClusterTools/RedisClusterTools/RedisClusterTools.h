
// RedisClusterTools.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CRedisClusterToolsApp:
// See RedisClusterTools.cpp for the implementation of this class
//

class CRedisClusterToolsApp : public CWinApp
{
public:
	CRedisClusterToolsApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CRedisClusterToolsApp theApp;
