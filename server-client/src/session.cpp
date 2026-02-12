#include "session.hpp"
#include <random>
#include <sstream>
#include <iomanip>

std::string SessionManager::generateSessionId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    for (int i = 0; i < 32; i++) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string SessionManager::createSession(int userId, const std::string& username) {
    std::string sessionId = generateSessionId();
    sessions_[sessionId] = new Session(userId, username, sessionId);
    return sessionId;
}

bool SessionManager::verifySession(const std::string& sessionId) {
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return false;
    }
    if (it->second->isExpired()) {
        delete it->second;
        sessions_.erase(it);
        return false;
    }
    return true;
}

Session* SessionManager::getSession(const std::string& sessionId) {
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) {
        return nullptr;
    }
    if (it->second->isExpired()) {
        delete it->second;
        sessions_.erase(it);
        return nullptr;
    }
    return it->second;
}

void SessionManager::removeSession(const std::string& sessionId) {
    auto it = sessions_.find(sessionId);
    if (it != sessions_.end()) {
        delete it->second;
        sessions_.erase(it);
    }
}
