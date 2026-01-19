#pragma once
#include <string>
#include <iostream>
#include <ctime>
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>
#include "json.hpp"

using json = nlohmann::json;

/**
 * Simple RabbitMQ Client using rabbitmq-c
 * Publishes events to message queue
 */
class RabbitMQClient {
public:
    /**
     * Constructor - connects to RabbitMQ
     */
    RabbitMQClient(const std::string& host, int port, const std::string& user, const std::string& password) 
        : connected_(false), conn_(nullptr), socket_(nullptr) {
        
        try {
            // Create connection
            conn_ = amqp_new_connection();
            socket_ = amqp_tcp_socket_new(conn_);
            
            if (!socket_) {
                std::cerr << "Failed to create TCP socket" << std::endl;
                return;
            }
            
            // Open socket
            int status = amqp_socket_open(socket_, host.c_str(), port);
            if (status) {
                std::cerr << "Failed to open socket to RabbitMQ" << std::endl;
                return;
            }
            
            // Login
            amqp_rpc_reply_t reply = amqp_login(
                conn_, 
                "/",           // vhost
                0,             // channel_max
                131072,        // frame_max
                0,             // heartbeat
                AMQP_SASL_METHOD_PLAIN,
                user.c_str(),
                password.c_str()
            );
            
            if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
                std::cerr << "RabbitMQ login failed" << std::endl;
                return;
            }
            
            // Open channel
            amqp_channel_open(conn_, 1);
            reply = amqp_get_rpc_reply(conn_);
            
            if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
                std::cerr << "Failed to open channel" << std::endl;
                return;
            }
            
            // Declare exchange
            amqp_exchange_declare(
                conn_,
                1,
                amqp_cstring_bytes("chat_events"),
                amqp_cstring_bytes("topic"),
                0,  // passive
                1,  // durable
                0,  // auto_delete
                0,  // internal
                amqp_empty_table
            );
            
            reply = amqp_get_rpc_reply(conn_);
            if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
                std::cerr << "Failed to declare exchange" << std::endl;
                return;
            }
            
            connected_ = true;
            std::cout << "Connected to RabbitMQ at " << host << ":" << port << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "RabbitMQ connection error: " << e.what() << std::endl;
            connected_ = false;
        }
    }
    
    /**
     * Destructor - cleanup
     */
    ~RabbitMQClient() {
        if (conn_) {
            amqp_channel_close(conn_, 1, AMQP_REPLY_SUCCESS);
            amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
            amqp_destroy_connection(conn_);
        }
    }
    
    /**
     * Publish event to RabbitMQ
     */
    void publishEvent(const std::string& routingKey, const json& eventData) {
        if (!connected_ || !conn_) {
            std::cerr << "RabbitMQ not connected" << std::endl;
            return;
        }
        
        try {
            // Convert JSON to string
            std::string messageBody = eventData.dump();
            
            // Prepare message properties
            amqp_basic_properties_t props;
            props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
            props.content_type = amqp_cstring_bytes("application/json");
            props.delivery_mode = 2;  // persistent
            
            // Publish message
            int result = amqp_basic_publish(
                conn_,
                1,  // channel
                amqp_cstring_bytes("chat_events"),
                amqp_cstring_bytes(routingKey.c_str()),
                0,  // mandatory
                0,  // immediate
                &props,
                amqp_cstring_bytes(messageBody.c_str())
            );
            
            if (result < 0) {
                std::cerr << "Failed to publish message" << std::endl;
                return;
            }
            
            std::cout << "Published event: " << routingKey << " -> " << messageBody.substr(0, 100) << "..." << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Failed to publish event: " << e.what() << std::endl;
        }
    }
    
    /**
     * Check if connected
     */
    bool isConnected() const {
        return connected_;
    }

private:
    bool connected_;
    amqp_connection_state_t conn_;
    amqp_socket_t* socket_;
};