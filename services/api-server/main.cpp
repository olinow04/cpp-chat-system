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

    // PATCH /api/users/:id - Update user data by ID
    svr.Patch(R"(/api/users/(\d+))", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse user ID from URL
            int userId = std::stoi(req.matches[1]);

            // Parse JSON request body
            json j = json::parse(req.body);

            // Get exisiting user from database
            auto user = db.getUserById(userId);

            // Check if user exists
            if(!user){
                json error = {{"error", "User not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Create updated user object
            User updateUser = *user;

            // Update fields if provided in request
            if(j.contains("email")){
                updateUser.email = j["email"];
            }

            if(j.contains("password")){
                updateUser.password_hash = j["password"]; 
            }

            if(j.contains("is_active")){
                updateUser.is_active = j["is_active"];
            }

            // Update user in database
            bool success = db.updateUser(updateUser);

            // Check if update failed
            if(!success){
                json error = {{"error", "Failed to update user"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            // Return success response with updated user data
            json response = {
                {"id", updateUser.id},
                {"username", updateUser.username},
                {"email", updateUser.email},
                {"is_active", updateUser.is_active},
                {"message", "User updated successfully"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;
          
        } catch(json::parse_error& e){
            // Handle invalid JSON format
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Update user error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    // DELETE /api/users/:id - Delete user by ID
    svr.Delete(R"(/api/users/(\d+))", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse user ID from URL
            int userId = std::stoi(req.matches[1]);

            // Check if user exists
            auto user = db.getUserById(userId);
            
            if(!user){
                json error = {{"error", "User not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Delete user from database
            bool success = db.deleteUser(userId);

            // Check if deletion failed
            if(!success){
                json error = {{"error", "Failed to delete user"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            // Return success response
            json response = {{"message", "User deleted successfully"}};

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Delete user error: " << e.what() << std::endl;
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

// GET /api/rooms/:id/members - Get members of a specific room
svr.Get(R"(/api/rooms/(\d+)/members)", [&db](const httplib::Request& req, httplib::Response& res){
    try {
        // Parse room ID from URL
        int roomId = std::stoi(req.matches[1]);

        // Get room members from database
        auto members = db.getRoomMembers(roomId);

        // Prepare response with member data
        json response = json::array();

        // Populate member data into JSON array
        for(const auto& user : members){
            response.push_back({
                {"id", user.id},
                {"username", user.username},
                {"email", user.email}
            });
        }
        
        // Return member list
        res.set_content(response.dump(), "application/json");
        res.status = 200;
    } catch(const std::exception& e){
        // Handle unexpected errors
        std::cerr << "Get room members error: " << e.what() << std::endl;
        json error = {{"error", "Internal server error"}};
        res.set_content(error.dump(), "application/json");
        res.status = 500;
    }
});

    // POST /api/rooms/:id/members - Add a user to a room
    svr.Post(R"(/api/rooms/(\d+)/members)", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse room ID from URL
            int roomId = std::stoi(req.matches[1]);
            
            // Parse JSON request body
            json j = json::parse(req.body);

            // Validate required fields
            if(!j.contains("user_id")){
                json error = {{"error", "Missing required field: user_id"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }
            
            // Extract user ID and optional role from request
            int userId = j["user_id"];
            std::string role = j.value("role", "member");

            // Check if room exists
            auto room = db.getRoomById(roomId);
            if(!room){
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Check if user exists
            auto user = db.getUserById(userId);
            if(!user){
                json error = {{"error", "User not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Check if user is already in the room
            if(db.isUserInRoom(userId, roomId)){
                json error = {{"error", "User is already a member of the room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 409;
                return;
            }

            // Add user to room in database
            bool success = db.addUserToRoom(userId, roomId, role);

            // Check if adding user failed
            if(!success){
                json error = {{"error", "Failed to add user to room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            // Return success response
            json response = {
                {"message", "User added to room successfully"},
                {"room_id", roomId},
                {"user_id", userId},
                {"role", role}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;
            } catch(json::parse_error& e){
                // Handle invalid JSON format
                json error = {{"error", "Invalid JSON format"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
            } catch(const std::exception& e){
                // Handle unexpected errors
                std::cerr << "Add user to room error: " << e.what() << std::endl;
                json error = {{"error", "Internal server error"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
            }
    });

    // PATCH /api/rooms/:id - Update room data
    svr.Patch(R"(/api/rooms/(\d+))", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse room ID from URL
            int roomId = std::stoi(req.matches[1]);
            
            // Parse JSON request body
            json j = json::parse(req.body);

            // Check if room exists
            auto room = db.getRoomById(roomId);

            if(!room){
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Create updated room object
            Room updateRoom = *room;

            // Update fields if provided in request
            if(j.contains("name")){
                updateRoom.name = j["name"];
            }

            if(j.contains("description")){
                updateRoom.description = j["description"];
            }

             // Update room in database
            bool success = db.updateRoom(updateRoom.id, updateRoom.name, updateRoom.description);

            // Check if update failed
            if(!success){
                json error = {{"error", "Failed to update room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            // Return success response with updated room data
            json response = {
                {"id", updateRoom.id},
                {"name", updateRoom.name},
                {"description", updateRoom.description},
                {"created_by", updateRoom.created_by},
                {"created_at", updateRoom.created_at},
                {"is_private", updateRoom.is_private},
                {"message", "Room updated successfully"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch(json::parse_error& e){
            // Handle invalid JSON format
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Update room error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    // DELETE /api/rooms/:id - Delete a room by ID
    svr.Delete(R"(/api/rooms/(\d+))", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse room ID from URL
            int roomId = std::stoi(req.matches[1]);

            // Check if room exists
            auto room = db.getRoomById(roomId);

            if(!room){
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Delete room from database
            bool success = db.deleteRoom(roomId);

            // Check if deletion failed
            if(!success){
                json error = {{"error", "Failed to delete room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            // Return success response
            json response = {{"message", "Room deleted successfully"}};
            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Delete room error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    // ==== MESSAGE ENDPOINTS ======

    // GET /api/rooms/:room_id/messages - Get messages from a specific room
    svr.Get(R"(/api/rooms/(\d+)/messages)", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse room ID from URL
            int roomId = std::stoi(req.matches[1]);

            // Check if room exists
            auto room = db.getRoomById(roomId);
            if(!room){
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Get paginantion parameters from query string
            int limit = 50;
            int offset = 0;

            if(req.has_param("limit")){
                limit = std::stoi(req.get_param_value("limit"));
            }

            if(req.has_param("offset")){
                offset = std::stoi(req.get_param_value("offset"));
            }

            // Get messages from database
            auto messages = db.getMessagesByRoom(roomId, limit, offset);

            // Prepare response with message data
            json response = json::array();

            // Populate message data into JSON array
            for(const auto& message : messages){
                response.push_back({
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

            // Return message list
            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Get room messages error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    // POST /api/rooms/:room_id/messages - Send a new message to a room
    svr.Post(R"(/api/rooms/(\d+)/messages)", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse room ID from URL
            int roomId = std::stoi(req.matches[1]);

            // Parse JSON request body
            json j = json::parse(req.body);

            // Validate required fields
            if(!j.contains("user_id") || !j.contains("content")){
                json error = {{"error", "Missing required fields: user_id, content"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            // Extract message data from request 
            int userId = j["user_id"];
            std::string content = j["content"];
            std::string messageType = j.value("message_type", "text");

            // Check if room exists 
            auto room = db.getRoomById(roomId);
            if(!room){
                json error = {{"error", "Room not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Check if user exists
            auto user = db.getUserById(userId);
            if(!user){
                json error = {{"error", "User not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Check if user is a member of the room
            if(!db.isUserInRoom(userId, roomId)){
                json error = {{"error", "User is not a member of the room"}};
                res.set_content(error.dump(), "application/json");
                res.status = 403;
                return;
            }

            // Create message in database
            auto createMessage = db.createMessage(roomId, userId, content, messageType);

            // Check if message creation failed
            if(!createMessage){
                json error = {{"error", "Failed to create message"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            // Return success response with message data
            json response = {
                {"id", createMessage->id},
                {"room_id", createMessage->room_id},
                {"user_id", createMessage->user_id},
                {"content", createMessage->content},
                {"message_type", createMessage->message_type},
                {"created_at", createMessage->created_at},
                {"edited_at", createMessage->edited_at},
                {"is_deleted", createMessage->is_deleted},
                {"message", "Message sent successfully"}
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
            std::cerr << "Create message error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    // GET /api/rooms/messages/:id - Get a specific message by ID   
    svr.Get(R"(/api/rooms/messages/(\d+))", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse message ID from URL
            int messageId = std::stoi(req.matches[1]);

            // Get message from database
            auto message = db.getMessageById(messageId);

            // Check if message exists
            if(!message){
                json error = {{"error", "Message not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Prepare response with message data 
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

            // Return message data
            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Get message error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    });

    // PATCH /api/messages/:id - Update a message by ID
    svr.Patch(R"(/api/messages/(\d+))", [&db](const httplib::Request& req, httplib::Response& res){
        try {
            // Parse message ID from URL
            int messageId = std::stoi(req.matches[1]);

            // Parse JSON request body
            json j = json::parse(req.body);

            // Get exisiting message from database
            auto message = db.getMessageById(messageId);

            // Check if message exists
            if(!message){
                json error = {{"error", "Message not found"}};
                res.set_content(error.dump(), "application/json");
                res.status = 404;
                return;
            }

            // Create updated message object
            Message updateMessage = *messsage;

            // Update fields if provided in request
            if(j.contains("content")){
                updateMessage.content = j["content"];
            }

            // Update message in database
            bool success = db.updateMessage(updateMessage.id, updateMessage.content);

            // Check if update failed
            if(!success){
                json error = {{"error", "Failed to update message"}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            // Return success response with updated message data
            json response = {
                {"id", updateMessage.id},
                {"room_id", updateMessage.room_id},
                {"user_id", updateMessage.user_id},
                {"content", updateMessage.content},
                {"message_type", updateMessage.message_type},
                {"created_at", updateMessage.created_at},
                {"edited_at", updateMessage.edited_at},
                {"is_deleted", updateMessage.is_deleted},
                {"message", "Message updated successfully"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch(json::parse_error& e){
            // Handle invalid JSON format
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch(const std::exception& e){
            // Handle unexpected errors
            std::cerr << "Update message error: " << e.what() << std::endl;
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