#pragma once

#include <iostream>
#include <string>
#include <set>
#include <vector>
#include "httplib.h"
#include "json.hpp"
#include "Database.h"
#include "PasswordHelper.hpp"
#include "Validator.hpp"
#include "RabbitMQClient.hpp"

using json = nlohmann::json;

/**
 * User-related HTTP Request Handlers
 * Handles all user authentication and management endpoints
 */
class UserHandlers {
private:
    Database& db_;
    RabbitMQClient& rabbitmq_;

    /**
     * Validate that JSON contains only allowed fields
     */
    static std::vector<std::string> validateAllowedFields(
        const json& j,
        const std::set<std::string>& allowedFields
    ) {
        std::vector<std::string> invalidFields;
        
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (allowedFields.find(it.key()) == allowedFields.end()) {
                invalidFields.emplace_back(it.key());
            }
        }
        
        return invalidFields;
    }

    /**
     * Send error response for invalid fields
     */
    static void sendInvalidFieldsError(
        httplib::Response& res,
        const std::vector<std::string>& invalidFields,
        const std::set<std::string>& allowedFields
    ) {
        std::string fieldsList;
        for (size_t i = 0; i < invalidFields.size(); ++i) {
            if (i > 0) {
                fieldsList += ", ";
            }
            fieldsList += "'" + invalidFields[i] + "'";
        }
        
        json error = {
            {"error", "Invalid fields: " + fieldsList},
            {"allowed_fields", allowedFields}
        };
        res.set_content(error.dump(), "application/json");
        res.status = 400;
    }

public:
    UserHandlers(Database& db, RabbitMQClient& rabbitmq)
        : db_(db), rabbitmq_(rabbitmq) {
    }

    /**
     * POST /api/register - Register a new user
     */
    void registerUser(const httplib::Request& req, httplib::Response& res) {
        try {
            json j = json::parse(req.body);

            static const std::set<std::string> allowedFields = {
                "username", "email", "password"
            };

            auto invalidFields = validateAllowedFields(j, allowedFields);
            if (!invalidFields.empty()) {
                sendInvalidFieldsError(res, invalidFields, allowedFields);
                return;
            }

            if (!j.contains("username") || !j.contains("email") || !j.contains("password")) {
                json error = {{"error", "Missing required fields:  username, email, password"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            const std::string& username = j["username"].get_ref<const std::string&>();
            const std::string& email = j["email"].get_ref<const std::string&>();
            const std::string& password = j["password"].get_ref<const std::string&>();

            if (!Validator::isValidUsername(username)) {
                json error = {{"error", "Invalid username format"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            if (!Validator::isValidEmail(email)) {
                json error = {{"error", "Invalid email format"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            if (!Validator::isValidPassword(password)) {
                json error = {{"error", "Password must be at least 8 characters long and contain both letters and numbers"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            auto user = db_.getUserByUsername(username);
            if (user) {
                json error = {{"error", "Username already exists"}};
                res.set_content(error.dump(), "application/json");
                res.status = 409;
                return;
            }

            user->username = username;
            user->email = email;
            user->password_hash = PasswordHelper::hashPassword(password);
            user->is_active = true;

            auto created = db_.createUser(*user);

            if (!created) {
                json error = {{"error", "Failed to create user"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {
                {"id", created->id},
                {"username", created->username},
                {"email", created->email},
                {"message", "User registered successfully"}
            };

            json event = {
                {"event_type", "user.registered"},
                {"user_id", created->id},
                {"username", created->username},
                {"email", created->email},
                {"timestamp", created->created_at}
            };

            rabbitmq_.publishEvent("user.registered", event);

            res.set_content(response.dump(), "application/json");
            res.status = 201;

        } catch (json::parse_error& e) {
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            std::cerr << "Register error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * POST /api/login - User login
     */
    void login(const httplib::Request& req, httplib::Response& res) {
        try {
            json j = json::parse(req.body);

            static const std::set<std::string> allowedFields = {
                "username", "password"
            };

            auto invalidFields = validateAllowedFields(j, allowedFields);
            if (!invalidFields.empty()) {
                sendInvalidFieldsError(res, invalidFields, allowedFields);
                return;
            }

            if (!j.contains("username") || !j.contains("password")) {
                json error = {{"error", "Missing required fields: username, password"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            const std::string& username = j["username"].get_ref<const std::string&>();
            const std::string& password = j["password"].get_ref<const std::string&>();
            
            auto user = db_.getUserByUsername(username);
            if (!user) {
                json error = {{"error", "Invalid credentials"}};
                res.set_content(error.dump(), "application/json");
                res.status = 401;
                return;
            }

            if (!PasswordHelper::verifyPassword(password, user->password_hash)) {
                json error = {{"error", "Invalid credentials"}};
                res.set_content(error.dump(), "application/json");
                res.status = 401;
                return;
            }

            if (!user->is_active) {
                json error = {{"error", "Account is disabled"}};
                res.set_content(error.dump(), "application/json");
                res.status = 403;
                return;
            }

            db_.updateLastLogin(user->id);

            json response = {
                {"id", user->id},
                {"username", user->username},
                {"email", user->email},
                {"message", "Login successful"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (json::parse_error& e) {
            json error = {{"error", "Invalid JSON"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            std::cerr << "Login error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * GET /api/users/:id - Get user by ID
     */
    void getUserById(const httplib::Request& req, httplib::Response& res) {
        try {
            int userId = std::stoi(req.matches[1]);
            auto user = db_.getUserById(userId);

            if (!user) {
                json error = {{"error", "User not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            json response = {
                {"id", user->id},
                {"username", user->username},
                {"email", user->email}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Get user error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * GET /api/users - Get all users
     */
    void getAllUsers(const httplib::Request&, httplib::Response& res) {
        try {
            auto users = db_.getAllUsers();
            json response = json::array();

            for (const auto& user : users) {
                response.emplace_back(json{
                    {"id", user.id},
                    {"username", user.username},
                    {"email", user.email},
                    {"created_at", user.created_at},
                    {"is_active", user.is_active}
                });
            }

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Get users error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * PATCH /api/users/:id - Update user data
     */
    void updateUser(const httplib::Request& req, httplib::Response& res) {
        try {
            int userId = std::stoi(req.matches[1]);
            json j = json::parse(req.body);

            static const std::set<std::string> allowedFields = {
                "email", "password", "is_active"
            };

            auto invalidFields = validateAllowedFields(j, allowedFields);
            if (!invalidFields.empty()) {
                sendInvalidFieldsError(res, invalidFields, allowedFields);
                return;
            }

            auto user = db_.getUserById(userId);

            if (!user) {
                json error = {{"error", "User not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            if (j.contains("email")) {
                const std::string& email = j["email"].get_ref<const std::string&>();
                if (!Validator::isValidEmail(email)) {
                    json error = {{"error", "Invalid email format"}};
                    res.set_content(error.dump(), "application/json");
                    res.status = 400;
                    return;
                }
                user->email = email;
            }

            if (j.contains("password")) {
                const std::string& password = j["password"].get_ref<const std::string&>();
                if (!Validator::isValidPassword(password)) {
                    json error = {{"error", "Password must be at least 8 characters and contain letters and numbers"}};
                    res.set_content(error.dump(), "application/json");
                    res.status = 400;
                    return;
                }
                user->password_hash = PasswordHelper::hashPassword(password);
            }

            if (j.contains("is_active")) {
                user->is_active = j["is_active"];
            }

            bool success = db_.updateUser(*user);

            if (!success) {
                json error = {{"error", "Failed to update user"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {
                {"id", user->id},
                {"username", user->username},
                {"email", user->email},
                {"is_active", user->is_active},
                {"message", "User updated successfully"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (json::parse_error& e) {
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            std::cerr << "Update user error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * DELETE /api/users/:id - Delete user
     */
    void deleteUser(const httplib::Request& req, httplib::Response& res) {
        try {
            int userId = std::stoi(req. matches[1]);
            auto user = db_.getUserById(userId);

            if (!user) {
                json error = {{"error", "User not found"}};
                res.set_content(error. dump(), "application/json");
                res.status = 404;
                return;
            }

            bool success = db_.deleteUser(userId);

            if (!success) {
                json error = {{"error", "Failed to delete user"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {{"message", "User deleted successfully"}};
            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Delete user error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }
};