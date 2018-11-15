
#include "rdma_two_sided_server_op.h"

RdmaTwoSidedServerOp::RdmaTwoSidedServerOp()
{

}

RdmaTwoSidedServerOp::~RdmaTwoSidedServerOp()
{

}

void RdmaTwoSidedServerOp::server_send_message(struct rdma_cm_id *id)
{
  struct conn_context *ctx = (struct conn_context *)id->context;

  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.opcode = IBV_WR_SEND;
  wr.sg_list = &sge;
  wr.num_sge = 1;
  wr.send_flags = IBV_SEND_SIGNALED;

  sge.addr = (uintptr_t)ctx->msg;
  sge.length = sizeof(*ctx->msg);
  sge.lkey = ctx->msg_mr->lkey;

  TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

void RdmaTwoSidedServerOp::server_post_receive(struct rdma_cm_id *id)
{
  struct ibv_recv_wr wr, *bad_wr = NULL;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.sg_list = NULL;
  wr.num_sge = 0;

  TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

void RdmaTwoSidedServerOp::server_on_pre_conn(struct rdma_cm_id *id, struct ibv_pd *pd, struct conn_context* ctx)
{
  //struct conn_context *ctx = (struct conn_context *)malloc(sizeof(struct conn_context));

  id->context = ctx;


  posix_memalign((void **)&ctx->buffer, sysconf(_SC_PAGESIZE), BUFFER_SIZE);
  TEST_Z(ctx->buffer_mr = ibv_reg_mr((pd), ctx->buffer, BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

  posix_memalign((void **)&ctx->msg, sysconf(_SC_PAGESIZE), sizeof(*ctx->msg));
  TEST_Z(ctx->msg_mr = ibv_reg_mr((pd), ctx->msg, sizeof(*ctx->msg), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

  ctx->buf_registered = true;
  server_post_receive(id);
}

void RdmaTwoSidedServerOp::server_on_connection(struct rdma_cm_id *id)
{
  struct conn_context *ctx = (struct conn_context *)id->context;

  ctx->msg->id = MSG_MR;
  ctx->msg->data.mr.addr = (uintptr_t)ctx->buffer_mr->addr;
  ctx->msg->data.mr.rkey = ctx->buffer_mr->rkey;

  server_send_message(id);
}

void RdmaTwoSidedServerOp::server_on_completion(struct ibv_wc *wc)
{
  printf("server_on_completion...\n");
  struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)wc->wr_id;
  struct conn_context *ctx = (struct conn_context *)id->context;

  if (wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM)
  {
    uint32_t size = ntohl(wc->imm_data);
    //printf("IBV_WC_RECV_RDMA_WITH_IMM  size=%ld\n", size );
    if (size == 0)
    {
      //ctx->msg->id = MSG_DONE;
      ctx->msg->id = MSG_READY;
      server_post_receive(id);

      ctx->buf_len = size;
      ctx->buf_recv_counter++;
      printf("counter = %d\n", ctx->buf_recv_counter++);
      while (ctx->can_recv == false)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      printf("server ctx has been marked false-0\n");
      server_send_message(id);
    }
    else
    {
      ssize_t ret;

      //printf("received %i bytes.  buf_prepared=%d\n", size, ctx->buf_prepared);

      //printf("ctx->buffer=%p  %s\n", ctx->buffer, ctx->buffer );
      server_post_receive(id);

      ctx->buf_len = size;
      ctx->buf_recv_counter++;
      printf("counter2 = %d\n", ctx->buf_recv_counter++);
      while (ctx->can_recv == false)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      printf("server ctx has been marked false\n");
      ctx->msg->id = MSG_READY;
      server_send_message(id);

    }

  }
}

void RdmaTwoSidedServerOp::server_on_disconnect(struct rdma_cm_id *id)
{
  struct conn_context *ctx = (struct conn_context *)id->context;


  ibv_dereg_mr(ctx->buffer_mr);
  ibv_dereg_mr(ctx->msg_mr);

  free(ctx->buffer);
  free(ctx->msg);

  printf("finished transferring\n");

  //free(ctx);
}



void RdmaTwoSidedServerOp::rc_server_loop(const char *port, struct conn_context* ctx)
{
  printf("rc_server_loop ctx =%p\n", ctx );
  struct sockaddr_in6 addr;
  struct rdma_cm_id *listener = NULL;
  struct rdma_event_channel *ec = NULL;

  memset(&addr, 0, sizeof(addr));
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(atoi(port));

  TEST_Z(ec = rdma_create_event_channel());
  TEST_NZ(rdma_create_id(ec, &listener, NULL, RDMA_PS_TCP));
  TEST_NZ(rdma_bind_addr(listener, (struct sockaddr *)&addr));
  TEST_NZ(rdma_listen(listener, 10)); /* backlog=10 is arbitrary */
  printf("server listenning... port = %ld\n", addr.sin6_port);
  server_event_loop(ec, 0, ctx); // don't exit on disconnect

  rdma_destroy_id(listener);
  rdma_destroy_event_channel(ec);
}




void RdmaTwoSidedServerOp::server_event_loop(struct rdma_event_channel *ec, int exit_on_disconnect, struct conn_context* ctx)
{
  struct rdma_cm_event *event = NULL;
  struct rdma_conn_param cm_params;

  server_build_params(&cm_params);

  while (rdma_get_cm_event(ec, &event) == 0)
  {
    struct rdma_cm_event event_copy;

    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);
    printf("Server Loop...\n");
    if (event_copy.event == RDMA_CM_EVENT_ADDR_RESOLVED)
    {
      server_build_connection(event_copy.id);

      server_on_pre_conn(event_copy.id, s_ctx->pd, ctx);


      TEST_NZ(rdma_resolve_route(event_copy.id, TIMEOUT_IN_MS));

    }
    else if (event_copy.event == RDMA_CM_EVENT_ROUTE_RESOLVED)
    {
      TEST_NZ(rdma_connect(event_copy.id, &cm_params));

    }
    else if (event_copy.event == RDMA_CM_EVENT_CONNECT_REQUEST)
    {
      server_build_connection(event_copy.id);

      server_on_pre_conn(event_copy.id, s_ctx->pd, ctx);

      TEST_NZ(rdma_accept(event_copy.id, &cm_params));

    }
    else if (event_copy.event == RDMA_CM_EVENT_ESTABLISHED)
    {
      server_on_connection(event_copy.id);

    }
    else if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED)
    {
      rdma_destroy_qp(event_copy.id);

      server_on_disconnect(event_copy.id);

      rdma_destroy_id(event_copy.id);

      if (exit_on_disconnect)
        break;

    }
    else
    {
      rc_die("unknown event\n");
    }
  }
}



void RdmaTwoSidedServerOp:: server_build_connection(struct rdma_cm_id *id)
{
  struct ibv_qp_init_attr qp_attr;

  server_build_context(id->verbs);
  server_build_qp_attr(&qp_attr);

  TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));
}

void RdmaTwoSidedServerOp:: server_build_context(struct ibv_context *verbs)
{
  if (s_ctx)
  {
    if (s_ctx->ctx != verbs)
      rc_die("cannot handle events in more than one context.");
    return;
  }

  s_ctx = (struct context *)malloc(sizeof(struct context));

  s_ctx->ctx = verbs;

  TEST_Z(s_ctx->pd = ibv_alloc_pd(s_ctx->ctx));
  TEST_Z(s_ctx->comp_channel = ibv_create_comp_channel(s_ctx->ctx));
  TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 10, NULL, s_ctx->comp_channel, 0)); /* cqe=10 is arbitrary */
  TEST_NZ(ibv_req_notify_cq(s_ctx->cq, 0));

  TEST_NZ(pthread_create(&s_ctx->cq_poller_thread, NULL, server_poll_cq, (s_ctx->comp_channel) ));
}

void RdmaTwoSidedServerOp:: server_build_params(struct rdma_conn_param *params)
{
  memset(params, 0, sizeof(*params));

  params->initiator_depth = params->responder_resources = 1;
  params->rnr_retry_count = 7; /* infinite retry */
  //add this one
  params->retry_count = 7;
}

void RdmaTwoSidedServerOp:: server_build_qp_attr(struct ibv_qp_init_attr *qp_attr)
{
  memset(qp_attr, 0, sizeof(*qp_attr));

  qp_attr->send_cq = s_ctx->cq;
  qp_attr->recv_cq = s_ctx->cq;
  qp_attr->qp_type = IBV_QPT_RC;

  qp_attr->cap.max_send_wr = 10;
  qp_attr->cap.max_recv_wr = 10;
  qp_attr->cap.max_send_sge = 1;
  qp_attr->cap.max_recv_sge = 1;
}


void * RdmaTwoSidedServerOp::server_poll_cq(void* void_ch)
{
  struct ibv_cq *cq;
  struct ibv_wc wc;
  struct ibv_comp_channel *channel = void_ch;
  void *ctx;
  while (1)
  {
    //TEST_NZ(ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx));
    TEST_NZ(ibv_get_cq_event(channel, &cq, &ctx));
    ibv_ack_cq_events(cq, 1);
    TEST_NZ(ibv_req_notify_cq(cq, 0));

    while (ibv_poll_cq(cq, 1, &wc))
    {
      if (wc.status == IBV_WC_SUCCESS)
      {
        //printf("server_on_completion SUCCESS\n");
        server_on_completion(&wc);
      }
      else
      {
        printf("status=%d\n", wc.status );
        rc_die("poll_cq: status is not IBV_WC_SUCCESS");
      }
    }
  }

  return NULL;
}