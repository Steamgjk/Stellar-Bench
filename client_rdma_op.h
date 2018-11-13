
#ifndef RDMA_CLIENT_H
#define RDMA_CLIENT_H
#include "rdma_common.h"
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
class client_rdma_op
{
public:
	client_rdma_op();
	int check_src_dst();
	int client_prepare_connection(struct sockaddr_in *s_addr);
	int client_pre_post_recv_buffer();
	int client_connect_to_server();
	int client_send_metadata_to_server();
	int client_send_metadata_to_server1(void* send_buf, size_t send_sz);
	int start_remote_write(size_t len, size_t offset);
	int client_remote_memory_ops();
	int client_disconnect_and_clean();
	~client_rdma_op();
private:
	/* These are basic RDMA resources */
	/* These are RDMA connection related resources */
	struct rdma_event_channel *cm_event_channel = NULL;
	struct rdma_cm_id *cm_client_id = NULL;
	struct ibv_pd *pd = NULL;
	struct ibv_comp_channel *io_completion_channel = NULL;
	struct ibv_cq *client_cq = NULL;
	struct ibv_qp_init_attr qp_init_attr;
	struct ibv_qp *client_qp;
	/* These are memory buffers related resources */
	struct ibv_mr *client_metadata_mr = NULL;
	struct ibv_mr * client_src_mr = NULL;
	struct ibv_mr *client_dst_mr = NULL;
	struct ibv_mr *server_metadata_mr = NULL;
	struct rdma_buffer_attr client_metadata_attr;
	struct rdma_buffer_attr server_metadata_attr;
	struct ibv_send_wr client_send_wr;
	struct ibv_send_wr *bad_client_send_wr = NULL;
	struct ibv_recv_wr server_recv_wr;
	struct ibv_recv_wr *bad_server_recv_wr = NULL;
	struct ibv_sge client_send_sge;
	struct ibv_sge server_recv_sge;
	/* Source and Destination buffers, where RDMA operations source and sink */
	char *src = NULL;
	char *dst = NULL;

};


#endif
