#include <iostream>
#include <csignal>
#include <cstdlib>
#include "server.hpp"

MessengerServer* gServer = nullptr;

void signalHandler(int sig) {
    (void)sig;
    if (gServer) {
        std::cout << "\n[Main] Shutting down..." << std::endl;
        gServer->stop();
    }
    exit(0);
}

int main(int argc, char** argv) {
    std::string dbConnStr = "host=localhost port=5432 dbname=mes_db user=shirkinson password=mirkill200853";
    int port = 5555;

    if (argc > 1) {
        dbConnStr = argv[1];
    }
    if (argc > 2) {
        port = std::atoi(argv[2]);
    }

    try {
        gServer = new MessengerServer(dbConnStr, port);
        
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        gServer->start();

        // Keep running
        while (gServer->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        delete gServer;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[Main] Exception: " << e.what() << std::endl;
        return 1;
    }
}
