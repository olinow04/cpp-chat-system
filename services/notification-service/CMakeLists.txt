# Notification Service
add_executable(notification_service
    main.cpp
    smtp_client.cpp
)

# Include directories
target_include_directories(notification_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Installation
install(TARGETS notification_service
    RUNTIME DESTINATION bin
)
