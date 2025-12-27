#pragma once 

#include <pqxx/pqxx>
#include <optional>
#include <string>
#include <vector>
#include <memory>

// User data structure
struct User{
    int id; 
    std::string username;
    std::string email;
    std::string password_hash;
    std::string created_at;
    std::string updated_at;
    std::string last_login;
    bool is_active;
};

// Room data structure
struct Room{
    int id;
    std::string name;
    std::string description;
    int created_by;
    std::string created_at;
    bool is_private;
};

// Message data structure
struct Message{
    int id;
    int room_id;
    int user_id;
    std::string content;
    std::string message_type;
    std::string created_at;
    std::string edited_at;
    bool is_deleted;
};


// Database access layer for user management
class Database {
    public: 
        explicit Database(const std::string& connectionString);
        ~Database();

        // Prevent copying
        Database(const Database&) = delete;
        Database& operator=(const Database&) = delete;

        // Allow moving
        Database(Database&&) = default;
        Database& operator=(Database&&) = default;
 
        // Connection management
        bool connect();
        void disconnect();
        bool isConnected() const;

        // ========== USER OPERATIONS ===========

        // CRUD operations
        std::optional<User> createUser(const User& user);
        bool updateUser(const User& user);
        bool deleteUser(int id);

        // Helper methods
        bool updateLastLogin(int id);
        bool setUserActive(int id, bool active);

        // Query methods
        std::optional<User> getUserByUsername(const std::string& username) const;
        std::optional<User> getUserById(int id) const;
        std::optional<User> getUserByEmail(const std::string& email) const;
        std::vector<User> getAllUsers() const;

        // ========== ROOM OPERATIONS ===========

        // CRUD operations
        std::optional<Room> createRoom(const std::string& name, const std::string& description, int created_by, bool is_private = false);
        bool updateRoom(int id, const std::string& name, const std::string& description);
        bool deleteRoom(int id);

        // Query methods
        std::optional<Room> getRoomById(int id) const;
        std::optional<Room> getRoomByName(const std::string& name) const;
        std::vector<Room> getAllRooms() const;
        std::vector<Room> getPublicRooms() const;
        std::vector<Room> getRoomsByUser(int user_id) const;

         // ========== ROOM MEMBER OPERATIONS ===========

        bool addUserToRoom(int user_id, int room_id, const std::string& role = "member");
        bool removeUserFromRoom(int user_id, int room_id);
        std::vector<User> getRoomMembers(int room_id) const;
        bool isUserInRoom(int user_id, int room_id) const;

        // ========== MESSAGE OPERATIONS ===========

        // CRUD operations
        std::optional<Message> createMessage(int room_id, int user_id, const std::string& content, const std::string& message_type = "text");
        bool updateMessage(int id, const std::string& content);
        bool deleteMessage(int id);

        // Query methods
        std::optional<Message> getMessageById(int id) const;
        std::vector<Message> getMessagesByRoom(int room_id, int limit = 50, int offset = 0) const;
        int getMessageCountInRoom(int room_id) const;

    private:
        std::unique_ptr<pqxx::connection> conn_;
        std::string connectionString_;
        bool connected_;

        // Helper function
        User rowToUser(const pqxx::row& row) const;
        Room rowToRoom(const pqxx::row& row) const;
        Message rowToMessage(const pqxx:: row& row) const;
};