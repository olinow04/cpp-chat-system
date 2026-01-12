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
#include <thread>
#include <chrono>
#include <cstdlib>
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h> 
#include "smtp_client.h"
#include "json.hpp"

using json = nlohmann::json;

/**
 * RabbitMQ Consumer Class
 * 
 * Responsibilities:
 * - Establishes connection to RabbitMQ broker
 * - Declares exchange and queue with proper bindings
 * - Consumes messages from notification_queue
 * - Routes events to appropriate email handlers
 * - Manages SMTP client lifecycle
 */
class RabbitMQConsumer {
public:
    /**
     * Constructor - Establishes connection to RabbitMQ and sets up queue
     */
    RabbitMQConsumer(const std::string& host, int port,
                     const std::string& user, const std::string& password,
                     const std::string& queueName,
                     SMTPClient* smtpClient = nullptr)
        : queueName_(queueName), 
          connected_(false), 
          conn_(nullptr), 
          socket_(nullptr), 
          smtpClient_(smtpClient) {

        try {
            std::cout << "Connecting to RabbitMQ at " << host << ":" << port << "..." << std::endl;

            // Create new AMQP connection object
            conn_ = amqp_new_connection();

            // Create TCP socket for connection
            socket_ = amqp_tcp_socket_new(conn_);

            if(!socket_) {
                std::cerr << "Failed to create TCP socket." << std::endl;
                return;
            }

            // Open TCP socket to RabbitMQ server
            int status = amqp_socket_open(socket_, host.c_str(), port);
            if(status) {
                std::cerr << "Failed to open socket to RabbitMQ" << std::endl;
                return;
            }

            // Login to RabbitMQ with credentials
            // vhost "/" = default virtual host
            // frame_max 131072 = 128KB max frame size
            // heartbeat 0 = disabled (no heartbeat checks)
            amqp_rpc_reply_t reply = amqp_login(
                conn_,
                "/",                    
                0,                      
                131072,                 
                0,                      
                AMQP_SASL_METHOD_PLAIN, 
                user.c_str(),
                password.c_str()
            );

            if(reply.reply_type != AMQP_RESPONSE_NORMAL) {
                std::cerr << "Failed to login to RabbitMQ" << std::endl;
                return;
            }

            // Open communication channel (channel 1)
            amqp_channel_open(conn_, 1);
            reply = amqp_get_rpc_reply(conn_);

            if(reply.reply_type != AMQP_RESPONSE_NORMAL) {
                std::cerr << "Failed to open channel" << std::endl;
                return;
            }

            std::cout << "Connected to RabbitMQ successfully" << std::endl;
            
            // Declare exchange (topic type allows routing by pattern matching)
            // durable = 1: Exchange survives RabbitMQ restart
            std::cout << "Declaring exchange 'chat_events'..." << std::endl;
            amqp_exchange_declare(
                conn_,
                1,                                      
                amqp_cstring_bytes("chat_events"),      
                amqp_cstring_bytes("topic"),            
                0,                                      
                1,                                      
                0,                                      
                0,                                      
                amqp_empty_table                        
            );

            reply = amqp_get_rpc_reply(conn_);
            if(reply.reply_type != AMQP_RESPONSE_NORMAL) {
                std::cerr << "Failed to declare exchange" << std::endl;
                return;
            }
            
            // Declare queue for storing events until consumed
            // durable = 1: Queue survives restart
            // exclusive = 0: Queue is shared (multiple consumers possible)
            amqp_queue_declare(
                conn_,
                1,                                      
                amqp_cstring_bytes(queueName_.c_str()), 
                0,                                      
                1,                                      
                0,                                      
                0,                                      
                amqp_empty_table                        
            );

            reply = amqp_get_rpc_reply(conn_);
            if(reply.reply_type != AMQP_RESPONSE_NORMAL) {
                std::cerr << "Failed to declare queue" << std::endl;
                return;
            }

            // Bind queue to exchange with routing keys
            // This determines which events this consumer receives
            std::cout << "Binding queue to exchange with routing keys..." << std::endl;

            // Bind routing key: user.registered
            amqp_queue_bind(
                conn_,
                1,                                       
                amqp_cstring_bytes(queueName_.c_str()),  
                amqp_cstring_bytes("chat_events"),       
                amqp_cstring_bytes("user.registered"),   
                amqp_empty_table                         
            );
            std::cout << "Bound to: user.registered" << std::endl;

            // Bind routing key: message.created
            amqp_queue_bind(
                conn_,
                1,
                amqp_cstring_bytes(queueName_.c_str()),
                amqp_cstring_bytes("chat_events"),
                amqp_cstring_bytes("message.created"),
                amqp_empty_table
            );
            std::cout << "Bound to: message.created" << std::endl;

            // Bind routing key: user.joined_room
            amqp_queue_bind(
                conn_,
                1,
                amqp_cstring_bytes(queueName_.c_str()),
                amqp_cstring_bytes("chat_events"),
                amqp_cstring_bytes("user.joined_room"),
                amqp_empty_table
            );
            std::cout << "Bound to: user.joined_room" << std::endl;

            // Start consuming messages from queue
            // no_ack = 1: Auto-acknowledge (message removed from queue immediately)
            // exclusive = 0: Allow other consumers to read from same queue
            std::cout << "Starting to consume messages..." << std::endl;
            amqp_basic_consume(
                conn_,
                1,                                      
                amqp_cstring_bytes(queueName_.c_str()), 
                amqp_empty_bytes,                       
                0,                                      
                1,                                      
                0,                                      
                amqp_empty_table                        
            );

            reply = amqp_get_rpc_reply(conn_);
            if(reply.reply_type != AMQP_RESPONSE_NORMAL) {
                std::cerr << "Failed to start consuming" << std::endl;
                return;
            }

            connected_ = true;
            std::cout << "Notification service is ready and listening!" << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "RabbitMQ connection error: " << e.what() << std::endl;
            connected_ = false;
        }
    }

    /**
     * Destructor - Clean up RabbitMQ connection resources
     */
    ~RabbitMQConsumer() {
        if(conn_) {
            amqp_channel_close(conn_, 1, AMQP_REPLY_SUCCESS);
            amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
            amqp_destroy_connection(conn_);
        }
    }

    /**
     * Start consuming messages in infinite loop (blocking)
     * 
     * This function blocks until the program is terminated.
     * It continuously waits for messages from RabbitMQ and processes them.
     * Timeout of 5 seconds allows graceful shutdown and periodic logging.
     */
    void startConsuming() {
        if(!connected_ || !conn_) {
            std::cerr << "Not connected to RabbitMQ" << std::endl;
            return;
        }

        std::cout << "Starting event processing loop..." << std::endl;

        // Infinite loop - process messages as they arrive
        while(true) {
            amqp_envelope_t envelope;

            // Release unused memory buffers (optimization)
            amqp_maybe_release_buffers(conn_);

            // Set timeout for waiting for messages (5 seconds)
            // This prevents infinite blocking and allows graceful shutdown
            struct timeval timeout;
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            // Wait for a message (blocks until message arrives or timeout)
            amqp_rpc_reply_t result = amqp_consume_message(conn_, &envelope, &timeout, 0);

            if(result.reply_type == AMQP_RESPONSE_NORMAL) {
                // Message received successfully

                // Extract message body (payload as string)
                std::string messageBody(
                    static_cast<char*>(envelope.message.body.bytes),
                    envelope.message.body.len
                );

                // Extract routing key (determines event type)
                std::string routingKey(
                    static_cast<char*>(envelope.routing_key.bytes),
                    envelope.routing_key.len
                );

                // Process the event
                processEvent(routingKey, messageBody);

                // Free envelope memory
                amqp_destroy_envelope(&envelope);

            } else if(result.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION) {
                // Library exception

                if(result.library_error == AMQP_STATUS_TIMEOUT) {
                    // Timeout - no messages available (this is normal)
                    std::cout << "[" << getCurrentTime() << "] No messages (timeout), waiting..." << std::endl;
                } else {
                    // Other error
                    std::cerr << "Error consuming message" << std::endl;
                    break;
                }
            } else {
                // Unexpected response
                std::cerr << "Unexpected response type" << std::endl;
                break;
            }
        }
    }

    /**
     * Check if connected to RabbitMQ
     */
    bool isConnected() const {
        return connected_;
    }

private:
    /**
     * Process received event and route to appropriate handler
     */
    void processEvent(const std::string& routingKey, const std::string& payload) {
        std::cout << "\n========= NEW EVENT =========" << std::endl;
        std::cout << "Time: " << getCurrentTime() << std::endl;
        std::cout << "Routing Key: " << routingKey << std::endl;
        std::cout << "Payload: " << payload << std::endl;
        std::cout << "=============================\n" << std::endl;

        // Check if SMTP client is configured
        if(!smtpClient_ || !smtpClient_->isConfigured()) {
            std::cerr << "SMTP not configured - simulating email send" << std::endl;
            simulateEmailSend(routingKey);
            return;
        }           

        // Route event to appropriate handler based on routing key
        if(routingKey == "user.registered") {
            sendWelcomeEmail(payload);
        } else if(routingKey == "message.created") {
            sendMessageNotification(payload);
        } else if(routingKey == "user.joined_room") {
            sendRoomJoinNotification(payload);
        } else {
            std::cout << "Unknown event type: " << routingKey << std::endl;
            std::cout << "Skipping notification." << std::endl;
        }
    }
    
    /**
     * Get current system time as formatted string
     */
    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::string timeStr = std::ctime(&time);
        timeStr.pop_back(); // Remove trailing newline
        return timeStr;
    }

    /**
     * Simulate email sending when SMTP is not configured
     */
    void simulateEmailSend(const std::string& routingKey) {
        std::cout << "To: [extracted from payload]" << std::endl;

        if(routingKey == "user.registered") {
            std::cout << "Subject: Welcome to C++ Chat System!" << std::endl;
        } else if(routingKey == "message.created") {
            std::cout << "Subject: New message in your chat room" << std::endl;
        } else if(routingKey == "user.joined_room") {
            std::cout << "Subject: You've joined a new room!" << std::endl;
        } 
        
        std::cout << "Body: [generated message]" << std::endl;
        simulateEmailDelay();
        std::cout << "Email simulated successfully (SMTP not configured)\n" << std::endl;
    }

    /**
     * Simulate email sending delay
     */
    void simulateEmailDelay() {
        std::cout << "Sending..." << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        std::cout << " Done!" << std::endl;
    }

    /**
     * Send welcome email for new user registration
     * 
     * Triggered by: user.registered event
     * Sent to: New user's email address
     * Purpose: Welcome new users and confirm successful registration
     */
    void sendWelcomeEmail(const std::string& payload) {
        std::cout << "\nACTION: Sending welcome email..." << std::endl;

        try {
            // Parse JSON payload
            json eventData = json::parse(payload);

            // Extract user data from JSON
            std::string recipientEmail = eventData.value("email", "unknown@example.com");
            std::string username = eventData.value("username", "User");
            int userId = eventData.value("user_id", 0);

            // Validate email exists
            if(recipientEmail == "unknown@example.com") {
                std::cerr << "No email provided in event payload. Skipping email." << std::endl;
                return;
            }

            std::cout << "To: " << recipientEmail << std::endl;
            std::cout << "User: " << username << " (ID: " << userId << ")" << std::endl;

            // Email subject
            std::string subject = "Welcome to C++ Chat System, " + username + "!";

            // Personalized email body
            std::string body = 
                "Hello " + username + "!\n\n"
                "Your account (ID: " + std::to_string(userId) + ") has been successfully created.\n\n"
                "---\n"
                "Your email: " + recipientEmail;

            // Send email via SMTP
            bool success = smtpClient_->sendEmail(recipientEmail, subject, body);

            if(success) {
                std::cout << "Welcome email sent successfully to " << recipientEmail << std::endl;
            } else {
                std::cout << "Failed to send welcome email" << std::endl;
            }

        } catch(const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            std::cerr << "Payload: " << payload << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "Error sending welcome email: " << e.what() << std::endl;
        }
    }
    
    /**
     * Send notification for new message in chat room
     * 
     * Triggered by: message.created event
     * Sent to: Sender's email 
     * Purpose: Notify user of new message in room
     */
    void sendMessageNotification(const std::string& payload) {
        std::cout << "\nACTION: Sending new message notification..." << std::endl;

        try {
            json eventData = json::parse(payload);

            // Extract message data
            int messageId = eventData.value("message_id", 0);
            int roomId = eventData.value("room_id", 0);
            std::string senderUsername = eventData.value("sender_username", "Unknown User");
            std::string senderEmail = eventData.value("sender_email", "");
            std::string roomName = eventData.value("room_name", "Unknown Room");
            std::string content = eventData.value("content", "");
            std::string messageType = eventData.value("message_type", "text");

            // Store message content
            std::string message = content;

            std::cout << "Message ID: " << messageId << std::endl;
            std::cout << "Room: " << roomName << " (ID: " << roomId << ")" << std::endl;
            std::cout << "Sender: " << senderUsername << " (" << senderEmail << ")" << std::endl;
            std::cout << "Message: " << message << std::endl;

            // Determine recipient email
            std::string recipientEmail = senderEmail;

            const char* testRecipient = std::getenv("TEST_EMAIL_RECIPIENT");
            if(testRecipient && strlen(testRecipient) > 0) {
                recipientEmail = testRecipient;
                std::cout << "Using test recipient from env: " << recipientEmail << std::endl;
            } else {
                std::cout << "Sending to sender: " << recipientEmail << std::endl;
            }

            // Validate email
            if(recipientEmail.empty() || recipientEmail.find('@') == std::string::npos) {
                std::cerr << "Invalid recipient email, skipping..." << std::endl;
                return;
            }

            std::string subject = "New message in \"" + roomName + "\"";

            std::string body = 
                "Hello!\n\n"
                "You have a new message in one of your chat rooms.\n\n"
                "Room: " + roomName + " (ID: " + std::to_string(roomId) + ")\n"
                "From: " + senderUsername + "\n"
                "Message Type: " + messageType + "\n\n"
                "Message:\n"
                "─────────────────────────────────────\n"
                "\"" + message + "\"\n"
                "─────────────────────────────────────\n\n"
                "---\n"
                "Message ID: " + std::to_string(messageId) + "\n"
                "Timestamp: " + eventData.value("timestamp", "N/A");

            bool success = smtpClient_->sendEmail(recipientEmail, subject, body);
        
            if(success) {
                std::cout << "Message notification email sent successfully to " << recipientEmail << std::endl;
            } else {
                std::cout << "Failed to send message notification email" << std::endl;
            }
        } catch(const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            std::cerr << "Payload: " << payload << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "Error sending message notification email: " << e.what() << std::endl;
        }
    }

    /**
     * Send notification when user is added to a room
     * 
     * Triggered by: user.joined_room event
     * Sent to: User who was added to the room
     * Purpose: Confirm room membership and provide room details
     */
    void sendRoomJoinNotification(const std::string& payload) {
        std::cout << "\nACTION: Sending room join notification..." << std::endl;
        
        try {
            // Parse JSON payload
            json eventData = json::parse(payload);
            
            // Extract room and user data
            int roomId = eventData.value("room_id", 0);
            int userId = eventData.value("user_id", 0);
            std::string roomName = eventData.value("room_name", "Unknown Room");
            std::string username = eventData.value("username", "User");
            std::string userEmail = eventData.value("user_email", "");
            std::string role = eventData.value("role", "member");
            
            std::cout << "Room: " << roomName << " (ID: " << roomId << ")" << std::endl;
            std::cout << "User: " << username << " (ID: " << userId << ")" << std::endl;
            std::cout << "Email: " << userEmail << std::endl;
            std::cout << "Role: " << role << std::endl;
            
            // Validate email
            if(userEmail.empty() || userEmail.find('@') == std::string::npos) {
                std::cerr << "No valid email found in payload, skipping..." << std::endl;
                return;
            }
            
            std::string recipientEmail = userEmail;
            
            std::string subject = "You've been added to \"" + roomName + "\"!";
            
            std::string body = 
                "Hello " + username + "!\n\n"
                "You have been added to a new chat room.\n\n"
                "Room Details:\n"
                "─────────────────────────────────────\n"
                "Name: " + roomName + "\n"
                "Room ID: " + std::to_string(roomId) + "\n"
                "Your Role: " + role + "\n"
                "─────────────────────────────────────\n\n"
                "---\n"
                "User ID: " + std::to_string(userId) + "\n"
                "Email: " + recipientEmail;
            
            bool success = smtpClient_->sendEmail(recipientEmail, subject, body);
            
            if(success) {
                std::cout << "Room join notification sent successfully to " << recipientEmail << "!" << std::endl;
            } else {
                std::cout << "Failed to send room join notification" << std::endl;
            }
            
        } catch(const json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
            std::cerr << "Payload: " << payload << std::endl;
        } catch(const std::exception& e) {
            std::cerr << "Error sending room join notification: " << e.what() << std::endl;
        }
    }

    // Member variables
    std::string queueName_;            // Name of the queue to consume from
    bool connected_;                   // Connection status flag
    amqp_connection_state_t conn_;     // RabbitMQ connection handle
    amqp_socket_t* socket_;            // TCP socket handle
    SMTPClient* smtpClient_;           // SMTP client for sending emails (nullptr = simulation mode)
};

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
    SMTPClient* smtpClient = nullptr;

    if(smtpHost && smtpPortStr && smtpUser && smtpPass) {
        int smtpPort = std::atoi(smtpPortStr);

        std::cout << "Configuring SMTP..." << std::endl;
        std::cout << "Server: " << smtpHost << ":" << smtpPort << std::endl;
        std::cout << "User: " << smtpUser << std::endl;

        smtpClient = new SMTPClient(smtpHost, smtpPort, smtpUser, smtpPass);

        if(smtpClient->isConfigured()) {
            std::cout << "SMTP configured successfully" << std::endl;
        } else {
            std::cerr << "SMTP configuration invalid" << std::endl;
            delete smtpClient;
            smtpClient = nullptr;
        }
    } else {
        std::cout << "SMTP credentials not found in environment" << std::endl;
        std::cout << "Set: SMTP_HOST, SMTP_PORT, SMTP_USER, SMTP_PASSWORD" << std::endl;
        std::cout << "Email sending will be simulated" << std::endl;
    }

    // Get RabbitMQ host from environment 
    const char* rabbitmqHost = std::getenv("RABBITMQ_HOST");
    if(!rabbitmqHost) {
        rabbitmqHost = "localhost";  // Fallback for local development
    }

    // Create RabbitMQ consumer
    RabbitMQConsumer consumer(
        rabbitmqHost,
        5672,
        "chatuser",
        "chatpass",
        "notification_queue",
        smtpClient
    );

    // Check if RabbitMQ connection was successful
    if(!consumer.isConnected()) {
        std::cerr << "Failed to connect to RabbitMQ. Exiting." << std::endl;
        
        if(smtpClient) {
            delete smtpClient;
        }
        
        return 1;
    }

    // Start consuming events (blocks forever until terminated)
    consumer.startConsuming();

    // Cleanup (never reached unless consumer exits)
    if(smtpClient) {
        delete smtpClient;
    }

    return 0;
}
