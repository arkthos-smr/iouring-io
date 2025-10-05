#pragma once
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <thread>
#include "Ring.hpp"
#include "sys/socket.h"
#include <netinet/tcp.h>
#include <fcntl.h>

inline void tune_socket(const int fd) {
    int flag = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        close(fd);
        throw std::runtime_error("setsockopt TCP_NODELAY failed");
    }

    if (setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(flag)) < 0) {
        close(fd);
        throw std::runtime_error("setsockopt TCP_QUICKACK failed");
    }
}

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

struct WriteEntry {
    int index, amount;
};

struct Connection {
    std::atomic<bool> is_writing { false };
    std::atomic<int> write_head { 0 };
    std::atomic<int> write_tail { 0 };
    WriteEntry writes[100];
};

template<size_t log_size, size_t message_size>
void run_raft_tcp(
    const unsigned int threads,
    const unsigned int connections,
    const unsigned int pipes,
    const unsigned char node_id,
    const unsigned char leader_id,
    std::vector<Address> &peers
) {
       std::vector<std::thread> local_workers;
    local_workers.reserve(threads);
    auto node_address = peers[node_id];
    std::vector<int> active_connections;

    if (leader_id == node_id) {
        for (int peerId = 0; peerId < peers.size(); peerId++) {
            if (peerId == node_id) continue;
            for (int connId = 0; connId < connections; connId++) {
                while (RUNNING.load(std::memory_order_relaxed)) {
                    const auto client_socket = socket(AF_INET, SOCK_STREAM, 0);

                    if (client_socket < 0) {
                        throw std::runtime_error("Failed to create server socket");
                    }

                    tune_socket(client_socket);

                    fprintf(stderr, "Connecting from nodeId=%d\n", node_id);
                    if (const auto peer_addr = reinterpret_cast<sockaddr *>(&peers[peerId].addr); connect(client_socket, peer_addr, sizeof(sockaddr_in)) < 0) {
                        close(client_socket);
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    } else {
                        active_connections.emplace_back(client_socket);
                        break;
                    }
                }
            }
        }
    } else {
        const auto server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            throw std::runtime_error("Failed to create server socket");
        }

        int flag = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
            close(server_socket);
            throw std::runtime_error("error setting reuseaddr opt");
        }
        tune_socket(server_socket);

        if (bind(server_socket, reinterpret_cast<sockaddr*>(&node_address.addr), sizeof(node_address.addr)) < 0) {
            close(server_socket);
            throw std::runtime_error("bind failed");
        }

        if (listen(server_socket, SOMAXCONN) < 0) {
            close(server_socket);
            throw std::runtime_error("listen failed");
        }

        fprintf(stderr, "Accepting connections on node_id=%d\n", node_id);

        while (RUNNING.load(std::memory_order_relaxed) && active_connections.size() != connections) {
            sockaddr_in cli_addr{};
            socklen_t addr_len = sizeof(cli_addr);
            const auto client_socket = accept(server_socket, reinterpret_cast<sockaddr*>(&cli_addr), &addr_len);
            if (client_socket < 0) {
                close(server_socket);
                throw std::runtime_error("Error acceping sockets!");
            }
            active_connections.emplace_back(client_socket);
        }
    }

    for (const int client_fd : active_connections) {
        const int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags == -1) throw std::runtime_error("get fcntl failed");
        if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) throw std::runtime_error("set fcntl failed");
    }

    constexpr unsigned int buffer_count = 128;
    std::atomic<unsigned char> acks[log_size];
    char log [log_size][message_size];

    for (size_t i = 0; i < log_size; ++i) {
        memset(log[i], 0, message_size);
    }

    for (int threadId = 0; threadId < threads; threadId++) {
        local_workers.emplace_back([connections, pipes, node_id, leader_id, &peers]() {
            char* buffer_pool = new char[buffer_count * message_size];
            std::atomic<int> buffer_grabber_head = buffer_count;
            int buffer_grabber[buffer_count];
            std::atomic<int> buffer_references[buffer_count];

            iovec iovecs[buffer_count];
            for (int i = 0; i < buffer_count; ++i) {
                iovecs[i].iov_base = buffer_pool + i * message_size;
                iovecs[i].iov_len = message_size;
                buffer_grabber[i] = i;
                buffer_references[i] = 0;
            }

            fprintf(stderr, "Node id: %d\n", node_id);
            run_ring(
                4096, 1,
                ring_init(

                    io_uring_register(_r_ring_fd, IORING_REGISTER_BUFFERS, iovecs, buffer_count);
                ),
                ring_loop(),
                ring_completions(

                )
            );

            delete[] buffer_pool;
        });
    }

    for (auto &worker : local_workers) {
        worker.join();
    }
}


