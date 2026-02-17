#include <pqxx/pqxx>
#include <iostream>
#include <string>
#include <vector>

class PostgresConnection {
public:
    PostgresConnection(const std::string& connstr) : connection(nullptr) {
        try {
            connection = new pqxx::connection(connstr);
            if (connection->is_open()) {
                std::cout << "[PSQL.Connection] Successfully connected to PostgreSQL database" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Connection] error: " << e.what() << std::endl;
            throw;
        }
    }

    ~PostgresConnection() {
        if (connection) {
            delete connection;
        }
    }

    pqxx::connection* getConnection() const {
        return connection;
    }

    bool isConnected() const {
        return connection && connection->is_open();
    }

private:
    pqxx::connection* connection;
};

class PostgresDatabase {

public:
    PostgresDatabase(const std::string& connstr) : pgConn(connstr) {}

    int createUser(const std::string& username) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "INSERT INTO users (username) VALUES (" + txn.quote(username) + ") RETURNING id"
            );
            txn.commit();

            if (res.empty()) {
                return -1;
            }

            return res[0]["id"].as<int>();
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] createUser error: " << e.what() << std::endl;
            throw;
        }
    }

    pqxx::result getUserByUsername(const std::string& username) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "SELECT id, username FROM users WHERE username = " + txn.quote(username)
            );
            txn.commit();
            return res;
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] getUserByUsername error: " << e.what() << std::endl;
            throw;
        }
    }

    pqxx::result getUserById(int userId) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "SELECT id, username FROM users WHERE id = " + txn.quote(userId)
            );
            txn.commit();
            return res;
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] getUserById error: " << e.what() << std::endl;
            throw;
        }
    }

    int createUserWithPassword(const std::string& username, const std::string& passwordHash) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "INSERT INTO users (username, password_hash) VALUES (" + 
                txn.quote(username) + ", " + txn.quote(passwordHash) + ") RETURNING id"
            );
            txn.commit();

            if (res.empty()) {
                return -1;
            }

            return res[0]["id"].as<int>();
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] createUserWithPassword error: " << e.what() << std::endl;
            throw;
        }
    }

    pqxx::result getUserCredentials(const std::string& username) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "SELECT id, username, password_hash FROM users WHERE username = " + txn.quote(username)
            );
            txn.commit();
            return res;
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] getUserCredentials error: " << e.what() << std::endl;
            throw;
        }
    }

    bool isConnected() const {
        return pgConn.isConnected();
    }

    int insertMessage(int senderId, int receiverId, const std::string& body) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "INSERT INTO messages (sender_id, receiver_id, body, is_read) VALUES (" +
                txn.quote(senderId) + ", " + txn.quote(receiverId) + ", " + txn.quote(body) + ", FALSE) "
                "RETURNING id"
            );
            txn.commit();

            if (res.empty()) {
                return -1;
            }

            return res[0]["id"].as<int>();
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] insertMessage error: " << e.what() << std::endl;
            throw;
        }
    }

    pqxx::result getMessagesBetween(int userA, int userB, int limit = 50, int offset = 0) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "SELECT id, sender_id, receiver_id, body, created_at, is_read "
                "FROM messages "
                "WHERE (sender_id = " + txn.quote(userA) + " AND receiver_id = " + txn.quote(userB) + ") "
                "   OR (sender_id = " + txn.quote(userB) + " AND receiver_id = " + txn.quote(userA) + ") "
                "ORDER BY created_at ASC "
                "LIMIT " + txn.quote(limit) + " OFFSET " + txn.quote(offset)
            );
            txn.commit();
            return res;
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] getMessagesBetween error: " << e.what() << std::endl;
            throw;
        }
    }

    pqxx::result getUserAvatarByUsername(const std::string& username) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "SELECT avatar_b64, avatar_mime FROM users WHERE username = " + txn.quote(username)
            );
            txn.commit();
            return res;
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] getUserAvatarByUsername error: " << e.what() << std::endl;
            throw;
        }
    }

    void setUserAvatar(int userId, const std::string& avatarB64, const std::string& avatarMime) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            txn.exec(
                "UPDATE users SET avatar_b64 = " + txn.quote(avatarB64) + ", "
                "avatar_mime = " + txn.quote(avatarMime) + " WHERE id = " + txn.quote(userId)
            );
            txn.commit();
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] setUserAvatar error: " << e.what() << std::endl;
            throw;
        }
    }

    void markMessagesRead(int receiverId, int senderId) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            txn.exec(
                "UPDATE messages SET is_read = TRUE "
                "WHERE receiver_id = " + txn.quote(receiverId) + " "
                "AND sender_id = " + txn.quote(senderId) + " "
                "AND is_read = FALSE"
            );
            txn.commit();
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] markMessagesRead error: " << e.what() << std::endl;
            throw;
        }
    }

    pqxx::result getInbox(int userId, int limit = 50, int offset = 0) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "SELECT id, sender_id, receiver_id, body, created_at "
                "FROM messages "
                "WHERE receiver_id = " + txn.quote(userId) + " "
                "ORDER BY created_at DESC "
                "LIMIT " + txn.quote(limit) + " OFFSET " + txn.quote(offset)
            );
            txn.commit();
            return res;
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] getInbox error: " << e.what() << std::endl;
            throw;
        }
    }

    int deleteChatMessages(int userId, int contactId) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "DELETE FROM messages "
                "WHERE (sender_id = " + txn.quote(userId) + " AND receiver_id = " + txn.quote(contactId) + ") "
                "   OR (sender_id = " + txn.quote(contactId) + " AND receiver_id = " + txn.quote(userId) + ") "
                "RETURNING id"
            );
            txn.commit();
            return static_cast<int>(res.size());
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] deleteChatMessages error: " << e.what() << std::endl;
            throw;
        }
    }

    pqxx::result getChatsForUser(int userId) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "SELECT DISTINCT u.username "
                "FROM messages m "
                "JOIN users u ON u.id = CASE "
                "  WHEN m.sender_id = " + txn.quote(userId) + " THEN m.receiver_id "
                "  ELSE m.sender_id "
                "END "
                "WHERE m.sender_id = " + txn.quote(userId) + " OR m.receiver_id = " + txn.quote(userId) + " "
                "ORDER BY u.username"
            );
            txn.commit();
            return res;
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] getChatsForUser error: " << e.what() << std::endl;
            throw;
        }
    }

    pqxx::result getChatsWithUnreadCounts(int userId) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "SELECT u.username, "
                "  COALESCE(SUM(CASE WHEN m.receiver_id = " + txn.quote(userId) + " "
                "  AND m.is_read = FALSE THEN 1 ELSE 0 END), 0) AS unread_count "
                "FROM messages m "
                "JOIN users u ON u.id = CASE "
                "  WHEN m.sender_id = " + txn.quote(userId) + " THEN m.receiver_id "
                "  ELSE m.sender_id "
                "END "
                "WHERE m.sender_id = " + txn.quote(userId) + " OR m.receiver_id = " + txn.quote(userId) + " "
                "GROUP BY u.username "
                "ORDER BY u.username"
            );
            txn.commit();
            return res;
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] getChatsWithUnreadCounts error: " << e.what() << std::endl;
            throw;
        }
    }

    std::vector<int> getChatPartnerIds(int userId) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(
                "SELECT DISTINCT CASE "
                "  WHEN sender_id = " + txn.quote(userId) + " THEN receiver_id "
                "  ELSE sender_id "
                "END AS partner_id "
                "FROM messages "
                "WHERE sender_id = " + txn.quote(userId) + " OR receiver_id = " + txn.quote(userId)
            );
            txn.commit();

            std::vector<int> partners;
            partners.reserve(res.size());
            for (auto row : res) {
                partners.push_back(row["partner_id"].as<int>());
            }
            return partners;
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] getChatPartnerIds error: " << e.what() << std::endl;
            throw;
        }
    }

    bool testConnection() {
        if (!pgConn.isConnected()) {
            std::cerr << "[PSQL.Database] Database not connected" << std::endl;
            return false;
        }

        try {
            pqxx::work txn(*pgConn.getConnection());

            pqxx::result insertRes = txn.exec(
                "INSERT INTO mes_db (name) VALUES ('test_row') RETURNING id, name, created_at"
            );

            if (insertRes.empty()) {
                std::cerr << "[PSQL.Database] Insert returned no rows" << std::endl;
                txn.commit();
                return false;
            }

            int insertedId = insertRes[0]["id"].as<int>();

            pqxx::result selectRes = txn.exec(
                "SELECT id, name, created_at FROM mes_db WHERE id = " + txn.quote(insertedId)
            );

            txn.commit();

            if (selectRes.empty()) {
                std::cerr << "[PSQL.Database] Select returned no rows" << std::endl;
                return false;
            }

            std::cout << "[PSQL.Database] Inserted and selected row: "
                      << selectRes[0]["id"].as<int>() << ", "
                      << selectRes[0]["name"].as<std::string>() << ", "
                      << selectRes[0]["created_at"].c_str() << std::endl;

            return true;
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] Test query error: " << e.what() << std::endl;
            return false;
        }
    }

    
private:
    PostgresConnection pgConn;

    pqxx::result* executeQuery(const std::string& query) {
        if (!pgConn.isConnected()) {
            throw std::runtime_error("[PSQL.Database] Database not connected");
        }

        try 
        {
            pqxx::work txn(*pgConn.getConnection());
            pqxx::result res = txn.exec(query);
            txn.commit();
            return new pqxx::result(res);
        } catch (const std::exception& e) {
            std::cerr << "[PSQL.Database] Query error: " << e.what() << std::endl;
            throw;
        }
    }

};
