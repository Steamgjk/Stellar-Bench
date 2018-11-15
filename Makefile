all: ps worker
CC=g++
TARGET = ps
TARGET1 = worker
TARGET2 = test_send
TARGET3 = test_recv
LIBS=-libverbs -lrdmacm -pthread -libverbs -lrdmacm
CFLAGS=-Wall -g -fpermissive -std=c++11
OBJS=ps.o server_rdma_op.o client_rdma_op.o rdma_common.o rdma_two_sided_client_op.o rdma_two_sided_server_op.o common.o
OBJS1=worker.o server_rdma_op.o client_rdma_op.o rdma_common.o rdma_two_sided_client_op.o rdma_two_sided_server_op.o common.o
OBJS2=test_send.o server_rdma_op.o client_rdma_op.o rdma_common.o rdma_two_sided_client_op.o rdma_two_sided_server_op.o common.o
OBJS3=test_recv.o server_rdma_op.o client_rdma_op.o rdma_common.o rdma_two_sided_client_op.o rdma_two_sided_server_op.o common.o

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)
$(TARGET1): $(OBJS1)
	$(CC) $(CFLAGS) -o $(TARGET1) $(OBJS1) $(LIBS)
$(TARGET2): $(OBJS2)
	$(CC) $(CFLAGS) -o $(TARGET1) $(OBJS1) $(LIBS)
$(TARGET3): $(OBJS3)
	$(CC) $(CFLAGS) -o $(TARGET1) $(OBJS1) $(LIBS)

test_send.o test_send.cpp
	$(CC) $(CFLAGS) -c test_send.cpp
test_recv.o test_recv.cpp
	$(CC) $(CFLAGS) -c test_recv.cpp
worker.o: worker.cpp
	$(CC) $(CFLAGS) -c worker.cpp
ps.o: ps.cpp
	$(CC) $(CFLAGS) -c ps.cpp
server_rdma_op.o: server_rdma_op.cpp
	$(CC) $(CFLAGS) -c server_rdma_op.cpp
client_rdma_op.o: client_rdma_op.cpp
	$(CC) $(CFLAGS) -c client_rdma_op.cpp 
rdma_common.o: rdma_common.cpp
	$(CC) $(CFLAGS) -c rdma_common.cpp
rdma_two_sided_client_op.o: rdma_two_sided_client_op.cpp
	$(CC) $(CFLAGS) -c rdma_two_sided_client_op.cpp
rdma_two_sided_server_op.o: rdma_two_sided_client_op.cpp
	$(CC) $(CFLAGS) -c rdma_two_sided_server_op.cpp
common.o: common.cpp
	$(CC) $(CFLAGS) -c common.cpp
clean:
	rm -rf *.o  *~
