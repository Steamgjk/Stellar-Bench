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


#include "rdma_two_sided_client_op.h"
#include "rdma_two_sided_server_op.h"


using namespace std;
#define CAP 2000

#define FILE_NAME "./yahoo-output/train-"
#define TEST_NAME "./yahoo-output/test"
#define N 1000990
#define M 624961
#define K  100 //主题个数

struct client_context c_ctx[CAP];
struct conn_context s_ctx[CAP];

#define QP_GROUP 1
int send_round_robin_idx[CAP];
int recv_round_robin_idx[CAP];

int WORKER_NUM = 4;
char* local_ips[CAP] = {"12.12.10.18", "12.12.10.18", "12.12.10.18", "12.12.10.18"};
int local_ports[CAP] = {4411, 4412, 4413, 4414};
char* remote_ips[CAP] = {"12.12.10.12", "12.12.10.15", "12.12.10.19", "12.12.10.17"};
int remote_ports[CAP] = {5511, 5512, 5513, 5514};




struct Block
{
    int block_id;
    int data_age;
    int sta_idx;
    int height; //height
    int ele_num;
    bool isP;
    vector<double> eles;
    Block()
    {

    }
    Block operator=(Block& bitem)
    {
        block_id = bitem.block_id;
        data_age = bitem.data_age;
        height = bitem.height;
        eles = bitem.eles;
        ele_num = bitem.ele_num;
        sta_idx = bitem.sta_idx;
        return *this;
    }
    void printBlock()
    {

        printf("block_id  %d\n", block_id);
        printf("data_age  %d\n", data_age);
        printf("ele_num  %d\n", ele_num);
        for (int i = 0; i < eles.size(); i++)
        {
            printf("%lf\t", eles[i]);
        }
        printf("\n");

    }
};
struct Updates
{
    int block_id;
    int clock_t;
    int ele_num;
    vector<double> eles;
    Updates()
    {

    }
    Updates operator=(Updates& uitem)
    {
        block_id = uitem.block_id;
        clock_t = uitem.clock_t;
        ele_num = uitem.ele_num;
        eles = uitem.eles;
        return *this;
    }

    void printUpdates()
    {
        printf("update block_id %d\n", block_id );
        printf("clock_t  %d\n", clock_t);
        printf("ele size %ld\n", ele_num);
        for (int i = 0; i < eles.size(); i++)
        {
            printf("%lf\t", eles[i]);
        }
        printf("\n");
    }
};
struct Block Pblocks[CAP];
struct Block Qblocks[CAP];
struct Updates Pupdts[CAP];
struct Updates Qupdts[CAP];


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

atomic_int recvCount(0);
bool canSend[CAP] = {false};
int worker_pidx[CAP];
int worker_qidx[CAP];

long long time_span[300];
int iter_t = 0;
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
        WORKER_NUM = atoi(argv[1]) ;
    }


    for (int gp = 0; gp < QP_GROUP; gp++)
    {
        for (int recv_thread_id = 0; recv_thread_id < WORKER_NUM; recv_thread_id++)
        {
            int thid = recv_thread_id + gp * WORKER_NUM;
            printf("thid=%d\n", thid );
            //std::thread recv_thread(recvTd, thid);
            //recv_thread.detach();

            std::thread recv_loop_thread(rdma_recvTd_loop, thid);
            recv_loop_thread.detach();

            std::thread recv_thread(rdma_recvTd, thid);
            recv_thread.detach();

        }
    }

    for (int gp = 0; gp < QP_GROUP; gp++)
    {
        for (int send_thread_id = 0; send_thread_id < WORKER_NUM; send_thread_id++)
        {
            int thid = send_thread_id + gp * WORKER_NUM;
            //std::thread send_thread(sendTd, thid);
            //send_thread.detach();

            std::thread send_loop_thread(rdma_sendTd_loop, thid);
            send_loop_thread.detach();

            std::thread send_thread(rdma_sendTd, thid);
            send_thread.detach();

        }
    }

    printf("wait for 3s\n");

    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    srand(1);
    //LoadTestRating();
    //printf("Load Complete\n");
    printf("start work\n");
    partitionP(WORKER_NUM, Pblocks);
    partitionQ(WORKER_NUM, Qblocks);
    for (int i = 0; i < WORKER_NUM; i++)
    {
        for (int j = 0; j < Pblocks[i].ele_num; j++)
        {
            //Pblocks[i].eles[j] = drand48() * 0.6;
            //Pblocks[i].eles[j] = drand48() * 0.3;
            //Pblocks[i].eles[j] = drand48() * 1.6;
            Pblocks[i].eles[j] = drand48() * 0.2;
        }
        for (int j = 0; j < Qblocks[i].ele_num; j++)
        {
            //Qblocks[i].eles[j] = drand48() * 0.6;
            //Qblocks[i].eles[j] = drand48() * 0.3;
            //Qblocks[i].eles[j] = drand48() * 1.6;
            Qblocks[i].eles[j] = drand48() * 0.2;
        }
    }

    for (int i = 0; i < WORKER_NUM; i++)
    {
        canSend[i] = false;
    }
    for (int i = 0; i < WORKER_NUM; i++)
    {
        worker_pidx[i] = worker_qidx[i] = i;
    }

    /*
        for (int send_thread_id = 0; send_thread_id < WORKER_NUM; send_thread_id++)
        {
            std::thread send_thread(sendTd, send_thread_id);
            send_thread.detach();
        }
        for (int recv_thread_id = 0; recv_thread_id < WORKER_NUM; recv_thread_id++)
        {
            std::thread recv_thread(recvTd, recv_thread_id);
            recv_thread.detach();
        }
    **/

    /*
        std::thread send_thread(rdma_sendTd, 2);
        send_thread.detach();
        std::thread recv_thread(rdma_recvTd, 2);
        recv_thread.detach();
    **/

    for (int i = 0; i < WORKER_NUM; i++)
    {
        worker_pidx[i] = i;
        worker_qidx[i] = 3 - i;
    }
    for (int i = 0; i < WORKER_NUM; i++)
    {
        send_round_robin_idx[i] = i;
        recv_round_robin_idx[i] = i;
    }
    struct timeval beg, ed;
    iter_t = 0;
    while (1 == 1)
    {
        srand(time(0));
        bool ret = false;
        //random_shuffle(worker_pidx, worker_pidx + WORKER_NUM); //迭代器
        random_shuffle(worker_qidx, worker_qidx + WORKER_NUM); //迭代器


        for (int i = 0; i < WORKER_NUM; i++)
        {
            printf("%d  [%d:%d]\n", i, worker_pidx[i], worker_qidx[i] );
        }

        //printf("here start to send\n");
        //getchar();
        printf("[%d]canSend...!\n", iter_t);
        for (int i = 0; i < WORKER_NUM; i++)
        {
            canSend[i] = true;
        }

        //getchar();
        while (recvCount != WORKER_NUM)
        {
            //cout << "RecvCount\t" << recvCount << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }


        if (iter_t == 0)
        {
            gettimeofday(&beg, 0);
        }
        if (recvCount == WORKER_NUM)
        {
            if (iter_t % 10 == 0 )
            {
                gettimeofday(&ed, 0);

                for (int bid = 0; bid < WORKER_NUM; bid++)
                {
                    //WriteLog(Pblocks[bid], Qblocks[bid], iter_t);
                }

                time_span[iter_t / 10] = (ed.tv_sec - beg.tv_sec) * 1000000 + ed.tv_usec - beg.tv_usec;
                printf("time= %d\t%lld\n", iter_t, time_span[iter_t / 10] );

            }


            recvCount = 0;
        }
        iter_t++;
        if (iter_t % 100 == 0)
        {
            for (int i = 0; i <= iter_t / 10; i++)
            {
                printf("%lld\n", time_span[i] );
            }

        }
        if (iter_t == 1200)
        {
            exit(0);
        }

    }

    return 0;
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

void WriteLog(Block & Pb, Block & Qb, int iter_cnt)
{
    char fn[100];
    sprintf(fn, "./Rtrack/Pblock-%d-%d", iter_cnt, Pb.block_id);
    ofstream pofs(fn, ios::trunc);
    for (int h = 0; h < Pb.height; h++)
    {
        for (int j = 0; j < K; j++)
        {
            pofs << Pb.eles[h * K + j] << " ";
        }
        pofs << endl;
    }
    printf("fn:%s\n", fn );
    sprintf(fn, "./Rtrack/Qblock-%d-%d", iter_cnt, Qb.block_id);
    ofstream qofs(fn, ios::trunc);
    for (int h = 0; h < Qb.height; h++)
    {
        for (int j = 0; j < K; j++)
        {
            qofs << Qb.eles[h * K + j] << " ";
        }
        qofs << endl;
    }
    printf("fn:%s\n", fn );
    //getchar();
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



void partitionP(int portion_num,  Block * Pblocks)
{
    int i = 0;
    int height = N / portion_num;
    int last_height = N - (portion_num - 1) * height;

    for (i = 0; i < portion_num; i++)
    {
        Pblocks[i].block_id = i;
        Pblocks[i].data_age = 0;
        Pblocks[i].eles.clear();
        Pblocks[i].height = height;
        int sta_idx = i * height;
        if ( i == portion_num - 1)
        {
            Pblocks[i].height = last_height;
        }
        Pblocks[i].sta_idx = sta_idx;
        //printf("i-%d sta_idx-%d\n", i, sta_idx );
        Pblocks[i].ele_num = Pblocks[i].height * K;
        Pblocks[i].eles.resize(Pblocks[i].ele_num);
    }

}

void partitionQ(int portion_num,  Block * Qblocks)
{
    int i = 0;
    int height = M / portion_num;
    int last_height = M - (portion_num - 1) * height;

    for (i = 0; i < portion_num; i++)
    {
        Qblocks[i].block_id = i;
        Qblocks[i].data_age = 0;
        Qblocks[i].eles.clear();
        Qblocks[i].height = height;
        int sta_idx = i * height;
        if ( i == portion_num - 1)
        {
            Qblocks[i].height = last_height;
        }
        Qblocks[i].sta_idx = sta_idx;
        Qblocks[i].ele_num = Qblocks[i].height * K;
        Qblocks[i].eles.resize(Qblocks[i].ele_num);

    }

}


void rdma_sendTd(int send_thread_id)
{
    int mapped_thread_id = send_thread_id % WORKER_NUM;
    size_t struct_sz = sizeof(Block);
    while (c_ctx[send_thread_id].buf_registered == false)
    {
        //printf("[%d] has not registered buffer\n", send_thread_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    printf("[%d] has registered send buffer\n", send_thread_id);
    while (1 == 1)
    {
        if ( send_round_robin_idx[mapped_thread_id] != send_thread_id || (canSend[mapped_thread_id] == false) )
        {
            //printf("canSend =%d\n", canSend[mapped_thread_id] );
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        //printf("iter_t=%d send_thread_id=%d mapped_thread_id=%d\n", iter_t, send_thread_id, mapped_thread_id );
        if (canSend[mapped_thread_id] == true)
        {
            int pbid = worker_pidx[mapped_thread_id];
            int qbid = worker_qidx[mapped_thread_id];
            //printf("%d] canSend pbid=%d  qbid=%d sid=%d\n", send_thread_id, pbid, qbid, send_thread_id % WORKER_NUM );
            size_t p_data_sz = sizeof(double) * Pblocks[pbid].eles.size();
            size_t p_total = struct_sz + p_data_sz;
            size_t q_data_sz = sizeof(double) * Qblocks[qbid].eles.size();
            size_t q_total = struct_sz + q_data_sz;
            size_t total_len = p_total + q_total;
            char* real_sta_buf = c_ctx[send_thread_id].buffer;

            memcpy(real_sta_buf, &(Pblocks[pbid]), struct_sz);
            memcpy(real_sta_buf + struct_sz, (char*) & (Pblocks[pbid].eles[0]), p_data_sz );
            memcpy(real_sta_buf + p_total, &(Qblocks[qbid]), struct_sz);
            memcpy(real_sta_buf + p_total + struct_sz , (char*) & (Qblocks[qbid].eles[0]), q_data_sz);

            //printf("[Td:%d] send success qbid=%d isP=%d  total_len=%ld qh=%d\n", send_thread_id, qbid, Qblocks[qbid].isP, total_len, Qblocks[qbid].height);

            c_ctx[send_thread_id].buf_len = total_len;
            c_ctx[send_thread_id].buf_prepared = true;

            send_round_robin_idx[mapped_thread_id] = (send_round_robin_idx[mapped_thread_id] + WORKER_NUM) % (WORKER_NUM * QP_GROUP);
            canSend[mapped_thread_id] = false;
        }

    }
}

void rdma_recvTd(int recv_thread_id)
{
    int mapped_thread_id = recv_thread_id % WORKER_NUM;
    size_t struct_sz = sizeof(Block);
    while (s_ctx[recv_thread_id].buf_registered == false)
    {
        //printf("[%d] recv has not registered buffer\n", recv_thread_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    printf("[%d] has registered receive buffer\n", recv_thread_id);
    while (1 == 1)
    {
        if (recv_round_robin_idx[mapped_thread_id] != recv_thread_id)
        {
            //printf("[%d] cazaizheli\n", recv_thread_id );
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
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
        Pblocks[block_idx].eles.resize(pb->ele_num);
        Pblocks[block_idx].isP = pb->isP;
        double*data_eles = (double*)(void*) (real_sta_buf + struct_sz);
        for (int i = 0; i < pb->ele_num; i++)
        {
            Pblocks[block_idx].eles[i] = data_eles[i];
        }

        //printf("[%d]successful reve one Block id=%d data_ele=%d\n", recv_thread_id, pb->block_id, pb->ele_num);

        size_t p_total = struct_sz + sizeof(double) * pb->ele_num;

        struct Block * qb = (struct Block*)(void*)(real_sta_buf + p_total);

        data_eles = (double*)(void*) (real_sta_buf + p_total + struct_sz);

        block_idx = qb->block_id ;
        Qblocks[block_idx].block_id = qb->block_id;
        Qblocks[block_idx].sta_idx = qb->sta_idx;
        Qblocks[block_idx].height = qb->height;
        Qblocks[block_idx].ele_num = qb->ele_num;
        Qblocks[block_idx].eles.resize(qb->ele_num);
        Qblocks[block_idx].isP = qb->isP;
        for (int i = 0; i < qb->ele_num; i++)
        {
            Qblocks[block_idx].eles[i] = data_eles[i];
        }

        //printf("[%d]successful recv another Block id=%d data_ele=%d\n", recv_thread_id, pb->block_id, pb->ele_num);

        //this buf I have read it, so please prepare new buf content
        s_ctx[recv_thread_id].buf_prepared = false;

        //printf("[%d]get pid=%d qid=%d  buf_prepared=%d\n", recv_thread_id, pb->block_id, qb->block_id, s_ctx[recv_thread_id].buf_prepared);

        recv_round_robin_idx[mapped_thread_id] = (recv_round_robin_idx[mapped_thread_id] + WORKER_NUM) % (WORKER_NUM * QP_GROUP);
        recvCount++;
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


