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

    // ====== USER ENDPOINTS ======

    // POST /api/register - Register a new user
    svr.Post("/api/register", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse JSON request body
            json j = json::parse(req.body);

            // Validate required fields
            if(!j.contains("username") || !j.contains("email") || !j.contains("password")){
                json error = {{"error", "Missing required fields: username, email, password"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            // Extract user data from request
            std::string username = j["username"];
            std::string email = j["email"];
            std::string password = j["password"];

            // Check if username already exists
            auto existingUser = db.getUserByUsername(username);
            if(existingUser){
                json error = {{"error", "Username already exists"}};
                res.set_content(error.dump(), "application/json");
                res.status = 409;
                return;
            }

            // Create new user object
            User newUser;
            newUser.username = username;
            newUser.email = email;
            newUser.password_hash = password; // TODO: Add bcrypt password hashing
            newUser.is_active = true;

            // Save user to database
            auto created = db.createUser(newUser);

            // Check if user creation failed
            if(!created){
                json error = {{"error", "Failed to create user"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            // Return success response with user data
            json response = {
                {"id", created->id},
                {"username", created->username},
                {"email", created->email},
                {"message", "User registered successfully"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 201;

    } catch(json::parse_error& e){
            // Handle invalid JSON format
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
    } catch(const std::exception& e){
        // Handle unexpected errors
        std::cerr << "Register error: " << e.what() << std::endl;
        json error = {{"error", "Internal server error"}};
        res.set_content(error.dump(), "application/json");
        res.status = 500;
    }
});

    // Start the HTTP server and listen on all interfaces at port 8080
    std::cout << "Starting server on port 8080..." << std::endl;
    svr.listen("0.0.0.0", 8080);

    return 0;
}