
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
		}
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

	void free_redis()
	{
		EnterCriticalSection(&m_cslocker);
		if ((m_redis_context != NULL) && (m_redis_context->err == 0))
		{
			redisFree(m_redis_context);
			m_redis_context = NULL;
		}
		LeaveCriticalSection(&m_cslocker);
	}
	void init_redis()
	{
		bool isunix = false;
		//redisContext* redis_context = NULL;
		redisReply* redis_reply = NULL;
		const char* hostname = m_host.length() ? m_host.c_str() : "10.0.3.252";
		int port = (m_port > 0 && m_port < 65536) ? m_port : 6379;
		const char* pass = m_pass.length() ? m_pass.c_str() : NULL;
		int slot = (m_slot >= 0) ? m_slot : (0);

		EnterCriticalSection(&m_cslocker);

		struct timeval timeout = { 1, 500000 }; // 1.5 seconds
		if (isunix)
		{
			m_redis_context = redisConnectUnixWithTimeout(hostname, timeout);
		}
		else
		{
			m_redis_context = redisConnectWithTimeout(hostname, port, timeout);
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
		if (pass)
		{
			redis_execute_command((std::string("AUTH ") + pass).c_str());
		}
		if (slot >= 0)
		{
			((CButton*)GetDlgItem(IDC_CHECK_CLUSTER))->SetCheck(redis_select(slot));
		}
		LeaveCriticalSection(&m_cslocker);
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
					thiz->redis_reinit();
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

public:
#define CONFIG_FNAME	"config.json"
#define CURRENT_INDEX	"current_index"
#define SERVER_LIST		"server_list"
#define SERVER_DESC		"desc"
#define SERVER_HOST		"host"
#define SERVER_PORT		"port"
#define SERVER_PASS		"pass"
#define SERVER_SLOT		"slot"
#define DOC_ALLOCATOR	m_document.GetAllocator()

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
	void redis_reinit()
	{
		free_redis();
		init_redis();
	}
	bool redis_ping()
	{
		bool result = false;
		EnterCriticalSection(&m_cslocker);
		if (m_redis_context != NULL)
		{
			// PING server
			redisReply* redis_reply = reinterpret_cast<redisReply*>(redisCommand(m_redis_context, "PING"));
			if (redis_reply != NULL)
			{
				printf("PING: %s\n", redis_reply->str);
				result = (!_stricmp(redis_reply->str, "PONG"));
				freeReplyObject(redis_reply);
				redis_reply = NULL;
			}
		}
		LeaveCriticalSection(&m_cslocker);
		return result;
	}
	bool redis_select(int index)
	{
		bool result = false;
		EnterCriticalSection(&m_cslocker);
		if (m_redis_context != NULL)
		{
			char czCommand[128] = { 0 };
			snprintf(czCommand, sizeof(czCommand)/sizeof(*czCommand), "SELECT %d", index);
			// SELECT index
			redisReply* redis_reply = reinterpret_cast<redisReply*>(redisCommand(m_redis_context, czCommand));
			if (redis_reply != NULL)
			{
				printf("SELECT: %s\n", redis_reply->str);
				result = (StrStrI(redis_reply->str, "not allowed in cluster mode") != NULL);
				freeReplyObject(redis_reply);
				redis_reply = NULL;
			}
		}
		LeaveCriticalSection(&m_cslocker);
		return result;
	}
	void redis_execute_command(const char * command)
	{
		EnterCriticalSection(&m_cslocker);
		if (m_redis_context != NULL)
		{
			redisReply* redis_reply = reinterpret_cast<redisReply*>(redisCommand(m_redis_context, command));
			if (redis_reply != NULL)
			{
				std::string prefix = std::string("[") + command + std::string("]") + std::string("\r\n\r\n");
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
		}
		LeaveCriticalSection(&m_cslocker);
	}
	afx_msg void OnCbnSelchangeComboHost();
};
