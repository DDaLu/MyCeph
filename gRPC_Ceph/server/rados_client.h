#ifndef GRPC_SERVER_RADOS_CLIENT_H

#include <rados/librados.hpp>
#include <iostream>
#include <string>

using std::cout;
using std::cerr;
using std::endl;

class RadosClient{
private:
    librados::IoCtx io_ctx;
    librados::Rados rados;
    const char* pool;
    const char* conf;
public:
    RadosClient(const char* conf):conf(conf){
        int ret;

        //初始化rados用户
        ret = rados.init("admin");
        if(ret < 0){
            cerr << "could'n initialize rados, error" << ret <<endl;
            exit(ret);
        }else{
            cout<< "just initialize rados" << endl;
        }

        ret = rados.conf_read_file(conf);
        if(ret < 0){
            cerr<<"failed to parse config file " << conf << " error" << ret <<endl;
            exit(ret);
        }

        rados.connect();
        if(ret < 0){
            cerr << "could not connect to cluster error" << ret <<endl;
            exit(ret);
        }else{
            cout<< "connected to the rados cluster" <<endl;
        }
    };

    //析构函数
    ~RadosClient(){
        cout<<"rados client exit" <<endl;
        rados.shutdown();
    };
    //写函数
    int write(std::string file_name, librados::bufferlist &data);
    //设置存储池pool
    int setup_pool(const char* pool_name);
};
#endif
