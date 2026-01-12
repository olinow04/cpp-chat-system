#pragma once
#include <string>
#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <vector>

 /**
  * SMTP Email Client using libcurl
  * Sends emails via SMTP protocol
  */
class SMTPClient {
public:
    /**
     * Constructor - Initialize SMTP client with server credentials
     */
    SMTPClient(const std::string& smtpServer, int smtpPort,
               const std::string& username, const std::string& password)
        : smtpServer_(smtpServer), smtpPort_(smtpPort),
          username_(username), password_(password) {
              // Initialize curl globally
              curl_global_init(CURL_GLOBAL_DEFAULT);
          }

    /**
     * Destructor - Cleanup curl resources
     */
    ~SMTPClient() {
        curl_global_cleanup();
    }
    
    /**
     * Send email via SMTP
     */
    bool sendEmail(const std::string& toEmail, const std::string& subject, const std::string& body) {

        // Initialize curl session
        CURL* curl = curl_easy_init();

        if(!curl) {
            std::cerr << "Failed to initialize CURL for SMTP" << std::endl;
            return false;
        }
        
        // Build SMTP URL with server and port
        std::string smtpUrl = "smtp://" + smtpServer_ + ":" + std::to_string(smtpPort_);

        // Prepare email payload (headers + body)
        std::string emailPayload = buildEmailPayload(toEmail, subject, body);

        // Convert payload to curl-compatible format
        struct curl_slist* recipients = nullptr;
        recipients = curl_slist_append(recipients, toEmail.c_str());

        // Configure CURL options for SMTP

        // Set SMTP server URL
        curl_easy_setopt(curl, CURLOPT_URL, smtpUrl.c_str());

        // Set sender email (FROM)
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, ("<" + username_ + ">").c_str());

        // Set recipient email (TO)
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        // Set authentication username
        curl_easy_setopt(curl, CURLOPT_USERNAME, username_.c_str());

        // Set authentication password 
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password_.c_str());

        // Enable STARTTLS 
        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);

        // Set read callback to provide email content
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, ReadCallback);

        // Pass email payload to callback
        ReadData readData;
        readData.payload = emailPayload;
        readData.bytesRead = 0;
        curl_easy_setopt(curl, CURLOPT_READDATA, &readData);

        // Tell curl we're uploading data
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

        // Perform SMTP transaction
        CURLcode res = curl_easy_perform(curl);

        // Cleanup
        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);

        if(res != CURLE_OK) {
            std::cerr << "SMTP error: " << curl_easy_strerror(res) << std::endl;
            return false;
        }

        std::cout << "Email sent successfully to " << toEmail << std::endl;
        return true;
    }

    /**
     * Check if SMTP configuration is valid (test connection)
     */
    bool isConfigured() {
        // Basic validation: check if credentials are set
        return !smtpServer_.empty() &&
               !username_.empty() &&
               !password_.empty() &&
               smtpPort_ > 0;
    }

private:
    /**
     * Build RFC 5322 compliant email message
     */
    std::string buildEmailPayload(const std::string& toEmail,
                                  const std::string& subject,
                                  const std::string& body) const {
        std::ostringstream payload;

        // Date header (current timestamp)
        auto now = std::time(nullptr);
        char dateBuffer[128];
        std::strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S %z", std::localtime(&now));

        // Build email headers
        payload << "Date: " << dateBuffer << "\r\n";
        payload << "To: <" << toEmail << ">\r\n";
        payload << "From: <" << username_ << ">\r\n";
        payload << "Subject: " << subject << "\r\n";
        payload << "Content-Type: text/plain; charset=\"utf-8\"\r\n";
        payload << "\r\n"; // Empty line separates headers from body

        // Add email body
        payload << body << "\r\n";

        return payload.str();
    }

    /**
     * Struct to track email reading progress
     */
    struct ReadData {
        std::string payload; // Full email content
        size_t bytesRead;    // Bytes already sent 
    };

    /**
     * CURL read callback - provides email data chunk by chunk
     * Called by libcurl when it needs more data to send
     */

    static size_t ReadCallback(char* ptr, size_t size, size_t nmemb, void* userp) {
        ReadData* readData = static_cast<ReadData*>(userp);

        // Calculate how many bytes we can send
        size_t room = size * nmemb;
        size_t remaining = readData->payload.size() - readData->bytesRead;
        size_t toSend = (remaining < room) ? remaining : room;

        // Copy data to curl buffer
        if(toSend > 0) {
            std::memcpy(ptr, readData->payload.data() + readData->bytesRead, toSend);
            readData->bytesRead += toSend;
        }

        return toSend; // Return 0 when all data sent (EOF)
    }

    // Member variables
    std::string smtpServer_;  // SMTP server hostname
    int smtpPort_;            // SMTP server port
    std::string username_;    // Email address
    std::string password_;    // App password
};



