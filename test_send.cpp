
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <cstring>
#include <cmath>
#include <time.h>
#include <vector>
#include <list>
#include <thread>
#include <chrono>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sys/time.h>
#include <map>
#include "mf_common.h"
#include "rdma_two_sided_client_op.h"
#include "rdma_two_sided_server_op.h"
using namespace std;

#define RIP "12.12.10.13"
#define LIP "12.12.11.13"
#define RPORT 4444
#define LPORT 5555

void rdma_sendTd_loop();
void rdma_recvTd_loop();
struct client_context c_ctx;
struct conn_context s_ctx;
void rdma_sendTd_loop()
{
	string remote_ip = RIP;
	int remote_port = RPORT;
	printf("remote_ip=%s  remote_port=%d\n", remote_ip.c_str(), remote_port);
	char str_port[100];
	sprintf(str_port, "%d", remote_port);
	RdmaTwoSidedClientOp ct;
	ct.rc_client_loop(remote_ip.c_str(), str_port, &(c_ctx));
}

void rdma_recvTd_loop()
{
	int bind_port =  LPORT;
	char str_port[100];
	sprintf(str_port, "%d", bind_port);
	printf("bind_port = %d\n", bind_port );
	RdmaTwoSidedServerOp rtos;
	rtos.rc_server_loop(str_port, &(s_ctx));
}

int main(int argc, const char * argv[])
{
	c_ctx.can_send = false;
	c_ctx.buf_prepared = false;
	c_ctx.buf_len = 100;
	int to_send_counter = 0;
	c_ctx.buffer = (char*)malloc(c_ctx.buf_len);
	std::thread send_loop_thread(rdma_sendTd_loop);
	c_ctx.buf_write_counter = 0;
	send_loop_thread.detach();

	printf("before start\n");
	getchar();
	while (1 == 1)
	{
		if (to_send_counter > c_ctx.buf_write_counter)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			printf("to_send_counter=%d buf_write_counter=%d\n", to_send_counter,  c_ctx.buf_write_counter);
		}
		else
		{
			for (int j = 0; j < c_ctx.buf_len; j++)
			{
				c_ctx.buffer[j] = 'a' + to_send_counter;
			}
			printf("to send ready\n");
			for (int j = 0; j < c_ctx.buf_len; j++)
			{
				printf("%c", c_ctx.buffer[j] );
			}
			printf("\n");
			c_ctx.can_send = true;
			to_send_counter++;
			getchar();
		}
	}

	while (1 == 1)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}