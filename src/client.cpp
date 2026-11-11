#include "client.hpp"

namespace FishRPC {

task<bool> RpcClient::connect(const char* ip, const char* port){

  int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC , 0);
  if(fd < 0)  throw std::runtime_error("socket error");

  sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(atoi(port));
  if(inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) throw std::runtime_error("inet_pton error");

  int res = co_await task_connect(&loop.get_ring(), (sockaddr*)&addr, sizeof(addr), fd);

  if(res < 0) throw std::runtime_error("connect error");

  server_fd = fd;
  loop.add_task(reader(fd));
  co_return true;
}

void RpcClient::run(task<void>&& main_task){
  loop.run(std::move(main_task));
}

task<void> RpcClient::reader(int fd){
  while(true){
    package_name::Frame content;
    if(!co_await read_frame(loop.get_ring(), fd, content)) break;

    auto resp = content.mutable_response();
    auto it = pending_.find(resp->id());

    if(it == pending_.end()) throw std::runtime_error("unknown request id");

    auto& awaiter = *it->second;
    awaiter.error = resp->error();
    awaiter.error_message = resp->error_message();
    awaiter.result = resp->result();

    awaiter.ready = 1;
    awaiter.handle_.resume();
  }

  co_return;
}

} // namespace FishRPC
