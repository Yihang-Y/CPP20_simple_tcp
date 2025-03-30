#include "EventLoop.h"
#include "TCPServer.h"
#include <iostream>

int main() {
    EventLoop loop;
    TCPServer server(&loop, "8080");
    
    std::cout << "Echo server is running on port 8080..." << std::endl;
    
    server.start();
    loop.loop();
    
    return 0;
}