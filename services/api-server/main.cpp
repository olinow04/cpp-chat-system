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

    // POST /api/login - User login
    svr.Post("/api/login", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse JSON request body
            json j = json::parse(req.body);

            // Validate required fields
            if(!j.contains("username") || !j.contains("password")){
                json error = {{"error", "Missing required fields: username, password"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            // Extract credentials from request
            std::string username = j["username"];
            std::string password = j["password"];

            // Get user from database
            auto user = db.getUserByUsername(username);

            // Check if user exists
            if(!user){
                json error = {{"error", "Invalid credentials"}};
                res.set_content(error.dump(), "application/json");
                res.status = 401;
                return;
            }
            // Verify password (TODO: Use bcrypt verify)
            if(user->password_hash != password){ 
                json error = {{"error", "Invalid credentials"}};
                res.set_content(error.dump(), "application/json");
                res.status = 401;
                return;
            }

            // Check if account is active
            if(!user->is_active){
                json error = {{"error", "Account is disabled"}};
                res.set_content(error.dump(), "application/json");
                res.status = 403;
                return;
            }

            // Update last login timestamp
            db.updateLastLogin(user->id);

            // Return success response with user data (without password)
            json response = {
                {"id", user->id},
                {"username", user->username},
                {"email", user->email},
                {"message", "Login successful"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch(json::parse_error& e){
            // Handle invalid JSON format
            json error = {{"error", "Invalid JSON"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Login error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });


    // GET /api/users/:id - Get user by ID
    svr.Get(R"(/api/users/(\d+))", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse user ID from URL
            int userId = std::stoi(req.matches[1]);

            // Get user from database
            auto user = db.getUserById(userId);

            // Check if user exists
            if(!user){
                json error = {{"error", "User not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Prepare response with user data
            json response = {
                {"id", user->id},
                {"username", user->username},
                {"email", user->email}
            };

            // Return user data
            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Get user error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    // ===== ROOM ENDPOINTS ======

    // GET /api/rooms - Get list of all chat rooms
    svr.Get("/api/rooms" , [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Fetch all chat rooms from database
            auto rooms = db.getAllRooms();
            // Prepare JSON response
            json response = json::array();
            // Populate room data into JSON array
            for(const auto& room : rooms){
                response.push_back({
                    {"id", room.id},
                    {"name", room.name},
                    {"description", room.description},
                    {"created_by", room.created_by},
                    {"created_at", room.created_at},
                    {"is_private", room.is_private}
                });
            }
            // Return room list
            res.set_content(response.dump(), "application/json");
            // Success status
            res.status = 200;
        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Get rooms error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    // GET /api/rooms/: id - Get room details by ID
    svr.Get(R"(/api/rooms/(\d+))", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse room ID from URL
            int roomId = std::stoi(req.matches[1]);
            
            // Get room from database
            auto room = db.getRoomById(roomId);

            // Check if room exists
            if(!room){
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Prepare response with room data
            json response = {
                {"id", room->id},
                {"name", room->name},
                {"description", room->description},
                {"created_by", room->created_by},
                {"created_at", room->created_at},
                {"is_private", room->is_private}
            };

            // Return room data
            res.set_content(response.dump(), "application/json");
            res.status = 200;
        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Get room error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    // POST /api/rooms - Create a new chat room
    svr.Post("/api/rooms", [&db](const httplib::Request& req, httplib::Response& res){
        try{
            // Parse JSON request body
            json j = json::parse(req.body);

            // Validate required fields
            if(!j.contains("name") || !j.contains("description") || !j.contains("created_by")){
                json error = {{"error", "Missing required fields: name, description, created_by"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            // Extract room data from request
            std::string name = j["name"];
            std::string description = j["description"];
            int created_by = j["created_by"];
            bool is_private = j.value("is_private", false);

            // Create room in database
            auto createRoom = db.createRoom(name, description, created_by, is_private);

            // Check if room creation failed
            if(!createRoom){
                json error = {{"error", "Failed to create room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            // Return success response with room data
            json response = {
                {"id", createRoom->id},
                {"name", createRoom->name},
                {"description", createRoom->description},
                {"created_by", createRoom->created_by},
                {"created_at", createRoom->created_at},
                {"is_private", createRoom->is_private},
                {"message", "Room created successfully"}
            };

            // Send response
            res.set_content(response.dump(), "application/json");
            res.status = 201;
        } catch(json::parse_error& e){
            // Handle invalid JSON format
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Create room error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    // GET /api/rooms/user/:id - Get rooms for a specific user
    svr.Get(R"(/api/rooms/user/(\d+))", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse user ID from URL
            int userId = std::stoi(req.matches[1]);

            // Get rooms for the user from database
            auto rooms = db.getRoomsByUser(userId);

            // Prepare response with room data
            json response = json::array();

            // Populate room data into JSON array
            for(const auto& room : rooms){
                response.push_back({
                    {"id", room.id},
                    {"name", room.name},
                    {"description", room.description},
                    {"created_by", room.created_by},
                    {"created_at", room.created_at},
                    {"is_private", room.is_private}
                });
            }

            // Return room list
            res.set_content(response.dump(), "application/json");
            res.status = 200;
    } catch(const std::exception& e){
        // Handle unexpected errors
        std::cerr << "Get user rooms error: " << e.what() << std::endl;
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