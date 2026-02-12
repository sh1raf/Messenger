#include "client.hpp"
#include <sstream>
#include <cstring>

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

    std::string msg = cmd + "\n";
    if (send(socket_, msg.c_str(), msg.length(), 0) < 0) {
        std::cerr << "[Client] Failed to send command" << std::endl;
        return "[ERROR] Send failed";
    }

    char buffer[4096] = {0};
    int n = recv(socket_, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        std::cerr << "[Client] Failed to receive response" << std::endl;
        return "[ERROR] Receive failed";
    }

    buffer[n] = '\0';
    std::string response(buffer);
    if (!response.empty() && response.back() == '\n') {
        response.pop_back();
    }
    return response;
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
