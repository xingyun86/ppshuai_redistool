
// RedisClusterToolsDlg.h : header file
//

#pragma once

#include <hiredis.h>


// CRedisClusterToolsDlg dialog
class CRedisClusterToolsDlg : public CDialogEx
{
	// Construction
public:
	CRedisClusterToolsDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_REDISCLUSTERTOOLS_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual void OnOK() {
		CString str;
		GetDlgItem(IDC_EDIT_COMMAND)->GetWindowText(str);
		if (str.GetLength())
		{
			redis_execute_command((const char*)str);
			((CEdit *)GetDlgItem(IDC_EDIT_RESULT))->LineScroll(((CEdit*)GetDlgItem(IDC_EDIT_RESULT))->GetLineCount());
		}
		((CEdit*)GetDlgItem(IDC_EDIT_COMMAND))->SetFocus();
	}
	virtual void OnCancel() {
		set_running(false);
		WaitForSingleObject(m_h_redis_thread, INFINITE);
		CloseHandle(m_h_redis_thread);
		m_h_redis_thread = INVALID_HANDLE_VALUE;
		free_redis();
		EndDialog(IDCANCEL);
	}
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnCbnSelchangeComboHost();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnBnClickedButtonEditConf();
	DECLARE_MESSAGE_MAP()

private:
	bool m_b_running;
	rapidjson::Document m_document;
	redisContext* m_redis_context;
	CRITICAL_SECTION m_cslocker;
	HANDLE m_h_redis_thread;
	DWORD m_dw_redis_thread;
	std::string m_host;
	int m_port;
	std::string m_pass;
	int m_slot;

	//
	int m_nHeightCommand;
	//
#define MAX_STR_LEN		100 * 1024 * 1024
#define CONFIG_FNAME	"config.json"
#define CURRENT_INDEX	"current_index"
#define SERVER_LIST		"server_list"
#define SERVER_DESC		"desc"
#define SERVER_HOST		"host"
#define SERVER_PORT		"port"
#define SERVER_PASS		"pass"
#define SERVER_SLOT		"slot"
#define DOC_ALLOCATOR	m_document.GetAllocator()

	void free_redis()
	{
		EnterCriticalSection(&m_cslocker);
		printf("%s:%d:%s lock\n", __FILE__, __LINE__, __func__);
		if ((m_redis_context != NULL) && (m_redis_context->err == 0))
		{
			redisFree(m_redis_context);
			m_redis_context = NULL;
		}
		printf("%s:%d:%s unlock\n", __FILE__, __LINE__, __func__);
		LeaveCriticalSection(&m_cslocker);
	}
	int init_redis()
	{
		int ret = (-1);
		bool isunix = false;
		//redisContext* redis_context = NULL;
		redisReply* redis_reply = NULL;
		const char* host = m_host.length() ? m_host.c_str() : "10.0.3.252";
		int port = (m_port > 0 && m_port < 65536) ? m_port : 6379;
		const char* pass = m_pass.length() ? m_pass.c_str() : NULL;
		int slot = (m_slot >= 0) ? m_slot : (0);

		EnterCriticalSection(&m_cslocker);
		printf("%s:%d:%s lock\n", __FILE__, __LINE__, __func__);

		struct timeval timeout = { 1, 500000 }; // 1.5 seconds
		if (isunix)
		{
			m_redis_context = redisConnectUnixWithTimeout(host, timeout);
		}
		else
		{
			m_redis_context = redisConnectWithTimeout(host, port, timeout);
		}
		if ((m_redis_context == NULL) || (m_redis_context->err != 0))
		{
			if (m_redis_context)
			{
				printf("Connection error: %s\n", m_redis_context->errstr);
				redisFree(m_redis_context);
				m_redis_context = NULL;
			}
			else
			{
				printf("Connection error: can't allocate redis context\n");
				m_redis_context = NULL;
			}
		}
		if (m_redis_context != NULL)
		{
			if (pass)
			{
				//redis_execute_command((std::string("AUTH ") + pass).c_str());
				redis_login(pass);
			}
			if (slot >= 0)
			{
				((CButton*)GetDlgItem(IDC_CHECK_CLUSTER))->SetCheck(redis_select(slot));
			}

			ret = 0;
		}
		LeaveCriticalSection(&m_cslocker);
		printf("%s:%d:%s unlock\n", __FILE__, __LINE__, __func__);

		CString strText = _T("");
		GetDlgItem(IDC_EDIT_RESULT)->GetWindowText(strText);
		if (strText.GetLength() > MAX_STR_LEN)
		{
			strText = _T("");
		}
		std::string prefix = (const char*)strText + std::string("[") + "connected to " + host + ":" + std::to_string(port) + ((ret ==0)?" success":" failed")  + std::string("]") + std::string("\r\n");
		GetDlgItem(IDC_EDIT_RESULT)->SetWindowText(prefix.c_str());
		return ret;
	}
	static DWORD redis_status_thread(LPVOID p)
	{
		CRedisClusterToolsDlg* thiz = reinterpret_cast<CRedisClusterToolsDlg*>(p);
		if (thiz)
		{
			thiz->set_running(true);
			while (thiz->is_running())
			{
				if (!thiz->redis_ping())
				{
					if (thiz->redis_reinit() == 0)
					{
						thiz->enable_client(TRUE);
					}
				}
				Sleep(3000);
			}
			thiz->set_running(false);
		}
		return 0;
	}
	void init_redis_status_thread()
	{
		m_h_redis_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&CRedisClusterToolsDlg::redis_status_thread, this, 0, &m_dw_redis_thread);
	}
	void init_ui_text()
	{
		SetWindowText(_T("Redis命令行管理工具(支持集群)[xingyun86]"));
		SetDlgItemText(IDC_STATIC_COMMAND, _T("命令行:"));
		SetDlgItemText(IDC_CHECK_CLUSTER, _T("集群"));
		SetDlgItemText(IDC_STATIC_HOST, _T("服务器地址:"));
		SetDlgItemText(IDC_STATIC_RESULT, _T("执行结果:"));

		SetDlgItemText(IDOK, _T("运行命令"));
		SetDlgItemText(IDC_BUTTON_EDITCONF, _T("编辑配置"));
		SetDlgItemText(IDCANCEL, _T("关闭程序"));

		CRect rcCommand;
		GetDlgItem(IDC_EDIT_COMMAND)->GetClientRect(&rcCommand);
		m_nHeightCommand = rcCommand.Height();

		resize_window();
	}
	void resize_window()
	{
#define H_BORDER 8
#define V_BORDER 8
#define H_SPACES 8
#define V_SPACES 8
		CRect rcWindow = { 0 };
		CRect rcClient = { 0 };
		CRect rcMiddle = { 0 };

		GetClientRect(&rcWindow);

		///////////////////////////////////////////////////////
		// Top
		//
		GetDlgItem(IDC_STATIC_HOST)->GetClientRect(&rcMiddle);
		rcClient.left = H_BORDER;
		rcClient.top = V_BORDER;
		rcClient.right = rcClient.left + rcMiddle.right;
		rcClient.bottom = rcClient.top + rcMiddle.bottom;
		GetDlgItem(IDC_STATIC_HOST)->MoveWindow(&rcClient, FALSE);

		GetDlgItem(IDC_COMBO_HOST)->GetClientRect(&rcMiddle);
		rcClient.left = rcClient.right + H_SPACES;
		rcClient.top = V_BORDER;
		rcClient.right = rcClient.left + rcMiddle.right;
		rcClient.bottom = rcClient.top + rcMiddle.bottom;
		GetDlgItem(IDC_COMBO_HOST)->MoveWindow(&rcClient, FALSE);

		GetDlgItem(IDC_CHECK_CLUSTER)->GetClientRect(&rcMiddle);
		rcClient.left = rcClient.right + H_SPACES;
		rcClient.top = V_BORDER;
		rcClient.right = rcClient.left + rcMiddle.right;
		rcClient.bottom = rcClient.top + rcMiddle.bottom;
		GetDlgItem(IDC_CHECK_CLUSTER)->MoveWindow(&rcClient, FALSE);

		GetDlgItem(IDOK)->GetClientRect(&rcMiddle);
		rcClient.left = rcClient.right + H_SPACES;
		rcClient.top = V_BORDER;
		rcClient.right = rcClient.left + rcMiddle.right;
		rcClient.bottom = rcClient.top + rcMiddle.bottom;
		GetDlgItem(IDOK)->MoveWindow(&rcClient, FALSE);

		GetDlgItem(IDC_BUTTON_EDITCONF)->GetClientRect(&rcMiddle);
		rcClient.left = rcClient.right + H_SPACES;
		rcClient.top = V_BORDER;
		rcClient.right = rcClient.left + rcMiddle.right;
		rcClient.bottom = rcClient.top + rcMiddle.bottom;
		GetDlgItem(IDC_BUTTON_EDITCONF)->MoveWindow(&rcClient, FALSE);

		GetDlgItem(IDCANCEL)->GetClientRect(&rcMiddle);
		rcClient.left = rcClient.right + H_SPACES;
		rcClient.top = V_BORDER;
		rcClient.right = rcClient.left + rcMiddle.right;
		rcClient.bottom = rcClient.top + rcMiddle.bottom;
		GetDlgItem(IDCANCEL)->MoveWindow(&rcClient, FALSE);

		///////////////////////////////////////////////////////
		// Center
		//
		GetDlgItem(IDC_STATIC_COMMAND)->GetClientRect(&rcMiddle);
		rcClient.left = H_BORDER;
		rcClient.top = rcClient.bottom + V_SPACES;
		rcClient.right = rcClient.left + rcMiddle.right;
		rcClient.bottom = rcClient.top + rcMiddle.bottom;
		GetDlgItem(IDC_STATIC_COMMAND)->MoveWindow(&rcClient, FALSE);

		GetDlgItem(IDC_EDIT_COMMAND)->GetClientRect(&rcMiddle);
		rcClient.left = H_BORDER;
		rcClient.top = rcClient.bottom + V_SPACES;
		rcClient.right = rcWindow.right - H_BORDER;// rcClient.left + rcMiddle.right;
		rcClient.bottom = rcClient.top + (m_nHeightCommand ? m_nHeightCommand : rcMiddle.Height());// rcMiddle.bottom;
		GetDlgItem(IDC_EDIT_COMMAND)->MoveWindow(&rcClient, FALSE);

		///////////////////////////////////////////////////////
		// Bottom
		//
		GetDlgItem(IDC_STATIC_RESULT)->GetClientRect(&rcMiddle);
		rcClient.left = H_BORDER;
		rcClient.top = rcClient.bottom + V_SPACES;
		rcClient.right = rcClient.left + rcMiddle.right;
		rcClient.bottom = rcClient.top + rcMiddle.bottom;
		GetDlgItem(IDC_STATIC_RESULT)->MoveWindow(&rcClient, FALSE);

		GetDlgItem(IDC_EDIT_RESULT)->GetClientRect(&rcMiddle);
		rcClient.left = H_BORDER;
		rcClient.top = rcClient.bottom + V_SPACES;
		rcClient.right = rcWindow.right - H_BORDER;// rcClient.left + rcMiddle.right;
		rcClient.bottom = rcWindow.bottom - V_BORDER;// rcClient.top + rcMiddle.bottom;
		GetDlgItem(IDC_EDIT_RESULT)->MoveWindow(&rcClient, FALSE);
		
		///////////////////////////////////////////////////////
		// Notify repaint
		//
		RedrawWindow();
	}

public:
	void enable_client(BOOL bEnabled)
	{
		//GetDlgItem(IDC_COMBO_HOST)->EnableWindow(bEnabled);
		GetDlgItem(IDOK)->EnableWindow(bEnabled);
		GetDlgItem(IDC_BUTTON_EDITCONF)->EnableWindow(bEnabled);
		GetDlgItem(IDC_EDIT_COMMAND)->EnableWindow(bEnabled);
		if (bEnabled)
		{
			((CEdit*)GetDlgItem(IDC_EDIT_COMMAND))->SetFocus();
		}
	}

	void init_config()
	{
		int n_current_index = 0;
		char* current_index = NULL;
		std::string f_data = "";
		long f_size = 0;

		FILE* p = fopen(CONFIG_FNAME, "rb");
		if (p)
		{
			f_size = _filelength(_fileno(p));
			if (f_size > 0)
			{
				f_data.resize(f_size, '\0');
				fread((void*)f_data.c_str(), 1, f_size, p);
			}
			fclose(p);
		}

		if (f_size <= 0)
		{
			printf("%s is empty!\n", CONFIG_FNAME);
			return;
		}

		m_host = "";
		m_port = 0;
		m_pass = "";
		m_slot = 0;

		if (m_document.IsObject())
		{
			if (m_document.HasMember(CURRENT_INDEX) && m_document[CURRENT_INDEX].GetType() == rapidjson::kStringType)
			{
				current_index = (char*)m_document[CURRENT_INDEX].GetString();
			}
		}
		else
		{
			m_document.SetObject();
		}
		if (m_document.Parse(f_data.c_str()).HasParseError())
		{
			printf("config.json format error!\n");
		}
		else
		{
			if (m_document.HasMember(SERVER_LIST) && m_document[SERVER_LIST].GetType() == rapidjson::kArrayType)
			{
				((CComboBox*)GetDlgItem(IDC_COMBO_HOST))->ResetContent();

				if (m_document.HasMember(CURRENT_INDEX))
				{
					m_document.RemoveMember(CURRENT_INDEX);
				}
				m_document.AddMember(CURRENT_INDEX, "", DOC_ALLOCATOR);
				rapidjson::Value& server_list = m_document[SERVER_LIST];
				for (rapidjson::SizeType i = 0; i < server_list.Size(); i++)
				{
					rapidjson::Value& server_item = server_list[i];
					if (server_item.HasMember(SERVER_DESC) && server_item[SERVER_DESC].GetType() == rapidjson::kStringType)
					{
						if (current_index && !strcmp(current_index, server_item[SERVER_DESC].GetString()))
						{
							m_document[CURRENT_INDEX].SetString(current_index, DOC_ALLOCATOR);
							n_current_index = ((CComboBox*)GetDlgItem(IDC_COMBO_HOST))->GetCount();
						}
						((CComboBox*)GetDlgItem(IDC_COMBO_HOST))->InsertString(((CComboBox*)GetDlgItem(IDC_COMBO_HOST))->GetCount(), server_item[SERVER_DESC].GetString());
					}
				}
				
				if (server_list.Size() > 0)
				{
					m_host = m_document[SERVER_LIST][n_current_index][SERVER_HOST].GetString();
					m_port = m_document[SERVER_LIST][n_current_index][SERVER_PORT].GetInt();
					m_pass = m_document[SERVER_LIST][n_current_index][SERVER_PASS].GetString();
					m_slot = m_document[SERVER_LIST][n_current_index][SERVER_SLOT].GetInt();
					m_document[CURRENT_INDEX].SetString(current_index ? current_index : m_document[SERVER_LIST][n_current_index][SERVER_DESC].GetString(), DOC_ALLOCATOR);
					((CComboBox*)GetDlgItem(IDC_COMBO_HOST))->SetCurSel(n_current_index);
				}
			}
		}
	}
	void switch_redis()
	{
		CString strCurrentIndex;
		((CComboBox*)GetDlgItem(IDC_COMBO_HOST))->GetLBText(((CComboBox*)GetDlgItem(IDC_COMBO_HOST))->GetCurSel(), strCurrentIndex);
		rapidjson::Value& server_list = m_document[SERVER_LIST];
		for (rapidjson::SizeType i = 0; i < server_list.Size(); i++)
		{
			rapidjson::Value& server_item = server_list[i];
			if (server_item.HasMember(SERVER_DESC) && server_item[SERVER_DESC].GetType() == rapidjson::kStringType)
			{
				if (!strcmp((const char *)strCurrentIndex, server_item[SERVER_DESC].GetString()))
				{
					free_redis();
					m_document[CURRENT_INDEX].SetString(strCurrentIndex, DOC_ALLOCATOR);
					m_host = server_item[SERVER_HOST].GetString();
					m_port = server_item[SERVER_PORT].GetInt();
					m_pass = server_item[SERVER_PASS].GetString();
					m_slot = server_item[SERVER_SLOT].GetInt();
					//init_redis();
					break;
				}
			}
		}
	}
	void set_running(bool b_running)
	{
		m_b_running = b_running;
	}
	bool is_running()
	{
		return m_b_running;
	}
	int redis_reinit()
	{
		free_redis();
		return init_redis();
	}
	bool redis_ping()
	{
		bool result = false;
		if (m_redis_context != NULL)
		{
			// PING server
			EnterCriticalSection(&m_cslocker);
			printf("%s:%d:%s lock\n", __FILE__, __LINE__, __func__);
			redisReply* redis_reply = reinterpret_cast<redisReply*>(redisCommand(m_redis_context, "PING"));
			LeaveCriticalSection(&m_cslocker);
			printf("%s:%d:%s unlock\n", __FILE__, __LINE__, __func__);
			if (redis_reply != NULL)
			{
				printf("PING: %s\n", redis_reply->str);
				result = (!_stricmp(redis_reply->str, "PONG"));
				freeReplyObject(redis_reply);
				redis_reply = NULL;
			}
		}
		return result;
	}
	bool redis_select(int index)
	{
		bool result = false;
		if (m_redis_context != NULL)
		{
			char czCommand[128] = { 0 };
			snprintf(czCommand, sizeof(czCommand)/sizeof(*czCommand), "SELECT %d", index);
			// SELECT index
			EnterCriticalSection(&m_cslocker);
			printf("%s:%d:%s lock\n", __FILE__, __LINE__, __func__);
			redisReply* redis_reply = reinterpret_cast<redisReply*>(redisCommand(m_redis_context, czCommand));
			LeaveCriticalSection(&m_cslocker);
			printf("%s:%d:%s unlock\n", __FILE__, __LINE__, __func__);
			if (redis_reply != NULL)
			{
				printf("SELECT: %s\n", redis_reply->str);
				result = (StrStrI(redis_reply->str, "not allowed in cluster mode") != NULL);
				freeReplyObject(redis_reply);
				redis_reply = NULL;
			}
		}
		return result;
	}

	bool redis_login(const char * pass)
	{
		bool result = false;
		if (m_redis_context != NULL)
		{
			char czCommand[4096] = { 0 };
			snprintf(czCommand, sizeof(czCommand) / sizeof(*czCommand), "AUTH %s", pass);
			// AUTH pass
			EnterCriticalSection(&m_cslocker);
			printf("%s:%d:%s lock\n", __FILE__, __LINE__, __func__);
			redisReply* redis_reply = reinterpret_cast<redisReply*>(redisCommand(m_redis_context, czCommand));
			LeaveCriticalSection(&m_cslocker);
			printf("%s:%d:%s unlock\n", __FILE__, __LINE__, __func__);
			if (redis_reply != NULL)
			{
				printf("AUTH: %s\n", redis_reply->str);
				result = (_stricmp(redis_reply->str, "OK") != NULL);
				freeReplyObject(redis_reply);
				redis_reply = NULL;
			}
		}
		return result;
	}
	void redis_execute_command(const char * command)
	{
		if (m_redis_context != NULL)
		{
			EnterCriticalSection(&m_cslocker);
			printf("%s:%d:%s lock\n", __FILE__, __LINE__, __func__);
			redisReply* redis_reply = reinterpret_cast<redisReply*>(redisCommand(m_redis_context, command));
			printf("%s:%d:%s unlock\n", __FILE__, __LINE__, __func__);
			LeaveCriticalSection(&m_cslocker);
			if (redis_reply != NULL)
			{
				CString strText = _T("");
				GetDlgItem(IDC_EDIT_RESULT)->GetWindowText(strText);
				if (strText.GetLength() > MAX_STR_LEN)
				{
					strText = _T("");
				}
				std::string prefix = (const char*)strText + std::string("[") + command + std::string("]") + std::string("\r\n");
				switch (redis_reply->type)
				{
				case REDIS_REPLY_ARRAY:
				{
					std::string str_array = "";
					for (size_t j = 0; j < redis_reply->elements; j++)
					{
						str_array.append(std::to_string(j)).append(") ").append(redis_reply->element[j]->str).append("\r\n");
					}
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + str_array + std::string("\r\n")).c_str());
				}
				break;
				case REDIS_REPLY_STRING:
				{
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + redis_reply->str + std::string("\r\n")).c_str());
				}
				break;
				case REDIS_REPLY_INTEGER:
				{
					char szText[128] = { 0 };
					snprintf(szText, sizeof(szText) / sizeof(*szText), ("%lld"), redis_reply->integer);
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + szText + std::string("\r\n")).c_str());
				}
				break;
				case REDIS_REPLY_NIL:
				{
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + _T("nil") + std::string("\r\n")).c_str());
				}
				break;
				case REDIS_REPLY_STATUS:
				{
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + redis_reply->str + std::string("\r\n")).c_str());
				}
				break;
				case REDIS_REPLY_ERROR:
				{
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + redis_reply->str + std::string("\r\n")).c_str());
#define CLUSTER_ASK_LEN		3
#define CLUSTER_ASK			"ASK"
#define CLUSTER_MOVED_LEN	5
#define CLUSTER_MOVED		"MOVED"
					if (redis_reply != NULL && (redis_reply->type == REDIS_REPLY_ERROR) &&
						(!_strnicmp(redis_reply->str, CLUSTER_ASK, CLUSTER_ASK_LEN) ||
							!_strnicmp(redis_reply->str, CLUSTER_MOVED, CLUSTER_MOVED_LEN)))
					{
						std::string temp_node = std::string(redis_reply->str).substr(std::string(redis_reply->str).rfind(" ") + 1);
						if (temp_node.find(":") != std::string::npos)
						{
							std::string host = temp_node.substr(0, temp_node.find(":"));
							int port = std::stoi(temp_node.substr(temp_node.find(":") + 1));

							EnterCriticalSection(&m_cslocker);
							printf("%s:%d:%s lock\n", __FILE__, __LINE__, __func__);
							redis_login_execute_command(host.c_str(), port, command);
							LeaveCriticalSection(&m_cslocker);
							printf("%s:%d:%s unlock\n", __FILE__, __LINE__, __func__);
						}
					}
				}
				break;
				default:
					break;
				}
				//printf("PING: %s\n", redis_reply->str);
				freeReplyObject(redis_reply);
				redis_reply = NULL;
			}
		}
	}
	void redis_login_execute_command(const char * _host, int _port, const char* _command)
	{
		bool result = false;
		bool isunix = false;
		bool iscluster = false;
		char czCommand[4096] = { 0 };
		redisContext* redis_context = NULL;
		redisReply* redis_reply = NULL;
		const char* host = _host ? _host : "10.0.3.252";
		int port = (_port > 0 && _port < 65536) ? _port : 6379;
		const char* pass = m_pass.length() ? m_pass.c_str() : NULL;
		int slot = (m_slot >= 0) ? m_slot : (0);

		struct timeval timeout = { 1, 500000 }; // 1.5 seconds
		if (isunix)
		{
			redis_context = redisConnectUnixWithTimeout(host, timeout);
		}
		else
		{
			redis_context = redisConnectWithTimeout(host, port, timeout);
		}
		if ((redis_context == NULL) || (redis_context->err != 0))
		{
			if (redis_context)
			{
				printf("Connection error: %s\n", redis_context->errstr);
				redisFree(redis_context);
				redis_context = NULL;
			}
			else
			{
				printf("Connection error: can't allocate redis context\n");
				redis_context = NULL;
			}
		}
		if (pass)
		{
			memset(czCommand, 0, sizeof(czCommand));
			snprintf(czCommand, sizeof(czCommand) / sizeof(*czCommand), "AUTH %s", pass);
			// AUTH pass
			redis_reply = reinterpret_cast<redisReply*>(redisCommand(redis_context, czCommand));
			if (redis_reply != NULL)
			{
				printf("AUTH: %s\n", redis_reply->str);
				result = (_stricmp(redis_reply->str, "OK") == 0);
				freeReplyObject(redis_reply);
				redis_reply = NULL;
				if (!result)
				{
					printf("AUTH error: %s\n", redis_context->errstr);
					redisFree(redis_context);
					redis_context = NULL;
				}
			}
		}
		if (slot >= 0)
		{
			memset(czCommand, 0, sizeof(czCommand));
			snprintf(czCommand, sizeof(czCommand) / sizeof(*czCommand), "SELECT %d", slot);
			// SELECT slot
			redis_reply = reinterpret_cast<redisReply*>(redisCommand(redis_context, czCommand));
			if (redis_reply != NULL)
			{
				printf("SELECT: %s\n", redis_reply->str);
				result = (_stricmp(redis_reply->str, "OK") == 0);
				iscluster = (StrStrI(redis_reply->str, "not allowed in cluster mode") != NULL);
				freeReplyObject(redis_reply);
				redis_reply = NULL;
				if (!result && !iscluster)
				{
					printf("SELECT error: %s\n", redis_context->errstr);
					redisFree(redis_context);
					redis_context = NULL;
				}
			}
		}
		if (redis_context != NULL)
		{
			// Command
			redis_reply = reinterpret_cast<redisReply*>(redisCommand(redis_context, _command));
			if (redis_reply != NULL)
			{
				CString strText = _T("");
				GetDlgItem(IDC_EDIT_RESULT)->GetWindowText(strText);
				if (strText.GetLength() > MAX_STR_LEN)
				{
					strText = _T("");
				}
				std::string prefix = (const char*)strText + std::string("[") + _command + std::string(" again]") + std::string("\r\n");
				switch (redis_reply->type)
				{
				case REDIS_REPLY_ARRAY:
				{
					std::string str_array = "";
					for (size_t j = 0; j < redis_reply->elements; j++)
					{
						str_array.append(std::to_string(j)).append(") ").append(redis_reply->element[j]->str).append("\r\n");
					}
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + str_array + std::string("\r\n")).c_str());
				}
				break;
				case REDIS_REPLY_STRING:
				{
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + redis_reply->str + std::string("\r\n")).c_str());
				}
				break;
				case REDIS_REPLY_INTEGER:
				{
					char szText[128] = { 0 };
					snprintf(szText, sizeof(szText) / sizeof(*szText), ("%lld"), redis_reply->integer);
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + szText + std::string("\r\n")).c_str());
				}
				break;
				case REDIS_REPLY_NIL:
				{
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + _T("nil") + std::string("\r\n")).c_str());
				}
				break;
				case REDIS_REPLY_STATUS:
				{
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + redis_reply->str + std::string("\r\n")).c_str());
				}
				break;
				case REDIS_REPLY_ERROR:
				{
					GetDlgItem(IDC_EDIT_RESULT)->SetWindowText((prefix + redis_reply->str + std::string("\r\n")).c_str());
				}
				break;
				default:
					break;
				}
				//printf("PING: %s\n", redis_reply->str);
				freeReplyObject(redis_reply);
				redis_reply = NULL;
			}

			redisFree(redis_context);
			redis_context = NULL;
		}
	}
};
