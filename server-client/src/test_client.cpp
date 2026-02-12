#include <iostream>
#include <chrono>
#include <thread>
#include "client.hpp"

int main() {
    std::cout << "=== Messenger Client Test ===" << std::endl;

    MessengerClient client("127.0.0.1", 5555);

    if (!client.connect()) {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    // Test 1: Register
    std::cout << "\n[Test 1] Register user alice" << std::endl;
    std::string regResp = client.registerUser("alice", "password123");
    std::cout << "Response: " << regResp << std::endl;
    if (regResp.find("[OK]") != 0) {
        std::cerr << "Register failed!" << std::endl;
        client.disconnect();
        return 1;
    }

    // Test 2: Register another user
    std::cout << "\n[Test 2] Register user bob" << std::endl;
    MessengerClient client2("127.0.0.1", 5555);
    if (!client2.connect()) {
        std::cerr << "Failed to connect for client2" << std::endl;
        return 1;
    }
    std::string regResp2 = client2.registerUser("bob", "password456");
    std::cout << "Response: " << regResp2 << std::endl;

    // Test 3: Login
    std::cout << "\n[Test 3] Login alice" << std::endl;
    client.disconnect();
    if (!client.connect()) {
        std::cerr << "Failed to reconnect" << std::endl;
        return 1;
    }
    std::string loginResp = client.login("alice", "password123");
    std::cout << "Response: " << loginResp << std::endl;
    std::cout << "SessionId: " << client.getSessionId() << ", UserId: " << client.getUserId() << std::endl;

    // Test 4: Send message
    std::cout << "\n[Test 4] Alice sends message to bob" << std::endl;
    std::string sendResp = client.sendMessage("bob", "Hello Bob!");
    std::cout << "Response: " << sendResp << std::endl;

    // Test 5: Get inbox (bob's side)
    std::cout << "\n[Test 5] Bob gets inbox" << std::endl;
    std::string inboxResp = client2.getInbox(20, 0);
    std::cout << "Response: " << inboxResp << std::endl;

    // Test 6: Get messages between alice and bob
    std::cout << "\n[Test 6] Alice gets messages with bob" << std::endl;
    std::string msgsResp = client.getMessages("bob", 50, 0);
    std::cout << "Response: " << msgsResp << std::endl;

    // Test 7: Logout
    std::cout << "\n[Test 7] Alice logout" << std::endl;
    std::string logoutResp = client.logout();
    std::cout << "Response: " << logoutResp << std::endl;

    client.disconnect();
    client2.disconnect();

    std::cout << "\n=== All tests completed ===" << std::endl;
    return 0;
}
