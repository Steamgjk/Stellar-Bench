#ifndef RDMA_SERVER_H
#define RDMA_SERVER_H

#include <fcntl.h>
#include <libgen.h>
#include <string>
#include <thread>
#include "common.h"
using namespace std;

struct conn_context
{
	char *buffer;
	size_t buf_len;
	bool buf_prepared;
	bool buf_registered;

	struct ibv_mr *buffer_mr;

	struct message *msg;
	struct ibv_mr *msg_mr;
};

class RdmaTwoSidedServerOp
{
public:
	RdmaTwoSidedServerOp();
	~RdmaTwoSidedServerOp();

	static void server_post_receive(struct rdma_cm_id *id);
	static void server_on_pre_conn(struct rdma_cm_id *id, struct ibv_pd *pd, struct conn_context* ctx);
	static void server_on_completion(struct ibv_wc *wc);
	static void server_on_connection(struct rdma_cm_id *id);
	static void server_on_disconnect(struct rdma_cm_id *id);
	static void server_send_message(struct rdma_cm_id *id);

	void server_event_loop(struct rdma_event_channel *ec, int exit_on_disconnect, struct conn_context* ctx );
	void rc_server_loop(const char *port, struct conn_context* ctx );
	void server_build_connection(struct rdma_cm_id *id);
	void server_build_context(struct ibv_context *verbs);
	void server_build_params(struct rdma_conn_param *params);
	void server_build_qp_attr(struct ibv_qp_init_attr *qp_attr);

	static void * server_poll_cq(void* void_channel);
private:
	struct context *s_ctx = NULL;


};

#endif