# Notification Service - Email notification microservice

# Create executable
add_executable(notification_service
    main.cpp
)

# Find required packages
find_package(CURL REQUIRED)

# Find RabbitMQ-C library
find_library(RABBITMQ_C_LIBRARY NAMES rabbitmq REQUIRED)
find_path(RABBITMQ_C_INCLUDE_DIR NAMES rabbitmq-c/amqp.h REQUIRED)

# Include directories
target_include_directories(notification_service
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external
        ${RABBITMQ_C_INCLUDE_DIR}
        ${CURL_INCLUDE_DIRS} # libcurl includes
)

# Link libraries
target_link_libraries(notification_service
    PRIVATE
        ${RABBITMQ_C_LIBRARY}
        CURL::libcurl # libcurl library
)

# Installation
install(TARGETS notification_service
    RUNTIME DESTINATION bin
)