#pragma once

#include <string>
#include <unordered_map>
#include <chrono>

class Session {
public:
    Session(int userId, const std::string& username, const std::string& sessionId)
        : userId_(userId), username_(username), sessionId_(sessionId),
          createdAt_(std::chrono::system_clock::now()) {}

    int getUserId() const { return userId_; }
    const std::string& getUsername() const { return username_; }
    const std::string& getSessionId() const { return sessionId_; }
    
    bool isExpired(int expirySeconds = 3600) const {
        auto now = std::chrono::system_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - createdAt_);
        return duration.count() > expirySeconds;
    }

private:
    int userId_;
    std::string username_;
    std::string sessionId_;
    std::chrono::system_clock::time_point createdAt_;
};

class SessionManager {
public:
    std::string createSession(int userId, const std::string& username);
    bool verifySession(const std::string& sessionId);
    Session* getSession(const std::string& sessionId);
    void removeSession(const std::string& sessionId);

private:
    std::unordered_map<std::string, Session*> sessions_;
    std::string generateSessionId();
};
