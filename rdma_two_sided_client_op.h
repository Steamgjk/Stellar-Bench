#ifndef RDMA_CLIENT_H
#define RDMA_CLIENT_H

#include <fcntl.h>
#include <libgen.h>
#include <string>
#include <thread>
#include "common.h"
using namespace std;

struct client_context
{
	char *buffer;
	size_t buf_len;
	bool can_send;
	bool buf_prepared;
	bool buf_registered;
	struct ibv_mr *buffer_mr;

	struct message *msg;
	struct ibv_mr *msg_mr;

	uint64_t peer_addr;
	uint32_t peer_rkey;
};

class RdmaTwoSidedClientOp
{
public:
	RdmaTwoSidedClientOp();
	~RdmaTwoSidedClientOp();

	static void client_write_remote(struct rdma_cm_id *id, uint32_t len);
	static void client_post_receive(struct rdma_cm_id *id);
	static void client_send_next_chunk(struct rdma_cm_id *id);

	static void client_on_pre_conn(struct rdma_cm_id *id, struct ibv_pd *pd);

	static void client_on_completion(struct ibv_wc *wc);

	void client_event_loop(struct rdma_event_channel *ec, int exit_on_disconnect);
	void rc_client_loop(const char *host, const char *port, void *context);
	void client_build_connection(struct rdma_cm_id *id);
	void client_build_context(struct ibv_context *verbs);
	void client_build_params(struct rdma_conn_param *params);
	void client_build_qp_attr(struct ibv_qp_init_attr *qp_attr);
	//static void * client_poll_cq(void *ctx);
	//static void * client::client_poll_cq(struct ibv_comp_channel *channel);
	static void * client_poll_cq(void* void_channel);
private:
	struct context *s_ctx = NULL;

};




#endif