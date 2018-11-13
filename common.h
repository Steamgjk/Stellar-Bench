#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>

#define TEST_NZ(x) do { if ( (x)) rc_die("error: " #x " failed (returned non-zero)." ); } while (0)
#define TEST_Z(x)  do { if (!(x)) rc_die("error: " #x " failed (returned zero/null)."); } while (0)

const int TIMEOUT_IN_MS = 500;

const size_t BUFFER_SIZE = 500 * 1024 * 1024;

enum message_id
{
	MSG_INVALID = 0,
	MSG_MR,
	MSG_READY,
	MSG_DONE
};

struct message
{
	int id;

	union
	{
		struct
		{
			uint64_t addr;
			uint32_t rkey;
		} mr;
	} data;
};


struct context
{
	struct ibv_context *ctx;
	struct ibv_pd *pd;
	struct ibv_cq *cq;
	struct ibv_comp_channel *comp_channel;

	pthread_t cq_poller_thread;
};

void rc_disconnect(struct rdma_cm_id *id);
void rc_die(const char *reason);




#endif
