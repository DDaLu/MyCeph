CC = gcc
CXX = g++
CFLAGS = -Wall -g -std=c++11
CFLAGS += `pkg-config --cflags protobuf grpc++ grpc`
GRPC_INSTALL_DIR = /usr/local
LIBRADOS_DIR = /home/watson/ceph
PKG_CONFIG_PATH=/root/grpc/cmake/build/libs/opt/pkgconfig:/root/grpc/cmake/build/third_party/protobuf:/root/grpc/third_party/protobuf:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig

CFLAGS += -I$(LIBRADOS_DIR)/src/include/ -L$(LIBRADOS_DIR)/build/lib -lrados -lceph-common -Wl,-rpath,$(LIBRADOS_DIR)/build/lib/
CFLAGS += -I$(GRPC_INSTALL_DIR)/include/ `pkg-config --libs protobuf grpc grpc++`

OBJ = file_server

server:
	export PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)
	g++ file_server.cpp rados_client.cpp pb/*.cc -o $(OBJ) $(CFLAGS)