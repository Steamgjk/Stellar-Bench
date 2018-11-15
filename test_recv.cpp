
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
#define RPORT 5555
#define LPORT 4444

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
	s_ctx.buf_recv_counter = 0;
	s_ctx.can_recv = true;
	std::thread recv_loop_thread(rdma_recvTd_loop);
	recv_loop_thread.detach();
	while ( s_ctx.buf_registered == false)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
	printf("BUF registered!\n");
	int read_counter = 0;
	while (1 == 1)
	{
		if (read_counter >= s_ctx.buf_recv_counter)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			printf("read_counter=%d  buf_recv_counter=%d\n", read_counter, s_ctx.buf_recv_counter );
		}
		else
		{
			printf("len= %d\n", s_ctx.buf_len );
			for (int i = 0; i < s_ctx.buf_len; i++)
			{
				printf("%c", s_ctx.buffer[i] );
			}
			printf("\n");
			read_counter++;
			s_ctx.can_recv = true;
		}
	}
}