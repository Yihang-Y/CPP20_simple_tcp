#include "TCPServer.h"

int main() {
    TCPServer server("8080");
    server.run();
}