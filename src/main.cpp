#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <csignal>
#include "Ring.hpp"
#include "Raft.h"



void shutdown(const int signum) {
    std::cout << "Received signal: " << signum << std::endl;
    RUNNING.store(false);
}

int main() {
    struct sigaction action {};
    action.sa_handler = shutdown;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT,  &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGQUIT, &action, nullptr);
    sigaction(SIGHUP,  &action, nullptr);

    std::vector<std::thread> workers{};
    std::vector peers{
        Address { "127.0.0.1", 6982 },
        Address { "127.0.0.1", 6981 }
    };
    workers.emplace_back([&](){ run_raft_udp<100,100, 5>(100, 0, 0, peers); });
    workers.emplace_back([&]() { run_raft_udp<100,100, 5>(1, 1, 0, peers); });

    while (RUNNING.load()) {
        pause();
    }

    for (auto &worker : workers) {
        worker.join();
    }

    std::cout << "Shutdown!" << std::endl;

    return 0;
}
