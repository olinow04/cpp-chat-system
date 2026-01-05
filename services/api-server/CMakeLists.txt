# API Server
add_executable(api_server
    main.cpp
    db.cpp
)

find_package(OpenSSL REQUIRED)

# Include directories
target_include_directories(api_server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${PostgreSQL_INCLUDE_DIRS}
        ${PQXX_INCLUDE_DIR}
        ${OPENSSL_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(api_server
    PRIVATE
        ${PostgreSQL_LIBRARIES}
        ${PQXX_LIBRARY}
        OpenSSL::Crypto 
        OpenSSL::SSL 
)

target_compile_options(api_server PRIVATE
    ${PQXX_CFLAGS_OTHER}                
)

# Installation
install(TARGETS api_server
    RUNTIME DESTINATION bin
)
