#pragma once

#include <iostream>
#include <string>
#include <set>
#include <vector>
#include "../external/httplib.h"
#include "../external/json.hpp"
#include "../clients/TranslationClient.hpp"

using json = nlohmann::json;

/**
 * Translation-related HTTP Request Handlers
 * Handles text translation between languages
 */
class TranslationHandlers {
private: 
    TranslationClient& translationClient_;

    static std::vector<std::string> validateAllowedFields(
        const json& j,
        const std::set<std::string>& allowedFields
    ) {
        std::vector<std::string> invalidFields;
        
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (allowedFields.find(it.key()) == allowedFields.end()) {
                invalidFields.emplace_back(it.key());
            }
        }
        
        return invalidFields;
    }

    static void sendInvalidFieldsError(
        httplib::Response& res,
        const std::vector<std::string>& invalidFields,
        const std::set<std::string>& allowedFields
    ) {
        std::string fieldsList;
        for (size_t i = 0; i < invalidFields.size(); ++i) {
            if (i > 0) {
                fieldsList += ", ";
            }
            fieldsList += "'" + invalidFields[i] + "'";
        }
        
        json error = {
            {"error", "Invalid fields: " + fieldsList},
            {"allowed_fields", allowedFields}
        };
        res.set_content(error.dump(), "application/json");
        res.status = 400;
    }

public:
    TranslationHandlers(TranslationClient& translationClient)
        : translationClient_(translationClient) {
    }

    /**
     * POST /api/translate - Translate text between languages
     */
    void translateText(const httplib::Request& req, httplib::Response& res) {
        try {
            json j = json::parse(req.body);

            static const std::set<std::string> allowedFields = {
                "text", "source_lang", "target_lang"
            };

            auto invalidFields = validateAllowedFields(j, allowedFields);
            if (!invalidFields.empty()) {
                sendInvalidFieldsError(res, invalidFields, allowedFields);
                return;
            }

            if (!j.contains("text") || !j.contains("target_lang")) {
                json error = {{"error", "Missing required fields: text, target_lang"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            constexpr int MAX_TEXT_LENGTH = 5000;
            constexpr int LANG_CODE_LENGTH = 2;

            const std::string& text = j["text"].get_ref<const std::string&>();
            const std::string sourceLang = j.value("source_lang", "auto");
            const std::string& targetLang = j["target_lang"].get_ref<const std::string&>();

            if (text.empty() || text.length() > MAX_TEXT_LENGTH) {
                json error = {{"error", "Text must be between 1 and 5000 characters"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            if (targetLang.length() != LANG_CODE_LENGTH || (sourceLang != "auto" && sourceLang.length() != LANG_CODE_LENGTH)) {
                json error = {{"error", "Invalid language code format (use 2-letter ISO 639-1 codes)"}};
                res.set_content(error.dump(), "application/json");
                res.status = 400;
                return;
            }

            std::string translatedText = (sourceLang == "auto")
                ? translationClient_.translateAuto(text, targetLang)
                : translationClient_.translate(text, sourceLang, targetLang);

            if (translatedText.empty()) {
                json error = {{"error", "Translation failed. Check if the language codes are supported."}};
                res.set_content(error.dump(), "application/json");
                res.status = 500;
                return;
            }

            json response = {
                {"original_text", text},
                {"translated_text", translatedText},
                {"source_lang", sourceLang},
                {"target_lang", targetLang},
                {"message", "Translation successful"}
            };

            res.set_content(response.dump(), "application/json");
            res.status = 200;

        } catch (json::parse_error& e) {
            json error = {{"error", "Invalid JSON format"}};
            res.set_content(error.dump(), "application/json");
            res.status = 400;
        } catch (const std::exception& e) {
            std::cerr << "Translation error: " << e.what() << std::endl;
            json error = {{"error", "Internal server error"}};
            res.set_content(error.dump(), "application/json");
            res.status = 500;
        }
    }
};