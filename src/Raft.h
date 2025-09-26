#pragma once
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <thread>

struct Address {
    std::string host;
    unsigned short port;
    sockaddr_in addr{};

    Address(const std::string& host, const unsigned short port) : host(host), port(port) {
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    }
};

void run_raft_tcp(
    unsigned int threads,
    unsigned int connections,
    unsigned int pipes,
    unsigned char node_id,
    unsigned char leader_id,
    std::vector<Address>& peers
);

void tune_socket(int fd);

enum class SocketType {
    CONNECTOR, ACCEPTOR
};

struct SocketData {
    SocketType type;
    sockaddr* target_addr = nullptr;
    int fd = -1;
};

struct SocketOptData {
    int fd;
    int opt_name;
};

struct SocketAcceptor {
    int socket_fd;
    sockaddr* cli {};
    socklen_t cli_len { };
};