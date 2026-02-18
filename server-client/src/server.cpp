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
    bool subscribed = false;
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
            } else if (cmd == "SEND_E2E") {
                response = handleSendMessageE2e(params["sessionId"], params["to"], params["body"], params["e2e"], params["e2e_pub"]);
            } else if (cmd == "GET_MESSAGES") {
                response = handleGetMessages(params["sessionId"], params["contact"]);
            } else if (cmd == "GET_MESSAGES_E2E") {
                response = handleGetMessages(params["sessionId"], params["contact"]);
            } else if (cmd == "GET_CHATS") {
                response = handleGetChats(params["sessionId"]);
            } else if (cmd == "GET_PROFILE") {
                response = handleGetProfile(params["username"]);
            } else if (cmd == "SET_AVATAR") {
                response = handleSetAvatar(params["sessionId"], params["data"], params["mime"]);
            } else if (cmd == "GET_INBOX") {
                response = handleGetInbox(params["sessionId"]);
            } else if (cmd == "DELETE_CHAT") {
                response = handleDeleteChat(params["sessionId"], params["contact"]);
            } else if (cmd == "SUBSCRIBE") {
                response = handleSubscribe(params["sessionId"], clientSocket);
                if (response.rfind("[OK]", 0) == 0) {
                    subscribed = true;
                }
            }

            sendMessage(clientSocket, response);
        }
    } catch (const std::exception& e) {
        std::cerr << "[Server] Client error: " << e.what() << std::endl;
    }

    if (subscribed) {
        unregisterSubscriber(clientSocket);
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
    std::string senderUsername = session->getUsername();
    try {
        pqxx::result receiverRes = db_.getUserByUsername(receiverUsername);
        if (receiverRes.empty()) {
            return "[ERROR] User not found";
        }
        int receiverId = receiverRes[0]["id"].as<int>();
        int msgId = db_.insertMessage(senderId, receiverId, body);

        const std::string event = "[EVENT] MESSAGE:from=" + senderUsername + 
                                  ":to=" + receiverUsername + ":body=" + body;
        notifyUsers({senderId, receiverId}, event);
        return "[OK] MessageSent:" + std::to_string(msgId);
    } catch (const std::exception& e) {
        return "[ERROR] " + std::string(e.what());
    }
}

std::string MessengerServer::handleSendMessageE2e(
    const std::string& sessionId,
    const std::string& receiverUsername,
    const std::string& body,
    const std::string& e2ePayload,
    const std::string& e2ePub
) {
    Session* session = sessionMgr_.getSession(sessionId);
    if (!session) {
        return "[ERROR] Invalid session";
    }

    int senderId = session->getUserId();
    std::string senderUsername = session->getUsername();
    try {
        pqxx::result receiverRes = db_.getUserByUsername(receiverUsername);
        if (receiverRes.empty()) {
            return "[ERROR] User not found";
        }
        int receiverId = receiverRes[0]["id"].as<int>();
        int msgId = db_.insertMessageE2e(senderId, receiverId, body, e2ePayload, e2ePub);

        const std::string event = "[EVENT] MESSAGE:from=" + senderUsername +
                                  ":to=" + receiverUsername + ":body=" + body;
        notifyUsers({senderId, receiverId}, event);
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
        
        // Combined operation: get messages and mark as read in single transaction
        pqxx::result msgs = db_.getMessagesAndMarkRead(userId, contactId, limit, offset);
        
        std::string response = "[OK] Messages:";
        for (auto row : msgs) {
            std::string isRead = row["is_read"].as<bool>() ? "1" : "0";
            std::string e2ePayload = row["e2e_payload"].is_null() ? "" : row["e2e_payload"].as<std::string>();
            std::string e2ePub = row["e2e_pub"].is_null() ? "" : row["e2e_pub"].as<std::string>();
            response += "|" + std::to_string(row["id"].as<int>()) + ":" + 
                       std::to_string(row["sender_id"].as<int>()) + ":" + 
                       isRead + ":" +
                       row["body"].as<std::string>() + ":" + e2ePayload + ":" + e2ePub;
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
        const std::string username = session->getUsername();
        std::vector<int> partners = db_.getChatPartnerIds(session->getUserId());
        partners.push_back(session->getUserId());
        const std::string event = "[EVENT] AVATAR:username=" + username;
        notifyUsers(partners, event);
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

std::string MessengerServer::handleSubscribe(const std::string& sessionId, int clientSocket) {
    Session* session = sessionMgr_.getSession(sessionId);
    if (!session) {
        return "[ERROR] Invalid session";
    }

    registerSubscriber(clientSocket, session->getUserId());
    return "[OK] SUBSCRIBED";
}

void MessengerServer::registerSubscriber(int clientSocket, int userId) {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    socketToUser_[clientSocket] = userId;
    userToSockets_[userId].insert(clientSocket);
}

void MessengerServer::unregisterSubscriber(int clientSocket) {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    auto it = socketToUser_.find(clientSocket);
    if (it == socketToUser_.end()) return;
    int userId = it->second;
    socketToUser_.erase(it);

    auto userIt = userToSockets_.find(userId);
    if (userIt != userToSockets_.end()) {
        userIt->second.erase(clientSocket);
        if (userIt->second.empty()) {
            userToSockets_.erase(userIt);
        }
    }
}

void MessengerServer::notifyUsers(const std::vector<int>& userIds, const std::string& payload) {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    std::vector<int> socketsToRemove;

    for (int userId : userIds) {
        auto it = userToSockets_.find(userId);
        if (it == userToSockets_.end()) continue;

        for (int sock : it->second) {
            std::string msg = payload + "\n";
            if (send(sock, msg.c_str(), msg.length(), 0) < 0) {
                socketsToRemove.push_back(sock);
            }
        }
    }

    for (int sock : socketsToRemove) {
        auto it = socketToUser_.find(sock);
        if (it == socketToUser_.end()) continue;
        int userId = it->second;
        socketToUser_.erase(it);

        auto userIt = userToSockets_.find(userId);
        if (userIt != userToSockets_.end()) {
            userIt->second.erase(sock);
            if (userIt->second.empty()) {
                userToSockets_.erase(userIt);
            }
        }
    }
}

std::string MessengerServer::handleDeleteChat(const std::string& sessionId, const std::string& contactUsername) {
    Session* session = sessionMgr_.getSession(sessionId);
    if (!session) {
        return "[ERROR] Invalid session";
    }

    if (contactUsername.empty()) {
        return "[ERROR] Contact username required";
    }

    int userId = session->getUserId();
    try {
        pqxx::result contactRes = db_.getUserByUsername(contactUsername);
        if (contactRes.empty()) {
            return "[ERROR] User not found";
        }
        int contactId = contactRes[0]["id"].as<int>();
        int removed = db_.deleteChatMessages(userId, contactId);
        return "[OK] ChatDeleted:count=" + std::to_string(removed);
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
