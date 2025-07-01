#pragma once
#include <memory>
#include <vector>
#include <string>
#include <SQLiteCpp/SQLiteCpp.h>
#include <nlohmann/json.hpp>

class AIChatStore {
public:
    AIChatStore(SQLite::Database* db);

    // AiChatSession
    void insert(const nlohmann::json& data);
    void update(const nlohmann::json& data);
    void remove(const nlohmann::json& data);

    // AiMessage
    void insertAiMessage(const nlohmann::json& data);
    void updateAiMessage(const nlohmann::json& data);
    void deleteAiMessage(const nlohmann::json& data);

private:
    SQLite::Database* db_;
}; 