//
//  main.cpp
//  linux_socket_api
//
//  Created by Jinkun Geng on 18/05/11.
//  Copyright (c) 2016年 Jinkun Geng. All rights reserved.
//

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
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


struct client_context c_ctx[CAP];
struct conn_context s_ctx[CAP];
/*
char* local_ips[CAP] = {"12.12.10.18", "12.12.10.18", "12.12.10.18", "12.12.10.18"};
int local_ports[CAP] = {4411, 4412, 4413, 4414};
char* remote_ips[CAP] = {"12.12.10.12", "12.12.10.15", "12.12.10.19", "12.12.10.17"};
int remote_ports[CAP] = {5511, 5512, 5513, 5514};
**/
char* local_ips[CAP] = {"12.12.11.13", "12.12.11.13", "12.12.11.13", "12.12.11.13"};
int local_ports[CAP] = {4411, 4412, 4413, 4414};
char* remote_ips[CAP] = {"12.12.11.13", "12.12.11.13", "12.12.11.13", "12.12.11.13"};
int remote_ports[CAP] = {5511, 5512, 5513, 5514};
struct Block Pblocks[CAP];
struct Block Qblocks[CAP];
int worker_num = WORKER_NUM;


void WriteLog(Block&Pb, Block&Qb, int iter_cnt);
int wait4connection(char*local_ip, int local_port);
void sendTd(int send_thread_id);
void recvTd(int recv_thread_id);
void rdma_sendTd(int send_thread_id);
void rdma_recvTd(int recv_thread_id);
void rdma_sendTd_loop(int send_thread_id);
void rdma_recvTd_loop(int recv_thread_id);
void InitContext();
void partitionP(int portion_num,  Block* Pblocks);
void partitionQ(int portion_num,  Block* Qblocks);
void InitFlag();
bool CanMerge(int coming_iter, int r_iter[], int len);
void partitionBlock(int rc_num, int dim, int portion_num,  Block * Blocks);

int recved_iter[CAP];
int sended_iter[CAP];
int worker_pidx[CAP];
int worker_qidx[CAP];
long long time_span[300];
int iter_t = 0;
int completed_iter = -1;
int main(int argc, const char * argv[])
{
    for (int i = 0; i < CAP; i++)
    {
        local_ports[i] = 10000 + i;
        remote_ports[i] = 20000 + i;
    }
    InitContext();
    //gen P and Q
    if (argc == 2)
    {
        worker_num = atoi(argv[1]) ;
    }

    int thid = 0;
    for (thid = 0; thid < worker_num; thid++)
    {
        printf("thid=%d\n", thid );
        std::thread recv_loop_thread(rdma_recvTd_loop, thid);
        recv_loop_thread.detach();
        std::thread recv_thread(rdma_recvTd, thid);
        recv_thread.detach();
    }
    printf("get char....\n");
    getchar();

    for (thid = 0; thid < worker_num; thid++)
    {
        std::thread send_loop_thread(rdma_sendTd_loop, thid);
        send_loop_thread.detach();
        std::thread send_thread(rdma_sendTd, thid);
        send_thread.detach();
    }

    printf("wait for 3s\n");

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    srand(1);
    //LoadTestRating();
    //printf("Load Complete\n");
    printf("start work\n");
    partitionBlock(N, K, worker_num, Pblocks);
    partitionBlock(M, K, worker_num, Qblocks);
    for (int i = 0; i < worker_num; i++)
    {
        for (int j = 0; j < Pblocks[i].ele_num; j++)
        {
            Pblocks[i].eles[j] = drand48() * 0.2;
        }
        for (int j = 0; j < Qblocks[i].ele_num; j++)
        {
            Qblocks[i].eles[j] = drand48() * 0.2;
        }
    }

    for (int i = 0; i < worker_num; i++)
    {
        recved_iter[i] = -1;
        sended_iter[i] = -1;
        worker_pidx[i] = worker_qidx[i] = i;
    }

    struct timeval beg, ed;
    iter_t = 0;
    gettimeofday(&beg, 0);
    while (1 == 1)
    {
        if (false == CanMerge(iter_t, recved_iter, worker_num))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        else
        {
            printf("hehhe\n");
            printf("%d\n", CanMerge(iter_t, recved_iter, worker_num));
        }
        printf("iter_t = %d recv_iter =%d\n", iter_t, recved_iter[0]);
        getchar();

        srand(time(0));
        random_shuffle(worker_qidx, worker_qidx + worker_num); //迭代器
        for (int i = 0; i < worker_num; i++)
        {
            printf("%d  [%d:%d]\n", i, worker_pidx[i], worker_qidx[i] );
        }

        if (iter_t % 10 == 0 )
        {
            gettimeofday(&ed, 0);
            time_span[iter_t / 10] = (ed.tv_sec - beg.tv_sec) * 1000000 + ed.tv_usec - beg.tv_usec;
            printf("time= %d\t%lld\n", iter_t, time_span[iter_t / 10] );

        }
        completed_iter = iter_t;
        iter_t++;
        if (iter_t == 1200)
        {
            exit(0);
        }

    }

    return 0;
}

bool CanMerge(int coming_iter, int r_iter[], int len)
{
    int i = 0;
    for (i = 0; i < len; i++)
    {
        if (coming_iter > r_iter[i] + 1)
        {
            return false;
        }
        else
        {
            printf("coming_iter=%d  r=%d\n", coming_iter, r_iter[i] );
        }
    }
    return true;
}
bool CanSend(int sended_age, int completed_age)
{
    if (sended_age <= completed_age)
    {
        return true;
    }
    else
    {
        return false;
    }

}
void InitContext()
{
    for (int i = 0; i < CAP; i++)
    {
        c_ctx[i].buf_prepared = false;
        c_ctx[i].buf_registered = false;
        s_ctx[i].buf_prepared = false;
        s_ctx[i].buf_registered = false;

    }
}

int wait4connection(char*local_ip, int local_port)
{
    int fd = socket(PF_INET, SOCK_STREAM , 0);
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    //转换成网络地址
    address.sin_port = htons(local_port);
    address.sin_family = AF_INET;
    //地址转换
    inet_pton(AF_INET, local_ip, &address.sin_addr);
    //设置socket buffer大小
    //int recvbuf = 4096;
    //int len = sizeof( recvbuf );
    //setsockopt( fd, SOL_SOCKET, SO_RCVBUF, &recvbuf, sizeof( recvbuf ) );
    //getsockopt( fd, SOL_SOCKET, SO_RCVBUF, &recvbuf, ( socklen_t* )&len );
    //printf( "the receive buffer size after settting is %d\n", recvbuf );
    //绑定ip和端口
    int check_ret = -1;
    do
    {
        printf("binding... %s  %d\n", local_ip, local_port);
        check_ret = bind(fd, (struct sockaddr*)&address, sizeof(address));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    while (check_ret >= 0);

    //创建监听队列，用来存放待处理的客户连接
    check_ret = listen(fd, 5);
    assert(check_ret >= 0);
    printf("listening... %s  %d\n", local_ip, local_port);
    struct sockaddr_in addressClient;
    socklen_t clientLen = sizeof(addressClient);
    //接受连接，阻塞函数
    int connfd = accept(fd, (struct sockaddr*)&addressClient, &clientLen);
    printf("get connection from %s  %d\n", inet_ntoa(addressClient.sin_addr), addressClient.sin_port);
    return connfd;

}



void partitionBlock(int rc_num, int dim, int portion_num,  Block * Blocks)
{
    int i = 0;
    int height = (rc_num + portion_num - 1) / portion_num;
    int acc_height = 0;
    for (i = 0; i < portion_num; i++)
    {
        Blocks[i].block_id = i;
        Blocks[i].data_age = 0;
        Blocks[i].sta_idx = i * height;
        if (acc_height + height <= rc_num)
        {
            Blocks[i].height = height;
        }
        else
        {
            Blocks[i].height = rc_num - acc_height;
        }
        acc_height += Blocks[i].height;
        Blocks[i].ele_num = Blocks[i].height * dim;
        Blocks[i].eles = Malloc(double, Blocks[i].ele_num);
    }

}


void rdma_sendTd(int send_thread_id)
{
    size_t struct_sz = sizeof(Block);
    while (c_ctx[send_thread_id].buf_registered == false)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    printf("[%d] has registered send buffer\n", send_thread_id);
    while (1 == 1)
    {
        if (false == CanSend(sended_iter[send_thread_id], completed_iter) )
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        {
            int pbid = worker_pidx[send_thread_id];
            int qbid = worker_qidx[send_thread_id];
            //printf("%d] canSend pbid=%d  qbid=%d sid=%d\n", send_thread_id, pbid, qbid, send_thread_id % WORKER_NUM );
            size_t p_data_sz = sizeof(double) * Pblocks[pbid].ele_num;
            size_t p_total = struct_sz + p_data_sz;
            size_t q_data_sz = sizeof(double) * Qblocks[qbid].ele_num;
            size_t q_total = struct_sz + q_data_sz;
            size_t total_len = p_total + q_total;
            char* real_sta_buf = c_ctx[send_thread_id].buffer;

            memcpy(real_sta_buf, &(Pblocks[pbid]), struct_sz);
            memcpy(real_sta_buf + struct_sz, (char*) & (Pblocks[pbid].eles[0]), p_data_sz );
            memcpy(real_sta_buf + p_total, &(Qblocks[qbid]), struct_sz);
            memcpy(real_sta_buf + p_total + struct_sz , (char*) & (Qblocks[qbid].eles[0]), q_data_sz);
            c_ctx[send_thread_id].buf_len = total_len;
            c_ctx[send_thread_id].buf_prepared = true;

            sended_iter[send_thread_id]++;
        }

    }
}

void rdma_recvTd(int recv_thread_id)
{
    size_t struct_sz = sizeof(Block);
    while (s_ctx[recv_thread_id].buf_registered == false)
    {
        //printf("[%d] recv has not registered buffer\n", recv_thread_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    printf("[%d] has registered receive buffer\n", recv_thread_id);
    while (1 == 1)
    {

        if (s_ctx[recv_thread_id].buf_prepared == false)
        {
            //printf("[%d] recv buf_prepared = false\n", recv_thread_id );
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        //printf("[%d] recv buf_prepared = true\n", recv_thread_id );

        char* real_sta_buf = s_ctx[recv_thread_id].buffer;
        struct Block * pb = (struct Block*)(void*)(real_sta_buf);
        int block_idx = pb->block_id ;
        Pblocks[block_idx].block_id = pb->block_id;
        Pblocks[block_idx].sta_idx = pb->sta_idx;
        Pblocks[block_idx].height = pb->height;
        Pblocks[block_idx].ele_num = pb->ele_num;
        Pblocks[block_idx].eles = Malloc(double, pb->ele_num);
        Pblocks[block_idx].isP = pb->isP;
        double*data_eles = (double*)(void*) (real_sta_buf + struct_sz);
        size_t data_sz = pb->ele_num * sizeof(double);
        memcpy(Pblocks[block_idx].eles, data_eles, data_sz);

        /*
                for (int i = 0; i < pb->ele_num; i++)
                {
                    Pblocks[block_idx].eles[i] = data_eles[i];
                }
        **/

        size_t p_total = struct_sz + data_sz;
        struct Block * qb = (struct Block*)(void*)(real_sta_buf + p_total);
        data_eles = (double*)(void*) (real_sta_buf + p_total + struct_sz);
        data_sz = qb->ele_num * sizeof(double);
        block_idx = qb->block_id ;
        Qblocks[block_idx].block_id = qb->block_id;
        Qblocks[block_idx].sta_idx = qb->sta_idx;
        Qblocks[block_idx].height = qb->height;
        Qblocks[block_idx].ele_num = qb->ele_num;
        Qblocks[block_idx].eles = Malloc(double, qb->ele_num);
        Qblocks[block_idx].isP = qb->isP;
        memcpy(Qblocks[block_idx].eles, data_eles, data_sz);
        /*
        for (int i = 0; i < qb->ele_num; i++)
        {
            Qblocks[block_idx].eles[i] = data_eles[i];
        }
        **/
        //this buf I have read it, so please prepare new buf content
        s_ctx[recv_thread_id].buf_prepared = false;
        recved_iter[recv_thread_id]++;
    }
}

void rdma_sendTd_loop(int send_thread_id)
{
    int mapped_thread_id = send_thread_id % WORKER_NUM;
    char* remote_ip = remote_ips[mapped_thread_id];
    int remote_port = remote_ports[send_thread_id];
    printf("send_thread_id=%d\n", send_thread_id);
    char str_port[100];
    sprintf(str_port, "%d", remote_port);
    RdmaTwoSidedClientOp ct;
    ct.rc_client_loop(remote_ip, str_port, &(c_ctx[send_thread_id]));
}

void rdma_recvTd_loop(int recv_thread_id)
{
    int bind_port =  local_ports[recv_thread_id];
    char str_port[100];
    sprintf(str_port, "%d", bind_port);
    RdmaTwoSidedServerOp rtos;
    rtos.rc_server_loop(str_port, &(s_ctx[recv_thread_id]));

}


