#include "TCPServer.h"
#include "IoUringScheduler.h"
#include "IoUringSchedulerAdapter.h"
int main() {
    // 创建调度器实例
    // IoUringScheduler scheduler;
    
    // 使用明确的调度器实例
    TCPServer server("8080", &getScheduler());
    
    // 运行服务器
    server.run();
    
    return 0;
}