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

#include <thread>


#include <deque>
bool g_running = false;
redisAsyncContext* g_redis_context = nullptr;
std::deque<std::string> g_task_list;

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
	g_redis_context = (redisAsyncContext*)(c);
}

void disconnectCallback(const redisAsyncContext* c, int status) {
	if (status != REDIS_OK) {
		printf("Error: %s\n", c->errstr);
		return;
	}
	printf("Disconnected...\n");
	g_redis_context = nullptr;
}

int test_main(const char * data) {
	//signal(SIGPIPE, SIG_IGN);
	
	g_running = true;

	std::thread redis_loop = std::thread([]() {
		uv_loop_t* loop = uv_default_loop();

		redisAsyncContext* c = redisAsyncConnect("10.0.3.103", 6379);
		if (c->err)
		{
			/* Let *c leak for now... */
			printf("Error: %s\n", c->errstr);
			return 1;
		}

		redisLibuvAttach(c, loop);
		redisAsyncSetConnectCallback(c, connectCallback);
		redisAsyncSetDisconnectCallback(c, disconnectCallback);

		/*redisAsyncCommand(c, [](redisAsyncContext* c, void* r, void* privdata) {
			redisReply* reply = reinterpret_cast<redisReply*>(r);
			if (reply == NULL) return;
			printf("result: %s\n", reply->str);
			}, NULL, "SET key %b", data, strlen(data));
		redisAsyncCommand(c, getCallback, (char*)"end-1", "GET key");*/
		uv_run(loop, UV_RUN_DEFAULT);
		});
	std::thread work_task = std::thread([]() {
		while (g_running)
		{
			if (g_task_list.size())
			{
				std::string cmd = g_task_list.front();
				g_task_list.pop_front();
				redisAsyncCommand(g_redis_context, [](redisAsyncContext* c, void* r, void* privdata) {
					redisReply* reply = reinterpret_cast<redisReply*>(r);
					if (reply == NULL) return;
					printf("result: %s\n", reply->str);
					}, nullptr, cmd.c_str());
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		}
		});
	if (redis_loop.joinable())
	{
		//redisAsyncCommand(c, getCallback, (char*)"end-1", "GET key");
		if(g_redis_context != nullptr)
			redisAsyncCommand(g_redis_context, [](redisAsyncContext* c, void* r, void* privdata) {
				/* Disconnect after receiving the reply to GET */
				redisAsyncDisconnect(c);
				}, nullptr, "GET key");
		redis_loop.join();
	}
	if (work_task.joinable())
	{
		g_running = false;
		work_task.join();
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
