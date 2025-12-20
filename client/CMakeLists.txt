# Chat Client
add_executable(chat_client
    chat_client.cpp
)

# Installation
install(TARGETS chat_client
    RUNTIME DESTINATION bin
)
