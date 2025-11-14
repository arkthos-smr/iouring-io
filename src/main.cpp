#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <csignal>
#include "Arkthos.h"
#include <vector>

void shutdown(const int signum) {
    const char* name = "UNKNOWN";
    switch (signum) {
        case SIGINT:  name = "SIGINT";  break;
        case SIGTERM: name = "SIGTERM"; break;
        case SIGQUIT: name = "SIGQUIT"; break;
        case SIGHUP:  name = "SIGHUP";  break;
    }

    fprintf(stderr, "\nShutting down with signal %s\n", name);
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

    std::array<std::thread, 1> workers{};
    workers[0] = thread_guard("Arkthos main thread", [&]() {
        run_arkthos_udp<Config>();
    });

    while (RUNNING.load()) pause();
    for (auto &worker : workers) worker.join();

    return 0;
}


