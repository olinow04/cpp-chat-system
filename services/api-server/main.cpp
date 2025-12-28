/**
 * API Server - REST API for Chat System
 * Provides HTTP endpoints for user authentication, room management, and messaging
 */

#include <iostream>
#include <string>
#include <memory>

#include "httplib.h"      // cpp-httplib: HTTP server library
#include "json.hpp"       // nlohmann/json: JSON parsing library
#include "db.h"           // Database access layer           

using json = nlohmann::json;

int main() {
    // Initialize HTTP server
    httplib::Server svr;

    // Connect to PostgreSQL database
    Database db("host=localhost port=5432 dbname=chatdb user=chatuser password=chatpass");

    if(!db.connect()){
        std::cerr << "Failed to connect to database. Exiting." << std:: endl;
        return 1;
    }

    std::cout << "Connected to database successfully." << std::endl;

    // Configure CORS (Cross-Origin Resource Sharing) headers
    svr.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });

    // Test endpoint to verify server is running
    svr.Get("/hi", [](const httplib::Request&, httplib::Response& res){
        res.set_content("Hello World!", "text/plain");
    });

    // Start the HTTP server and listen on all interfaces at port 8080
    std::cout << "Starting server on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}