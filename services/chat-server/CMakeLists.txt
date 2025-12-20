# Chat Server
add_executable(chat_server
    main.cpp
    chat_room.cpp
    message_handler.cpp
)

# Include directories
target_include_directories(chat_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Installation
install(TARGETS chat_server
    RUNTIME DESTINATION bin
)
