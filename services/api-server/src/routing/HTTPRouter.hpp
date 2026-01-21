#pragma once

#include "../external/httplib.h"
#include "../database/Database.h"
#include "../clients/RabbitMQClient.hpp"
#include "../clients/TranslationClient.hpp"
#include "../handlers/UserHandlers.hpp"
#include "../handlers/RoomHandlers.hpp"
#include "../handlers/MessageHandlers.hpp"
#include "../handlers/TranslationHandlers.hpp"

/**
 * HTTP Router - Central routing configuration
 * Registers all API endpoints with their respective handlers
 */
class HTTPRouter {
private:
    httplib::Server& server_;
    UserHandlers userHandlers_;
    RoomHandlers roomHandlers_;
    MessageHandlers messageHandlers_;
    TranslationHandlers translationHandlers_;

public:
    /**
     * Constructor - Initialize all handlers
     */
    HTTPRouter(httplib::Server& server, Database& db, RabbitMQClient& rabbitmq, TranslationClient& translationClient)
        : server_(server),
          userHandlers_(db, rabbitmq),
          roomHandlers_(db, rabbitmq),
          messageHandlers_(db, rabbitmq),
          translationHandlers_(translationClient) {
    }

    /**
     * Register all API routes
     */
    void registerRoutes() {
        // Configure CORS
        server_.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type");
        });

        // Health check
        server_.Get("/hi", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("Hello World!", "text/plain");
        });

        // ====== USER ROUTES ======

        server_.Post("/api/register", [this](const httplib::Request& req, httplib::Response& res) {
            userHandlers_.registerUser(req, res);
        });

        server_.Post("/api/login", [this](const httplib::Request& req, httplib::Response& res) {
            userHandlers_.login(req, res);
        });

        server_.Get(R"(/api/users/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            userHandlers_.getUserById(req, res);
        });

        server_.Get("/api/users", [this](const httplib::Request& req, httplib::Response& res) {
            userHandlers_.getAllUsers(req, res);
        });

        server_.Patch(R"(/api/users/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            userHandlers_.updateUser(req, res);
        });

        server_.Delete(R"(/api/users/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            userHandlers_.deleteUser(req, res);
        });

        // ====== ROOM ROUTES ======

        server_.Get("/api/rooms", [this](const httplib::Request& req, httplib::Response& res) {
            roomHandlers_.getAllRooms(req, res);
        });

        server_.Get(R"(/api/rooms/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            roomHandlers_.getRoomById(req, res);
        });

        server_.Post("/api/rooms", [this](const httplib::Request& req, httplib::Response& res) {
            roomHandlers_.createRoom(req, res);
        });

        server_.Get(R"(/api/rooms/user/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            roomHandlers_.getRoomsByUser(req, res);
        });

        server_.Get(R"(/api/rooms/(\d+)/members)", [this](const httplib::Request& req, httplib::Response& res) {
            roomHandlers_.getRoomMembers(req, res);
        });

        server_.Post(R"(/api/rooms/(\d+)/members)", [this](const httplib::Request& req, httplib::Response& res) {
            roomHandlers_.addUserToRoom(req, res);
        });

        server_.Patch(R"(/api/rooms/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            roomHandlers_.updateRoom(req, res);
        });

        server_.Delete(R"(/api/rooms/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            roomHandlers_.deleteRoom(req, res);
        });

        server_.Delete(R"(/api/rooms/(\d+)/members/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            roomHandlers_.removeUserFromRoom(req, res);
        });

        // ====== MESSAGE ROUTES ======

        server_.Get(R"(/api/rooms/(\d+)/messages)", [this](const httplib::Request& req, httplib::Response& res) {
            messageHandlers_.getRoomMessages(req, res);
        });

        server_.Post(R"(/api/rooms/(\d+)/messages)", [this](const httplib::Request& req, httplib::Response& res) {
            messageHandlers_.sendMessage(req, res);
        });

        server_.Get(R"(/api/rooms/messages/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            messageHandlers_.getMessageById(req, res);
        });

        server_.Patch(R"(/api/messages/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            messageHandlers_.updateMessage(req, res);
        });

        server_.Delete(R"(/api/messages/(\d+))", [this](const httplib::Request& req, httplib::Response& res) {
            messageHandlers_.deleteMessage(req, res);
        });

        // ====== TRANSLATION ROUTE ======

        server_.Post("/api/translate", [this](const httplib::Request& req, httplib::Response& res) {
            translationHandlers_.translateText(req, res);
        });
    }
};