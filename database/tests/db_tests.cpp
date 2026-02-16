#include <iostream>
#include <cstdlib>
#include <string>
#include "postgresql.hpp"

static std::string buildConnStrFromEnv() {
    const char* pgconn = std::getenv("PGCONN");
    if (pgconn && *pgconn) {
        return std::string(pgconn);
    }

    const char* host = std::getenv("PGHOST");
    const char* port = std::getenv("PGPORT");
    const char* dbname = std::getenv("PGDATABASE");
    const char* user = std::getenv("PGUSER");
    const char* password = std::getenv("PGPASSWORD");

    if (!host || !dbname || !user) {
        return {};
    }

    std::string conn = "host=" + std::string(host);
    if (port && *port) conn += " port=" + std::string(port);
    conn += " dbname=" + std::string(dbname);
    conn += " user=" + std::string(user);
    if (password && *password) conn += " password=" + std::string(password);
    return conn;
}

static bool ensure(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[TEST] FAIL: " << message << std::endl;
        return false;
    }
    std::cout << "[TEST] OK: " << message << std::endl;
    return true;
}

int main(int argc, char** argv) {
    std::string connstr;
    if (argc > 1) {
        connstr = argv[1];
    } else {
        connstr = buildConnStrFromEnv();
    }

    if (connstr.empty()) {
        std::cerr << "[TEST] No connection string provided.\n"
                  << "Set PGCONN or PGHOST/PGPORT/PGDATABASE/PGUSER[/PGPASSWORD],\n"
                  << "or pass connection string as argv[1]." << std::endl;
        return 2;
    }

    try {
        PostgresDatabase db(connstr);

        bool allOk = true;

        int userAId = db.createUser("test_user_a");
        int userBId = db.createUser("test_user_b");
        allOk &= ensure(userAId > 0, "createUser(test_user_a)");
        allOk &= ensure(userBId > 0, "createUser(test_user_b)");

        pqxx::result userA = db.getUserByUsername("test_user_a");
        allOk &= ensure(!userA.empty(), "getUserByUsername(test_user_a)");

        int msgId = db.insertMessage(userAId, userBId, "hello from tests");
        allOk &= ensure(msgId > 0, "insertMessage(userA->userB)");

        pqxx::result convo = db.getMessagesBetween(userAId, userBId, 50, 0);
        allOk &= ensure(!convo.empty(), "getMessagesBetween(userA,userB)");

        pqxx::result inbox = db.getInbox(userBId, 50, 0);
        allOk &= ensure(!inbox.empty(), "getInbox(userB)");

        if (allOk) {
            std::cout << "[TEST] All checks passed." << std::endl;
            return 0;
        }

        std::cerr << "[TEST] Some checks failed." << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "[TEST] Exception: " << e.what() << std::endl;
        return 1;
    }
}
