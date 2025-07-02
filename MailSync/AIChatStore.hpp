#pragma once
#include <memory>
#include <vector>
#include <string>
#include <SQLiteCpp/SQLiteCpp.h>
#include <nlohmann/json.hpp>
#include "MailStore.hpp"

class AIChatStore {
public:
    explicit AIChatStore(MailStore* store);

    // AiChatSession
    void insert(const nlohmann::json& payload);
    void update(const nlohmann::json& payload);
    void remove(const nlohmann::json& payload);

    // AiMessage
    void insertAiMessage(const nlohmann::json& payload);
    void updateAiMessage(const nlohmann::json& payload);
    void deleteAiMessage(const nlohmann::json& payload);

private:
    MailStore* store_;
}; 