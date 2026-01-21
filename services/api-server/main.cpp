/**
 * API Server - REST API for Chat System
 * Provides HTTP endpoints for user authentication, room management, and messaging
 */

#include <iostream>
#include <string>
#include <memory>

#include "external/httplib.h"
#include "src/database/Database.h"
#include "src/clients/RabbitMQClient.hpp"
#include "src/clients/TranslationClient.hpp"
#include "src/routing/HTTPRouter.hpp"

/**
 * Application configuration constants
 */
namespace Config {
    constexpr const char* DB_CONNECTION_STRING = "host=localhost port=5432 dbname=chatdb user=chatuser password=chatpass";
    constexpr const char* RABBITMQ_HOST = "localhost";
    constexpr int RABBITMQ_PORT = 5672;
    constexpr const char* RABBITMQ_USER = "chatuser";
    constexpr const char* RABBITMQ_PASS = "chatpass";
    constexpr const char* TRANSLATION_API_URL = "http://localhost:5001";
    constexpr const char* SERVER_HOST = "0.0.0.0";
    constexpr int SERVER_PORT = 8080;
}

/**
 * Main function - Entry point for API server
 * 
 * Workflow:
 * 1. Connect to PostgreSQL database
 * 2. Connect to RabbitMQ for event publishing
 * 3. Initialize Translation API client
 * 4. Setup HTTP routes via HTTPRouter
 * 5. Start HTTP server on port 8080
 */
int main() {
    // Initialize HTTP server
    httplib::Server svr;

    // Connect to PostgreSQL database
    Database db(Config::DB_CONNECTION_STRING);

    if (!db.connect()) {
        std::cerr << "Failed to connect to database. Exiting." << std::endl;
        return 1;
    }

    std::cout << "Connected to database successfully." << std::endl;

    // Connect to RabbitMQ
    RabbitMQClient rabbitmq(Config::RABBITMQ_HOST, Config::RABBITMQ_PORT, Config::RABBITMQ_USER, Config::RABBITMQ_PASS);

    if (!rabbitmq.isConnected()) {
        std::cerr << "Warning: RabbitMQ not connected. Events will not be published." << std::endl;
    }

    // Initialize Translation Client
    TranslationClient translationClient(Config::TRANSLATION_API_URL);

    if (!translationClient.isAvailable()) {
        std::cerr << "Warning: Translation API not available. Translation features will be disabled." << std::endl;
    } else {
        std::cout << "Translation API connected successfully." << std::endl;
    }

    // Initialize router and register all routes
    HTTPRouter router(svr, db, rabbitmq, translationClient);
    router.registerRoutes();

    // Start the HTTP server and listen on all interfaces at port 8080
    std::cout << "Starting server on port " << Config::SERVER_PORT << "..." << std::endl;
    svr.listen(Config::SERVER_HOST, Config::SERVER_PORT);

    return 0;
}