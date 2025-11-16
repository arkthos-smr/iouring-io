#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <csignal>
#include "ArkthosTcp.h"
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
    try {
        struct sigaction action{};
        action.sa_handler = shutdown;
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT, &action, nullptr);
        sigaction(SIGTERM, &action, nullptr);
        sigaction(SIGQUIT, &action, nullptr);
        sigaction(SIGHUP, &action, nullptr);

        run_arkthos_tcp<ConfigTcp>();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error\n";
        return 1;
    }

    return 0;
}


