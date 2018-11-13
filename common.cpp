#include "common.h"


void rc_disconnect(struct rdma_cm_id *id)
{
  rdma_disconnect(id);
}

void rc_die(const char *reason)
{
  fprintf(stderr, "%s\n", reason);
  exit(EXIT_FAILURE);
}