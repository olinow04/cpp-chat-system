#pragma once

#include <iostream>
#include <string>
#include <set>
#include <vector>
#include "../external/httplib.h"
#include "../external/json.hpp"
#include "../database/Database.h"
#include "../utils/Validator.hpp"
#include "../clients/RabbitMQClient.hpp"

using json = nlohmann::json;

/**
 * Room-related HTTP Request Handlers
 * Handles all room management and membership endpoints
 */
class RoomHandlers {
private:
    Database& db_;
    RabbitMQClient& rabbitmq_;

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
    RoomHandlers(Database& db, RabbitMQClient& rabbitmq)
        : db_(db), rabbitmq_(rabbitmq) {
    }

    /**
     * GET /api/rooms - Get all rooms
     */
    void getAllRooms(const httplib::Request&, httplib::Response& res) {
        try {
            auto rooms = db_.getAllRooms();
            json response = json::array();

            for (const auto& room : rooms) {
                response.emplace_back(json{
                    {"id", room.id},
                    {"name", room.name},
                    {"description", room.description},
                    {"created_by", room.created_by},
                    {"created_at", room.created_at},
                    {"is_private", room.is_private}
                });
            }

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Get rooms error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * GET /api/rooms/:id - Get room by ID
     */
    void getRoomById(const httplib::Request& req, httplib::Response& res) {
        try {
            int roomId = std::stoi(req.matches[1]);
            auto room = db_.getRoomById(roomId);

            if (!room) {
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            json response = {
                {"id", room->id},
                {"name", room->name},
                {"description", room->description},
                {"created_by", room->created_by},
                {"created_at", room->created_at},
                {"is_private", room->is_private}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Get room error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * POST /api/rooms - Create a new room
     */
    void createRoom(const httplib::Request& req, httplib::Response& res) {
        try {
            json j = json::parse(req.body);

            static const std::set<std::string> allowedFields = {
                "name", "description", "created_by", "is_private"
            };

            auto invalidFields = validateAllowedFields(j, allowedFields);
            if (!invalidFields.empty()) {
                sendInvalidFieldsError(res, invalidFields, allowedFields);
                return;
            }

            if (!j.contains("name") || !j.contains("description") || !j.contains("created_by")) {
                json error = {{"error", "Missing required fields: name, description, created_by"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            const std::string& name = j["name"].get_ref<const std::string&>();
            if (!Validator::isValidRoomName(name)) {
                json error = {{"error", "Invalid room name (must be 1-100 characters)"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            const std::string& description = j["description"].get_ref<const std::string&>();
            if (!Validator::isValidRoomDescription(description)) {
                json error = {{"error", "Description too long (max 500 characters)"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            int createdBy = j["created_by"].get<int>();
            auto creator = db_.getUserById(createdBy);
            if (!creator) {
                json error = {{"error", "Creator user not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            auto room = db_.getRoomByName(name);
            if (room) {
                json error = {{"error", "Room name already exists"}};
                res.set_content(error.dump(), "application/json");
                res.status = 409;
                return;
            }

            auto createdRoom = db_.createRoom(
                name,
                description,
                createdBy,
                j.value("is_private", false)
            );

            if (!createdRoom) {
                json error = {{"error", "Failed to create room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {
                {"id", createdRoom->id},
                {"name", createdRoom->name},
                {"description", createdRoom->description},
                {"created_by", createdRoom->created_by},
                {"created_at", createdRoom->created_at},
                {"is_private", createdRoom->is_private},
                {"message", "Room created successfully"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 201;

        } catch (json::parse_error& e) {
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            std::cerr << "Create room error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * GET /api/rooms/user/:id - Get rooms for specific user
     */
    void getRoomsByUser(const httplib::Request& req, httplib::Response& res) {
        try {
            int userId = std::stoi(req.matches[1]);

            auto rooms = db_.getRoomsByUser(userId);
            json response = json::array();

            for (const auto& room : rooms) {
                response.emplace_back(json{
                    {"id", room.id},
                    {"name", room.name},
                    {"description", room.description},
                    {"created_by", room.created_by},
                    {"created_at", room.created_at},
                    {"is_private", room.is_private}
                });
            }

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Get user rooms error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * GET /api/rooms/:id/members - Get room members
     */
    void getRoomMembers(const httplib::Request& req, httplib::Response& res) {
        try {
            int roomId = std::stoi(req.matches[1]);

            auto members = db_.getRoomMembers(roomId);
            json response = json::array();

            for (const auto& user : members) {
                response.emplace_back(json{
                    {"id", user.id},
                    {"username", user.username},
                    {"email", user.email}
                });
            }

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Get room members error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * POST /api/rooms/:id/members - Add user to room
     */
    void addUserToRoom(const httplib::Request& req, httplib::Response& res) {
        try {
            int roomId = std::stoi(req.matches[1]);
            json j = json::parse(req.body);

            static const std::set<std::string> allowedFields = {
                "user_id", "role"
            };

            auto invalidFields = validateAllowedFields(j, allowedFields);
            if (!invalidFields.empty()) {
                sendInvalidFieldsError(res, invalidFields, allowedFields);
                return;
            }

            if (!j.contains("user_id")) {
                json error = {{"error", "Missing required field: user_id"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            int userId = j["user_id"];
            std::string role = j.value("role", "member");

            auto room = db_.getRoomById(roomId);
            if (!room) {
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            auto user = db_.getUserById(userId);
            if (!user) {
                json error = {{"error", "User not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            if (db_.isUserInRoom(userId, roomId)) {
                json error = {{"error", "User is already a member of the room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 409;
                return;
            }

            bool success = db_.addUserToRoom(userId, roomId, role);

            if (!success) {
                json error = {{"error", "Failed to add user to room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {
                {"message", "User added to room successfully"},
                {"room_id", roomId},
                {"user_id", userId},
                {"role", role}
            };

            json event = {
                {"event_type", "user.joined_room"},
                {"room_id", roomId},
                {"user_id", userId},
                {"room_name", room->name},
                {"username", user->username},
                {"user_email", user->email},
                {"role", role}
            };

            rabbitmq_.publishEvent("user.joined_room", event);

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (json::parse_error& e) {
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            std::cerr << "Add user to room error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * PATCH /api/rooms/:id - Update room
     */
    void updateRoom(const httplib::Request& req, httplib::Response& res) {
        try {
            int roomId = std::stoi(req.matches[1]);
            json j = json::parse(req.body);

            static const std::set<std::string> allowedFields = {
                "name", "description"
            };

            auto invalidFields = validateAllowedFields(j, allowedFields);
            if (!invalidFields.empty()) {
                sendInvalidFieldsError(res, invalidFields, allowedFields);
                return;
            }

            auto room = db_.getRoomById(roomId);

            if (!room) {
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }
            
            if (j.contains("name")) {
                const std::string& name = j["name"].get_ref<const std::string&>();
                if (!Validator::isValidRoomName(name)) {
                    json error = {{"error", "Invalid room name (must be 1-100 characters)"}};
                    res.set_content(error.dump(), "application/json");
                    res.status = 400;
                    return;
                }

                auto currentRoom = db_.getRoomByName(name);
                if (currentRoom && currentRoom->id != roomId) {
                    json error = {{"error", "Room name already exists"}};
                    res.set_content(error.dump(), "application/json");
                    res.status = 409;
                    return;
                }

                room->name = name;
            }

            if (j.contains("description")) {
                const std::string& description = j["description"].get_ref<const std::string&>();
                if (!Validator::isValidRoomDescription(description)) {
                    json error = {{"error", "Description too long (max 500 characters)"}};
                    res.set_content(error.dump(), "application/json");
                    res.status = 400;
                    return;
                }

                room->description = description;
            }

            bool success = db_.updateRoom(room->id, room->name, room->description);

            if (!success) {
                json error = {{"error", "Failed to update room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {
                {"id", room->id},
                {"name", room->name},
                {"description", room->description},
                {"created_by", room->created_by},
                {"created_at", room->created_at},
                {"is_private", room->is_private},
                {"message", "Room updated successfully"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (json::parse_error& e) {
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            std::cerr << "Update room error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * DELETE /api/rooms/:id - Delete room
     */
    void deleteRoom(const httplib::Request& req, httplib::Response& res) {
        try {
            int roomId = std::stoi(req.matches[1]);

            auto room = db_.getRoomById(roomId);

            if (!room) {
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            bool success = db_.deleteRoom(roomId);

            if (!success) {
                json error = {{"error", "Failed to delete room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {{"message", "Room deleted successfully"}};
            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Delete room error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * DELETE /api/rooms/:room_id/members/:user_id - Remove user from room
     */
    void removeUserFromRoom(const httplib::Request& req, httplib::Response& res) {
        try {
            int roomId = std::stoi(req.matches[1]);
            int userId = std::stoi(req.matches[2]);

            auto room = db_.getRoomById(roomId);
            if (!room) {
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            auto user = db_.getUserById(userId);
            if (!user) {
                json error = {{"error", "User not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            if (!db_.isUserInRoom(userId, roomId)) {
                json error = {{"error", "User is not a member of the room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            bool success = db_.removeUserFromRoom(userId, roomId);

            if (!success) {
                json error = {{"error", "Failed to remove user from room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {
                {"message", "User removed from room successfully"},
                {"room_id", roomId},
                {"user_id", userId}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Remove user from room error:  " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }
};