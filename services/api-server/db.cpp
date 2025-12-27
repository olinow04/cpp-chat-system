#include "db.h"
#include <iostream>

Database::Database(const std::string& connectionString)
    : connectionString_(connectionString), connected_(false) {}

Database::~Database() {
    disconnect();
}

// Connection management

bool Database::connect() {
    try {
        // Create PostgreSQL database connection
        conn_ = std::make_unique<pqxx::connection>(connectionString_);
        // Check if connection was successfully opened
        connected_ = conn_->is_open();
        if(connected_){
            std::cout << "Connected to database : " << conn_->dbname() << std::endl;
        }
        return connected_;
    } catch (const std::exception& e) {
        std::cerr << "Database connection error: " << e.what() << std::endl;
        connected_ = false;
        return false;
    }
}

void Database::disconnect() {
    conn_.reset();
    connected_ = false;
}

bool Database::isConnected() const {
    return connected_;
}

// ========== USER OPERATIONS ===========

User Database::rowToUser(const pqxx::row& row) const {
    return User{
        // Convert PostgreSQL row values to C++ types
        row["id"].as<int>(),
        row["username"].as<std::string>(),
        row["email"].as<std::string>(),
        row["password_hash"].as<std::string>(),
        row["created_at"].as<std::string>(),
        // Handle NULL values - convert to empty string
        row["updated_at"].is_null() ? "" : row["updated_at"].as<std::string>(),
        row["last_login"].is_null() ? "" : row["last_login"].as<std::string>(),
        row["is_active"].as<bool>()
    };
}

std::optional<User> Database::createUser(const User& user) {
    if(!connected_) return std::nullopt;
    try {
        // Begin transaction for data write
        pqxx::work txn(*conn_);
        // Execute parameterized query 
        pqxx::result r = txn.exec(
            "INSERT INTO users (username, email, password_hash, is_active) "
            "VALUES ($1, $2, $3, $4)"
            " RETURNING *",
            pqxx::params(user.username, user.email, user.password_hash, user.is_active)
        );
        // Commit transaction
        txn.commit();
        if(!r.empty()){
            std::cout << "User created: " << user.username << std::endl;
            return rowToUser(r[0]);
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Create user error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool Database::updateUser(const User& user) {
    if(!connected_) return false;
    try {
        // User update transaction
        pqxx::work txn(*conn_);
        // Handle NULL for last_login if string is empty
        if (user.last_login.empty()) {
            txn.exec(
                "UPDATE users SET email=$1, password_hash=$2, "
                "is_active=$3, updated_at=CURRENT_TIMESTAMP "
                "WHERE id=$4",
                pqxx::params(user.email, user.password_hash, user.is_active, user.id)
            );
        } else {
            txn.exec(
                "UPDATE users SET email=$1, password_hash=$2, "
                "last_login=$3, is_active=$4, updated_at=CURRENT_TIMESTAMP "
                "WHERE id=$5",
                pqxx::params(user.email, user.password_hash, user.last_login, user.is_active, user.id)
            );
        }
        txn.commit();
        std::cout << "User updated: " << user.id << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Update user error: " << e.what() << std::endl;
        return false;
    }
}

bool Database::updateLastLogin(int id) {
    if(!connected_) return false;
    try {
        pqxx::work txn(*conn_);
        // Execute UPDATE with parameter - CURRENT_TIMESTAMP function on PostgreSQL side
        txn.exec(
            "UPDATE users SET last_login=CURRENT_TIMESTAMP WHERE id=$1",
            pqxx::params(id)
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Update last login error: " << e.what() << std::endl;
        return false;
    }
}

bool Database::setUserActive(int id, bool active) {
    if(!connected_) return false;
    try {
        pqxx::work txn(*conn_);
        // Parameterized UPDATE query 
        txn.exec(
            "UPDATE users SET is_active=$1 WHERE id=$2",
            pqxx::params(active, id)
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Set user active error: " << e.what() << std::endl;
        return false;
    }
}

bool Database::deleteUser(int id) {
    if(!connected_) return false;
    try {
        pqxx::work txn(*conn_);
        // DELETE with parameter 
        txn.exec("DELETE FROM users WHERE id=$1", pqxx::params(id));
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Delete user error: " << e.what() << std::endl;
        return false;
    }
}

std::optional<User> Database::getUserByUsername(const std::string& username) const {
    if(!connected_) return std::nullopt;
    try {
        // Read-only transaction
        pqxx::work txn(*conn_);
        // Execute SELECT with parameter
        pqxx::result r = txn.exec("SELECT * FROM users WHERE username=$1", pqxx::params(username));
        // Check if result contains any rows
        if(!r.empty()) {
            return rowToUser(r[0]);
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Get user by username error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<User> Database::getUserById(int id) const {
    if(!connected_) return std::nullopt;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec("SELECT * FROM users WHERE id=$1", pqxx::params(id));
        if(!r.empty()) {
            return rowToUser(r[0]);
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Get user by ID error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<User> Database::getUserByEmail(const std::string& email) const {
    if(!connected_) return std::nullopt;
    try {
        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec("SELECT * FROM users WHERE email=$1", pqxx::params(email));
        if(!r.empty()) {
            return rowToUser(r[0]);
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Get user by email error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<User> Database::getAllUsers() const {
    std::vector<User> users;
    if(!connected_) return users;
    try {
        pqxx::work txn(*conn_);
        // SELECT without parameters - fetch all records
        pqxx::result r = txn.exec("SELECT * FROM users");
        // Iterate through result - pqxx::result works like a container
        for(const auto& row : r) {
            users.push_back(rowToUser(row));
        }
    } catch (const std::exception& e) {
        std::cerr << "Get all users error: " << e.what() << std::endl;
    }
    return users;
}

// ========== ROOM OPERATIONS ===========

Room Database::rowToRoom(const pqxx::row& row) const {
    // Convert PostgreSQL row to Room struct
    // Handle NULL values for description and created_by fields
    return Room{
        row["id"].as<int>(),
        row["name"].as<std::string>(),
        row["description"].is_null() ? "" : row["description"].as<std::string>(),
        row["created_by"].is_null() ? 0 : row["created_by"].as<int>(),
        row["created_at"].as<std::string>(),
        row["is_private"].as<bool>()
    };
}

std::optional<Room> Database::createRoom(const std::string& name, const std::string& description, int created_by, bool is_private){
    if(!connected_) return std::nullopt;
    try {
        // Begin transaction for room creation
        pqxx::work txn(*conn_);
        // Execute parameterized INSERT query with RETURNING clause
        pqxx::result r = txn.exec(
            "INSERT INTO rooms (name, description, created_by, is_private) "
            "VALUES ($1, $2, $3, $4) RETURNING *",
            pqxx::params(name, description, created_by, is_private)
        );
        // Commit transaction
        txn.commit();

        if(!r.empty()) {
            std::cout << "Room created: " << name << std::endl;
            return rowToRoom(r[0]);
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Create room error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool Database::updateRoom(int id, const std::string& name, const std::string& description){
    if(!connected_) return false;
    try {
        // Room update transaction
        pqxx::work txn(*conn_);
        // Execute UPDATE with parameters
        txn.exec(
            "UPDATE rooms SET name=$1, description=$2 WHERE id=$3",
            pqxx::params(name, description, id)
        );
        txn.commit();
        std::cout << "Room updated: " << id << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Update room error: " << e.what() << std::endl;
        return false;
    }
}

bool Database::deleteRoom(int id){
    if(!connected_) return false;
    try {
        pqxx::work txn(*conn_);
        // DELETE room with parameterized query
        txn.exec("DELETE FROM rooms WHERE id=$1", pqxx::params(id));
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Delete room error: " << e.what() << std::endl;
        return false;
    }
}

std::optional<Room> Database::getRoomByName(const std::string& name) const{
    if(!connected_) return std::nullopt;
    try {
        // Read-only transaction
        pqxx::work txn(*conn_);
        // Execute SELECT with room name parameter
        pqxx::result r = txn.exec("SELECT * FROM rooms WHERE name=$1", pqxx::params(name));
        if(!r.empty()) {
            return rowToRoom(r[0]);
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Get room by name error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<Room> Database::getRoomById(int id) const{
    if(!connected_) return std::nullopt;
    try {
        // Read-only transaction
        pqxx::work txn(*conn_);
        // Execute SELECT with room id parameter
        pqxx::result r = txn.exec("SELECT * FROM rooms WHERE id=$1", pqxx::params(id));
        if(!r.empty()) {
            return rowToRoom(r[0]);
        }
        return std::nullopt;
    } catch (const std::exception& e) {
        std::cerr << "Get room by id error: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::vector<Room> Database::getAllRooms() const{
    std::vector<Room> rooms;
    if(!connected_) return rooms;
    try {
        pqxx::work txn(*conn_);
        // Fetch all rooms ordered by creation date (newest first)
        pqxx::result r = txn.exec("SELECT * FROM rooms ORDER BY created_at DESC");
        // Iterate through result set and convert each row
        for(const auto& row : r){
            rooms.push_back(rowToRoom(row));
        }
    } catch (const std::exception& e) {
        std::cerr << "Get all rooms error: " << e.what() << std::endl;
    }
    return rooms;
}

std::vector<Room> Database::getPublicRooms() const{
    std::vector<Room> rooms;
    if(!connected_) return rooms;
    try {
        pqxx::work txn(*conn_);
        // Fetch only public rooms (is_private=false) ordered by creation date
        pqxx::result r = txn.exec("SELECT * FROM rooms WHERE is_private=false ORDER BY created_at DESC");
        // Iterate through result set and convert each row
        for(const auto& row : r){
            rooms.push_back(rowToRoom(row));
        }
    } catch (const std::exception& e) {
        std::cerr << "Get public rooms error: " << e.what() << std::endl;
    }
    return rooms;
}

std::vector<Room> Database::getRoomsByUser(int user_id) const{
    std::vector<Room> rooms;
    if(!connected_) return rooms;
    try {
        // Read-only transaction
        pqxx::work txn(*conn_);
        // Fetch all rooms where user is a member
        // JOIN with room_members to find user's rooms, ordered by newest first
        pqxx::result r = txn.exec(
            "SELECT r.* FROM rooms r "
            "JOIN room_members rm ON r.id = rm.room_id "
            "WHERE rm.user_id = $1 "
            "ORDER BY r.created_at DESC",
            pqxx::params(user_id)
        );
        // Convert each room row to Room object
        for(const auto& row : r){
            rooms.push_back(rowToRoom(row));
        }
    } catch (const std::exception& e) {
        std::cerr << "Get rooms by user error: " << e.what() << std::endl;
    }
    return rooms;
}

// ========== ROOM MEMBER OPERATIONS ===========

bool Database::addUserToRoom(int user_id, int room_id, const std::string& role){
    if(!connected_) return false;
    try {
        // Begin transaction for adding user to room
        pqxx::work txn(*conn_);

        // Execute INSERT with ON CONFLICT to prevent duplicates
        txn.exec(
            "INSERT INTO room_members (user_id, room_id, role) "
            "VALUES ($1, $2, $3) "
            "ON CONFLICT (room_id, user_id) DO NOTHING",
            pqxx::params(user_id, room_id, role)
        );
        txn.commit();
        std::cout << "User " << user_id << " added to room " << room_id << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Add user to room error: " << e.what() << std::endl;
        return false;
    }
}

bool Database::removeUserFromRoom(int user_id, int room_id){
    if(!connected_) return false;
    try {
        // Begin transaction for removing user from room
        pqxx::work txn(*conn_);
        // Execute DELETE with user and room parameters
        txn.exec(
            "DELETE FROM room_members WHERE user_id = $1 AND room_id = $2",
            pqxx::params(user_id, room_id)
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Remove user from room error: " << e.what() << std::endl;
        return false;
    }
}

std::vector<User> Database::getRoomMembers(int room_id) const{
    std::vector<User> members;
    if(!connected_) return members;
    try {
        // Read-only transaction
        pqxx::work txn(*conn_);
        // Fetch all users belonging to the specified room
        // JOIN with room_members table and order by join date
        pqxx::result r = txn.exec(
            "SELECT u.* FROM users u "
            "JOIN room_members rm ON u.id = rm.user_id "
            "WHERE rm.room_id = $1 "
            "ORDER BY rm.joined_at",
            pqxx::params(room_id)
        );
        // Convert each row to User object
        for(const auto& row : r){
            members.push_back(rowToUser(row));
        }
        return members;
    } catch (const std::exception& e) {
        std::cerr << "Get room members error: " << e.what() << std::endl;
    }
    return members;
}

bool Database::isUserInRoom(int user_id, int room_id) const{
    if(!connected_) return false;
    try {
        // Read-only transaction
        pqxx::work txn(*conn_);
        // Check if membership record exists
        pqxx::result r = txn.exec(
            "SELECT 1 FROM room_members WHERE user_id = $1 AND room_id = $2",
            pqxx::params(user_id, room_id)
        );
        return !r.empty();
    } catch (const std::exception& e) {
        std::cerr << "Is user in room error: " << e.what() << std::endl;
        return false;
    }
}