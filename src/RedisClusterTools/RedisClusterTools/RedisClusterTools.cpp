
// RedisClusterTools.cpp : Defines the class behaviors for the application.
//

#include "pch.h"
#include "framework.h"
#include "RedisClusterTools.h"
#include "RedisClusterToolsDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CRedisClusterToolsApp

BEGIN_MESSAGE_MAP(CRedisClusterToolsApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CRedisClusterToolsApp construction

CRedisClusterToolsApp::CRedisClusterToolsApp()
{
	// support Restart Manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only CRedisClusterToolsApp object

CRedisClusterToolsApp theApp;


// CRedisClusterToolsApp initialization
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <hiredis.h>
#include <async.h>
#include <adapters/libuv.h>

void getCallback(redisAsyncContext* c, void* r, void* privdata) {
	redisReply* reply = reinterpret_cast<redisReply*>(r);
	if (reply == NULL) return;
	printf("argv[%s]: %s\n", (char*)privdata, reply->str);

	/* Disconnect after receiving the reply to GET */
	redisAsyncDisconnect(c);
}

void connectCallback(const redisAsyncContext* c, int status) {
	if (status != REDIS_OK) {
		printf("Error: %s\n", c->errstr);
		return;
	}
	printf("Connected...\n");
}

void disconnectCallback(const redisAsyncContext* c, int status) {
	if (status != REDIS_OK) {
		printf("Error: %s\n", c->errstr);
		return;
	}
	printf("Disconnected...\n");
}

int test_main(int argc, char** argv) {
	//signal(SIGPIPE, SIG_IGN);
	uv_loop_t* loop = uv_default_loop();

	redisAsyncContext* c = redisAsyncConnect("10.0.3.252", 6379);
	if (c->err)
	{
		/* Let *c leak for now... */
		printf("Error: %s\n", c->errstr);
		return 1;
	}

	redisLibuvAttach(c, loop);
	redisAsyncSetConnectCallback(c, connectCallback);
	redisAsyncSetDisconnectCallback(c, disconnectCallback);
	redisAsyncCommand(c, [](redisAsyncContext* c, void* r, void* privdata) {
			redisReply* reply = reinterpret_cast<redisReply*>(r);
			if (reply == NULL) return;
			printf("argv[%s]: %s\n", (char*)privdata, reply->str);
		}, NULL, "SET key %b", argv[argc - 1], strlen(argv[argc - 1]));
	redisAsyncCommand(c, getCallback, (char*)"end-1", "GET key");
	uv_run(loop, UV_RUN_DEFAULT);
	return 0;
}
BOOL CRedisClusterToolsApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	AfxEnableControlContainer();

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager* pShellManager = new CShellManager;

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

#if defined(_DEBUG) || defined(DEBUG)
	/*FILE * pStdIn = NULL;
	FILE* pStdOut = NULL;
	FILE* pStdErr = NULL;
	if (AllocConsole())
	{
		pStdIn = _tfreopen(_T("CONIN$"), _T("rb"), stdin);
		pStdOut = _tfreopen(_T("CONOUT$"), _T("wb"), stdout);
		pStdErr = _tfreopen(_T("CONOUT$"), _T("wb"), stderr);
	}*/
	{
		test_main(0,0);
	}
	return FALSE;
#else
	//no op

	FILE* pStdIn = NULL;
	FILE* pStdOut = NULL;
	FILE* pStdErr = NULL;
	if (AllocConsole())
	{
		pStdIn = _tfreopen(_T("CONIN$"), _T("rb"), stdin);
		pStdOut = _tfreopen(_T("CONOUT$"), _T("wb"), stdout);
		pStdErr = _tfreopen(_T("CONOUT$"), _T("wb"), stderr);
	}
	{
		test_main(0, 0);
	}
	getchar();
	return FALSE;
#endif

	CRedisClusterToolsDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}
	else if (nResponse == -1)
	{
		TRACE(traceAppMsg, 0, "Warning: dialog creation failed, so application is terminating unexpectedly.\n");
		TRACE(traceAppMsg, 0, "Warning: if you are using MFC controls on the dialog, you cannot #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS.\n");
	}

#if defined(_DEBUG) || defined(DEBUG)
	/*if (FreeConsole())
	{
		if (pStdIn)
		{
			fclose(pStdIn);
			pStdIn = NULL;
		}
		if (pStdOut)
		{
			fclose(pStdOut);
			pStdOut = NULL;
		}
		if (pStdErr)
		{
			fclose(pStdErr);
			pStdErr = NULL;
		}
	}*/
#else
	//no op
#endif
	// Delete the shell manager created above.
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

