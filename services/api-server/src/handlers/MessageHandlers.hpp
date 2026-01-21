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
 * Message-related HTTP Request Handlers
 * Handles all message CRUD operations
 */
class MessageHandlers {
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
    MessageHandlers(Database& db, RabbitMQClient& rabbitmq)
        : db_(db), rabbitmq_(rabbitmq) {
    }

    /**
     * GET /api/rooms/:id/messages - Get messages from a room
     */
    void getRoomMessages(const httplib::Request& req, httplib::Response& res) {
        try {
            int roomId = std::stoi(req.matches[1]);
            auto room = db_.getRoomById(roomId);

            if (!room) {
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            constexpr int DEFAULT_LIMIT = 50;
            constexpr int DEFAULT_OFFSET = 0;

            int limit = req.has_param("limit") ? std::stoi(req.get_param_value("limit")) : DEFAULT_LIMIT;
            int offset = req.has_param("offset") ? std::stoi(req.get_param_value("offset")) : DEFAULT_OFFSET;

            auto messages = db_.getMessagesByRoom(roomId, limit, offset);
            json response = json::array();

            for (const auto& message : messages) {
                response.emplace_back(json{
                    {"id", message.id},
                    {"room_id", message.room_id},
                    {"user_id", message.user_id},
                    {"content", message.content},
                    {"message_type", message.message_type},
                    {"created_at", message.created_at},
                    {"edited_at", message.edited_at},
                    {"is_deleted", message.is_deleted}
                });
            }

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Get room messages error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * POST /api/rooms/:id/messages - Send a message to a room
     */
    void sendMessage(const httplib::Request& req, httplib::Response& res) {
        try {
            int roomId = std::stoi(req.matches[1]);
            json j = json::parse(req.body);

            static const std::set<std::string> allowedFields = {
                "user_id", "content", "message_type"
            };

            auto invalidFields = validateAllowedFields(j, allowedFields);
            if (!invalidFields.empty()) {
                sendInvalidFieldsError(res, invalidFields, allowedFields);
                return;
            }

            if (!j.contains("user_id") || !j.contains("content")) {
                json error = {{"error", "Missing required fields: user_id, content"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            const std::string& content = j["content"].get_ref<const std::string&>();
            if (!Validator::isValidMessageContent(content)) {
                json error = {{"error", "Invalid message content (must be 1-1000 characters)"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            const std::string messageType = j.value("message_type", "text");
            if (messageType != "text" && messageType != "image" && messageType != "file") {
                json error = {{"error", "Invalid message type (must be 'text', 'image', or 'file')"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            auto room = db_.getRoomById(roomId);
            if (!room) {
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            int userId = j["user_id"].get<int>();
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
                res.status = 403;
                return;
            }

            auto createdMessage = db_.createMessage(
                roomId,
                userId,
                content,
                messageType
            );

            if (!createdMessage) {
                json error = {{"error", "Failed to create message"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {
                {"id", createdMessage->id},
                {"room_id", createdMessage->room_id},
                {"user_id", createdMessage->user_id},
                {"content", content},
                {"message_type", createdMessage->message_type},
                {"created_at", createdMessage->created_at},
                {"edited_at", createdMessage->edited_at},
                {"is_deleted", createdMessage->is_deleted},
                {"message", "Message sent successfully"}
            };

            json event = {
                {"event_type", "message.created"},
                {"message_id", createdMessage->id},
                {"room_id", createdMessage->room_id},
                {"user_id", createdMessage->user_id},
                {"sender_username", user->username},
                {"sender_email", user->email},
                {"room_name", room->name},
                {"content", content},
                {"message_type", createdMessage->message_type},
                {"timestamp", createdMessage->created_at}
            };

            rabbitmq_.publishEvent("message.created", event);

            res.set_content(response.dump(), "application/json");
            res.status = 201;

        } catch (json::parse_error& e) {
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            std::cerr << "Create message error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * GET /api/rooms/messages/:id - Get message by ID
     */
    void getMessageById(const httplib::Request& req, httplib::Response& res) {
        try {
            int messageId = std::stoi(req.matches[1]);

            auto message = db_.getMessageById(messageId);

            if (!message) {
                json error = {{"error", "Message not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            json response = {
                {"id", message->id},
                {"room_id", message->room_id},
                {"user_id", message->user_id},
                {"content", message->content},
                {"message_type", message->message_type},
                {"created_at", message->created_at},
                {"edited_at", message->edited_at},
                {"is_deleted", message->is_deleted}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Get message error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * PATCH /api/messages/:id - Update message
     */
    void updateMessage(const httplib::Request& req, httplib::Response& res) {
        try {
            int messageId = std::stoi(req.matches[1]);
            json j = json::parse(req.body);

            static const std::set<std::string> allowedFields = {
                "content"
            };

            auto invalidFields = validateAllowedFields(j, allowedFields);
            if (!invalidFields.empty()) {
                sendInvalidFieldsError(res, invalidFields, allowedFields);
                return;
            }

            auto message = db_.getMessageById(messageId);

            if (!message) {
                json error = {{"error", "Message not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            if (message->is_deleted) {
                json error = {{"error", "Cannot update a deleted message"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            const std::string& content = j["content"].get_ref<const std::string&>();
            if (!Validator::isValidMessageContent(content)) {
                json error = {{"error", "Invalid message content (must be 1-1000 characters)"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
                }

            message->content = content;

            bool success = db_.updateMessage(message->id, message->content);

            if (!success) {
                json error = {{"error", "Failed to update message"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {
                {"id", message->id},
                {"room_id", message->room_id},
                {"user_id", message->user_id},
                {"content", message->content},
                {"message_type", message->message_type},
                {"created_at", message-> created_at},
                {"edited_at", message->edited_at},
                {"is_deleted", message->is_deleted},
                {"message", "Message updated successfully"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (json::parse_error& e) {
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            std::cerr << "Update message error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }

    /**
     * DELETE /api/messages/:id - Delete message (soft delete)
     */
    void deleteMessage(const httplib::Request& req, httplib::Response& res) {
        try {
            int messageId = std::stoi(req.matches[1]);

            auto message = db_.getMessageById(messageId);

            if (!message) {
                json error = {{"error", "Message not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            bool success = db_.deleteMessage(messageId);

            if (!success) {
                json error = {{"error", "Failed to delete message"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {{"message", "Message deleted successfully"}};
            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (const std::exception& e) {
            std::cerr << "Delete message error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }
};