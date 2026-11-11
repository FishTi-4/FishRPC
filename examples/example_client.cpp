#include <iostream>
#include "client.hpp"
#include "coroutine.hpp"
#include "Task.pb.h"

using namespace FishRPC;
using namespace package_name;
using namespace Task_protocol;

FishRPC::task<void> demo(FishRPC::RpcClient& c)
{
    if (co_await c.connect("127.0.0.1", "8080") < 0)
        throw std::runtime_error("connect failed");

    ADD_REQ req;
    req.set_a(3);
    req.set_b(4);
    ADD_RESP resp = co_await c.call<ADD_REQ, ADD_RESP>("add", req);
    std::cout << "3 + 4 = " << resp.result() << "\n";

}

int main()
{
    FishRPC::RpcClient client;
    try {
        client.run(demo(client)); // 阻塞,直到 demo 结束
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
