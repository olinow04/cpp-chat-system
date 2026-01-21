#pragma once
#include <string>
#include <curl/curl.h>
#include <../external/json.hpp>
#include <iostream>

using json = nlohmann::json;

/**
 * LibreTranslate API Client
 * Translates text between languages using LibreTranslate API
 */
class TranslationClient {
public:
    /**
     * Constructor - sets up LibreTranslate API endpoint
     */
    TranslationClient(const std::string& apiUrl = "http://localhost:5001")
        : apiUrl_(apiUrl) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }

    /**
     * Destructor - cleanup curl
     */    
    ~TranslationClient() {
        curl_global_cleanup();
    }

    /**
     * Translate text from source language to target language
     */
    std::string translate(const std::string& text, const std::string& sourceLang, const std::string& targetLang) {
        // Initialize curl session
        CURL* curl = curl_easy_init();
        if(!curl) {
            std::cerr << "Failed to initialize CURL" << std::endl;
            return "";
        }

        // Initialize curl session
        json payload = {
            {"q", text},
            {"source", sourceLang},
            {"target", targetLang},    
        };
        std::string jsonPayload = payload.dump();

        // Response buffer
        std::string responseBuffer;

        std::string url = apiUrl_ + "/translate";

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());

        // Set headers
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Set write callback to capture response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);

        // Set timeout (5 seconds)
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

        // Perform the request
        CURLcode res = curl_easy_perform(curl);

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        // Check for errors
        if(res != CURLE_OK) {
            std::cerr << "CURL request failed: " << curl_easy_strerror(res) << std::endl;
            return "";
        }

        // Parse JSON response
        try {
            json response = json::parse(responseBuffer);

            if(response.contains("translatedText")) {
                return response["translatedText"];
            } else {
                std::cerr << "Translation API error: " << response["error"] << std::endl;
                return "";
            }
        } catch(const json::parse_error& e) {
            std::cerr << "Json parse error: " << e.what() << std::endl;
            std::cerr << "Response: " << responseBuffer << std::endl;
            return "";
        }
    }

    /**
     * Auto-detect source language and translate to target
     */
    std::string translateAuto(const std::string& text, const std::string& targetLang) {
        return translate(text, "auto", targetLang);
    }
        
    /**
     * Check if API is available
     */
    bool isAvailable() {
        CURL* curl = curl_easy_init();
        if(!curl) {
            return false;
        }
        
        std::string url = apiUrl_ + "/languages";
        std::string responseBuffer;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        return (res == CURLE_OK);
    }

private:

    /**
     * CURL write callback - accumulates response data
     */
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
        size_t totalSize = size * nmemb;
        std::string* buffer = static_cast<std::string*>(userp);
        buffer->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    std::string apiUrl_; // LibreTranslate API base URL
 };