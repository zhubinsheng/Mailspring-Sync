#include "AIChatStore.hpp"
#include "MailUtils.hpp"
#include <ctime>
#include <SQLiteCpp/SQLiteCpp.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

//非高频调用 需预编译语句
namespace {
    static std::shared_ptr<spdlog::logger> getLogger() {
        return spdlog::get("logger");
    }
}

AIChatStore::AIChatStore(MailStore* store) : store_(store) {}

void AIChatStore::insert(const nlohmann::json& payload) {
    store_->assertCorrectThread();
    auto logger = getLogger();
    if (!payload.contains("userId") || payload["userId"].is_null() || !payload["userId"].is_string() || payload["userId"].get<std::string>().empty()) {
        logger->error("InsertAiChatSessionTask: userId is required and must be a non-empty string");
        return;
    }
    std::string id = MailUtils::idRandomlyGenerated();
    std::string userId = payload["userId"].get<std::string>();
    std::string title = payload.value("title", "New Chat");
    std::string now = MailUtils::iso8601StringFromTime(time(nullptr));
    std::string createdAt = now;
    std::string updatedAt = now;
    SQLite::Statement query(store_->db(),
        "INSERT INTO AiChatSession "
        "(id, userId, title, createdAt, updatedAt) "
        "VALUES (:id, :userId, :title, :createdAt, :updatedAt)");
    query.bind(":id", id);
    query.bind(":userId", userId);
    query.bind(":title", title);
    query.bind(":createdAt", createdAt);
    query.bind(":updatedAt", updatedAt);

    logger->info("UpdateAiChatSessionTask use sql: {}", query.getQuery());
    query.exec();
}

void AIChatStore::update(const nlohmann::json& payload) {
    store_->assertCorrectThread();
    auto logger = getLogger();
    if (!payload.contains("id") || payload["id"].is_null() || !payload["id"].is_string() || payload["id"].get<std::string>().empty()) {
        logger->error("UpdateAiChatSessionTask: id is required and must be a non-empty string");
        return;
    }
    std::string id = payload["id"].get<std::string>();
    std::string now = MailUtils::iso8601StringFromTime(time(nullptr));
    std::string sql = "UPDATE AiChatSession SET ";
    bool first = true;
    std::vector<std::string> fields = {"title"};
    for (const auto& field : fields) {
        if (payload.contains(field) && !payload[field].is_null()) {
            if (!first) sql += ", ";
            sql += field + "= :" + field;
            first = false;
        }
    }
    if (!first) sql += ", ";
    sql += "updatedAt=:updatedAt WHERE id=:id";
    SQLite::Statement query(store_->db(), sql);
    for (const auto& field : fields) {
        if (payload.contains(field) && !payload[field].is_null()) {
            if (field == "title") query.bind(":" + field, payload["title"].get<std::string>());
        }
    }
    query.bind(":updatedAt", now);
    query.bind(":id", id);
    logger->info("UpdateAiChatSessionTask use sql: {}", query.getQuery());
    query.exec();
}

void AIChatStore::remove(const nlohmann::json& payload) {
    store_->assertCorrectThread();
    auto logger = getLogger();
    if (!payload.contains("id") || payload["id"].is_null() || !payload["id"].is_string() || payload["id"].get<std::string>().empty()) {
        logger->error("DestroyAiChatSessionTask: id is required");
        return;
    }
    std::string id = payload["id"].get<std::string>();
    SQLite::Statement query(store_->db(), "DELETE FROM AiChatSession WHERE id=:id");
    query.bind(":id", id);
    query.exec();
}

void AIChatStore::insertAiMessage(const nlohmann::json& payload) {
    store_->assertCorrectThread();
    auto logger = getLogger();
    std::string now = MailUtils::iso8601StringFromTime(time(nullptr));
    if (!payload.contains("sessionId") || payload["sessionId"].is_null() || !payload["sessionId"].is_string() || payload["sessionId"].get<std::string>().empty()) {
        logger->error("InsertAiMessage: sessionId is required and must be a non-empty string");
        return;
    }
    if (!payload.contains("role") || payload["role"].is_null() || !payload["role"].is_string() || payload["role"].get<std::string>().empty()) {
        logger->error("InsertAiMessage: role is required and must be a non-empty string");
        return;
    }
    if (!payload.contains("content") || payload["content"].is_null() || !payload["content"].is_string()) {
        logger->error("InsertAiMessage: content is required and must be a string");
        return;
    }
    if (!payload.contains("isUnread") || !payload["isUnread"].is_number_integer()) {
        logger->error("InsertAiMessage: isUnread is required and must be an integer");
        return;
    }
    std::string id = MailUtils::idRandomlyGenerated();
    SQLite::Statement query(store_->db(),
        "INSERT INTO AiMessage (id, sessionId, role, content, createdAt, updatedAt, isUnread) "
        "VALUES (:id, :sessionId, :role, :content, :createdAt, :updatedAt, :isUnread)");
    query.bind(":id", id);
    query.bind(":sessionId", payload["sessionId"].get<std::string>());
    query.bind(":role", payload["role"].get<std::string>());
    query.bind(":content", payload["content"].get<std::string>());
    query.bind(":createdAt", now);
    query.bind(":updatedAt", now);
    query.bind(":isUnread", payload["isUnread"].get<int>());
    logger->info("InsertAiMessage use sql: {}", query.getQuery());
    query.exec();
}

void AIChatStore::updateAiMessage(const nlohmann::json& payload) {
    store_->assertCorrectThread();
    auto logger = getLogger();
    std::string now = MailUtils::iso8601StringFromTime(time(nullptr));
    if (!payload.contains("id") || payload["id"].is_null() || !payload["id"].is_string() || payload["id"].get<std::string>().empty()) {
        logger->error("UpdateAiMessage: id is required and must be a non-empty string");
        return;
    }
    std::string id = payload["id"].get<std::string>();

    std::string sql = "UPDATE AiMessage SET ";
    bool first = true;
    if (payload.contains("content") && payload["content"].is_string()) {
        sql += "content=:content";
        first = false;
    }
    if (payload.contains("isUnread") && payload["isUnread"].is_number_integer()) {
        if (!first) sql += ", ";
        sql += "isUnread=:isUnread";
        first = false;
    }
    if (first) {
        logger->warn("UpdateAiMessage: nothing to update for id={}", id);
        return;
    }
    sql += ", updatedAt=:updatedAt WHERE id=:id";

    SQLite::Statement query(store_->db(), sql);
    if (payload.contains("content") && payload["content"].is_string())
        query.bind(":content", payload["content"].get<std::string>());
    if (payload.contains("isUnread") && payload["isUnread"].is_number_integer())
        query.bind(":isUnread", payload["isUnread"].get<int>());
    query.bind(":updatedAt", now);
    query.bind(":id", id);

    logger->info("UpdateAiMessage use sql: {}", query.getQuery());
    query.exec();
}

void AIChatStore::deleteAiMessage(const nlohmann::json& payload) {
    store_->assertCorrectThread();
    auto logger = getLogger();
    if (!payload.contains("id") || payload["id"].is_null() || !payload["id"].is_string() || payload["id"].get<std::string>().empty()) {
        logger->error("DeleteAiMessage: id is required and must be a non-empty string");
        return;
    }
    std::string id = payload["id"].get<std::string>();
    SQLite::Statement query(store_->db(), "DELETE FROM AiMessage WHERE id=:id");
    query.bind(":id", id);
    logger->info("DeleteAiMessage use sql: {}", query.getQuery());
    query.exec();
}