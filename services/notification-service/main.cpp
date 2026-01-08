/**
 * Notification Service - Email Notification Microservice
 * Consumes events from RabbitMQ and sends email notifications
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h> 

/**
 * RabbitMQ Consumer Class
 * Handles connection, queue setup, and event consumption
 */
class RabbitMQConsumer {
public:
    /**
     * Constructor - Establishes connection to RabbitMQ and sets up queue
     */
    RabbitMQConsumer(const std::string& host, int port,
                     const std::string& user, const std::string& password,
                     const std::string& queueName)
        : queueName_(queueName), connected_(false), conn_(nullptr), socket_(nullptr) {

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
            amqp_rpc_reply_t reply = amqp_login(
                conn_,
                "/",                    // vhost (virtual host)
                0,                      // channel max (0 = no limit)         
                131072,                 // frame_max (max frame size in bytes)
                0,                      // heartbeat (0 = disabled)
                AMQP_SASL_METHOD_PLAIN, // authentication method
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
            
            // Declare exchange
            std::cout << "Declaring exchange 'chat_events'..." << std::endl;
            amqp_exchange_declare(
                conn_,
                1,                                      // channel
                amqp_cstring_bytes("chat_events"),      // exchange name
                amqp_cstring_bytes("topic"),            // exchange type (topic allows routing by keys)
                0,                                      // passive (0 = create if not exists)
                1,                                      // durable (1 = survive RabbitMQ restart)
                0,                                      // auto_delete (0 = don't delete when unused)      
                0,                                      // internal (0 = clients can publish)
                amqp_empty_table                        // arguments
            );

            reply = amqp_get_rpc_reply(conn_);
            if(reply.reply_type != AMQP_RESPONSE_NORMAL) {
                std::cerr << "Failed to declare exchange" << std::endl;
                return;
            }
            
            // Declare queue for storing events
            amqp_queue_declare(
                conn_,
                1,                                      // channel
                amqp_cstring_bytes(queueName_.c_str()), // queue name
                0,                                      // passive (0 = create if not exists)
                1,                                      // durable (1 = survive restart)
                0,                                      // exclusive (0 = shared, not private)
                0,                                      // auto_delete (0 = don't delete when consumer disconnects)
                amqp_empty_table                        // arguments
            );


            reply = amqp_get_rpc_reply(conn_);
            if(reply.reply_type != AMQP_RESPONSE_NORMAL) {
                std::cerr << "Failed to declare queue" << std::endl;
                return;
            }

            // Bind queue to exchange with routing keys
            std::cout << "Binding queue to exchange with routing keys..." << std::endl;

            // Bind routing key:  user.registered
            amqp_queue_bind(
                conn_,
                1,                                       // channel
                amqp_cstring_bytes(queueName_.c_str()),  // queue
                amqp_cstring_bytes("chat_events"),       // exchange
                amqp_cstring_bytes("user.registered"),   // routing key
                amqp_empty_table                         // arguments
            );
            std::cout << "Bound to: user.registered" << std::endl;

            // Bind routing key:  message.created
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
            std::cout << "Starting to consume messages..." << std::endl;
            amqp_basic_consume(
                conn_,
                1,                                      // channel
                amqp_cstring_bytes(queueName_.c_str()), // queue
                amqp_empty_bytes,                       // consumer_tag (auto-generated)
                0,                                      // no_local (0 = receive own messages)
                1,                                      // no_ack (1 = auto-acknowledge, 0 = manual ack)
                0,                                      // exclusive (0 = shared consumer)
                amqp_empty_table                        // arguments
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
     * Destructor - Clean up RabbitMQ connection
     */
    ~RabbitMQConsumer() {
        if(conn_) {
            // Close channel gracefully
            amqp_channel_close(conn_, 1, AMQP_REPLY_SUCCESS);
            // Close connection gracefully
            amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
            // Free memory
            amqp_destroy_connection(conn_);
        }
    }

    /**
     * Start consuming messages in infinite loop (blocking)
     * This function runs until the program is terminated
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

            // Release unused memory buffers
            amqp_maybe_release_buffers(conn_);

            // Set timeout for waiting for messages (5 seconds)
            struct timeval timeout;
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            // Wait for a message (blocks until message arrives or timeout)
            amqp_rpc_reply_t result = amqp_consume_message(conn_, &envelope, &timeout, 0);

            if(result.reply_type == AMQP_RESPONSE_NORMAL) {
                // Message received successfully! 

                // Extract message body (payload as string)
                std::string messageBody(
                    static_cast<char*>(envelope.message.body.bytes),
                    envelope.message.body.len
                );

                // Extract routing key
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
                    std::cout << "["<< getCurrentTime() << "] No messages (timeout), waiting..." << std::endl;
                } else {
                    // Other error
                    std::cerr << "Error consuming message:" << std::endl;
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
         * Process received event and simulate email sending
         */
    void processEvent(const std::string& routingKey, const std::string& payload) {
        std::cout << "\n ========= NEW EVENT =========" << std::endl;
        std::cout << "Time: " << getCurrentTime() << std::endl;
        std::cout << "Routing Key: " << routingKey << std::endl;
        std::cout << "Payload: " << payload << std::endl;
        std::cout << "=============================\n" << std::endl;

        // Route event to appropriate handler based on routing key
        if(routingKey == "user.registered") {
            // Handle user registration event
            std::cout << "\n Action: Sending welcome email..." << std::endl;
            std::cout << "To: [user email from payload]" << std::endl;
            std::cout << "Subject: Welcome to Chat System!" << std::endl;
            std::cout << "Body: Thank you for registering! Your account is ready!" << std::endl;
            simulateEmailDelay();
            std::cout << "Welcome email sent successfully!\n" << std::endl;

        } else if(routingKey == "message.created") {
            // Handle new message event
            std::cout << "\n Action: Sending new message notification..." << std::endl;
            std::cout << "To: [offline room members]" << std::endl;
            std::cout << "Subject: New Message in your Chat Room" << std::endl;
            std::cout << "Body: You have a new message from [sender]" << std::endl;
            simulateEmailDelay();
            std::cout << "New message notification sent successfully!\n" << std::endl;

        } else if(routingKey == "user.joined_room") {
            // Handle user joined room event
            std::cout << "\n Action: Sending room join notification..." << std::endl;
            std::cout << "To: [user email from payload]" << std::endl;
            std::cout << "Subject: You've joined a new room!" << std::endl;
            std::cout << "Body: Welcome to [room name]. Start chatting now!" << std::endl;
            simulateEmailDelay();
            std::cout << "Room join notification sent successfully!\n" << std::endl;
        } else {
            // Unknown event type
            std::cout << "\nUnknown event type: " << routingKey << std::endl;
            std::cout << "Skipping notification...\n" << std::endl;
        }
    }
    
    /**
     * Get current system time as formatted string
     */
    std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::string timeStr = std::ctime(&time);
        timeStr.pop_back();
        return timeStr;
    }

    /**
     * Simulate email sending delay (1. 5 seconds)
     * Represents time needed for SMTP communication
     */
    void simulateEmailDelay() {
        std::cout << "Sending..." << std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        std::cout << " Done!" << std::endl;
    }

    // Member variables
    std::string queueName_;            // Name of the queue to consume from
    bool connected_;                   // Connection status flag
    amqp_connection_state_t conn_;    // RabbitMQ connection handle
    amqp_socket_t* socket_;           // TCP socket handle
};

/**
 * Main function - Entry point for notification service
 */
int main() {
    std::cout << "Starting Notification Service..." << std::endl;

    // Create RabbitMQ consumer instance
    RabbitMQConsumer consumer(
        "localhost",           // RabbitMQ hostname
        5672,                  // RabbitMQ port
        "chatuser",            // Username
        "chatpass",            // Password
        "notification_queue"   // Queue name
    );

    // Check if connection was successful
    if(!consumer.isConnected()) {
        std::cerr << "Failed to connect to RabbitMQ. Exiting." << std::endl;
        return 1;
    }

    // Start consuming events (blocks forever)
    consumer.startConsuming();

    return 0;
}
    
