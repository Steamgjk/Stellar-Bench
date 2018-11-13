
#ifndef RDMA_SERVER_H
#define RDMA_SERVER_H

#include "rdma_common.h"

class server_rdma_op
{
public:
	server_rdma_op();
	int setup_client_resources();
	int start_rdma_server(struct sockaddr_in *server_addr);
	int accept_client_connection();
	int send_server_metadata_to_client1(void* buf_to_rwrite, size_t buf_sz);
	int disconnect_and_cleanup();
	int rdma_server_init(char* local_ip, int local_port, void* register_buf, size_t register_sz);
	~server_rdma_op();
private:
	struct rdma_event_channel *cm_event_channel = NULL;
	struct rdma_cm_id *cm_server_id = NULL;
	struct rdma_cm_id *cm_client_id = NULL;
	struct ibv_pd *pd = NULL;
	struct ibv_comp_channel *io_completion_channel = NULL;
	struct ibv_cq *cq = NULL;
	struct ibv_qp_init_attr qp_init_attr;
	struct ibv_qp *client_qp = NULL;
	/* RDMA memory resources */
	struct ibv_mr *client_metadata_mr = NULL;
	struct ibv_mr * server_buffer_mr = NULL;
	struct ibv_mr * server_metadata_mr = NULL;
	struct rdma_buffer_attr client_metadata_attr;
	struct rdma_buffer_attr server_metadata_attr;
	struct ibv_recv_wr client_recv_wr;
	struct ibv_recv_wr *bad_client_recv_wr = NULL;
	struct ibv_sge client_recv_sge;

};


#endif

