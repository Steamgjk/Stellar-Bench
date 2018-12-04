//
//  main.cpp
//  linux_socket_api
//
//  Created by Jinkun Geng
//  Copyright (c) 2016年 bikang. All rights reserved.
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
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <map>
#include "mf_common.h"
#include "rdma_two_sided_client_op.h"
#include "rdma_two_sided_server_op.h"
using namespace std;
#define GROUP_NUM 1
#define DIM_NUM 4

struct client_context c_ctx[CAP];
struct conn_context s_ctx[CAP];

/**Yahoo!Music**/
double yita = 0.001;
double theta = 0.05;


/*
char* remote_ips[CAP] = {"12.12.10.18", "12.12.10.18", "12.12.10.18", "12.12.10.18"};
int remote_ports[CAP] = {4411, 4412, 4413, 4414};
char* local_ips[CAP] = {"12.12.10.12", "12.12.10.15", "12.12.10.19", "12.12.10.17"};
int local_ports[CAP] = {5511, 5512, 5513, 5514};
**/
char* local_ips[CAP] = {"12.12.11.13", "12.12.11.13", "12.12.11.13", "12.12.11.13"};
int remote_ports[CAP] = {4411, 4412, 4413, 4414};
char* remote_ips[CAP] = {"12.12.11.13", "12.12.11.13", "12.12.11.13", "12.12.11.13"};
int local_ports[CAP] = {5511, 5512, 5513, 5514};

#define ThreshIter 1000
#define SEQ_LEN 5000
#define WORKER_THREAD_NUM 4

struct Block Pblock;
struct Block Qblock;
double* oldP;
double* oldQ;
vector<long> hash_ids;
std::vector<long> rates;
int block_seq[SEQ_LEN];


int wait4connection(char*local_ip, int local_port);
void sendTd(int send_thread_id);
void recvTd(int recv_thread_id);
void rdma_sendTd(int send_thread_id);
void rdma_recvTd(int recv_thread_id);
void rdma_sendTd_loop(int send_thread_id);
void rdma_recvTd_loop(int recv_thread_id);
void InitContext();
void submf();
void FakeSubMF();
void WriteLog(Block&Pb, Block&Qb, int iter_cnt);
void LoadRmatrix(int file_no, map<long, double>& myMap);
void CalcUpdt(int thread_id);
void LoadData();
bool CanCompute(int coming_iter, int recved_age);
bool CanSend(int to_send_iter, int completed_age);

int thread_id = -1;
struct timeval start, stop, diff;

int iter_t = 0;
int recved_age = -1;
int to_send_age = 0;
int completed_iter = -1;

int main(int argc, const char * argv[])
{
    for (int i = 0; i < CAP; i++)
    {
        local_ports[i] = 20000 + i;
        remote_ports[i] = 10000 + i;
    }
    int thresh_log = 1200;
    thread_id = atoi(argv[1]);
    if (argc >= 3)
    {
        thresh_log = atoi(argv[2]);
    }
    int th_id = thread_id;
    printf("recv th_id=%d\n", th_id );
    InitContext();
    std::thread recv_loop_thread(rdma_recvTd_loop, th_id);
    recv_loop_thread.detach();
    std::thread recv_thread(rdma_recvTd, th_id);
    recv_thread.detach();

    printf("wait for you for 3s\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    std::thread send_loop_thread(rdma_sendTd_loop, th_id);
    send_loop_thread.detach();
    std::thread send_thread(rdma_sendTd, th_id);
    send_thread.detach();
    memset(&start, 0, sizeof(struct timeval));
    memset(&stop, 0, sizeof(struct timeval));
    memset(&diff, 0, sizeof(struct timeval));

    iter_t = 0;

    while (1 == 1)
    {

        if (true == CanCompute(iter_t, recved_age))
        {
            if (thread_id == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            }
            //SGD
            int row_sta_idx = Pblock.sta_idx;
            int row_len = Pblock.height;
            int col_sta_idx = Qblock.sta_idx;
            int col_len = Qblock.height;
            int ele_num = row_len * col_len;
            //printf("before submf\n");
            //submf();
            //printf("after submf\n");
            FakeSubMF();
            completed_iter = iter_t;
            iter_t++;
            printf("completed_iter=%d to_compute=%d\n", completed_iter, iter_t );
        }
        else
        {
            //printf("NO iter_t=%d  recved_age=%d\n", iter_t, recved_age);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }


}
bool CanCompute(int coming_iter, int recved_age)
{
    if (coming_iter <= recved_age + 2)
    {
        return true;
    }
    else
    {
        return false;
    }
}
bool CanSend(int to_send_iter, int completed_age)
{
    if (to_send_iter == completed_age)
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
        c_ctx[i].buf_registered = false;
        c_ctx[i].buf_write_counter = 0;

        s_ctx[i].buf_registered = false;
        s_ctx[i].buf_recv_counter = 0;
    }
}


void LoadData()
{

}

void FakeSubMF()
{
    /*
        for (int k = 0; k < K; ++k)
        {
            Pblock.eles[i * K + k] += yita * (error * oldQ[j * K + k] - theta * oldP[i * K + k]);
        }
    **/
    int sample_num = 90000;

    double error = 0.0;
    printf("p-height=%d q-height=%d\n", Pblock.height, Qblock.height );
    for (int sn = 0; sn < sample_num; sn++)
    {
        //read a rating entry
        double r = 0.0;
        double predict_r = 0.0;
        int pr = random() % Pblock.height;
        int qc = random() % Qblock.height;
        /*
        if (sn <= 5)
        {
            printf("sn=%d pr=%d qc=%d\n", sn, pr, qc);
        }
        **/
        for (int k = 0; k < K; k++)
        {
            predict_r += Pblock.eles[pr * K + k] * Qblock.eles[qc * K + k];
        }

        error = r - predict_r;
        for (int k = 0; k < K; ++k)
        {
            Pblock.eles[pr * K + k] += yita * (error * Qblock.eles[qc * K + k] - theta * Pblock.eles[pr * K + k]);
            Qblock.eles[qc * K + k] += yita * (error * Pblock.eles[pr * K + k] - theta * Qblock.eles[qc * K + k]);
        }
        if (sn % 5000 == 0)
        {
            printf("sn == %d\n", sn );
        }
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
    //绑定ip和端口
    int check_ret = -1;
    do
    {
        printf("binding...\n");
        check_ret = bind(fd, (struct sockaddr*)&address, sizeof(address));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    while (check_ret >= 0);
    printf("bind ok\n");
    //创建监听队列，用来存放待处理的客户连接
    check_ret = listen(fd, 5);
    assert(check_ret >= 0);

    struct sockaddr_in addressClient;
    socklen_t clientLen = sizeof(addressClient);

    printf("thread %d listening at %s %d\n", thread_id, local_ip, local_port );
    //接受连接，阻塞函数
    int connfd = accept(fd, (struct sockaddr*)&addressClient, &clientLen);
    return connfd;

}

void rdma_sendTd_loop(int send_thread_id)
{
    char* remote_ip = remote_ips[send_thread_id];
    int remote_port = remote_ports[send_thread_id];
    char str_port[100];
    sprintf(str_port, "%d", remote_port);
    RdmaTwoSidedClientOp ct;
    ct.rc_client_loop(remote_ip, str_port, &(c_ctx[send_thread_id]));
}

void rdma_recvTd_loop(int recv_thread_id)
{
    int bind_port = local_ports[recv_thread_id];
    char str_port[100];
    sprintf(str_port, "%d", bind_port);
    RdmaTwoSidedServerOp rtos;
    rtos.rc_server_loop(str_port, &(s_ctx[recv_thread_id]));

}


void rdma_sendTd(int send_thread_id)
{

    size_t struct_sz = sizeof(Block);

    printf("[%d] has registered send buffer\n", send_thread_id);
    while (1 == 1)
    {
        //printf("canSend=%d\n", canSend );
        if (true == CanSend(to_send_age, completed_iter))
        {
            printf("Td:%d cansend  %d\n", thread_id, to_send_age );
            size_t p_data_sz = sizeof(double) * Pblock.ele_num;
            size_t q_data_sz = sizeof(double) * Qblock.ele_num;
            size_t p_total = struct_sz + p_data_sz;
            size_t q_total = struct_sz + q_data_sz;
            size_t total_len = p_total + q_total;
            char* buf = c_ctx[send_thread_id].buffer;
            memcpy(buf, &(Pblock), struct_sz);
            memcpy(buf + struct_sz, (Pblock.eles), p_data_sz);
            memcpy(buf + p_total, &(Qblock), struct_sz);
            memcpy(buf + p_total + struct_sz , (Qblock.eles), q_data_sz);
            c_ctx[send_thread_id].buf_len = total_len;
            c_ctx[send_thread_id].can_send = true;
            printf("should have sent %d\n", to_send_age );
            to_send_age++;
        }
        else
        {
            //printf("completed_iter=%d to_send_age=%d\n", completed_iter, to_send_age );
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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
        //buf_recv_counter
        int to_recv_age = recved_age + 1;
        if (to_recv_age >= s_ctx[recv_thread_id].buf_recv_counter)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;

        }
        printf("to_recv_age=%d\n", to_recv_age );
        char* real_sta_buf = s_ctx[recv_thread_id].buffer;

        struct Block* pb = (struct Block*)(void*)real_sta_buf;
        Pblock.block_id = pb->block_id;

        Pblock.sta_idx = pb->sta_idx;
        Pblock.height = pb->height;
        Pblock.ele_num = pb->ele_num;
        Pblock.eles = Malloc(double, pb->ele_num);
        double* data_eles = (double*)(void*) (real_sta_buf + struct_sz);
        memcpy(Pblock.eles, data_eles, sizeof(double)*Pblock.ele_num);
        Pblock.data_age = pb->data_age;

        size_t p_total = struct_sz + sizeof(double) * (pb->ele_num);
        struct Block* qb = (struct Block*)(void*)(real_sta_buf + p_total);
        Qblock.block_id = qb->block_id;
        Qblock.sta_idx = qb->sta_idx;
        Qblock.height = qb->height;
        Qblock.ele_num = qb-> ele_num;
        Qblock.eles = Malloc(double, qb->ele_num);
        data_eles = (double*)(void*)(real_sta_buf + p_total + struct_sz);
        memcpy(Qblock.eles, data_eles, sizeof(double)*Qblock.ele_num);
        Qblock.data_age = qb->data_age;
        printf("pid=%d page=%d\n", Pblock.block_id, Pblock.data_age );
        printf("qid=%d qage=%d\n", Qblock.block_id, Qblock.data_age );
        /*
                for (int i = 0; i < Pblock.height; i++)
                {
                    if (Pblock.eles[i * K] != 0.1 && Pblock.eles[i * K] != 0.2 && Pblock.eles[i * K] != 0.3 && Pblock.eles[i * K] != 0.0)
                    {
                        printf("[%d]%f\t", i, Pblock.eles[i * K] );
                    }
                }
                printf("\n");
        **/
        //this buf I have read it, so please prepare new buf content
        recved_age++;
        s_ctx[recv_thread_id].can_recv = true;
        printf("recved_age=%d\n", recved_age );

    }

}
