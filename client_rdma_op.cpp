/*
 * This is RDMA client side code for the assignment 8 in the
 * Advanced Computer Network (ACN) course.
 *
 * Author: Animesh Trivedi
 *         atr@zurich.ibm.com (atrivedi@student.ethz.ch)
 */

#include "client_rdma_op.h"

client_rdma_op::client_rdma_op()
{

}
client_rdma_op::~client_rdma_op()
{

}
/* This is our testing function */
int client_rdma_op::check_src_dst()
{
	debug("src: '%s'\n", src);
	debug("dst: '%s'\n", dst);

	int ret =  memcmp((void*) src, (void*) dst, sizeof(src));
	printf("ret = %d errno = %d \n", ret, errno);

	return ret;
}

/* This function prepares client side connection resources for an RDMA connection */
int client_rdma_op::client_prepare_connection(struct sockaddr_in *s_addr)
{
	struct rdma_cm_event *cm_event = NULL;
	int ret = -1;
	/*  Open a channel used to report asynchronous communication event */
	cm_event_channel = rdma_create_event_channel();
	if (!cm_event_channel)
	{
		rdma_error("Creating cm event channel failed, errno: %d \n", -errno);
		return -errno;
	}
	printf("RDMA CM event channel is created at : %p \n", cm_event_channel);
	/* rdma_cm_id is the connection identifier (like socket) which is used
	 * to define an RDMA connection.
	 */
	ret = rdma_create_id(cm_event_channel, &cm_client_id,
	                     NULL,
	                     RDMA_PS_TCP);
	if (ret)
	{
		rdma_error("Creating cm id failed with errno: %d \n", -errno);
		return -errno;
	}
	/* Resolve destination and optional source addresses from IP addresses  to
	 * an RDMA address.  If successful, the specified rdma_cm_id will be bound
	 * to a local device. */
	ret = rdma_resolve_addr(cm_client_id, NULL, (struct sockaddr*) s_addr, 2000);
	if (ret)
	{
		rdma_error("Failed to resolve address, errno: %d \n", -errno);
		return -errno;
	}
	printf("waiting for cm event: RDMA_CM_EVENT_ADDR_RESOLVED\n");
	ret  = process_rdma_cm_event(cm_event_channel,
	                             RDMA_CM_EVENT_ADDR_RESOLVED,
	                             &cm_event);
	if (ret)
	{
		rdma_error("Failed to receive a valid event, ret = %d \n", ret);
		return ret;
	}
	printf("ack event\n");
	/* we ack the event */
	ret = rdma_ack_cm_event(cm_event);
	if (ret)
	{
		rdma_error("Failed to acknowledge the CM event, errno: %d\n", -errno);
		return -errno;
	}
	printf("RDMA address will be resolved \n");

	/* Resolves an RDMA route to the destination address in order to
	 * establish a connection */
	ret = rdma_resolve_route(cm_client_id, 2000);
	if (ret)
	{
		rdma_error("Failed to resolve route, erno: %d \n", -errno);
		return -errno;
	}
	printf("waiting for cm event: RDMA_CM_EVENT_ROUTE_RESOLVED\n");
	ret = process_rdma_cm_event(cm_event_channel,
	                            RDMA_CM_EVENT_ROUTE_RESOLVED,
	                            &cm_event);
	if (ret)
	{
		rdma_error("Failed to receive a valid event, ret = %d \n", ret);
		return ret;
	}
	/* we ack the event */
	ret = rdma_ack_cm_event(cm_event);
	if (ret)
	{
		rdma_error("Failed to acknowledge the CM event, errno: %d \n", -errno);
		return -errno;
	}
	debug("Trying to connect to server at : %s port: %d \n",
	      inet_ntoa(s_addr->sin_addr),
	      ntohs(s_addr->sin_port));
	/* Protection Domain (PD) is similar to a "process abstraction"
	 * in the operating system. All resources are tied to a particular PD.
	 * And accessing recourses across PD will result in a protection fault.
	 */
	pd = ibv_alloc_pd(cm_client_id->verbs);
	if (!pd)
	{
		rdma_error("Failed to alloc pd, errno: %d \n", -errno);
		return -errno;
	}
	debug("pd allocated at %p \n", pd);
	/* Now we need a completion channel, were the I/O completion
	 * notifications are sent. Remember, this is different from connection
	 * management (CM) event notifications.
	 * A completion channel is also tied to an RDMA device, hence we will
	 * use cm_client_id->verbs.
	 */
	io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
	if (!io_completion_channel)
	{
		rdma_error("Failed to create IO completion event channel, errno: %d\n",
		           -errno);
		return -errno;
	}
	debug("completion event channel created at : %p \n", io_completion_channel);
	/* Now we create a completion queue (CQ) where actual I/O
	 * completion metadata is placed. The metadata is packed into a structure
	 * called struct ibv_wc (wc = work completion). ibv_wc has detailed
	 * information about the work completion. An I/O request in RDMA world
	 * is called "work" ;)
	 */
	client_cq = ibv_create_cq(cm_client_id->verbs /* which device*/,
	                          20000 /* maximum capacity*/,
	                          NULL /* user context, not used here */,
	                          io_completion_channel /* which IO completion channel */,
	                          0 /* signaling vector, not used here*/);
	if (!client_cq)
	{
		rdma_error("Failed to create CQ, errno: %d \n", -errno);
		return -errno;
	}
	debug("CQ created at %p with %d elements \n", client_cq, client_cq->cqe);
	ret = ibv_req_notify_cq(client_cq, 0);
	if (ret)
	{
		rdma_error("Failed to request notifications, errno: %d\n", -errno);
		return -errno;
	}
	/* Now the last step, set up the queue pair (send, recv) queues and their capacity.
	  * The capacity here is define statically but this can be probed from the
	* device. We just use a small number as defined in rdma_common.h */
	bzero(&qp_init_attr, sizeof qp_init_attr);
	qp_init_attr.cap.max_recv_sge = MAX_SGE; /* Maximum SGE per receive posting */
	qp_init_attr.cap.max_recv_wr = 1; /* Maximum receive posting capacity */
	qp_init_attr.cap.max_send_sge = MAX_SGE; /* Maximum SGE per send posting */
	qp_init_attr.cap.max_send_wr = 15000; /* Maximum send posting capacity */
	qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC = Reliable connection */
	/* We use same completion queue, but one can use different queues */
	qp_init_attr.recv_cq = client_cq; /* Where should I notify for receive completion operations */
	qp_init_attr.send_cq = client_cq; /* Where should I notify for send completion operations */
	/*Lets create a QP */
	ret = rdma_create_qp(cm_client_id /* which connection id */,
	                     pd /* which protection domain*/,
	                     &qp_init_attr /* Initial attributes */);
	if (ret)
	{
		rdma_error("Failed to create QP, errno: %d \n", -errno);
		return -errno;
	}
	client_qp = cm_client_id->qp;
	debug("QP created at %p \n", client_qp);
	return 0;
}

/* Pre-posts a receive buffer before calling rdma_connect () */
int client_rdma_op::client_pre_post_recv_buffer()
{
	int ret = -1;
	server_metadata_mr = rdma_buffer_register(pd,
	                     &server_metadata_attr,
	                     sizeof(server_metadata_attr),
	                     (IBV_ACCESS_LOCAL_WRITE));
	if (!server_metadata_mr)
	{
		rdma_error("Failed to setup the server metadata mr , -ENOMEM\n");
		return -ENOMEM;
	}
	server_recv_sge.addr = (uint64_t) server_metadata_mr->addr;
	server_recv_sge.length = (uint32_t) server_metadata_mr->length;
	server_recv_sge.lkey = (uint32_t) server_metadata_mr->lkey;
	/* now we link it to the request */
	bzero(&server_recv_wr, sizeof(server_recv_wr));
	server_recv_wr.sg_list = &server_recv_sge;
	server_recv_wr.num_sge = 1;
	//why not server_recv_wr.opcode = IBV_WR_RECV??
	ret = ibv_post_recv(client_qp /* which QP */,
	                    &server_recv_wr /* receive work request*/,
	                    &bad_server_recv_wr /* error WRs */);
	if (ret)
	{
		rdma_error("Failed to pre-post the receive buffer, errno: %d \n", ret);
		return ret;
	}
	debug("Receive buffer pre-posting is successful \n");
	return 0;
}

/* Connects to the RDMA server */
int client_rdma_op::client_connect_to_server()
{
	struct rdma_conn_param conn_param;
	struct rdma_cm_event *cm_event = NULL;
	int ret = -1;
	bzero(&conn_param, sizeof(conn_param));
	conn_param.initiator_depth = 3;
	conn_param.responder_resources = 3;
	conn_param.retry_count = 3; // if fail, then how many times to retry
	do
	{
		ret = rdma_connect(cm_client_id, &conn_param);
		if (ret)
		{
			rdma_error("Failed to connect to remote host , errno: %d\n", -errno);

			//return -errno;
		}
	}
	while (ret != 0);

	debug("waiting for cm event: RDMA_CM_EVENT_ESTABLISHED\n");
	ret = process_rdma_cm_event(cm_event_channel,
	                            RDMA_CM_EVENT_ESTABLISHED,
	                            &cm_event);
	if (ret)
	{
		rdma_error("Failed to get cm event, ret = %d \n", ret);
		return ret;
	}
	ret = rdma_ack_cm_event(cm_event);
	if (ret)
	{
		rdma_error("Failed to acknowledge cm event, errno: %d\n",
		           -errno);
		return -errno;
	}
	printf("The client is connected successfully \n");
	return 0;
}

/* Send client side src buffer metadata to the server. This metadata on
 * the server side is unused. This is shown for the illustration purpose. */
int client_rdma_op::client_send_metadata_to_server()
{
	struct ibv_wc wc[2];
	int ret = -1;
	//strlen-sizeof
	client_src_mr = rdma_buffer_register(pd,
	                                     src,
	                                     BLOCK_SZ,
	                                     (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE));
	if (!client_src_mr)
	{
		rdma_error("Failed to register the first buffer, ret = %d \n", ret);
		return ret;
	}
	/* we prepare metadata for the first buffer */
	client_metadata_attr.address = (uint64_t) client_src_mr->addr;
	client_metadata_attr.length = client_src_mr->length;
	client_metadata_attr.stag.local_stag = client_src_mr->lkey;
	/* now we register the metadata memory */
	client_metadata_mr = rdma_buffer_register(pd,
	                     &client_metadata_attr,
	                     sizeof(client_metadata_attr),
	                     IBV_ACCESS_LOCAL_WRITE);
	if (!client_metadata_mr)
	{
		rdma_error("Failed to register the client metadata buffer, ret = %d \n", ret);
		return ret;
	}
	/* now we fill up SGE */
	client_send_sge.addr = (uint64_t) client_metadata_mr->addr;
	client_send_sge.length = (uint32_t) client_metadata_mr->length;
	client_send_sge.lkey = client_metadata_mr->lkey;
	/* now we link to the send work request */
	bzero(&client_send_wr, sizeof(client_send_wr));
	client_send_wr.sg_list = &client_send_sge;
	client_send_wr.num_sge = 1;
	client_send_wr.opcode = IBV_WR_SEND;
	client_send_wr.send_flags = IBV_SEND_SIGNALED;
	/* Now we post it */
	ret = ibv_post_send(client_qp,
	                    &client_send_wr,
	                    &bad_client_send_wr);
	if (ret)
	{
		rdma_error("Failed to send client metadata, errno: %d \n",
		           -errno);
		return -errno;
	}
	/* at this point we are expecting 2 work completion. One for our
	 * send and one for recv that we will get from the server for
	 * its buffer information */
	ret = process_work_completion_events(io_completion_channel, wc, 2);
	if (ret != 2)
	{
		rdma_error("We failed to get 2 work completions , ret = %d \n", ret);
		return ret;
	}
	debug("Server sent us its buffer location and credentials, showing \n");
	show_rdma_buffer_attr(&server_metadata_attr);
	return 0;
}

int client_rdma_op::client_send_metadata_to_server1(void* send_buf, size_t send_sz)
{
	struct ibv_wc wc[2];
	int ret = -1;
	//strlen-sizeof
	printf("send_buf=%p  send_sz= %ld\n", send_buf, send_sz );
	client_src_mr = rdma_buffer_register(pd,
	                                     send_buf,
	                                     send_sz,
	                                     (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE));
	if (!client_src_mr)
	{
		rdma_error("Failed to register the first buffer, ret = %d error=%d \n", ret, errno);
		return ret;
	}
	/* we prepare metadata for the first buffer */
	client_metadata_attr.address = (uint64_t) client_src_mr->addr;
	client_metadata_attr.length = client_src_mr->length;
	client_metadata_attr.stag.local_stag = client_src_mr->lkey;
	/* now we register the metadata memory */
	client_metadata_mr = rdma_buffer_register(pd,
	                     &client_metadata_attr,
	                     sizeof(client_metadata_attr),
	                     IBV_ACCESS_LOCAL_WRITE);
	if (!client_metadata_mr)
	{
		rdma_error("Failed to register the client metadata buffer, ret = %d \n", ret);
		return ret;
	}
	/* now we fill up SGE */
	client_send_sge.addr = (uint64_t) client_metadata_mr->addr;
	client_send_sge.length = (uint32_t) client_metadata_mr->length;
	client_send_sge.lkey = client_metadata_mr->lkey;
	/* now we link to the send work request */
	bzero(&client_send_wr, sizeof(client_send_wr));
	client_send_wr.sg_list = &client_send_sge;
	client_send_wr.num_sge = 1;
	client_send_wr.opcode = IBV_WR_SEND;
	client_send_wr.send_flags = IBV_SEND_SIGNALED;
	/* Now we post it */
	ret = ibv_post_send(client_qp,
	                    &client_send_wr,
	                    &bad_client_send_wr);
	if (ret)
	{
		rdma_error("Failed to send client metadata, errno: %d \n",
		           -errno);
		return -errno;
	}
	/* at this point we are expecting 2 work completion. One for our
	 * send and one for recv that we will get from the server for
	 * its buffer information */
	ret = process_work_completion_events(io_completion_channel, wc, 2);
	if (ret != 2)
	{
		rdma_error("We failed to get 2 work completions , ret = %d \n", ret);
		return ret;
	}
	debug("Server sent us its buffer location and credentials, showing \n");
	show_rdma_buffer_attr(&server_metadata_attr);
	return 0;
}

/* This function does :
 * 1) Prepare memory buffers for RDMA operations
 * 1) RDMA write from src -> remote buffer
 * 2) RDMA read from remote bufer -> dst
 */
int client_rdma_op::client_remote_memory_ops()
{
	int ret = -1;

	struct ibv_send_wr *bad_wr = NULL;

	/***************************
	 * Send RDMA write request *
	 ***************************/

	// In RDMA write, the sge is used to tell the local CA what memory
	// we want to transfer to the remote CA.
	// The remote address is set below in rdma_write_wr.wr.rdma.remote_addr.
	struct ibv_sge rdma_write_sge;
	rdma_write_sge.addr = (uint64_t)client_src_mr->addr;
	rdma_write_sge.length = client_src_mr->length;
	rdma_write_sge.lkey = client_src_mr->lkey;

	// Create work request to send to local CA.
	struct ibv_send_wr rdma_write_wr;
	bzero(&rdma_write_wr, sizeof(rdma_write_wr));
	rdma_write_wr.sg_list = &rdma_write_sge;
	rdma_write_wr.num_sge = 1;
	rdma_write_wr.opcode = IBV_WR_RDMA_WRITE;
	rdma_write_wr.wr.rdma.remote_addr = server_metadata_attr.address;
	rdma_write_wr.wr.rdma.rkey = server_metadata_attr.stag.local_stag;
	int cnt = 0;
	int* tmp_int = (int*)(void*)src;
	*tmp_int = 0;
	debug("Trying to perform RDMA write... tmp_int=%d\n", *tmp_int);
	getchar();

	while (1 == 1)
	{

		int ele_num = random() % 10;
		*tmp_int = ele_num;
		size_t data_sz = ele_num * sizeof(double);
		double*d_data = (double*)malloc(data_sz);
		for (int i = 0; i < ele_num; i++)
		{
			d_data[i] = drand48();
			printf("%lf\t", d_data[i] );
		}
		printf("\n");
		char* buf = (char*)(void*) tmp_int;
		memcpy(buf + sizeof(int), d_data, data_sz);
		printf("cnt=%d *src =%d\n",  cnt, *((int*)((void*)src)) );

		ret = ibv_post_send(client_qp, &rdma_write_wr, &bad_wr);

		if (ret == 12)
		{
			debug("ret = %d  cnt=%d *src =%d\n", ret, cnt, *src);
			sleep(1);
		}

		getchar();


	}


	debug("FIN Performed RMDA write... tmp_int= %d\n", *tmp_int);

	if (ret)
	{
		rdma_error("Failed to do rdma write, errno: %d\n", -ret);
		return -ret;
	}

	return 0;
}

int client_rdma_op::start_remote_write(size_t len, size_t offset)
{
	int ret = -1;

	struct ibv_send_wr *bad_wr = NULL;

	/***************************
	 * Send RDMA write request *
	 ***************************/

	// In RDMA write, the sge is used to tell the local CA what memory
	// we want to transfer to the remote CA.
	// The remote address is set below in rdma_write_wr.wr.rdma.remote_addr.
	struct ibv_sge rdma_write_sge;
	rdma_write_sge.addr = (uint64_t)client_src_mr->addr;
	//rdma_write_sge.length = client_src_mr->length;
	rdma_write_sge.length = len;
	debug("len is changed to %d\n", len );
	rdma_write_sge.lkey = client_src_mr->lkey;

	// Create work request to send to local CA.
	struct ibv_send_wr rdma_write_wr;
	bzero(&rdma_write_wr, sizeof(rdma_write_wr));
	rdma_write_wr.sg_list = &rdma_write_sge;
	rdma_write_wr.num_sge = 1;
	rdma_write_wr.opcode = IBV_WR_RDMA_WRITE;
	rdma_write_wr.wr.rdma.remote_addr = server_metadata_attr.address + offset;
	rdma_write_wr.wr.rdma.rkey = server_metadata_attr.stag.local_stag;

	debug("Trying to perform RDMA write...\n");

	//while (1 == 1)
	{
		ret = ibv_post_send(client_qp, &rdma_write_wr, &bad_wr);

		if (ret == 12)
		{
			debug("ret = %d\n", ret);
			printf("ret = %d\n", ret );
			sleep(1);
		}


	}


	debug("FIN Performed RMDA write...\n");

	if (ret)
	{
		rdma_error("Failed to do rdma write, errno: %d\n", -ret);
		return -ret;
	}

	return 0;
}

/* This function disconnects the RDMA connection from the server and cleans up
 * all the resources.
 */
int client_rdma_op::client_disconnect_and_clean()
{
	struct rdma_cm_event *cm_event = NULL;
	int ret = -1;
	/* active disconnect from the client side */
	ret = rdma_disconnect(cm_client_id);
	if (ret)
	{
		rdma_error("Failed to disconnect, errno: %d \n", -errno);
		//continuing anyways
	}
	ret = process_rdma_cm_event(cm_event_channel,
	                            RDMA_CM_EVENT_DISCONNECTED,
	                            &cm_event);
	if (ret)
	{
		rdma_error("Failed to get RDMA_CM_EVENT_DISCONNECTED event, ret = %d\n",
		           ret);
		//continuing anyways
	}
	ret = rdma_ack_cm_event(cm_event);
	if (ret)
	{
		rdma_error("Failed to acknowledge cm event, errno: %d\n",
		           -errno);
		//continuing anyways
	}
	/* Destroy QP */
	rdma_destroy_qp(cm_client_id);
	/* Destroy client cm id */
	ret = rdma_destroy_id(cm_client_id);
	if (ret)
	{
		rdma_error("Failed to destroy client id cleanly, %d \n", -errno);
		// we continue anyways;
	}
	/* Destroy CQ */
	ret = ibv_destroy_cq(client_cq);
	if (ret)
	{
		rdma_error("Failed to destroy completion queue cleanly, %d \n", -errno);
		// we continue anyways;
	}
	/* Destroy completion channel */
	ret = ibv_destroy_comp_channel(io_completion_channel);
	if (ret)
	{
		rdma_error("Failed to destroy completion channel cleanly, %d \n", -errno);
		// we continue anyways;
	}
	/* Destroy memory buffers */
	rdma_buffer_deregister(server_metadata_mr);
	rdma_buffer_deregister(client_metadata_mr);
	rdma_buffer_deregister(client_src_mr);
	rdma_buffer_deregister(client_dst_mr);
	/* We free the buffers */
	free(src);
	free(dst);
	/* Destroy protection domain */
	ret = ibv_dealloc_pd(pd);
	if (ret)
	{
		rdma_error("Failed to destroy client protection domain cleanly, %d \n", -errno);
		// we continue anyways;
	}
	rdma_destroy_event_channel(cm_event_channel);
	printf("Client resource clean up is complete \n");
	return 0;
}


