# API Server
add_executable(api_server
    main.cpp
    db.cpp
    user_handler.cpp
)

# Include directories
target_include_directories(api_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Installation
install(TARGETS api_server
    RUNTIME DESTINATION bin
)
