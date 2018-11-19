
#include "rdma_two_sided_client_op.h"



RdmaTwoSidedClientOp::RdmaTwoSidedClientOp()
{

}
RdmaTwoSidedClientOp::~RdmaTwoSidedClientOp()
{

}
void RdmaTwoSidedClientOp::client_write_remote(struct rdma_cm_id *id, uint32_t len)
{
  struct client_context *ctx = (struct client_context *)id->context;

  struct ibv_send_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
  wr.send_flags = IBV_SEND_SIGNALED;
  wr.imm_data = htonl(len);
  wr.wr.rdma.remote_addr = ctx->peer_addr;
  wr.wr.rdma.rkey = ctx->peer_rkey;

  if (len)
  {
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)ctx->buffer;
    sge.length = len;
    sge.lkey = ctx->buffer_mr->lkey;
  }

  TEST_NZ(ibv_post_send(id->qp, &wr, &bad_wr));
}

void RdmaTwoSidedClientOp::client_post_receive(struct rdma_cm_id *id)
{
  struct client_context *ctx = (struct client_context *)id->context;

  struct ibv_recv_wr wr, *bad_wr = NULL;
  struct ibv_sge sge;

  memset(&wr, 0, sizeof(wr));

  wr.wr_id = (uintptr_t)id;
  wr.sg_list = &sge;
  wr.num_sge = 1;

  sge.addr = (uintptr_t)ctx->msg;
  sge.length = sizeof(*ctx->msg);
  sge.lkey = ctx->msg_mr->lkey;

  TEST_NZ(ibv_post_recv(id->qp, &wr, &bad_wr));
}

void RdmaTwoSidedClientOp::client_send_next_chunk(struct rdma_cm_id *id)
{
  struct client_context *ctx = (struct client_context *)id->context;
  //printf("check buf prepared\n");
  /*
  while (!ctx->buf_prepared)
  {
    //printf("to send has not well prepared\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  **/
  //printf("client send buf has been marked prepared = true\n");
  /*
  char*str = "iamok";
  ctx->buf_len = strlen(str);
  memcpy(ctx->buffer, str, ctx->buf_len);
  **/
  //printf("send tchunk...\n");
  //printf("buf= %s  len=%ld\n", ctx->buffer, ctx->buf_len );

  client_write_remote(id, ctx->buf_len);
  printf("send chunk success\n");
  ctx->buf_prepared = false;
}



void RdmaTwoSidedClientOp::client_on_pre_conn(struct rdma_cm_id *id, struct ibv_pd *pd)
{
  struct client_context *ctx = (struct client_context *)id->context;
  posix_memalign((void **)&ctx->buffer, sysconf(_SC_PAGESIZE), BUFFER_SIZE);

//s_ctx->pd
  TEST_Z(ctx->buffer_mr = ibv_reg_mr((pd), ctx->buffer, BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE));

  posix_memalign((void **)&ctx->msg, sysconf(_SC_PAGESIZE), sizeof(*ctx->msg));


  TEST_Z(ctx->msg_mr = ibv_reg_mr((pd), ctx->msg, sizeof(*ctx->msg), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE));

  ctx->buf_registered = true;
  client_post_receive(id);
  printf("after client_post_receive\n");
}

void RdmaTwoSidedClientOp::client_on_completion(struct ibv_wc *wc)
{
  struct rdma_cm_id *id = (struct rdma_cm_id *)(uintptr_t)(wc->wr_id);
  struct client_context *ctx = (struct client_context *)id->context;
  if (wc->opcode & IBV_WC_RECV)
  {
    if (ctx->msg->id == MSG_MR)
    {
      ctx->peer_addr = ctx->msg->data.mr.addr;
      ctx->peer_rkey = ctx->msg->data.mr.rkey;

      //printf("received MR, sending file name(obsolete), send chunk\n");
      //send_file_name(id);

      while (ctx->can_send == false)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      printf("Before send chunk\n");
      for (int ii = 0; ii < ctx->buf_len; ii++)
      {
        printf("%c", ctx->buffer[ii] );
      }
      printf("\n");

      client_send_next_chunk(id);
      printf("send chunk1\n");
      ctx->can_send = false;
      ctx->buf_write_counter++;

    }
    else if (ctx->msg->id == MSG_READY)
    {
      //printf("received READY, sending chunk\n");

      while (ctx->can_send == false)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      client_send_next_chunk(id);
      printf("send chunk2\n");
      ctx->can_send = false;
      ctx->buf_write_counter++;

    }
    else if (ctx->msg->id == MSG_DONE)
    {

      printf("received DONE, disconnecting\n");

      rc_disconnect(id);
      return;
    }
    else
    {
      printf("Comehere \n");
    }

    client_post_receive(id);
  }
  else
  {
    printf("wc-opcode=%d  IBV_WC_RDMA_WRITE=%d\n", wc->opcode, IBV_WC_RDMA_WRITE  );
  }
}



void RdmaTwoSidedClientOp::rc_client_loop(const char *host, const char *port, void *context)
{
  printf("enter rc_client_loop\n");
  struct addrinfo *addr;
  struct rdma_cm_id *conn = NULL;
  struct rdma_event_channel *ec = NULL;
  struct rdma_conn_param cm_params;

  TEST_NZ(getaddrinfo(host, port, NULL, &addr));

  TEST_Z(ec = rdma_create_event_channel());
  TEST_NZ(rdma_create_id(ec, &conn, NULL, RDMA_PS_TCP));
  TEST_NZ(rdma_resolve_addr(conn, NULL, addr->ai_addr, TIMEOUT_IN_MS));

  freeaddrinfo(addr);

  conn->context = context;

  printf("check1\n");
  client_build_params(&cm_params);
  printf("check2\n");

  client_event_loop(ec, 1); // exit on disconnect

  rdma_destroy_event_channel(ec);
}



void RdmaTwoSidedClientOp::client_event_loop(struct rdma_event_channel *ec, int exit_on_disconnect)
{
  struct rdma_cm_event *event = NULL;
  struct rdma_conn_param cm_params;

  client_build_params(&cm_params);
  printf("check3\n");
  while (rdma_get_cm_event(ec, &event) == 0)
  {
    struct rdma_cm_event event_copy;

    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);
    printf("check4\n");
    if (event_copy.event == RDMA_CM_EVENT_ADDR_RESOLVED)
    {
      printf("check5\n");
      client_build_connection(event_copy.id);
      printf("check5.5\n");
      client_on_pre_conn(event_copy.id, s_ctx->pd);
      printf("check6\n");
      TEST_NZ(rdma_resolve_route(event_copy.id, TIMEOUT_IN_MS));
    }
    else if (event_copy.event == RDMA_CM_EVENT_ROUTE_RESOLVED)
    {
      TEST_NZ(rdma_connect(event_copy.id, &cm_params));
    }
    else if (event_copy.event == RDMA_CM_EVENT_CONNECT_REQUEST)
    {
      printf("check7\n");
      client_build_connection(event_copy.id);
      printf("check9\n");
      client_on_pre_conn(event_copy.id, s_ctx->pd);
      printf("check8\n");
      TEST_NZ(rdma_accept(event_copy.id, &cm_params));

    }
    else if (event_copy.event == RDMA_CM_EVENT_ESTABLISHED)
    {
      /*
      if (s_on_connect_cb)
        s_on_connect_cb(event_copy.id);
        **/
    }
    else if (event_copy.event == RDMA_CM_EVENT_DISCONNECTED)
    {
      rdma_destroy_qp(event_copy.id);
      /*
      if (s_on_disconnect_cb)
        s_on_disconnect_cb(event_copy.id);
      **/
      rdma_destroy_id(event_copy.id);

      if (exit_on_disconnect)
        break;

    }
    else
    {
      //rc_die("unknown event\n");
    }
  }
}



void RdmaTwoSidedClientOp::client_build_connection(struct rdma_cm_id *id)
{
  struct ibv_qp_init_attr qp_attr;
  client_build_context(id->verbs);
  client_build_qp_attr(&qp_attr);
  printf("check 00\n");
  TEST_NZ(rdma_create_qp(id, s_ctx->pd, &qp_attr));
  printf("check 01\n");
}

void RdmaTwoSidedClientOp::client_build_context(struct ibv_context *verbs)
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
  TEST_Z(s_ctx->cq = ibv_create_cq(s_ctx->ctx, 10, NULL, (s_ctx->comp_channel), 0));
  /* cqe=10 is arbitrary */
  TEST_NZ(ibv_req_notify_cq(s_ctx->cq, 0));

  //client_poll_cq(NULL);
  TEST_NZ(pthread_create(&s_ctx->cq_poller_thread, NULL, client_poll_cq, (s_ctx->comp_channel) ));

}

void RdmaTwoSidedClientOp::client_build_params(struct rdma_conn_param *params)
{
  memset(params, 0, sizeof(*params));

  params->initiator_depth = params->responder_resources = 1;
  params->rnr_retry_count = 7; /* infinite retry */
  //add this one
  params->retry_count = 7;
}

void RdmaTwoSidedClientOp::client_build_qp_attr(struct ibv_qp_init_attr *qp_attr)
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


void * RdmaTwoSidedClientOp::client_poll_cq(void* void_ch)
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
        client_on_completion(&wc);
      else
      {
        printf("client status=%d  IBV_WC_RETRY_EXC_ERR=%d\n", wc.status, IBV_WC_RETRY_EXC_ERR);
        //rc_die("client poll_cq: status is not IBV_WC_SUCCESS");
      }
    }
  }

  return NULL;
}