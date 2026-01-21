#pragma once
#include <string>
#include <regex>


/**
 * Input validation helpers 
 */
class Validator {
public:
    /**
     * Validate email format
     */
    static bool isValidEmail(const std::string& email) {
        const std::regex pattern(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        return std::regex_match(email, pattern);
    }
    
    /**
     * Validate password strength
     * At least 8 characters, contains letter and number
     */
    static bool isValidPassword(const std::string& password) {
        if(password.length() < 8) return false;
        
        bool hasLetter = false;
        bool hasDigit = false;
        
        for(char c : password) {
            if(std::isalpha(c)) hasLetter = true;
            if(std::isdigit(c)) hasDigit = true;
        }
        
        return hasLetter && hasDigit;
    }
    
    /**
     * Validate username
     * 3-20 characters, alphanumeric and underscore only
     */
    static bool isValidUsername(const std::string& username) {
        if(username.length() < 3 || username.length() > 20) return false;
        
        const std::regex pattern(R"(^[a-zA-Z0-9_]+$)");
        return std::regex_match(username, pattern);
    }
    
    /**
     * Validate room name
     * 1-100 characters, not empty
     */
    static bool isValidRoomName(const std::string& name) {
        return !name.empty() && name.length() <= 100;
    }
    
    /**
     * Validate message content
     * Not empty, max 1000 characters
     */
    static bool isValidMessageContent(const std::string& content) {
        return !content.empty() && content.length() <= 1000;
    }

    /**
     * Validate room description
     * Max 500 characters
     */
    static bool isValidRoomDescription(const std::string& description) {
        return description.length() <= 500;
    }
};