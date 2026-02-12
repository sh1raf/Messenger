#include "server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

MessengerServer::MessengerServer(const std::string& dbConnStr, int port)
    : port_(port), serverSocket_(-1), running_(false), db_(dbConnStr) {
    if (!db_.isConnected()) {
        throw std::runtime_error("[Server] Failed to connect to database");
    }
    std::cout << "[Server] Connected to database" << std::endl;
}

MessengerServer::~MessengerServer() {
    stop();
}

void MessengerServer::start() {
    if (running_) return;

    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        throw std::runtime_error("[Server] Failed to create socket");
    }

    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(serverSocket_);
        throw std::runtime_error("[Server] setsockopt failed");
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(serverSocket_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(serverSocket_);
        throw std::runtime_error("[Server] Failed to bind socket");
    }

    if (listen(serverSocket_, 5) < 0) {
        close(serverSocket_);
        throw std::runtime_error("[Server] Failed to listen on socket");
    }

    running_ = true;
    acceptThread_ = std::thread(&MessengerServer::acceptConnections, this);
    std::cout << "[Server] Started on port " << port_ << std::endl;
}

void MessengerServer::stop() {
    if (!running_) return;

    running_ = false;
    if (serverSocket_ >= 0) {
        shutdown(serverSocket_, SHUT_RDWR);
        close(serverSocket_);
        serverSocket_ = -1;
    }

    if (acceptThread_.joinable()) {
        acceptThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(threadsMutex_);
        for (auto& t : clientThreads_) {
            if (t.joinable()) {
                t.join();
            }
        }
        clientThreads_.clear();
    }

    std::cout << "[Server] Stopped" << std::endl;
}

bool MessengerServer::isRunning() const {
    return running_;
}

void MessengerServer::acceptConnections() {
    while (running_) {
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            if (running_) {
                std::cerr << "[Server] accept() failed" << std::endl;
            }
            continue;
        }

        std::cout << "[Server] New client connection from " 
                  << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << std::endl;

        {
            std::lock_guard<std::mutex> lock(threadsMutex_);
            clientThreads_.emplace_back(&MessengerServer::handleClient, this, clientSocket);
        }
    }
}

void MessengerServer::handleClient(int clientSocket) {
    try {
        while (running_) {
            std::string request = receiveMessage(clientSocket);
            if (request.empty()) {
                break;
            }

            std::cout << "[Server] Received: " << request << std::endl;

            std::string cmd;
            std::unordered_map<std::string, std::string> params;
            parseCommand(request, cmd, params);

            std::string response = "[ERROR] Unknown command";

            if (cmd == "REGISTER") {
                response = handleRegister(params["username"], params["password"]);
            } else if (cmd == "LOGIN") {
                response = handleLogin(params["username"], params["password"]);
            } else if (cmd == "LOGOUT") {
                response = handleLogout(params["sessionId"]);
            } else if (cmd == "SEND") {
                response = handleSendMessage(params["sessionId"], params["to"], params["body"]);
            } else if (cmd == "GET_MESSAGES") {
                response = handleGetMessages(params["sessionId"], params["contact"]);
            } else if (cmd == "GET_CHATS") {
                response = handleGetChats(params["sessionId"]);
            } else if (cmd == "GET_PROFILE") {
                response = handleGetProfile(params["username"]);
            } else if (cmd == "SET_AVATAR") {
                response = handleSetAvatar(params["sessionId"], params["data"], params["mime"]);
            } else if (cmd == "GET_INBOX") {
                response = handleGetInbox(params["sessionId"]);
            }

            sendMessage(clientSocket, response);
        }
    } catch (const std::exception& e) {
        std::cerr << "[Server] Client error: " << e.what() << std::endl;
    }

    close(clientSocket);
}

void MessengerServer::sendMessage(int sock, const std::string& response) {
    std::string msg = response + "\n";
    if (send(sock, msg.c_str(), msg.length(), 0) < 0) {
        throw std::runtime_error("Failed to send message");
    }
}

std::string MessengerServer::receiveMessage(int sock) {
    char buffer[1024] = {0};
    int n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        return "";
    }
    buffer[n] = '\0';
    std::string msg(buffer);
    // Remove trailing newline if present
    if (!msg.empty() && msg.back() == '\n') {
        msg.pop_back();
    }
    return msg;
}

std::string MessengerServer::handleRegister(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) {
        return "[ERROR] Username and password required";
    }

    try {
        pqxx::result existing = db_.getUserByUsername(username);
        if (!existing.empty()) {
            return "[ERROR] User already exists";
        }

        std::string passwordHash = PasswordHash::hash(password);
        int userId = db_.createUserWithPassword(username, passwordHash);

        if (userId <= 0) {
            return "[ERROR] Failed to create user";
        }

        std::string sessionId = sessionMgr_.createSession(userId, username);
        return "[OK] REGISTER:sessionId=" + sessionId + ":userId=" + std::to_string(userId);
    } catch (const std::exception& e) {
        return "[ERROR] " + std::string(e.what());
    }
}

std::string MessengerServer::handleLogin(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) {
        return "[ERROR] Username and password required";
    }

    try {
        pqxx::result res = db_.getUserCredentials(username);
        if (res.empty()) {
            return "[ERROR] User not found";
        }

        int userId = res[0]["id"].as<int>();
        std::string storedHash = res[0]["password_hash"].as<std::string>();

        if (!PasswordHash::verify(password, storedHash)) {
            return "[ERROR] Invalid password";
        }

        std::string sessionId = sessionMgr_.createSession(userId, username);
        return "[OK] LOGIN:sessionId=" + sessionId + ":userId=" + std::to_string(userId);
    } catch (const std::exception& e) {
        return "[ERROR] " + std::string(e.what());
    }
}

std::string MessengerServer::handleLogout(const std::string& sessionId) {
    sessionMgr_.removeSession(sessionId);
    return "[OK] LOGOUT";
}

std::string MessengerServer::handleSendMessage(const std::string& sessionId, const std::string& receiverUsername, const std::string& body) {
    Session* session = sessionMgr_.getSession(sessionId);
    if (!session) {
        return "[ERROR] Invalid session";
    }

    int senderId = session->getUserId();
    try {
        pqxx::result receiverRes = db_.getUserByUsername(receiverUsername);
        if (receiverRes.empty()) {
            return "[ERROR] User not found";
        }
        int receiverId = receiverRes[0]["id"].as<int>();
        int msgId = db_.insertMessage(senderId, receiverId, body);
        return "[OK] MessageSent:" + std::to_string(msgId);
    } catch (const std::exception& e) {
        return "[ERROR] " + std::string(e.what());
    }
}

std::string MessengerServer::handleGetMessages(const std::string& sessionId, const std::string& contactUsername, int limit, int offset) {
    Session* session = sessionMgr_.getSession(sessionId);
    if (!session) {
        return "[ERROR] Invalid session";
    }

    int userId = session->getUserId();
    try {
        pqxx::result contactRes = db_.getUserByUsername(contactUsername);
        if (contactRes.empty()) {
            return "[ERROR] User not found";
        }
        int contactId = contactRes[0]["id"].as<int>();
        pqxx::result msgs = db_.getMessagesBetween(userId, contactId, limit, offset);
        db_.markMessagesRead(userId, contactId);
        
        std::string response = "[OK] Messages:";
        for (auto row : msgs) {
            std::string isRead = row["is_read"].as<bool>() ? "1" : "0";
            response += "|" + std::to_string(row["id"].as<int>()) + ":" + 
                       std::to_string(row["sender_id"].as<int>()) + ":" + 
                       isRead + ":" +
                       row["body"].as<std::string>();
        }
        return response;
    } catch (const std::exception& e) {
        return "[ERROR] " + std::string(e.what());
    }
}

std::string MessengerServer::handleSearchUsers(const std::string& query) {
    // TODO: Implement fuzzy search
    return "[OK] Users:";
}

std::string MessengerServer::handleGetChats(const std::string& sessionId) {
    Session* session = sessionMgr_.getSession(sessionId);
    if (!session) {
        return "[ERROR] Invalid session";
    }

    int userId = session->getUserId();
    try {
        pqxx::result res = db_.getChatsWithUnreadCounts(userId);
        std::string response = "[OK] Chats:";
        for (auto row : res) {
            response += "|" + row["username"].as<std::string>() + ":" + row["unread_count"].as<std::string>();
        }
        return response;
    } catch (const std::exception& e) {
        return "[ERROR] " + std::string(e.what());
    }
}

std::string MessengerServer::handleGetProfile(const std::string& username) {
    if (username.empty()) {
        return "[ERROR] Username required";
    }

    try {
        pqxx::result res = db_.getUserAvatarByUsername(username);
        if (res.empty()) {
            return "[ERROR] User not found";
        }

        std::string avatarB64 = res[0]["avatar_b64"].is_null() ? "" : res[0]["avatar_b64"].as<std::string>();
        std::string avatarMime = res[0]["avatar_mime"].is_null() ? "" : res[0]["avatar_mime"].as<std::string>();

        return "[OK] Profile:username=" + username + ":avatar_b64=" + avatarB64 + ":mime=" + avatarMime;
    } catch (const std::exception& e) {
        return "[ERROR] " + std::string(e.what());
    }
}

std::string MessengerServer::handleSetAvatar(const std::string& sessionId, const std::string& avatarB64, const std::string& avatarMime) {
    Session* session = sessionMgr_.getSession(sessionId);
    if (!session) {
        return "[ERROR] Invalid session";
    }

    if (avatarB64.empty() || avatarMime.empty()) {
        return "[ERROR] Avatar data required";
    }

    try {
        db_.setUserAvatar(session->getUserId(), avatarB64, avatarMime);
        return "[OK] AvatarUpdated";
    } catch (const std::exception& e) {
        return "[ERROR] " + std::string(e.what());
    }
}

std::string MessengerServer::handleGetInbox(const std::string& sessionId, int limit, int offset) {
    Session* session = sessionMgr_.getSession(sessionId);
    if (!session) {
        return "[ERROR] Invalid session";
    }

    int userId = session->getUserId();
    try {
        pqxx::result msgs = db_.getInbox(userId, limit, offset);
        std::string response = "[OK] Inbox:";
        for (auto row : msgs) {
            response += "|" + std::to_string(row["id"].as<int>()) + ":" + 
                       std::to_string(row["sender_id"].as<int>()) + ":" + 
                       row["body"].as<std::string>();
        }
        return response;
    } catch (const std::exception& e) {
        return "[ERROR] " + std::string(e.what());
    }
}

bool MessengerServer::parseCommand(const std::string& data, std::string& cmd, 
                                    std::unordered_map<std::string, std::string>& params) {
    std::istringstream iss(data);
    iss >> cmd;
    
    std::string pair;
    while (iss >> pair) {
        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos) {
            std::string key = pair.substr(0, eqPos);
            std::string value = pair.substr(eqPos + 1);

            if (key == "body") {
                std::string rest;
                std::getline(iss, rest);
                if (!rest.empty() && rest.front() == ' ') {
                    rest.erase(0, 1);
                }
                if (!rest.empty()) {
                    value += " " + rest;
                }
            }

            params[key] = value;
        }
    }
    return true;
}
