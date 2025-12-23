#include "db.h"
#include <iostream>

Database::Database(const std::string& connectionString)
    : connectionString_(connectionString), connected_(false) {}

Database::~Database() {
    disconnect();
}

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

User Database::rowToUser(const pqxx::row& row) const {
    return User{
        // Convert PostgreSQL row values to C++ types
        row["id"].as<int>(),
        row["uuid"].as<std::string>(),
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


