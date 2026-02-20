#include "client.hpp"
#include <sstream>
#include <cstring>
#include <cerrno>
#include <sys/time.h>

MessengerClient::MessengerClient(const std::string& host, int port)
    : host_(host), port_(port), socket_(-1), userId_(-1) {}

MessengerClient::~MessengerClient() {
    disconnect();
}

bool MessengerClient::connect() {
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        std::cerr << "[Client] Failed to create socket" << std::endl;
        return false;
    }

    timeval timeout;
    timeout.tv_sec = 30;
    timeout.tv_usec = 0;
    if (setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "[Client] Failed to set receive timeout" << std::endl;
    }
    if (setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        std::cerr << "[Client] Failed to set send timeout" << std::endl;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_);

    if (inet_pton(AF_INET, host_.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "[Client] Invalid address" << std::endl;
        close(socket_);
        socket_ = -1;
        return false;
    }

    if (::connect(socket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[Client] Connection failed" << std::endl;
        close(socket_);
        socket_ = -1;
        return false;
    }

    std::cout << "[Client] Connected to " << host_ << ":" << port_ << std::endl;
    return true;
}

void MessengerClient::disconnect() {
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
        sessionId_ = "";
        userId_ = -1;
        std::cout << "[Client] Disconnected" << std::endl;
    }
}

bool MessengerClient::isConnected() const {
    return socket_ >= 0;
}

std::string MessengerClient::buildCommand(const std::string& cmd, 
                                          const std::unordered_map<std::string, std::string>& params) {
    std::string result = cmd;
    for (const auto& p : params) {
        result += " " + p.first + "=" + p.second;
    }
    return result;
}

std::string MessengerClient::sendCommand(const std::string& cmd) {
    if (!isConnected()) {
        return "[ERROR] Not connected";
    }

    if (!sendAll(cmd + "\n")) {
        std::cerr << "[Client] Failed to send command" << std::endl;
        return "[ERROR] Send failed";
    }

    std::string response;
    if (!readLine(response)) {
        std::cerr << "[Client] Failed to receive response" << std::endl;
        return "[ERROR] Receive failed";
    }

    return response;
}

bool MessengerClient::sendAll(const std::string& data) {
    size_t totalSent = 0;
    while (totalSent < data.size()) {
#ifdef MSG_NOSIGNAL
        ssize_t sent = send(socket_, data.c_str() + totalSent, data.size() - totalSent, MSG_NOSIGNAL);
#else
        ssize_t sent = send(socket_, data.c_str() + totalSent, data.size() - totalSent, 0);
#endif
        if (sent <= 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}

bool MessengerClient::readLine(std::string& line) {
    const size_t maxMessageSize = 1024 * 1024;
    while (true) {
        size_t newlinePos = recvBuffer_.find('\n');
        if (newlinePos != std::string::npos) {
            line = recvBuffer_.substr(0, newlinePos);
            recvBuffer_.erase(0, newlinePos + 1);
            return true;
        }

        char chunk[1024];
        int n = recv(socket_, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            if (n < 0 && errno == EINTR) {
                continue;
            }
            return false;
        }

        recvBuffer_.append(chunk, static_cast<size_t>(n));
        if (recvBuffer_.size() > maxMessageSize) {
            return false;
        }
    }
}

std::string MessengerClient::registerUser(const std::string& username, const std::string& password) {
    std::unordered_map<std::string, std::string> params = {
        {"username", username},
        {"password", password}
    };
    std::string cmd = buildCommand("REGISTER", params);
    std::string response = sendCommand(cmd);

    // Parse response: [OK] REGISTER:sessionId=...:userId=...
    if (response.find("[OK]") == 0) {
        size_t sessionPos = response.find("sessionId=");
        size_t userIdPos = response.find("userId=");
        if (sessionPos != std::string::npos && userIdPos != std::string::npos) {
            size_t sessionEnd = response.find(":", sessionPos);
            sessionId_ = response.substr(sessionPos + 10, sessionEnd - (sessionPos + 10));
            userId_ = std::stoi(response.substr(userIdPos + 7));
        }
    }

    return response;
}

std::string MessengerClient::login(const std::string& username, const std::string& password) {
    std::unordered_map<std::string, std::string> params = {
        {"username", username},
        {"password", password}
    };
    std::string cmd = buildCommand("LOGIN", params);
    std::string response = sendCommand(cmd);

    // Parse response: [OK] LOGIN:sessionId=...:userId=...
    if (response.find("[OK]") == 0) {
        size_t sessionPos = response.find("sessionId=");
        size_t userIdPos = response.find("userId=");
        if (sessionPos != std::string::npos && userIdPos != std::string::npos) {
            size_t sessionEnd = response.find(":", sessionPos);
            sessionId_ = response.substr(sessionPos + 10, sessionEnd - (sessionPos + 10));
            userId_ = std::stoi(response.substr(userIdPos + 7));
        }
    }

    return response;
}

std::string MessengerClient::logout() {
    std::unordered_map<std::string, std::string> params = {
        {"sessionId", sessionId_}
    };
    std::string cmd = buildCommand("LOGOUT", params);
    std::string response = sendCommand(cmd);
    sessionId_ = "";
    userId_ = -1;
    return response;
}

std::string MessengerClient::sendMessage(const std::string& to, const std::string& body) {
    std::unordered_map<std::string, std::string> params = {
        {"sessionId", sessionId_},
        {"to", to},
        {"body", body}
    };
    return sendCommand(buildCommand("SEND", params));
}

std::string MessengerClient::getMessages(const std::string& contact, int limit, int offset) {
    std::unordered_map<std::string, std::string> params = {
        {"sessionId", sessionId_},
        {"contact", contact},
        {"limit", std::to_string(limit)},
        {"offset", std::to_string(offset)}
    };
    return sendCommand(buildCommand("GET_MESSAGES", params));
}

std::string MessengerClient::getInbox(int limit, int offset) {
    std::unordered_map<std::string, std::string> params = {
        {"sessionId", sessionId_},
        {"limit", std::to_string(limit)},
        {"offset", std::to_string(offset)}
    };
    return sendCommand(buildCommand("GET_INBOX", params));
}
