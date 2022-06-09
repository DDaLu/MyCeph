# 环境准备

## ceph 环境准备
- 操作系统: centos7
- ceph 版本 ：v12.2.13

### 编译过程

#### 编译依赖包和运行程序
```shell script
$ git clone -b v12.2.13 git://github.com/ceph/ceph

$ git submodule update --init --recursive

$ ./install-deps.sh

$ ./do_cmake.sh

$ cd build

$ make
```

#### 启动测试集群
- 启动测试集群目的是为了配套测试使用
    ```shell script

    $ cd build
    $ make vstart        # builds just enough to run vstart
    $ MON=3 MDS=0 OSD=3 RGW=1 ../src/vstart.sh -d -n -x -l -b
    $ ./bin/ceph -s
    ```
- 关闭测试集群

    ```shell script
    $ ../src/stop.sh
    ```

## grpc sever 环境准备
### 编译安装grpc
// 安装cmake
$ sudo apt install -y cmake
$ sudo apt install -y build-essential autoconf libtool pkg-config

- 安装grpc
$ git clone --recurse-submodules -b v1.35.0 https://github.com/grpc/grpc
$ cd grpc
$ mkdir -p cmake/build
$ pushd cmake/build
$ cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      ../..
$ make -j20
$ make install
$ popd

# 编译程序
使用 `Makefile` 中的编译命令，记得修改相应的路径
make server

如果无法运行，则手动运行 Makefile中的命令

# 运行程序
./file_server -c /home/dovefi/ceph12/build/ceph.conf
Server listening on 0.0.0.0:6888. Press Ctrl-C to end.

> - -c: 测试ceph集群的配置文件路径

### 参考文档
- [grpc 官网教程](https://grpc.io/docs/languages/cpp/quickstart/)
- [github 文件上传](https://github.com/yitzikc/grpc-file-exchange)
- [ceph 编译安装](https://github.com/ceph/ceph/tree/v12.2.13)
- [librados(C) api](https://docs.ceph.com/en/latest/rados/api/librados/#librados-c)
