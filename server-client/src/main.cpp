#include <iostream>
#include <csignal>
#include <cstdlib>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
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

bool redirectLogs(const std::string& logPath) {
    int fd = open(logPath.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd < 0) {
        return false;
    }
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
    return true;
}

bool daemonize(const std::string& logPath) {
    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid > 0) {
        exit(0);
    }

    if (setsid() < 0) {
        return false;
    }

    int fd = open("/dev/null", O_RDONLY);
    if (fd >= 0) {
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    return redirectLogs(logPath);
}

int main(int argc, char** argv) {
    std::string dbConnStr = "host=localhost port=5432 dbname=mes_db user=shirkinson password=mirkill200853";
    int port = 5555;
    bool runAsDaemon = false;
    std::string logPath;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--daemon") {
            runAsDaemon = true;
        } else if (arg == "--log" && i + 1 < argc) {
            logPath = argv[++i];
        } else if (arg.rfind("--log=", 0) == 0) {
            logPath = arg.substr(6);
        } else if (arg.rfind("--db=", 0) == 0) {
            dbConnStr = arg.substr(5);
        } else if (arg.rfind("--port=", 0) == 0) {
            port = std::atoi(arg.substr(7).c_str());
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "[Main] Unknown option: " << arg << std::endl;
            return 1;
        } else {
            positional.push_back(arg);
        }
    }

    if (!positional.empty()) {
        dbConnStr = positional[0];
    }
    if (positional.size() > 1) {
        port = std::atoi(positional[1].c_str());
    }

    if (runAsDaemon && logPath.empty()) {
        logPath = "server.log";
    }

    try {
        if (runAsDaemon) {
            if (!daemonize(logPath)) {
                std::cerr << "[Main] Failed to daemonize" << std::endl;
                return 1;
            }
        } else if (!logPath.empty()) {
            if (!redirectLogs(logPath)) {
                std::cerr << "[Main] Failed to open log file" << std::endl;
                return 1;
            }
        }

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
