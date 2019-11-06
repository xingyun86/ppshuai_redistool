// libuv_test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <hiredis.h>
#include <async.h>
#include <adapters/libuv.h>
#include <map>
#include <thread>
#include <string>
#include <mutex>

#include <deque>
bool g_running = false;
redisAsyncContext* g_redis_context = nullptr;
std::deque<std::string> g_task_list;
bool g_redis_cluster = true;
int g_redis_slot_pos = 1;
std::string g_redis_pass = "phxUXkvaA4YfS8Cm";
std::mutex g_redis_locker;
class redis_extra {
public:
	redis_extra(std::map<std::string, redis_extra>* _cluster_redis)
	{
		ip = "";
		port = 0;
		c = nullptr;
		cluster_redis = _cluster_redis;
	}
	std::string ip;
	int port;
	redisAsyncContext* c;
	std::map<std::string, redis_extra> * cluster_redis;
};
std::map<std::string, redis_extra> g_cluster_redis_addrs;

void getCallback(redisAsyncContext* c, void* r, void* privdata) {
	redisReply* reply = reinterpret_cast<redisReply*>(r);
	printf("argv[%s]: %x\n", (char*)privdata, reply);
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
	redis_extra* re = reinterpret_cast<redis_extra*>(c->data);
	re->c = (redisAsyncContext*)(c);

	if (g_redis_pass.length() > 0)
	{
		redisAsyncCommand((redisAsyncContext*)(c), [](redisAsyncContext* c, void* r, void* privdata) {
			redisReply* reply = reinterpret_cast<redisReply*>(r);
			if (reply == NULL) return;
			printf("result: %s\n", reply->str);
			g_redis_context = (redisAsyncContext*)(c);
			}, NULL, "AUTH %s", g_redis_pass.c_str());
	}
	else
	{
		g_redis_context = (redisAsyncContext*)(c);
	}
	if (g_redis_cluster == false)
	{
		if (g_redis_slot_pos)
		{
			std::string command ="SELECT " + std::to_string(g_redis_slot_pos);
			redisAsyncCommand((redisAsyncContext*)(c), [](redisAsyncContext* c, void* r, void* privdata) {
				redisReply* reply = reinterpret_cast<redisReply*>(r);
				if (reply == NULL) return;
				printf("result: %s\n", reply->str);
				}, (void *)command.c_str(), command.c_str());
		}
	}
}

void disconnectCallback(const redisAsyncContext* c, int status) {
	if (status != REDIS_OK) {
		printf("Error: %s\n", c->errstr);
		return;
	}
	printf("Disconnected...\n");
	g_redis_context = nullptr;
	redis_extra* re = reinterpret_cast<redis_extra*>(c->data);
	re->c = nullptr;
}
std::mutex m_action_lock;
std::condition_variable m_action_cond;
int test_main(const char* data) {
	g_cluster_redis_addrs.insert({
		{ "10.0.1.20:7000",redis_extra(&g_cluster_redis_addrs) },
		{ "10.0.1.22:7005",redis_extra(&g_cluster_redis_addrs) },
		{ "10.0.1.20:7001",redis_extra(&g_cluster_redis_addrs) },
	});
	g_running = true;

	std::thread redis_loop = std::thread([]() {
		uv_loop_t* loop = uv_default_loop();
		for (auto & it : g_cluster_redis_addrs)
		{
			it.second.ip = it.first.substr(0, it.first.find(":"));
			it.second.port = std::stoi(it.first.substr(it.first.find(":")+1));
			redisAsyncContext* c = redisAsyncConnect(it.second.ip.c_str(), it.second.port);
			/*redisOptions options = { 0 };
			struct timeval tv = { 0, 1000000 };
			options.type = REDIS_CONN_TCP;
			options.endpoint.tcp.ip = it.second.ip.c_str();
			options.endpoint.tcp.port = it.second.port;
			options.timeout = &tv;*/
			//REDIS_OPTIONS_SET_TCP(&options, ip, port);
			//redisAsyncContext* c = redisAsyncConnectWithOptions(&options);
			if (c->err)
			{
				/* Let *c leak for now... */
				printf("Error: %s\n", c->errstr);
				return;
			}
			c->data = (void*)&it.second;

			redisLibuvAttach(c, loop);
			redisAsyncSetConnectCallback(c, connectCallback);
			redisAsyncSetDisconnectCallback(c, disconnectCallback);
		}

		/*redisAsyncCommand(c, [](redisAsyncContext* c, void* r, void* privdata) {
			redisReply* reply = reinterpret_cast<redisReply*>(r);
			if (reply == NULL) return;
			printf("result: %s\n", reply->str);
			}, NULL, "SET key %b", data, strlen(data));
		redisAsyncCommand(c, getCallback, (char*)"end-1", "GET key");*/
		uv_run(loop, UV_RUN_DEFAULT);
		});
#define T_MOVED	"MOVED"
#define L_MOVED	5
	{
		std::string* p = new std::string;
		std::string* q = new std::string;
		p->assign("get foo");
		std::string* ptr[] = { p, q };
		std::unique_lock<std::mutex> lock(m_action_lock);
		while (g_task_list.empty())
		{
			m_action_cond.wait(lock);
		}
		redisAsyncCommand(g_redis_context, [](redisAsyncContext* c, void* r, void* privdata) {
			std::string* p = reinterpret_cast<std::string*>(privdata);
			std::string rep = "";
			redis_extra* re = reinterpret_cast<redis_extra*>(g_redis_context->data);
			redisReply* reply = reinterpret_cast<redisReply*>(r);
			if (reply == nullptr)
			{
				goto __leave_clean_free__;
			}
			printf("result1: %s\n", reply->str);
			if (reply->str == nullptr)
			{
				goto __leave_clean_free__;
			}
			rep = reply->str;

			if (rep.length() > L_MOVED&& rep.compare(0, L_MOVED, T_MOVED) == 0)
			{
				std::string dst = rep.substr(rep.rfind(" ") + 1);
				std::string* p2 = new std::string;
				printf("new p2!\n");
				p2->insert(0, p->c_str());
				redisAsyncCommand(re->cluster_redis->at(dst).c, [](redisAsyncContext* c, void* r, void* privdata) {
					std::string* p2 = reinterpret_cast<std::string*>(privdata);
					redisReply* reply = reinterpret_cast<redisReply*>(r);
					if (reply == NULL)
					{
						goto __leave_clean_free2__;
					}
					printf("result2: %s\n", reply->str);
					if (reply->str == nullptr)
					{
						goto __leave_clean_free2__;
					}
					{
						std::lock_guard<std::mutex> guard(m_action_lock);
						if (p2->compare(0, 3, "get") == 0)
						{
							g_task_list.push_back("set foo " + std::to_string(time(0)));
						}
						else
						{
							g_task_list.push_back("get foo");
						}
						m_action_cond.notify_one();
					}

				__leave_clean_free2__:
					if (p2 != nullptr)
					{
						p2->clear();
						delete p2;
						printf("free p2!\n");
					}
					}, p2, p->c_str());
			}
		__leave_clean_free__:
			if (p != nullptr)
			{
				p->clear();
				delete p;
				printf("free p!\n");
			}
			}, (void*)p, p->c_str());
		getchar();
	}
	std::thread work_task = std::thread([]() {
		g_task_list.push_back("get foo");
		while (g_running)
		{
			std::unique_lock<std::mutex> lock(m_action_lock);
			while (g_task_list.empty()) 
			{
				if (g_running == false)
				{
					break;
				}
				m_action_cond.wait(lock);
			}
			if (g_running == false)
			{
				break;
			}
			Sleep(3000);
			
			if (g_task_list.size() && g_redis_context != nullptr)
			{
				std::string* p = new std::string;
				printf("new p!\n");
				p->insert(0, g_task_list.front().c_str(), g_task_list.front().length());
				g_task_list.pop_front();
				lock.unlock();
				redisAsyncCommand(g_redis_context, [](redisAsyncContext* c, void* r, void* privdata) {
					std::string* p = reinterpret_cast<std::string*>(privdata);
					std::string rep = "";
					redis_extra* re = reinterpret_cast<redis_extra*>(g_redis_context->data);
					redisReply* reply = reinterpret_cast<redisReply*>(r);
					if (reply == nullptr)
					{
						goto __leave_clean__;
					}
					printf("result1: %s\n", reply->str);
					if (reply->str == nullptr)
					{
						goto __leave_clean__;
					}
					rep = reply->str;
					
					if (rep.length() > L_MOVED&& rep.compare(0, L_MOVED, T_MOVED) == 0)
					{
						std::string dst = rep.substr(rep.rfind(" ")+1);
						std::string* p2 = new std::string;
						printf("new p2!\n");
						p2->insert(0, p->c_str());
						redisAsyncCommand(re->cluster_redis->at(dst).c, [](redisAsyncContext* c, void* r, void* privdata) {
							std::string* p2 = reinterpret_cast<std::string*>(privdata);
							redisReply* reply = reinterpret_cast<redisReply*>(r);
							if (reply == NULL) 
							{
								goto __leave_clean2__;
							}
							printf("result2: %s\n", reply->str);
							if (reply->str == nullptr)
							{
								goto __leave_clean2__;
							}
							{
								std::lock_guard<std::mutex> guard(m_action_lock);
								if (p2->compare(0, 3, "get") == 0)
								{
									g_task_list.push_back("set foo " + std::to_string(time(0)));
								}
								else
								{
									g_task_list.push_back("get foo");
								}
								m_action_cond.notify_one();
							}

						__leave_clean2__:
							if (p2 != nullptr)
							{
								p2->clear();
								delete p2;
								printf("free p2!\n");
							}
							}, p2, p->c_str());
					}
					else
					{
						std::lock_guard<std::mutex> guard(m_action_lock);
						if (p->compare(0, 3, "get") == 0)
						{
							g_task_list.push_back("set foo " + std::to_string(time(0)));
						}
						else
						{
							g_task_list.push_back("get foo");
						}
						m_action_cond.notify_one();
					}

				__leave_clean__:
					if (p != nullptr)
					{
						p->clear();
						delete p;
						printf("free p!\n");
					}
					}, (void *)p, p->c_str());
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		}
		});
	
	getchar();
	if (work_task.joinable())
	{
		g_running = false;
		work_task.join();
	}

	if (redis_loop.joinable())
	{
		for (auto& it : g_cluster_redis_addrs)
		{
			if (it.second.c != nullptr)
				redisAsyncCommand(it.second.c, [](redisAsyncContext* c, void* r, void* privdata) {
				/* Disconnect after receiving the reply to GET */
				redisAsyncDisconnect(c);
					}, nullptr, "GET key");
		}
		redis_loop.join();
	}
	return 0;
}

int main()
{
	test_main("test");
    std::cout << "Hello World!\n";
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
