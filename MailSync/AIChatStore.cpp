#include "AIChatStore.hpp"
#include "MailUtils.hpp"
#include <ctime>
#include <SQLiteCpp/SQLiteCpp.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace {
    static std::shared_ptr<spdlog::logger> getLogger() {
        return spdlog::get("logger");
    }
}

AIChatStore::AIChatStore(SQLite::Database* db) : db_(db) {}

void AIChatStore::insert(const nlohmann::json& data) {
    auto logger = getLogger();
    if (!data.contains("userId") || data["userId"].is_null() || !data["userId"].is_string() || data["userId"].get<std::string>().empty()) {
        logger->error("InsertAiChatSessionTask: userId is required and must be a non-empty string");
        return;
    }
    std::string id = MailUtils::idRandomlyGenerated();
    std::string userId = data.value("userId");
    std::string title = data.value("title", "New Chat");
    std::string now = MailUtils::iso8601StringFromTime(time(nullptr));
    std::string createdAt = now;
    std::string updatedAt = now;
    SQLite::Statement query(*db_,
        "INSERT INTO AiChatSession "
        "(id, userId, title, createdAt, updatedAt) "
        "VALUES (:id, :userId, :title, :createdAt, :updatedAt)");
    query.bind(":id", id);
    query.bind(":userId", userId);
    query.bind(":title", title);
    query.bind(":createdAt", createdAt);
    query.bind(":updatedAt", updatedAt);

    logger->info("UpdateAiChatSessionTask use sql: {}", query.sql());
    query.exec();
}

void AIChatStore::update(const nlohmann::json& data) {
    auto logger = getLogger();
    if (!data.contains("id") || data["id"].is_null() || !data["id"].is_string() || data["id"].get<std::string>().empty()) {
        logger->error("UpdateAiChatSessionTask: id is required and must be a non-empty string");
        return;
    }
    std::string id = data["id"].get<std::string>();
    std::string now = MailUtils::iso8601StringFromTime(time(nullptr));
    std::string sql = "UPDATE AiChatSession SET ";
    bool first = true;
    std::vector<std::string> fields = {"title"};
    for (const auto& field : fields) {
        if (data.contains(field) && !data[field].is_null()) {
            if (!first) sql += ", ";
            sql += field + "= :" + field;
            first = false;
        }
    }
    if (!first) sql += ", ";
    sql += "updatedAt=:updatedAt WHERE id=:id";
    SQLite::Statement query(*db_, sql);
    for (const auto& field : fields) {
        if (data.contains(field) && !data[field].is_null()) {
            if (field == "title") query.bind(":" + field, data["title"].get<std::string>());
        }
    }
    query.bind(":updatedAt", now);
    query.bind(":id", id);
    logger->info("UpdateAiChatSessionTask use sql: {}", query.sql());
    query.exec();
}

void AIChatStore::remove(const nlohmann::json& data) {
    auto logger = getLogger();
    if (!data.contains("id") || data["id"].is_null() || !data["id"].is_string() || data["id"].get<std::string>().empty()) {
        logger->error("DestroyAiChatSessionTask: id is required");
        return;
    }
    std::string id = data.value("id", "");
    SQLite::Statement query(*db_, "DELETE FROM AiChatSession WHERE id=:id");
    query.bind(":id", id);
    query.exec();
}

void AIChatStore::insertAiMessage(const nlohmann::json& data) {
    auto logger = getLogger();
    std::string now = MailUtils::iso8601StringFromTime(time(nullptr));
    if (!data.contains("sessionId") || data["sessionId"].is_null() || !data["sessionId"].is_string() || data["sessionId"].get<std::string>().empty()) {
        logger->error("InsertAiMessage: sessionId is required and must be a non-empty string");
        return;
    }
    if (!data.contains("role") || data["role"].is_null() || !data["role"].is_string() || data["role"].get<std::string>().empty()) {
        logger->error("InsertAiMessage: role is required and must be a non-empty string");
        return;
    }
    if (!data.contains("content") || data["content"].is_null() || !data["content"].is_string()) {
        logger->error("InsertAiMessage: content is required and must be a string");
        return;
    }
    if (!data.contains("isUnread") || !data["isUnread"].is_number_integer()) {
        logger->error("InsertAiMessage: isUnread is required and must be an integer");
        return;
    }
    std::string id = data.contains("id") && data["id"].is_string() && !data["id"].get<std::string>().empty() ? data["id"].get<std::string>() : MailUtils::idRandomlyGenerated();
    SQLite::Statement query(*db_,
        "INSERT INTO AiMessage (id, sessionId, role, content, createdAt, updatedAt, isUnread) "
        "VALUES (:id, :sessionId, :role, :content, :createdAt, :updatedAt, :isUnread)");
    query.bind(":id", id);
    query.bind(":sessionId", data["sessionId"].get<std::string>());
    query.bind(":role", data["role"].get<std::string>());
    query.bind(":content", data["content"].get<std::string>());
    query.bind(":createdAt", now);
    query.bind(":updatedAt", now);
    query.bind(":isUnread", data["isUnread"].get<int>());
    logger->info("InsertAiMessage use sql: {}", query.sql());
    query.exec();
}

void AIChatStore::updateAiMessage(const nlohmann::json& data) {
    auto logger = getLogger();
    std::string now = MailUtils::iso8601StringFromTime(time(nullptr));
    if (!data.contains("id") || data["id"].is_null() || !data["id"].is_string() || data["id"].get<std::string>().empty()) {
        logger->error("UpdateAiMessage: id is required and must be a non-empty string");
        return;
    }
    std::string id = data["id"].get<std::string>();

    std::string sql = "UPDATE AiMessage SET ";
    bool first = true;
    if (data.contains("content") && data["content"].is_string()) {
        sql += "content=:content";
        first = false;
    }
    if (data.contains("isUnread") && data["isUnread"].is_number_integer()) {
        if (!first) sql += ", ";
        sql += "isUnread=:isUnread";
        first = false;
    }
    if (first) {
        logger->warn("UpdateAiMessage: nothing to update for id={}", id);
        return;
    }
    sql += ", updatedAt=:updatedAt WHERE id=:id";

    SQLite::Statement query(*db_, sql);
    if (data.contains("content") && data["content"].is_string())
        query.bind(":content", data["content"].get<std::string>());
    if (data.contains("isUnread") && data["isUnread"].is_number_integer())
        query.bind(":isUnread", data["isUnread"].get<int>());
    query.bind(":updatedAt", now);
    query.bind(":id", id);

    logger->info("UpdateAiMessage use sql: {}", query.sql());
    query.exec();
}

void AIChatStore::deleteAiMessage(const nlohmann::json& data) {
    auto logger = getLogger();
    if (!data.contains("id") || data["id"].is_null() || !data["id"].is_string() || data["id"].get<std::string>().empty()) {
        logger->error("DeleteAiMessage: id is required and must be a non-empty string");
        return;
    }
    std::string id = data["id"].get<std::string>();
    SQLite::Statement query(*db_, "DELETE FROM AiMessage WHERE id=:id");
    query.bind(":id", id);
    logger->info("DeleteAiMessage use sql: {}", query.sql());
    query.exec();
}