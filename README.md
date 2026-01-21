# C++ Chat System

Microservice-based chat system with REST API, email notifications, and automatic translations.

## Features

- **RESTful API** - User, room, and message management
- **Asynchronous notifications** - Email notifications via RabbitMQ
- **Translations** - Automatic message translation (LibreTranslate)
- **Security** - Password hashing (bcrypt), data validation
- **Event-driven** - Microservices communicating through message broker

## Tech Stack

**Backend:** C++, CMake, cpp-httplib, nlohmann/json, libpqxx  
**Database:** PostgreSQL 15  
**Messaging:** RabbitMQ, Redis  
**Services:** LibreTranslate API, Custom SMTP Client  
**DevOps:** Docker, Docker Compose

## Architecture

```
Client (HTTP) ──▶ API Server (REST) ──▶ PostgreSQL
                      │
                      ├──▶ LibreTranslate (Translations)
                      │
                      ▼
                  RabbitMQ (Events)
                      │
                      ▼
              Notification Service ──▶ SMTP Server
```

**Microservices:**
- **API Server** - REST endpoints, business logic, database access
- **Notification Service** - Asynchronous email processing via RabbitMQ
- **PostgreSQL** - Users, rooms, messages storage
- **RabbitMQ** - Event queue (user.registered, message.created, user.joined_room)
- **LibreTranslate** - Translation service

## Quick Start

### Prerequisites
```bash
# macOS
brew install cmake postgresql libpqxx rabbitmq-c

# Linux (Ubuntu/Debian)
sudo apt install build-essential cmake libpq-dev libpqxx-dev librabbitmq-dev
```

### Build & Run
```bash
# Clone repository
git clone https://github.com/olinow04/cpp-chat-system.git
cd cpp-chat-system

# Build project
mkdir build && cd build
cmake .. && cmake --build .
cd ..

# Start services with Docker
docker-compose up -d postgres rabbitmq redis libretranslate

# Run API Server
./build/bin/api_server

# Run Notification Service (in another terminal)
./build/bin/notification_service
```

### Test API
```bash
curl http://localhost:8080/hi
# Response: Hello World!
```

## API Endpoints

Base URL: `http://localhost:8080`

### Users

| Method | Endpoint | Description | Body |
|--------|----------|-------------|------|
| POST | `/api/register` | Register new user | `{username, email, password}` |
| POST | `/api/login` | User login | `{username, password}` |
| GET | `/api/users` | List all users | - |
| GET | `/api/users/:id` | Get user by ID | - |
| PATCH | `/api/users/:id` | Update user | `{email?, is_active?}` |
| DELETE | `/api/users/:id` | Delete user | - |

### Rooms

| Method | Endpoint | Description | Body |
|--------|----------|-------------|------|
| GET | `/api/rooms` | List all rooms | - |
| GET | `/api/rooms/:id` | Get room by ID | - |
| POST | `/api/rooms` | Create new room | `{name, description?, is_private?}` |
| GET | `/api/rooms/user/:id` | Get user's rooms | - |
| GET | `/api/rooms/:id/members` | Get room members | - |
| POST | `/api/rooms/:id/members` | Add user to room | `{user_id}` |
| PATCH | `/api/rooms/:id` | Update room | `{name?, description?}` |
| DELETE | `/api/rooms/:id` | Delete room | - |
| DELETE | `/api/rooms/:id/members/:userId` | Remove user from room | - |

### Messages

| Method | Endpoint | Description | Body |
|--------|----------|-------------|------|
| GET | `/api/rooms/:id/messages` | Get room messages | Query: `?limit=50&offset=0` |
| POST | `/api/rooms/:id/messages` | Send message | `{user_id, content}` |
| GET | `/api/rooms/messages/:id` | Get message by ID | - |
| PATCH | `/api/messages/:id` | Update message | `{content}` |
| DELETE | `/api/messages/:id` | Delete message | - |

### Translation

| Method | Endpoint | Description | Body |
|--------|----------|-------------|------|
| POST | `/api/translate` | Translate text | `{text, source, target}` |

**Example Requests:**
```bash
# Register user
curl -X POST http://localhost:8080/api/register \
  -H "Content-Type: application/json" \
  -d '{"username":"john","email":"john@example.com","password":"StrongPassword123!"}'

# Create room
curl -X POST http://localhost:8080/api/rooms \
  -H "Content-Type: application/json" \
  -d '{"name":"Tech Talk","description":"Discuss tech"}'

# Send message
curl -X POST http://localhost:8080/api/rooms/1/messages \
  -H "Content-Type: application/json" \
  -d '{"user_id":1,"content":"Hello everyone!"}'

# Translate text
curl -X POST http://localhost:8080/api/translate \
  -H "Content-Type: application/json" \
  -d '{"text":"Hello world","source":"en","target":"pl"}'
```

## Project Structure

```
cpp-chat-system/
├── CMakeLists.txt              # Root build configuration
├── docker-compose.yml          # Service orchestration
│
├── services/
│   ├── api-server/
│   │   ├── CMakeLists.txt
│   │   ├── main.cpp           # Application entry point
│   │   ├── src/
│   │   │   ├── database/
│   │   │   │   ├── Database.h         # Database interface
│   │   │   │   └── Database.cpp       # PostgreSQL implementation
│   │   │   ├── handlers/
│   │   │   │   ├── UserHandlers.hpp   # User endpoint handlers
│   │   │   │   ├── RoomHandlers.hpp   # Room endpoint handlers
│   │   │   │   ├── MessageHandlers.hpp # Message endpoint handlers
│   │   │   │   └── TranslationHandlers.hpp # Translation handlers
│   │   │   ├── clients/
│   │   │   │   ├── RabbitMQClient.hpp # Event publisher
│   │   │   │   └── TranslationClient.hpp # LibreTranslate client
│   │   │   ├── utils/
│   │   │   │   ├── PasswordHelper.hpp # Password hashing
│   │   │   │   └── Validator.hpp      # Input validation
│   │   │   └── routing/
│   │   │       └── HTTPRouter.hpp     # Route configuration
│   │   └── external/
│   │       ├── httplib.h         # HTTP server library
│   │       └── json.hpp          # JSON library
│   │
│   └── notification-service/
│       ├── CMakeLists.txt
│       ├── main.cpp           # Service entry point
│       ├── src/
│       │   ├── consumers/
│       │   │   └── RabbitMQConsumer.hpp # Event consumer
│       │   └── clients/
│       │       └── SMTPClient.hpp     # Email sender (libcurl)
│       └── external/
│           └── json.hpp          # JSON library
│
├── database/
│   └── init.sql              # Database schema & initialization
│
└── build/                    # Generated build files
    └── bin/
        ├── api_server
        └── notification_service
```

## Technical Implementation

- **Modular Architecture** - Separated handler classes for each domain
- **Database Layer** - Custom PostgreSQL wrapper with libpqxx
- **Event Publishing** - RabbitMQ integration for async operations
- **HTTP Server** - cpp-httplib with RESTful routing and CORS
- **SMTP Client** - Custom implementation using libcurl with STARTTLS
- **Input Validation** - Comprehensive data validation
- **Error Handling** - Structured JSON error responses

## License

This is an **educational project** created for learning purposes.

## Author

**Oliwier Nowak** - [GitHub](https://github.com/olinow04)






