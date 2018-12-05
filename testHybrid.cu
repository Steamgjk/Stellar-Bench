#include "gpu_mf.h"
#include <stdio.h>
#include <stdlib.h>
#include <thread>
using namespace std;
void rdma_recvTd_loop(int id)
{

}
int main()
{
	std::thread recv_loop_thread(rdma_recvTd_loop, th_id);
	recv_loop_thread.detach();
	printf("hello\n");
}