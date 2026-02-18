#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "postgresql.hpp"
#include "password_hash.hpp"
#include "session.hpp"

class MessengerServer {
public:
    MessengerServer(const std::string& dbConnStr, int port = 5555);
    ~MessengerServer();

    void start();
    void stop();
    bool isRunning() const;

private:
    int port_;
    int serverSocket_;
    bool running_;
    std::thread acceptThread_;
    std::vector<std::thread> clientThreads_;
    std::mutex threadsMutex_;
    std::mutex subscribersMutex_;

    PostgresDatabase db_;
    SessionManager sessionMgr_;

    std::unordered_map<int, int> socketToUser_;
    std::unordered_map<int, std::unordered_set<int>> userToSockets_;

    void acceptConnections();
    void handleClient(int clientSocket);
    void sendMessage(int sock, const std::string& response);
    std::string receiveMessage(int sock);
    
    // Protocol handlers
    std::string handleRegister(const std::string& username, const std::string& password);
    std::string handleLogin(const std::string& username, const std::string& password);
    std::string handleLogout(const std::string& sessionId);
    std::string handleSendMessage(const std::string& sessionId, const std::string& receiverUsername, const std::string& body);
    std::string handleSendMessageE2e(const std::string& sessionId, const std::string& receiverUsername, const std::string& body, const std::string& e2ePayload, const std::string& e2ePub);
    std::string handleGetMessages(const std::string& sessionId, const std::string& contactUsername, int limit = 50, int offset = 0);
    std::string handleSearchUsers(const std::string& query);
    std::string handleGetChats(const std::string& sessionId);
    std::string handleGetProfile(const std::string& username);
    std::string handleSetAvatar(const std::string& sessionId, const std::string& avatarB64, const std::string& avatarMime);
    std::string handleGetInbox(const std::string& sessionId, int limit = 20, int offset = 0);
    std::string handleDeleteChat(const std::string& sessionId, const std::string& contactUsername);
    std::string handleSubscribe(const std::string& sessionId, int clientSocket);

    void registerSubscriber(int clientSocket, int userId);
    void unregisterSubscriber(int clientSocket);
    void notifyUsers(const std::vector<int>& userIds, const std::string& payload);

    // Helper
    bool parseCommand(const std::string& data, std::string& cmd, std::unordered_map<std::string, std::string>& params);
};
