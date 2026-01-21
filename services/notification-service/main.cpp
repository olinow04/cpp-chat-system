/**
 * Notification Service - Email Notification Microservice
 * 
 * Purpose: 
 * - Consumes events from RabbitMQ message queue
 * - Sends email notifications via SMTP for user actions
 * - Implements asynchronous, event-driven architecture
 * 
 * Events handled:
 * - user.registered: Welcome email for new users
 * - message.created: Notification for new messages in rooms
 * - user.joined_room: Confirmation when user joins a room
 * 
 * Architecture:
 * - Event-driven: Reacts to events published by api-server
 * - Asynchronous: Does not block API responses (background processing)
 * - Microservice: Independent service, loosely coupled with other components
 */

#include <iostream>
#include <string>
#include <cstdlib>
#include "src/clients/SMTPClient.hpp"
#include "src/consumers/RabbitMQConsumer.hpp"

/**
 * Application configuration constants
 */
namespace Config {
    constexpr const char* DEFAULT_RABBITMQ_HOST = "localhost";
    constexpr int RABBITMQ_PORT = 5672;
    constexpr const char* RABBITMQ_USER = "chatuser";
    constexpr const char* RABBITMQ_PASS = "chatpass";
    constexpr const char* QUEUE_NAME = "notification_queue";
}

/**
 * Main function - Entry point for notification service
 * 
 * Workflow:
 * 1. Load SMTP credentials from environment variables
 * 2. Initialize SMTP client (or use simulation mode if not configured)
 * 3. Connect to RabbitMQ
 * 4. Start consuming events (blocks until terminated)
 * 5. Cleanup resources on exit
 */
int main() {
    std::cout << "NOTIFICATION SERVICE\n" << std::endl;
    std::cout << "Email Notification Microservice" << std::endl;
    std::cout << "\nStarting Notification Service..." << std::endl;

    // Get SMTP configuration from environment variables
    const char* smtpHost = std::getenv("SMTP_HOST");
    const char* smtpPortStr = std::getenv("SMTP_PORT");
    const char* smtpUser = std::getenv("SMTP_USER");
    const char* smtpPass = std::getenv("SMTP_PASSWORD");

    // Initialize SMTP client if credentials are provided
    std::unique_ptr<SMTPClient> smtpClientPtr = nullptr;

    if (smtpHost && smtpPortStr && smtpUser && smtpPass) {
        int smtpPort = std::atoi(smtpPortStr);

        std::cout << "Configuring SMTP..." << std::endl;
        std::cout << "Server: " << smtpHost << ":" << smtpPort << std::endl;
        std::cout << "User: " << smtpUser << std::endl;

        smtpClientPtr = std::make_unique<SMTPClient>(smtpHost, smtpPort, smtpUser, smtpPass);
        if (smtpClientPtr->isConfigured()) {
            std::cout << "SMTP configured successfully" << std::endl;
        } else {
            std::cerr << "SMTP configuration invalid" << std::endl;
            smtpClientPtr.reset();
        }
    } else {
        std::cout << "SMTP credentials not found in environment" << std::endl;
        std::cout << "Set: SMTP_HOST, SMTP_PORT, SMTP_USER, SMTP_PASSWORD" << std::endl;
        std::cout << "Email sending will be simulated" << std::endl;
    }

    // Get RabbitMQ host from environment
    const char* rabbitmqHost = std::getenv("RABBITMQ_HOST");
    if (!rabbitmqHost) {
        rabbitmqHost = Config::DEFAULT_RABBITMQ_HOST;  // Fallback for local development
    }

    // Create RabbitMQ consumer
    RabbitMQConsumer consumer(
        rabbitmqHost,
        Config::RABBITMQ_PORT,
        Config::RABBITMQ_USER,
        Config::RABBITMQ_PASS,
        Config::QUEUE_NAME,
        std::move(smtpClientPtr)
    );

    // Check if RabbitMQ connection was successful
    if (!consumer.isConnected()) {
        std::cerr << "Failed to connect to RabbitMQ. Exiting." << std::endl;
        return 1;
    }

    // Start consuming events (blocks forever until terminated)
    consumer.startConsuming();

    return 0;
}