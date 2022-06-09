#include <rados/librados.hpp>
#include <iostream>
#include <string>

#include "rados_client.h"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

int RadosClient::write(string file_name, librados::bufferlist &data){
    cout<< "start write object"<<endl;
    int ret;
    ret = io_ctx.write_full(file_name,data);
    if(ret < 0){
        cerr << "could not write object! error "<< ret <<endl;
        ret = EXIT_FAILURE;
    }else{
        cout << "create object file success" << endl;
    }
    return ret;
}

int RadosClient::setup_pool(const char* pool_name){
    int ret;
    pool = pool_name;
    ret = rados.pool_lookup(pool);
    if(ret < 0){
        cerr << "could not lookup pool, maybe pool not exit! error" << ret << endl;
        return ret;
    }else{
        cout<< " lookup success! "<< endl; 
    }

    librados::IoCtx ctx;
    ret = rados.ioctx_create(pool_name,ctx);
    if(ret < 0){
        cerr << "cloud not set up ioctx error" << ret <<endl;
        return ret;
    }else{
        cout << "create an ioctx for pool" << endl;
    }
    io_ctx = ctx;
    return ret;
}