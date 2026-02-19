#pragma once

#include <string>
#include <unordered_map>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class MessengerClient {
public:
    MessengerClient(const std::string& host, int port);
    ~MessengerClient();

    bool connect();
    void disconnect();
    bool isConnected() const;

    // Auth
    std::string registerUser(const std::string& username, const std::string& password);
    std::string login(const std::string& username, const std::string& password);
    std::string logout();

    // Messages
    std::string sendMessage(const std::string& to, const std::string& body);
    std::string getMessages(const std::string& contact, int limit = 50, int offset = 0);
    std::string getInbox(int limit = 20, int offset = 0);

    // Session
    void setSessionId(const std::string& sessionId) { sessionId_ = sessionId; }
    const std::string& getSessionId() const { return sessionId_; }
    int getUserId() const { return userId_; }

private:
    std::string host_;
    int port_;
    int socket_;
    std::string sessionId_;
    int userId_;
    std::string recvBuffer_;

    std::string sendCommand(const std::string& cmd);
    std::string buildCommand(const std::string& cmd, const std::unordered_map<std::string, std::string>& params);
    bool sendAll(const std::string& data);
    bool readLine(std::string& line);
};
