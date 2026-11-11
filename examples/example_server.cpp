#include "server.hpp"
#include "Task.pb.h"

FishRPC::RpcServer server;
void reg(){
  server.register_method<Task_protocol::ADD_REQ, Task_protocol::ADD_RESP>("add",[](Task_protocol::ADD_REQ req){
      Task_protocol::ADD_RESP resp;
      resp.set_result(req.a() + req.b());
      return resp;
    }
  );

}

int main(){
  reg();
  std::cout << "服务器启动" << std::endl;
  server.start("8080", 1);

}