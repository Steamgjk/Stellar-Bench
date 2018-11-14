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

#define CAP 2000
#include "rdma_two_sided_client_op.h"
#include "rdma_two_sided_server_op.h"

using namespace std;
#define GROUP_NUM 1
#define DIM_NUM 4

struct client_context c_ctx[CAP];
struct conn_context s_ctx[CAP];

#define FILE_NAME "./yahoo-output/train-"
#define TEST_NAME "./yahoo-output/test"
#define N 1000990
#define M 624961
#define K  100 //主题个数

/**Yahoo!Music**/
double yita = 0.001;
double theta = 0.05;
#define WORKER_NUM 1
#define WORKER_N_1 4
#define QP_GROUP 1

char* remote_ips[CAP] = {"12.12.10.18", "12.12.10.18", "12.12.10.18", "12.12.10.18"};
int remote_ports[CAP] = {4411, 4412, 4413, 4414};

char* local_ips[CAP] = {"12.12.10.12", "12.12.10.15", "12.12.10.19", "12.12.10.17"};
int local_ports[CAP] = {5511, 5512, 5513, 5514};

int send_round_robin_idx = 0;
int recv_round_robin_idx = 0;
#define ThreshIter 1000
#define SEQ_LEN 5000
#define WORKER_THREAD_NUM 30

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
struct Block Pblock;
struct Block Qblock;
struct Updates Pupdt;
struct Updates Qupdt;
vector<double> oldP;
vector<double> oldQ;
vector<long> hash_ids;
std::vector<long> rates;

bool canSend = false;
bool hasRecved = false;
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
void WriteLog(Block&Pb, Block&Qb, int iter_cnt);
void LoadRmatrix(int file_no, map<long, double>& myMap);
void CalcUpdt(int thread_id);
void LoadData();
void LoadData4();
void LoadRequiredData(int row, int col, int data_idx);
void InitFlag();

int thread_id = -1;
struct timeval start, stop, diff;
vector<bool> StartCalcUpdt;
map<long, double> RMap;
map<long, double> RMaps[8][8];

std::vector<long> hash_for_row_threads[10][10][WORKER_THREAD_NUM];
std::vector<double> rates_for_row_threads[10][10][WORKER_THREAD_NUM];
std::vector<long> hash_for_col_threads[10][10][WORKER_THREAD_NUM];
std::vector<double> rates_for_col_threads[10][10][WORKER_THREAD_NUM];
int iter_cnt = 0;
long long calcTimes[2000];
long long calc_time;
long long load_time;
long long loadTimes[2000];
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

    for (int i = 0; i < QP_GROUP; i++)
    {
        int th_id = thread_id + i * WORKER_N_1;
        printf("recv th_id=%d\n", th_id );
        std::thread recv_loop_thread(rdma_recvTd_loop, th_id);
        recv_loop_thread.detach();
        std::thread recv_thread(rdma_recvTd, th_id);
        //std::thread recv_thread(recvTd, thread_id);
        recv_thread.detach();
    }


    printf("wait for you for 3s\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));


    for (int i = 0; i < QP_GROUP; i++)
    {
        int th_id = thread_id + i * WORKER_N_1;
        std::thread send_loop_thread(rdma_sendTd_loop, th_id);
        send_loop_thread.detach();
        std::thread send_thread(rdma_sendTd, th_id);
        //std::thread send_thread(sendTd, thread_id);
        send_thread.detach();
    }

    StartCalcUpdt.resize(WORKER_THREAD_NUM);
    for (int i = 0; i < WORKER_THREAD_NUM; i++)
    {
        StartCalcUpdt[i] = false;
    }
    canSend = false;
    memset(&start, 0, sizeof(struct timeval));
    memset(&stop, 0, sizeof(struct timeval));
    memset(&diff, 0, sizeof(struct timeval));


    iter_cnt = 0;
    calc_time = 0;
    bool isstart = false;

    //LoadData();
    //LoadData4();
    //printf("Load Rating Success\n");

    std::vector<thread> td_vec;
    //printf("wait for you for 3s\n");
    //std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    for (int i = 0; i < WORKER_THREAD_NUM; i++)
    {
        //std::thread td(CalcUpdt, i);
        td_vec.push_back(std::thread(CalcUpdt, i));
    }
    //printf("come here\n");
    for (int i = 0; i < WORKER_THREAD_NUM; i++)
    {
        td_vec[i].detach();
        //printf("%d  has detached\n", i );
    }
    printf("detached well\n");

    while (1 == 1)
    {
        //printf(" hasRecved? %d\n", hasRecved);
        //std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        if (hasRecved)
        {
            //printf("has Received itercnt=%d\n", iter_cnt);
            if (!isstart)
            {
                isstart = true;
                gettimeofday(&start, 0);
            }

            //SGD
            int row_sta_idx = Pblock.sta_idx;
            int row_len = Pblock.height;
            int col_sta_idx = Qblock.sta_idx;
            int col_len = Qblock.height;
            int ele_num = row_len * col_len;
            //printf("before submf\n");
            submf();
            //printf("after submf\n");
            iter_cnt++;


            if (iter_cnt % 10 == 0)
            {
                //WriteLog(Pblock, Qblock, iter_cnt);
                calcTimes[iter_cnt / 10] = calc_time;
                loadTimes[iter_cnt / 10] = load_time;
            }
            if (iter_cnt % 100 == 0)
            {
                for (int i = 0; i <= 100; i++)
                {
                    printf("%lld\n", calcTimes[i] );
                }
                for (int i = 0; i <= 100; i++)
                {
                    printf("%lld\n", loadTimes[i] );
                }
                //exit(0);
            }

            if (iter_cnt == thresh_log )
            {
                gettimeofday(&stop, 0);

                long long mksp = (stop.tv_sec - start.tv_sec) * 1000000 + stop.tv_usec - start.tv_usec;
                printf("itercnt = %d  time = %lld\n", iter_cnt, mksp);
                //WriteLog(Pblock, Qblock, iter_cnt);
                //exit(0);
            }
            canSend = true;
            //printf("canSend = true\n");
            hasRecved = false;

        }
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
void LoadRmatrix(int file_no, map<long, double>& myMap)
{
    char fn[100];
    sprintf(fn, "%s%d", FILE_NAME, file_no);
    ifstream ifs(fn);
    if (!ifs.is_open())
    {
        printf("fail-Rmatrix to open the file %s\n", fn);
        exit(-1);
    }
    int cnt = 0;
    long hash_idx = -1;
    double ra = 0;
    long row_idx, col_idx;

    while (!ifs.eof())
    {
        ifs >> hash_idx >> ra;
        if (hash_idx >= 0)
        {
            myMap.insert(pair<long, double>(hash_idx, ra));
            cnt++;
            if (cnt % 1000000 == 0)
            {
                printf("cnt = %ld\n", cnt );
            }
        }
    }
}

void LoadData()
{
    char fn[100];
    long hash_id;
    double rate;
    long cnt = 0;
    for (int row = 0; row < WORKER_NUM; row++)
    {
        for (int col = 0; col < WORKER_NUM; col++)
        {
            for (int td = 0; td < WORKER_THREAD_NUM; td++)
            {
                hash_for_row_threads[row][col][td].clear();
                rates_for_row_threads[row][col][td].clear();
                hash_for_col_threads[row][col][td].clear();
                rates_for_col_threads[row][col][td].clear();
            }

        }
    }
    for (int data_idx = 0; data_idx < 64; data_idx++)
    {
        int row = data_idx / DIM_NUM;
        int col = data_idx % DIM_NUM;
        row /= 2;
        col /= 2;
        sprintf(fn, "%s%d", FILE_NAME, data_idx);
        //printf("fn=%s  :[%d][%d]\n", fn, row, col );
        ifstream ifs(fn);
        if (!ifs.is_open())
        {
            printf("fail-LoadD to open %s\n", fn );
            exit(-1);
        }
        cnt = 0;
        long ridx, cidx;

        while (!ifs.eof())
        {
            hash_id = -1;
            ifs >> hash_id >> rate;
            if (hash_id >= 0)
            {
                ridx = ((hash_id) / M) % WORKER_THREAD_NUM;
                cidx = ((hash_id) % M) % WORKER_THREAD_NUM;

                hash_for_row_threads[row][col][ridx].push_back(hash_id);
                rates_for_row_threads[row][col][ridx].push_back(rate);
                hash_for_col_threads[row][col][cidx].push_back(hash_id);
                rates_for_col_threads[row][col][cidx].push_back(rate);
            }
        }
    }
}
void LoadData4()
{
    char fn[100];
    long hash_id;
    double rate;
    long cnt = 0;
    for (int row = 0; row < WORKER_NUM; row++)
    {
        for (int col = 0; col < WORKER_NUM; col++)
        {
            for (int td = 0; td < WORKER_THREAD_NUM; td++)
            {
                hash_for_row_threads[row][col][td].clear();
                rates_for_row_threads[row][col][td].clear();
                hash_for_col_threads[row][col][td].clear();
                rates_for_col_threads[row][col][td].clear();
            }

        }
    }
    for (int data_idx = 0; data_idx < 16; data_idx++)
    {
        int row = data_idx / DIM_NUM;
        int col = data_idx % DIM_NUM;
        sprintf(fn, "%s%d", FILE_NAME, data_idx);
        //printf("fn=%s  :[%d][%d]\n", fn, row, col );
        ifstream ifs(fn);
        if (!ifs.is_open())
        {
            printf("fail-LoadD4 to open %s\n", fn );
            exit(-1);
        }
        cnt = 0;
        long ridx, cidx;

        while (!ifs.eof())
        {
            hash_id = -1;
            ifs >> hash_id >> rate;
            //scale
            rate = rate / 100;
            if (hash_id >= 0)
            {
                ridx = ((hash_id) / M) % WORKER_THREAD_NUM;
                cidx = ((hash_id) % M) % WORKER_THREAD_NUM;

                hash_for_row_threads[row][col][ridx].push_back(hash_id);
                rates_for_row_threads[row][col][ridx].push_back(rate);
                hash_for_col_threads[row][col][cidx].push_back(hash_id);
                rates_for_col_threads[row][col][cidx].push_back(rate);
            }
        }
        printf("Load %s okay\n", fn );

    }
}

void LoadRequiredData(int row, int col, int data_idx)
{
    char fn[100];
    long hash_id;
    double rate;
    long cnt = 0;
    sprintf(fn, "%s%d", FILE_NAME, data_idx);
    printf("fn-required=%s  :[%d][%d]\n", fn, row, col );
    ifstream ifs(fn);
    if (!ifs.is_open())
    {
        printf("fail-required to open %s\n", fn );
        exit(-1);
    }
    cnt = 0;
    long ridx, cidx;
    hash_id = -1;
    while (!ifs.eof())
    {
        ifs >> hash_id >> rate;
        //min-max scaling for Yahoo!Music
        rate = rate / 100;
        if (hash_id >= 0)
        {
            ridx = ((hash_id) / M) % WORKER_THREAD_NUM;
            cidx = ((hash_id) % M) % WORKER_THREAD_NUM;
            hash_for_row_threads[row][col][ridx].push_back(hash_id);
            rates_for_row_threads[row][col][ridx].push_back(rate);
            hash_for_col_threads[row][col][cidx].push_back(hash_id);
            rates_for_col_threads[row][col][cidx].push_back(rate);
        }

    }

}


void WriteLog(Block&Pb, Block&Qb, int iter_cnt)
{
    char fn[100];
    sprintf(fn, "./PS-track/Pblock-%d-%d", iter_cnt, Pb.block_id);
    ofstream pofs(fn, ios::trunc);
    for (int h = 0; h < Pb.height; h++)
    {
        for (int j = 0; j < K; j++)
        {
            pofs << Pb.eles[h * K + j] << " ";
        }
        pofs << endl;
    }
    sprintf(fn, "./PS-track/Qblock-%d-%d", iter_cnt, Qb.block_id);
    ofstream qofs(fn, ios::trunc);
    for (int h = 0; h < Qb.height; h++)
    {
        for (int j = 0; j < K; j++)
        {
            qofs << Qb.eles[h * K + j] << " ";
        }
        qofs << endl;
    }
}


void CalcUpdt(int td_id)
{

    while (1 == 1)
    {

        int p_block_idx = Pblock.block_id;
        int q_block_idx = Qblock.block_id;
        if (StartCalcUpdt[td_id] == true)
        {
            //printf("enter CalcUpdt\n");
            int times_thresh = 5000;
            int row_sta_idx = Pblock.sta_idx;
            int col_sta_idx = Qblock.sta_idx;
            size_t rtsz;
            size_t ctsz;
            rtsz = hash_for_row_threads[p_block_idx][q_block_idx][td_id].size();
            ctsz = hash_for_col_threads[p_block_idx][q_block_idx][td_id].size();
            if (rtsz == 0 || ctsz == 0)
            {
                //printf("p %d q %d td=%d empty\n", p_block_idx, q_block_idx, td_id );
                //exit(0);
                StartCalcUpdt[td_id] = false;
                continue;
            }
            int rand_idx = -1;
            for (times_thresh; times_thresh > 0; times_thresh--)
            {
                //printf("times_thresh=%d\n", times_thresh );
                rand_idx = random() % rtsz;
                long real_hash_idx = hash_for_row_threads[p_block_idx][q_block_idx][td_id][rand_idx];
                long i = real_hash_idx / M - row_sta_idx;
                long j = real_hash_idx % M - col_sta_idx;
                double error = rates_for_row_threads[p_block_idx][q_block_idx][td_id][rand_idx];
                if (i < 0 || j < 0 || i >= Pblock.height || j >= Qblock.height)
                {
                    //printf("[%d] continue i=%ld j=%ld  ph=%d  qh=%d \n", td_id, i, j , Pblock.height, Qblock.height);
                    //getchar();
                    continue;
                }
                for (int k = 0; k < K; ++k)
                {
                    error -= oldP[i * K + k] * oldQ[j * K + k];
                }
                /*
                if (error >= 300 )
                {
                    error = 300;
                }
                if (error <= -300)
                {
                    error = -300;
                }
                **/
                for (int k = 0; k < K; ++k)
                {
                    Pblock.eles[i * K + k] += yita * (error * oldQ[j * K + k] - theta * oldP[i * K + k]);



                    if (Pblock.eles[i * K + k] + 1 == Pblock.eles[i * K + k] - 1)
                    {
                        printf("p %d q %d  error =%lf i=%d j=%d k=%d rand_idx=%d vale=%lf pvale=%lf  qvalue=%lf\n", p_block_idx, q_block_idx, error, i, j, k, rand_idx,  rates_for_col_threads[p_block_idx][q_block_idx][td_id][rand_idx], oldP[i * K + k], oldQ[j * K + k] );
                        getchar();
                    }
                }

                rand_idx = random() % ctsz;
                real_hash_idx = hash_for_col_threads[p_block_idx][q_block_idx][td_id][rand_idx];
                i = real_hash_idx / M - row_sta_idx;
                j = real_hash_idx % M - col_sta_idx;
                if (i < 0 || j < 0 || i >= Pblock.height || j >= Qblock.height)
                {
                    //printf("[%d] continue l11 \n", td_id);
                    continue;
                }
                error = rates_for_col_threads[p_block_idx][q_block_idx][td_id][rand_idx];

                for (int k = 0; k < K; ++k)
                {
                    error -= oldP[i * K + k] * oldQ[j * K + k];
                }
                /*
                if (error >= 300 )
                {
                    error = 300;
                }
                if (error <= -300)
                {
                    error = -300;
                }
                **/
                for (int k = 0; k < K; ++k)
                {

                    Qblock.eles[j * K + k] += yita * (error * oldP[i * K + k] - theta * oldQ[j * K + k]);
                    if (Qblock.eles[j * K + k] + 1 == Qblock.eles[j * K + k] - 1)
                    {
                        printf("p %d q %d  error =%lf i=%d j=%d k=%d rand_idx=%d vale=%lf pvale=%lf  qvalue=%lf\n", p_block_idx, q_block_idx, error, i, j, k, rand_idx,  rates_for_col_threads[p_block_idx][q_block_idx][td_id][rand_idx], oldP[i * K + k], oldQ[j * K + k] );
                        getchar();

                    }
                }
            }
            //printf("Fini %d\n", td_id);
            StartCalcUpdt[td_id] = false;


        }

    }


}
void submf()
{
    int minN = Pblock.height;
    int minM = Qblock.height;

    int row_sta_idx = Pblock.sta_idx;
    int col_sta_idx = Qblock.sta_idx;
    int row_len = Pblock.height;
    int col_len = Qblock.height;

    int Psz = Pblock.height * K;
    int Qsz = Qblock.height * K;
    //printf("copying ...\n");
    oldP = Pblock.eles;
    //printf("copy P fin ele=%ld Psz = %d Ph=%d K=%d\n", oldP.size(), Psz, Pblock.height, K);
    oldQ = Qblock.eles;
    //printf("copy Q fin ele=%ld Qsz=%d Qh=%d  K=%d\n", oldQ.size(), Qsz, Qblock.height, K);
    //getchar();
    /*
    for (int i = 0; i < Psz; i++)
    {
        //printf("[%d]:%lf\n", i, oldP[i] );
        if (oldP[i] > 100 || oldP[i] < -100)
        {
            printf("P Exception! [%d] %lf\n", i, oldP[i]);
            getchar();
        }

    }
    //printf("comere hhe\n");
    for (int i = 0; i < Qsz; i++)
    {
        if (oldQ[i] > 100 || oldQ[i] < -100)
        {
            printf("Q Exception! [%d] %lf\n", i, oldQ[i]);
            getchar();
        }

    }
    **/
    //printf("enter submf22\n");

    struct timeval beg, ed;
    long long mksp;
    gettimeofday(&beg, 0);

    /*
        int r1 = Pblock.block_id * 2;
        int c1 = Qblock.block_id * 2;
        int f1 = r1 * 8 + c1;
        int f2 = r1 * 8 + c1 + 1;
        int f3 = (r1 + 1) * 8 + c1;
        int f4 = (r1 + 1) * 8 + c1 + 1;

        int row = Pblock.block_id;
        int col = Qblock.block_id;
        //printf("row=%d col=%d\n", row, col );
        for (int td = 0; td < WORKER_THREAD_NUM; td++)
        {
            hash_for_row_threads[row][col][td].clear();
            rates_for_row_threads[row][col][td].clear();
            hash_for_col_threads[row][col][td].clear();
            rates_for_col_threads[row][col][td].clear();
        }
        LoadRequiredData(row, col, f1);
        LoadRequiredData(row, col, f2);
        LoadRequiredData(row, col, f3);
        LoadRequiredData(row, col, f4);
    **/
    /*
    for (int td = 0; td < WORKER_THREAD_NUM; td++)
    {
        {
            printf("[%d][%d] %ld\n", row, col, hash_for_row_threads[row][col][td] );
        }
    }
    **/


    int row = Pblock.block_id;
    int col = Qblock.block_id;
    //printf("row=%d col=%d\n", row, col );
    for (int td = 0; td < WORKER_THREAD_NUM; td++)
    {
        hash_for_row_threads[row][col][td].clear();
        rates_for_row_threads[row][col][td].clear();
        hash_for_col_threads[row][col][td].clear();
        rates_for_col_threads[row][col][td].clear();
    }
    int f1 = row * 4 + col;
    printf("row=%d col=%d\n", row, col );
    LoadRequiredData(row, col, f1);

    gettimeofday(&ed, 0);
    mksp = (ed.tv_sec - beg.tv_sec) * 1000000 + ed.tv_usec - beg.tv_usec;
    load_time += mksp;
    printf("Load time = %lld\n", mksp);


    bool canbreak = true;
    for (int ii = 0; ii < WORKER_THREAD_NUM; ii++)
    {
        StartCalcUpdt[ii] = 1;
    }

    while (1 == 1)
    {
        canbreak = true;
        for (int ii = 0; ii < WORKER_THREAD_NUM; ii++)
        {

            if (StartCalcUpdt[ii] == true)
            {
                //printf("ii=%d, %d \n", ii, StartCalcUpdt[ii] );
                canbreak = false;
            }

        }
        if (canbreak)
        {
            break;
        }
    }


    gettimeofday(&ed, 0);
    mksp = (ed.tv_sec - beg.tv_sec) * 1000000 + ed.tv_usec - beg.tv_usec;

    calc_time += mksp;
    printf("Calc  time = %lld\n", mksp);


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
    char* remote_ip = remote_ips[send_thread_id % WORKER_N_1];
    int remote_port = remote_ports[send_thread_id];
    int mapped_thread_id = send_thread_id / WORKER_N_1;
    char str_port[100];
    sprintf(str_port, "%d", remote_port);
    RdmaTwoSidedClientOp ct;
    ct.rc_client_loop(remote_ip, str_port, &(c_ctx[mapped_thread_id]));
}

void rdma_recvTd_loop(int recv_thread_id)
{
    int bind_port = local_ports[recv_thread_id];
    char str_port[100];
    sprintf(str_port, "%d", bind_port);
    int mapped_thread_id = recv_thread_id / WORKER_N_1;
    RdmaTwoSidedServerOp rtos;
    rtos.rc_server_loop(str_port, &(s_ctx[mapped_thread_id]));

}


void rdma_sendTd(int send_thread_id)
{

    size_t struct_sz = sizeof(Block);
    int mapped_thread_id = send_thread_id / WORKER_N_1;
    while (c_ctx[mapped_thread_id].buf_registered == false)
    {
        //printf("[%d] has not registered buffer\n", send_thread_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    printf("[%d] has registered send buffer\n", send_thread_id);
    while (1 == 1)
    {
        //printf("canSend=%d\n", canSend );
        if (canSend)
        {
            //struct timeval st, et, tspan;

            printf("Td:%d cansend\n", thread_id );

            size_t p_data_sz = sizeof(double) * Pblock.ele_num;
            size_t q_data_sz = sizeof(double) * Qblock.ele_num;
            size_t p_total = struct_sz + p_data_sz;
            size_t q_total = struct_sz + q_data_sz;
            size_t total_len = p_total + q_total;



            char* buf = c_ctx[mapped_thread_id].buffer;

            memcpy(buf, &(Pblock), struct_sz);
            memcpy(buf + struct_sz, (char*) & (Pblock.eles[0]), p_data_sz);

            memcpy(buf + p_total, &(Qblock), struct_sz);
            memcpy(buf + p_total + struct_sz , (char*) & (Qblock.eles[0]), q_data_sz);



            c_ctx[mapped_thread_id].buf_len = total_len;
            c_ctx[mapped_thread_id].buf_prepared = true;

            printf("[%d][%d] Marked send buf  p_total = %ld p_data_sz=%ld q_total=%ld q_data_sz=%ld total_len=%ld\n", send_thread_id, mapped_thread_id, p_total, p_data_sz, q_total, q_data_sz, total_len);

            canSend = false;
        }

    }

}

void rdma_recvTd(int recv_thread_id)
{

    size_t struct_sz = sizeof(Block);

    int mapped_thread_id = recv_thread_id / WORKER_N_1;
    while (s_ctx[mapped_thread_id].buf_registered == false)
    {
        //printf("[%d] recv has not registered buffer\n", recv_thread_id);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    printf("[%d] has registered receive buffer\n", recv_thread_id);
    while (1 == 1)
    {
        if (mapped_thread_id != recv_round_robin_idx % QP_GROUP)
        {
            continue;
        }
        /*
        struct timeval st, et;
        gettimeofday(&st, 0);
        */
        if (s_ctx[mapped_thread_id].buf_prepared == false)
        {
            //printf("[%d] recv buf prepared = false\n", recv_thread_id );
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        //printf("[%d] recv buf prepared = true\n", recv_thread_id );

        char* real_sta_buf = s_ctx[mapped_thread_id].buffer;

        struct Block* pb = (struct Block*)(void*)real_sta_buf;
        Pblock.block_id = pb->block_id;
        Pblock.data_age = pb->data_age;
        Pblock.sta_idx = pb->sta_idx;
        Pblock.height = pb->height;
        Pblock.ele_num = pb->ele_num;
        Pblock.eles.resize(pb->ele_num);
        double* data_eles = (double*)(void*) (real_sta_buf + struct_sz);
        for (int i = 0; i < Pblock.ele_num; i++)
        {
            Pblock.eles[i] = data_eles[i];
        }


        size_t p_total = struct_sz + sizeof(double) * (pb->ele_num);
        struct Block* qb = (struct Block*)(void*)(real_sta_buf + p_total);
        Qblock.block_id = qb->block_id;
        Qblock.data_age = qb->data_age;
        Qblock.sta_idx = qb->sta_idx;
        Qblock.height = qb->height;
        Qblock.ele_num = qb-> ele_num;
        Qblock.eles.resize(qb->ele_num);
        //printf("[%d]get qblock id=%d  ele_num=%d  isP=%d qb=%p\n", recv_thread_id,  qb->block_id, qb->ele_num, qb->isP, qb);
        //data_eles = (double*)(void*)(to_recv_block_mem + BLOCK_MEM_SZ + struct_sz);
        data_eles = (double*)(void*)(real_sta_buf + p_total + struct_sz);
        for (int i = 0; i < Qblock.ele_num; i++)
        {
            Qblock.eles[i] = data_eles[i];
        }
        /*
        gettimeofday(&et, 0);
        long long mksp = (et.tv_sec - st.tv_sec) * 1000000 + et.tv_usec - st.tv_usec;
        printf("[%d]:recv two blocks time = %lld\n", recv_thread_id, mksp);
        **/


        //this buf I have read it, so please prepare new buf content
        s_ctx[mapped_thread_id].buf_prepared = false;

        printf("[%d]get pblock id=%d  ele_num=%d  qblock id=%d  ele_num=%d\n", recv_thread_id, pb->block_id, pb->ele_num, qb->block_id, qb->ele_num);

        recv_round_robin_idx++;
        hasRecved = true;
    }

}
