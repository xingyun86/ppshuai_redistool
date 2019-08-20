
// RedisClusterToolsDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "RedisClusterTools.h"
#include "RedisClusterToolsDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CRedisClusterToolsDlg dialog



CRedisClusterToolsDlg::CRedisClusterToolsDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_REDISCLUSTERTOOLS_DIALOG, pParent)
{
	m_b_running = false;
	m_redis_context = NULL;
	m_dw_redis_thread = 0;
	m_host = "";
	m_port = 0;
	m_pass = "";
	m_slot = 0;
	InitializeCriticalSection(&m_cslocker);
	m_h_redis_thread = INVALID_HANDLE_VALUE;
	m_nHeightCommand = 0;
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CRedisClusterToolsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CRedisClusterToolsDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_CBN_SELCHANGE(IDC_COMBO_HOST, &CRedisClusterToolsDlg::OnCbnSelchangeComboHost)
	ON_WM_SIZE()
	ON_BN_CLICKED(IDC_BUTTON_EDITCONF, &CRedisClusterToolsDlg::OnBnClickedButtonEditConf)
END_MESSAGE_MAP()


// CRedisClusterToolsDlg message handlers

BOOL CRedisClusterToolsDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	
	init_ui_text();

	init_config();

	init_redis_status_thread();
	
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CRedisClusterToolsDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CRedisClusterToolsDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CRedisClusterToolsDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CRedisClusterToolsDlg::OnCbnSelchangeComboHost()
{
	// TODO: Add your control notification handler code here
	this->enable_client(FALSE);
	switch_redis();
}


void CRedisClusterToolsDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here
	if(this->IsWindowVisible())
	{
		resize_window();
	}
}

void CRedisClusterToolsDlg::OnBnClickedButtonEditConf()
{
	// TODO: Add your control notification handler code here
	this->enable_client(FALSE);
	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	si.cb = sizeof(si);
	_TCHAR tzCommandLine[4096] = { 0 };
	snprintf(tzCommandLine, sizeof(tzCommandLine) / sizeof(*tzCommandLine), _T("notepad %s"), CONFIG_FNAME);
	// Start the child process. 
	if (!CreateProcess(NULL,   // No module name (use command line)
		tzCommandLine,        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi)           // Pointer to PROCESS_INFORMATION structure
		)
	{
		printf("CreateProcess failed (%d).\n", GetLastError());
	}
	else
	{
		// Wait until child process exits.
		WaitForSingleObject(pi.hProcess, INFINITE);

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		
		// Reload
		init_config();
	}
	this->enable_client(TRUE);
}
