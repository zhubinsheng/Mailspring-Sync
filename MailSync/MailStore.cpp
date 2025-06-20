//
//  MailStore.cpp
//  MailSync
//
//  Created by Ben Gotow on 6/15/17.
//  Copyright © 2017 Foundry 376. All rights reserved.
//
//  Use of this file is subject to the terms and conditions defined
//  in 'LICENSE.md', which is part of the Mailspring-Sync package.
//

#include "MailStore.hpp"
#include "MailUtils.hpp"
#include "MailStoreTransaction.hpp"
#include "SyncException.hpp"
#include "constants.h"

#include "Folder.hpp"
#include "Message.hpp"
#include "Thread.hpp"

using namespace mailcore;
using namespace std;

std::atomic<int> globalLabelsVersion {1};

#pragma mark Metadata

Metadata MetadataFromJSON(const json & metadata) {
    Metadata m;
    m.objectType = metadata["object_type"].get<string>();
    m.objectId = metadata["object_id"].get<string>();
    m.accountId = metadata["aid"].get<string>();
    m.pluginId = metadata["plugin_id"].get<string>();
    m.version = metadata["v"].get<uint32_t>();
    m.value = metadata["value"];
    return m;
}

#pragma mark MessageAttributes

MessageAttributes MessageAttributesForMessage(IMAPMessage * msg) {
    auto m = MessageAttributes{};
    m.uid = msg->uid();
    m.unread = bool(!(msg->flags() & MessageFlagSeen));
    m.starred = bool(msg->flags() & MessageFlagFlagged);
    m.labels = std::vector<std::string>{};
    
    Array * labels = msg->gmailLabels();
    bool draftLabelPresent = false;
    bool trashSpamLabelPresent = false;
    if (labels != nullptr) {
        for (unsigned int ii = 0; ii < labels->count(); ii ++) {
            string str = ((String *)labels->objectAtIndex(ii))->UTF8Characters();
            // Gmail exposes Trash and Spam as folders and labels. We want them
            // to be folders so we ignore their presence as labels.
            if ((str == "\\Trash") || (str == "\\Spam")) {
                trashSpamLabelPresent = true;
                continue;
            }
            if ((str == "\\Draft")) {
                draftLabelPresent = true;
            }
            m.labels.push_back(str);
        }
        sort(m.labels.begin(), m.labels.end());
    }
    
    m.draft = (bool(msg->flags() & MessageFlagDraft) || draftLabelPresent) && !trashSpamLabelPresent;
    
    return m;
}

bool MessageAttributesMatch(MessageAttributes a, MessageAttributes b) {
    return a.unread == b.unread && a.starred == b.starred && a.uid == b.uid && a.labels == b.labels;
}


#pragma mark MailStore

MailStore::MailStore() :
    _db(MailUtils::getEnvUTF8("CONFIG_DIR_PATH") + FS_PATH_SEP + "edgehill.db", SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE),
    _stmtBeginTransaction(_db, "BEGIN IMMEDIATE TRANSACTION"),
    _stmtRollbackTransaction(_db, "ROLLBACK"),
    _stmtCommitTransaction(_db, "COMMIT"),
    _owningThread(spdlog::details::os::thread_id()),
    _labelCacheVersion(0),
    _labelCache()
{
    _db.setBusyTimeout(10 * 1000);
    
    // Note: These are properties of the connection, so they must be set regardless
    // of whether the database setup queries are run.
    
    // https://www.sqlite.org/intern-v-extern-blob.html
    // A database page size of 8192 or 16384 gives the best performance for large BLOB I/O.
    SQLite::Statement(_db, "PRAGMA journal_mode = WAL").executeStep();
    SQLite::Statement(_db, "PRAGMA main.page_size = 4096").exec();
    SQLite::Statement(_db, "PRAGMA main.cache_size = 10000").exec();
    SQLite::Statement(_db, "PRAGMA main.synchronous = NORMAL").exec();
}

static int CURRENT_VERSION = 9;
static string VACUUM_TIME_KEY = "VACUUM_TIME";
static time_t VACUUM_INTERVAL = 14 * 24 * 60 * 60; // 14 days

void MailStore::migrate() {
    SQLite::Statement uv(_db, "PRAGMA user_version");
    uv.executeStep();
    int version = uv.getColumn(0).getInt();
    uv.reset();
    
    string verb = version == 0 ? "Setup" : "Migration";
    
    if (version < 1) {
        for (string sql : V1_SETUP_QUERIES) {
            SQLite::Statement(_db, sql).exec();
        }
    }
    if (version < 2) {
        for (string sql : V2_SETUP_QUERIES) {
            SQLite::Statement(_db, sql).exec();
        }
    }
    if (version < 3) {
        // This one will be time consuming - display window
        cout << "\nRunning " << verb;
        cout.flush();
        for (string sql : V3_SETUP_QUERIES) {
            SQLite::Statement(_db, sql).exec();
        }
    }
    if (version < 4) {
        for (string sql : V4_SETUP_QUERIES) {
            SQLite::Statement(_db, sql).exec();
        }
    }
    if (version < 6) {
        for (string sql : V6_SETUP_QUERIES) {
            SQLite::Statement(_db, sql).exec();
        }
    }
    if (version < 7) {
        for (string sql : V7_SETUP_QUERIES) {
            SQLite::Statement(_db, sql).exec();
        }
    }
    if (version < 8) {
        for (string sql : V8_SETUP_QUERIES) {
            SQLite::Statement(_db, sql).exec();
        }
    }

    if (version < 9) {
        for (string sql : V9_SETUP_QUERIES) {
            SQLite::Statement(_db, sql).exec();
        }
    }
    
    // Update the version flag. Note that we don't want to go from v3 back to v2
    // if the user re-opens an older version of the app.
    if (version < CURRENT_VERSION) {
        SQLite::Statement(_db, "PRAGMA user_version = " + to_string(CURRENT_VERSION)).exec();
    }

    // Initialize VACUUM timer if we're on version 0, but make everyone coming
    // from old versions VACUUM for the first time.
    if (version == 0) saveKeyValue(VACUUM_TIME_KEY, to_string(time(0)));
    string vacuumTimeS = getKeyValue(VACUUM_TIME_KEY);
    time_t vacuumTime = vacuumTimeS != "" ? stol(vacuumTimeS) : 0;

    // VACUUM if it's been a while
    if (time(0) - vacuumTime > VACUUM_INTERVAL) {
        cout << "\nRunning Vacuum\n";
        cout.flush();
        
        // Update vacuum timer first so we don't re-attempt vacuuming if it fails
        saveKeyValue(VACUUM_TIME_KEY, to_string(time(0)));
        
        try {
            SQLite::Statement(_db, "VACUUM").exec();
        } catch (std::exception & ex) {
            // Vacuuming can fail if we run out of disk space and isn't mandatory,
            // so we fail silently and still return 0 to allow the app to launch.
            cout << "\n" << "Vacuuming failed with SQLite error:";
            cout << "\n" << ex.what();
        }
    }
}

void MailStore::assertCorrectThread() {
    /* Because we re-use SQLite prepared statements and a single SQLite connection
     per worker, it's extremely important that all calls to each MailStore are made
     from a single thread. We capture a threadId when you open the MailStore and
     require that all subseuqent calls are from that thread.
     
     Otherwise, it's possible for two threads to bind to the same prepared query,
     prepare half the values, and execute it, creating a rediculous data inconsistency.
     */
    if (spdlog::details::os::thread_id() != _owningThread) {
        spdlog::get("logger")->error("MailStore thread assertion failure: function called on {} instead of {}", spdlog::details::os::thread_id(), _owningThread);
        throw SyncException("assertion-failure", "MailStore thread assertion failure", false);
    }
}

void MailStore::resetForAccount(string accountId) {
    assertCorrectThread();
    for (string sql : ACCOUNT_RESET_QUERIES) {
        SQLite::Statement statement {_db, sql };
        statement.bind(1, accountId);
        statement.exec();
    }
    
    // reset the metadata stream cursor so we re-fetch metadata on resync
    saveKeyValue("cursor-" + accountId, "0");

    SQLite::Statement(_db, "VACUUM").exec();
}

SQLite::Database & MailStore::db()
{
    return this->_db;
}

map<uint32_t, MessageAttributes> MailStore::fetchMessagesAttributesInRange(Range range, Folder & folder) {
    assertCorrectThread();
    SQLite::Statement query(this->_db, "SELECT id, unread, starred, remoteUID, remoteXGMLabels FROM Message WHERE accountId = ? AND remoteFolderId = ? AND remoteUID >= ? AND remoteUID <= ?");
    query.bind(1, folder.accountId());
    query.bind(2, folder.id());
    query.bind(3, (long long)(range.location));
    
    // Range is uint64_t, and "*" is represented by UINT64_MAX.
    // SQLite doesn't support UINT64 and the conversion /can/ fail.
    if (range.length == UINT64_MAX) {
        query.bind(4, LLONG_MAX);
    } else {
        query.bind(4, (long long)(range.location + range.length));
    }

    map<uint32_t, MessageAttributes> results {};

    while (query.executeStep()) {
        MessageAttributes attrs{};
        uint32_t uid = (uint32_t)query.getColumn("remoteUID").getInt64();
        attrs.uid = uid;
        attrs.starred = query.getColumn("starred").getInt() != 0;
        attrs.unread = query.getColumn("unread").getInt() != 0;
        
        vector<string> labels{};
        for (const auto i : json::parse(query.getColumn("remoteXGMLabels").getString())) {
            labels.push_back(i.get<string>());
        }
        attrs.labels = labels;

        results[uid] = attrs;
    }
    
    return results;
}

uint32_t MailStore::fetchMessageUIDAtDepth(Folder & folder, uint32_t depth, uint32_t before) {
    assertCorrectThread();
    SQLite::Statement query(this->_db, "SELECT remoteUID FROM Message WHERE accountId = ? AND remoteFolderId = ? AND remoteUID < ? ORDER BY remoteUID DESC LIMIT 1 OFFSET ?");
    query.bind(1, folder.accountId());
    query.bind(2, folder.id());
    query.bind(3, before);
    query.bind(4, depth);
    if (query.executeStep()) {
        return query.getColumn("remoteUID").getUInt();
    }
    query.reset();
    return 1;
}

string MailStore::getKeyValue(string key) {
    assertCorrectThread();
    SQLite::Statement query(this->_db, "SELECT value FROM _State WHERE id = ?");
    query.bind(1, key);
    if (query.executeStep()) {
        return query.getColumn(0).getString();
    }
    query.reset();
    return "";
}

void MailStore::saveKeyValue(string key, string value) {
    assertCorrectThread();
    SQLite::Statement query(this->_db, "REPLACE INTO _State (id, value) VALUES (?, ?)");
    query.bind(1, key);
    query.bind(2, value);
    query.exec();
}

vector<shared_ptr<Label>> MailStore::allLabelsCache(string accountId) {
    // todo bg: this assumes a single accountId will ever be used
    if (_labelCacheVersion != globalLabelsVersion) {
        _labelCache = findAll<Label>(Query().equal("accountId", accountId));
        _labelCacheVersion = globalLabelsVersion;
    }
    return _labelCache;
}

void MailStore::beginTransaction() {
    assertCorrectThread();
    _stmtBeginTransaction.exec();
    _stmtBeginTransaction.reset();
    _transactionOpen = true;
}


void MailStore::rollbackTransaction() {
    // Note: when a transaction is interrupted and we roll it back,
    // running the statement again produces the error again? Unclear...
    _saveUpdateQueries = {};
    _saveInsertQueries = {};
    _removeQueries = {};
    _stmtRollbackTransaction.exec();
    _stmtRollbackTransaction.reset();
    _transactionOpen = false;
}

// This method allows you to perform work in a transaction and then prevent the
// transaction from emitting any deltas to the client app. If you KNOW the
// transaction is only changing internal data, you can safely do this without the
// client falling out of sync and it can be a performance win in key places where
// many unnecessary updates would cause thrashing on the JS side.
void MailStore::unsafeEraseTransactionDeltas() {
    _transactionDeltas = {};
}

void MailStore::commitTransaction() {
    _stmtCommitTransaction.exec();
    _stmtCommitTransaction.reset();
    
    // emit all of the deltas
    if (_transactionDeltas.size()) {
        SharedDeltaStream()->emit(_transactionDeltas, _streamMaxDelay);
        _transactionDeltas = {};
    }
    _transactionOpen = false;

}

void MailStore::save(MailModel * model) {
    assertCorrectThread();

    // DEBUG: 在save方法开始时打印模型基本信息  
    spdlog::get("logger")->info("save: Entering save method for model type='{}', id='{}', version={}", 
                                model->tableName(), model->id(), model->version());

    model->incrementVersion();
    model->beforeSave(this);

    auto tableName = model->tableName();
    
    if (model->version() > 1) {
        if (!_saveUpdateQueries.count(tableName)) {
            string pairs{""};
            for (const auto col : model->columnsForQuery()) {
                if (col == "id") {
                    continue;
                }
                pairs += (col + " = :" + col + ",");
            }
            pairs.pop_back();
            
            auto stmt = make_shared<SQLite::Statement>(this->_db, "UPDATE " + tableName + " SET " + pairs + " WHERE id = :id");
            _saveUpdateQueries[tableName] = stmt;
        }
        auto query = _saveUpdateQueries[tableName];
        query->reset();
        
        // DEBUG: 在bindToQuery调用前
        spdlog::get("logger")->info("save: About to call bindToQuery for UPDATE on table '{}', model id='{}'", tableName, model->id());
        
        model->bindToQuery(query.get());
        
        // DEBUG: bindToQuery调用成功，准备执行查询
        spdlog::get("logger")->info("save: bindToQuery completed successfully for UPDATE, executing query...");
        
        query->exec();
        
    } else {
        if (!_saveInsertQueries.count(tableName)) {
            string cols{""};
            string values{""};
            for (const auto col : model->columnsForQuery()) {
                cols += col + ",";
                values += ":" + col + ",";
            }
            cols.pop_back();
            values.pop_back();
            
            auto stmt = make_shared<SQLite::Statement>(this->_db, "INSERT INTO " + tableName + " (" + cols + ") VALUES (" + values + ")");
            _saveInsertQueries[tableName] = stmt;
        }
        
        auto query = _saveInsertQueries[tableName];
        query->reset();
        
        // DEBUG: 在bindToQuery调用前
        spdlog::get("logger")->info("save: About to call bindToQuery for INSERT on table '{}', model id='{}'", tableName, model->id());
        
        model->bindToQuery(query.get());
        
        // DEBUG: bindToQuery调用成功，准备执行查询
        spdlog::get("logger")->info("save: bindToQuery completed successfully for INSERT, executing query...");
        
        query->exec();
    }

    model->afterSave(this);

    if (tableName == "Label") {
        globalLabelsVersion += 1;
    }

    DeltaStreamItem delta {DELTA_TYPE_PERSIST, model};
    _emit(delta);
}

void MailStore::saveFolderStatus(Folder * folder, json & initialStatus) {
    json & changedStatus = folder->localStatus();
    if (changedStatus == initialStatus) {
        return;
    }

    {
        MailStoreTransaction transaction(this, "saveFolderStatus");
        auto current = find<Folder>(Query().equal("accountId", folder->accountId()).equal("id", folder->id()));
        if (current == nullptr) {
            return;
        }
        for (auto it = changedStatus.begin(); it != changedStatus.end(); ++it) {
            if (initialStatus.count(it.key()) == 0 || initialStatus[it.key()] != it.value()) {
                current->localStatus()[it.key()] = it.value();
            }
        }
        save(current.get());
        transaction.commit();
    }
}

void MailStore::remove(MailModel * model) {
    assertCorrectThread();
    auto tableName = model->tableName();
    if (!_removeQueries.count(tableName)) {
        _removeQueries[tableName] = make_shared<SQLite::Statement>(this->_db, "DELETE FROM " + tableName + " WHERE id = ?");
    }
    auto query = _removeQueries[tableName];
    query->reset();
    query->bind(1, model->id());
    query->exec();

    model->afterRemove(this);

    if (model->tableName() == "Label") {
        globalLabelsVersion += 1;
    }

    DeltaStreamItem delta {DELTA_TYPE_UNPERSIST, model};
    _emit(delta);
}

void MailStore::_emit(DeltaStreamItem & delta) {
    if (_transactionOpen) {
        _transactionDeltas.push_back(delta);
    } else {
        SharedDeltaStream()->emit(delta, _streamMaxDelay);
    }
}

shared_ptr<MailModel> MailStore::findGeneric(string type, Query query) {
    assertCorrectThread();
    transform(type.begin(), type.end(), type.begin(), ::tolower);

    if (type == "message") {
        return find<Message>(query);
    } else if (type == "thread") {
        return find<Thread>(query);
    } else if (type == "contact") {
        return find<Contact>(query);
    }
    assert(false);
}

vector<shared_ptr<MailModel>> MailStore::findAllGeneric(string type, Query query) {
    assertCorrectThread();
    transform(type.begin(), type.end(), type.begin(), ::tolower);

    if (type == "message") {
        auto results = findAll<Message>(query);
        std::vector<std::shared_ptr<MailModel>> baseResults(results.begin(), results.end());
        return baseResults;
    } else if (type == "thread") {
        auto results = findAll<Thread>(query);
        std::vector<std::shared_ptr<MailModel>> baseResults(results.begin(), results.end());
        return baseResults;
    } else if (type == "contact") {
        auto results = findAll<Contact>(query);
        std::vector<std::shared_ptr<MailModel>> baseResults(results.begin(), results.end());
        return baseResults;
    }
    assert(false);
}

vector<Metadata> MailStore::findAndDeleteDetatchedPluginMetadata(string accountId, string objectId) {
    assertCorrectThread();
    if (!_saveInsertQueries.count("metadata")) {
        auto stmt = make_shared<SQLite::Statement>(db(), "SELECT version, value, pluginId, objectType FROM DetatchedPluginMetadata WHERE objectId = ? AND accountId = ?");
        _saveInsertQueries["metadata"] = stmt;
    }
    
    vector<Metadata> results;
    auto st = _saveInsertQueries["metadata"];
    st->reset();
    st->bind(1, objectId);
    st->bind(2, accountId);
    while (st->executeStep()) {
        Metadata m;
        m.accountId = accountId;
        m.version = st->getColumn("version").getInt();
        m.value = json::parse(st->getColumn("value").getString());
        m.pluginId = st->getColumn("pluginId").getString();
        m.objectType = st->getColumn("objectType").getString();
        m.objectId = objectId;
        results.push_back(m);
    }
    if (results.size()) {
        SQLite::Statement dt(db(), "DELETE FROM DetatchedPluginMetadata WHERE objectId = ? AND accountId = ?");
        dt.bind(1, objectId);
        dt.bind(2, accountId);
        dt.exec();
    }
    return results;
}

void MailStore::saveDetatchedPluginMetadata(Metadata & m) {
    assertCorrectThread();
    SQLite::Statement st(db(), "REPLACE INTO DetatchedPluginMetadata (objectId, objectType, accountId, pluginId, value, version) VALUES (?,?,?,?,?,?)");
    st.bind(1, m.objectId);
    st.bind(2, m.objectType);
    st.bind(3, m.accountId);
    st.bind(4, m.pluginId);
    st.bind(5, m.value.dump());
    st.bind(6, m.version);
    st.exec();
}

void MailStore::setStreamDelay(int streamMaxDelay) {
    _streamMaxDelay = streamMaxDelay;
}

#pragma mark Summary Queries

shared_ptr<Summary> MailStore::findSummaryForThread(string accountId, string threadId) {
    assertCorrectThread();
    return find<Summary>(Query().equal("accountId", accountId).equal("threadId", threadId));
}

void MailStore::handleSummaryUpdate(json data, shared_ptr<Account> account) {
    assertCorrectThread();
    
    // 打印收到的完整数据用于调试
    spdlog::get("logger")->info("handleSummaryUpdate received data: {}", data.dump());
    
    // DEBUG: 检查account对象
    if (!account) {
        spdlog::get("logger")->error("handleSummaryUpdate: account is null!");
        return;
    }
    spdlog::get("logger")->info("handleSummaryUpdate: account object exists");
    
    if (data.count("threadId") == 0 || data["threadId"].is_null()) {
        spdlog::get("logger")->error("handleSummaryUpdate: threadId is required and cannot be null");
        return;
    }

    // DEBUG: 尝试获取threadId
    spdlog::get("logger")->info("handleSummaryUpdate: About to extract threadId from JSON");
    string threadId;
    try {
        threadId = data["threadId"].get<string>();
        spdlog::get("logger")->info("handleSummaryUpdate: threadId extracted: '{}'", threadId);
    } catch (const std::exception& e) {
        spdlog::get("logger")->error("handleSummaryUpdate: Exception extracting threadId: {}", e.what());
        return;
    }
    
    // DEBUG: 尝试获取account->accountId() - 添加防护性检查
    spdlog::get("logger")->info("handleSummaryUpdate: About to call account->accountId()");
    string accountId;
    try {
        // 首先检查account对象的内部数据结构
        if (account->_data.count("aid") == 0 || account->_data["aid"].is_null()) {
            spdlog::get("logger")->error("handleSummaryUpdate: account _data['aid'] is missing or null");
            // 尝试从任务数据中获取accountId作为fallback
            if (data.count("aid") > 0 && !data["aid"].is_null()) {
                accountId = data["aid"].get<string>();
                spdlog::get("logger")->info("handleSummaryUpdate: Using accountId from task data as fallback: '{}'", accountId);
            } else {
                spdlog::get("logger")->error("handleSummaryUpdate: Cannot determine accountId from either account object or task data");
                return;
            }
        } else {
            accountId = account->accountId();
            spdlog::get("logger")->info("handleSummaryUpdate: account->accountId() returned: '{}'", accountId);
        }
    } catch (const std::exception& e) {
        spdlog::get("logger")->error("handleSummaryUpdate: Exception calling account->accountId(): {}", e.what());
        // 尝试从任务数据中获取accountId作为fallback
        if (data.count("aid") > 0 && !data["aid"].is_null()) {
            try {
                accountId = data["aid"].get<string>();
                spdlog::get("logger")->info("handleSummaryUpdate: Using accountId from task data as fallback: '{}'", accountId);
            } catch (const std::exception& e2) {
                spdlog::get("logger")->error("handleSummaryUpdate: Exception extracting accountId from task data: {}", e2.what());
                return;
            }
        } else {
            spdlog::get("logger")->error("handleSummaryUpdate: Cannot determine accountId from either account object or task data");
            return;
        }
    }
    
    spdlog::get("logger")->info("handleSummaryUpdate: About to call findSummaryForThread");
    auto existing = findSummaryForThread(accountId, threadId);

    if (!existing) {
        spdlog::get("logger")->info("handleSummaryUpdate: Creating new Summary object");
        string summaryId = MailUtils::idRandomlyGenerated();
        spdlog::get("logger")->info("handleSummaryUpdate: Generated summaryId: '{}'", summaryId);
        
        try {
            existing = make_shared<Summary>(summaryId, accountId, 0);
            spdlog::get("logger")->info("handleSummaryUpdate: Summary object created successfully");
        } catch (const std::exception& e) {
            spdlog::get("logger")->error("handleSummaryUpdate: Exception creating Summary object: {}", e.what());
            return;
        }
        
        try {
            existing->setThreadId(threadId);
            spdlog::get("logger")->info("handleSummaryUpdate: setThreadId completed");
        } catch (const std::exception& e) {
            spdlog::get("logger")->error("handleSummaryUpdate: Exception in setThreadId: {}", e.what());
            return;
        }
        
        // 设置 messageId（如果提供的话）
        if (data.count("messageId") > 0 && !data["messageId"].is_null()) {
            try {
                string messageId = data["messageId"].get<string>();
                existing->setMessageId(messageId);
                spdlog::get("logger")->info("handleSummaryUpdate: setMessageId completed with value: '{}'", messageId);
            } catch (const std::exception& e) {
                spdlog::get("logger")->error("handleSummaryUpdate: Exception setting messageId: {}", e.what());
                return;
            }
        } else {
            try {
                existing->setMessageId(""); // 设置默认值
                spdlog::get("logger")->info("handleSummaryUpdate: setMessageId completed with empty value");
            } catch (const std::exception& e) {
                spdlog::get("logger")->error("handleSummaryUpdate: Exception setting empty messageId: {}", e.what());
                return;
            }
        }
    } else {
        spdlog::get("logger")->info("handleSummaryUpdate: Using existing Summary object with id: '{}'", existing->id());
    }

    spdlog::get("logger")->info("handleSummaryUpdate: Starting field updates");

    // 更新字符串字段
    const vector<string> stringFields = {"messageSummary", "briefSummary", "threadSummary", "category"};
    for (const string& field : stringFields) {
        if (data.count(field) > 0 && !data[field].is_null()) {
            try {
                string value = data[field].get<string>();
                spdlog::get("logger")->info("handleSummaryUpdate: Setting {} to '{}'", field, value);
                
                if (field == "messageSummary") {
                    existing->setMessageSummary(value);
                } else if (field == "briefSummary") {
                    existing->setBriefSummary(value);
                } else if (field == "threadSummary") {
                    existing->setThreadSummary(value);
                } else if (field == "category") {
                    existing->setCategory(value);
                }
                
                spdlog::get("logger")->info("handleSummaryUpdate: Successfully set {}", field);
            } catch (const std::exception& e) {
                spdlog::get("logger")->error("handleSummaryUpdate: Exception setting {}: {}", field, e.what());
                return;
            }
        } else {
            spdlog::get("logger")->info("handleSummaryUpdate: Skipping {} (not present or null)", field);
        }
    }

    // 更新布尔字段
    if (data.count("important") > 0 && !data["important"].is_null()) {
        try {
            bool value = data["important"].get<bool>();
            spdlog::get("logger")->info("handleSummaryUpdate: Setting important to {}", value);
            existing->setImportant(value);
            spdlog::get("logger")->info("handleSummaryUpdate: Successfully set important");
        } catch (const std::exception& e) {
            spdlog::get("logger")->error("handleSummaryUpdate: Exception setting important: {}", e.what());
            return;
        }
    }
    
    if (data.count("emergency") > 0 && !data["emergency"].is_null()) {
        try {
            bool value = data["emergency"].get<bool>();
            spdlog::get("logger")->info("handleSummaryUpdate: Setting emergency to {}", value);
            existing->setEmergency(value);
            spdlog::get("logger")->info("handleSummaryUpdate: Successfully set emergency");
        } catch (const std::exception& e) {
            spdlog::get("logger")->error("handleSummaryUpdate: Exception setting emergency: {}", e.what());
            return;
        }
    }

    // DEBUG: 在保存Summary前打印对象状态
    spdlog::get("logger")->info("handleSummaryUpdate: About to save Summary object with id='{}', accountId='{}', threadId='{}', version={}", 
                                existing->id(), existing->accountId(), existing->threadId(), existing->version());
    spdlog::get("logger")->info("handleSummaryUpdate: Summary fields - messageId='{}', messageSummary='{}', briefSummary='{}', threadSummary='{}'", 
                                existing->messageId(), existing->messageSummary(), existing->briefSummary(), existing->threadSummary());

    save(existing.get());
    
    // Summary 更新后，自动推送对应线程中的所有 Message 数据（包含 Summary 信息）
    spdlog::get("logger")->info("handleSummaryUpdate: Auto-triggering message updates with summary data");
    triggerMessagesWithSummaryUpdate(accountId, threadId);
}

void MailStore::handleSummaryDelete(json data, shared_ptr<Account> account) {
    assertCorrectThread();
    
    // 打印收到的完整数据用于调试
    spdlog::get("logger")->info("handleSummaryDelete received data: {}", data.dump());
    
    if (data.count("threadId") == 0 || data["threadId"].is_null()) {
        spdlog::get("logger")->error("handleSummaryDelete: threadId is required and cannot be null");
        return;
    }
    string threadId = data["threadId"].get<string>();
    
    // 添加防护性检查获取accountId
    string accountId;
    try {
        // 首先检查account对象的内部数据结构
        if (!account || account->_data.count("aid") == 0 || account->_data["aid"].is_null()) {
            spdlog::get("logger")->error("handleSummaryDelete: account is null or _data['aid'] is missing/null");
            // 尝试从任务数据中获取accountId作为fallback
            if (data.count("aid") > 0 && !data["aid"].is_null()) {
                accountId = data["aid"].get<string>();
                spdlog::get("logger")->info("handleSummaryDelete: Using accountId from task data as fallback: '{}'", accountId);
            } else {
                spdlog::get("logger")->error("handleSummaryDelete: Cannot determine accountId from either account object or task data");
                return;
            }
        } else {
            accountId = account->accountId();
            spdlog::get("logger")->info("handleSummaryDelete: account->accountId() returned: '{}'", accountId);
        }
    } catch (const std::exception& e) {
        spdlog::get("logger")->error("handleSummaryDelete: Exception calling account->accountId(): {}", e.what());
        // 尝试从任务数据中获取accountId作为fallback
        if (data.count("aid") > 0 && !data["aid"].is_null()) {
            try {
                accountId = data["aid"].get<string>();
                spdlog::get("logger")->info("handleSummaryDelete: Using accountId from task data as fallback: '{}'", accountId);
            } catch (const std::exception& e2) {
                spdlog::get("logger")->error("handleSummaryDelete: Exception extracting accountId from task data: {}", e2.what());
                return;
            }
        } else {
            spdlog::get("logger")->error("handleSummaryDelete: Cannot determine accountId from either account object or task data");
            return;
        }
    }
    
    auto existing = findSummaryForThread(accountId, threadId);
    if (existing) {
        remove(existing.get());
    }else{
        spdlog::get("logger")->error("handleSummaryDelete: summary not found for threadId: {}", threadId);
    }
    
    // Summary 删除后，也要推送对应线程中的 Message 数据（不包含 Summary 信息）
    spdlog::get("logger")->info("handleSummaryDelete: Auto-triggering message updates after summary deletion");
    triggerMessagesWithSummaryUpdate(accountId, threadId);
}

#pragma mark Contact Relation Queries

shared_ptr<ContactRelation> MailStore::findContactRelation(string accountId, string email) {
    assertCorrectThread();
    return find<ContactRelation>(Query().equal("accountId", accountId).equal("email", email));
}

void MailStore::updateContactRelation(string accountId, string email, string relation) {
    assertCorrectThread();
    auto existing = findContactRelation(accountId, email);
    if (existing) {
        existing->setRelation(relation);
        
        // DEBUG: 在保存现有ContactRelation前打印对象状态
        spdlog::get("logger")->info("updateContactRelation: About to save existing ContactRelation with id='{}', accountId='{}', email='{}', relation='{}', version={}", 
                                    existing->id(), existing->accountId(), existing->email(), existing->relation(), existing->version());
        
            save(existing.get());
    } else {
        auto newRelation = make_shared<ContactRelation>(accountId, email, relation);
        
        // DEBUG: 在保存新ContactRelation前打印对象状态
        spdlog::get("logger")->info("updateContactRelation: About to save new ContactRelation with id='{}', accountId='{}', email='{}', relation='{}', version={}", 
                                    newRelation->id(), newRelation->accountId(), newRelation->email(), newRelation->relation(), newRelation->version());
        
        save(newRelation.get());
    }
}

void MailStore::handleContactRelationUpdate(json data, shared_ptr<Account> account) {
    assertCorrectThread();
    
    // 打印收到的完整数据用于调试
    spdlog::get("logger")->info("handleContactRelationUpdate received data: {}", data.dump());
    
    // DEBUG: 检查account对象
    if (!account) {
        spdlog::get("logger")->error("handleContactRelationUpdate: account is null!");
        return;
    }
    spdlog::get("logger")->info("handleContactRelationUpdate: account object exists");
    
    if (data.count("email") == 0 || data["email"].is_null()) {
        spdlog::get("logger")->error("handleContactRelationUpdate: email is required and cannot be null");
        return;
    }
    if (data.count("relation") == 0 || data["relation"].is_null()) {
        spdlog::get("logger")->error("handleContactRelationUpdate: relation is required and cannot be null");
        return;
    }
    
    // DEBUG: 尝试获取account->accountId() - 添加防护性检查
    spdlog::get("logger")->info("handleContactRelationUpdate: About to call account->accountId()");
    string accountId;
    try {
        // 首先检查account对象的内部数据结构
        if (account->_data.count("aid") == 0 || account->_data["aid"].is_null()) {
            spdlog::get("logger")->error("handleContactRelationUpdate: account _data['aid'] is missing or null");
            // 尝试从任务数据中获取accountId作为fallback
            if (data.count("aid") > 0 && !data["aid"].is_null()) {
                accountId = data["aid"].get<string>();
                spdlog::get("logger")->info("handleContactRelationUpdate: Using accountId from task data as fallback: '{}'", accountId);
            } else {
                spdlog::get("logger")->error("handleContactRelationUpdate: Cannot determine accountId from either account object or task data");
                return;
            }
        } else {
            accountId = account->accountId();
            spdlog::get("logger")->info("handleContactRelationUpdate: account->accountId() returned: '{}'", accountId);
        }
    } catch (const std::exception& e) {
        spdlog::get("logger")->error("handleContactRelationUpdate: Exception calling account->accountId(): {}", e.what());
        // 尝试从任务数据中获取accountId作为fallback
        if (data.count("aid") > 0 && !data["aid"].is_null()) {
            try {
                accountId = data["aid"].get<string>();
                spdlog::get("logger")->info("handleContactRelationUpdate: Using accountId from task data as fallback: '{}'", accountId);
            } catch (const std::exception& e2) {
                spdlog::get("logger")->error("handleContactRelationUpdate: Exception extracting accountId from task data: {}", e2.what());
                return;
            }
        } else {
            spdlog::get("logger")->error("handleContactRelationUpdate: Cannot determine accountId from either account object or task data");
            return;
        }
    }
    
    // DEBUG: 尝试从JSON获取字符串
    spdlog::get("logger")->info("handleContactRelationUpdate: About to extract email from JSON");
    string email;
    try {
        email = data["email"].get<string>();
        spdlog::get("logger")->info("handleContactRelationUpdate: email extracted: '{}'", email);
    } catch (const std::exception& e) {
        spdlog::get("logger")->error("handleContactRelationUpdate: Exception extracting email: {}", e.what());
        return;
    }
    
    spdlog::get("logger")->info("handleContactRelationUpdate: About to extract relation from JSON");
    string relation;
    try {
        relation = data["relation"].get<string>();
        spdlog::get("logger")->info("handleContactRelationUpdate: relation extracted: '{}'", relation);
    } catch (const std::exception& e) {
        spdlog::get("logger")->error("handleContactRelationUpdate: Exception extracting relation: {}", e.what());
        return;
    }
    
    spdlog::get("logger")->info("handleContactRelationUpdate: About to call updateContactRelation with accountId='{}', email='{}', relation='{}'", 
                                accountId, email, relation);
    
    updateContactRelation(accountId, email, relation);
    
    spdlog::get("logger")->info("handleContactRelationUpdate: updateContactRelation completed successfully");
}

void MailStore::handleContactRelationDelete(json data, shared_ptr<Account> account) {
    assertCorrectThread();
    
    // 打印收到的完整数据用于调试
    spdlog::get("logger")->info("handleContactRelationDelete received data: {}", data.dump());
    
    if (data.count("email") == 0 || data["email"].is_null()) {
        spdlog::get("logger")->error("handleContactRelationDelete: email is required and cannot be null");
        return;
    }
    string email = data["email"].get<string>();
    
    // 添加防护性检查获取accountId
    string accountId;
    try {
        // 首先检查account对象的内部数据结构
        if (!account || account->_data.count("aid") == 0 || account->_data["aid"].is_null()) {
            spdlog::get("logger")->error("handleContactRelationDelete: account is null or _data['aid'] is missing/null");
            // 尝试从任务数据中获取accountId作为fallback
            if (data.count("aid") > 0 && !data["aid"].is_null()) {
                accountId = data["aid"].get<string>();
                spdlog::get("logger")->info("handleContactRelationDelete: Using accountId from task data as fallback: '{}'", accountId);
            } else {
                spdlog::get("logger")->error("handleContactRelationDelete: Cannot determine accountId from either account object or task data");
                return;
            }
        } else {
            accountId = account->accountId();
            spdlog::get("logger")->info("handleContactRelationDelete: account->accountId() returned: '{}'", accountId);
        }
    } catch (const std::exception& e) {
        spdlog::get("logger")->error("handleContactRelationDelete: Exception calling account->accountId(): {}", e.what());
        // 尝试从任务数据中获取accountId作为fallback
        if (data.count("aid") > 0 && !data["aid"].is_null()) {
            try {
                accountId = data["aid"].get<string>();
                spdlog::get("logger")->info("handleContactRelationDelete: Using accountId from task data as fallback: '{}'", accountId);
            } catch (const std::exception& e2) {
                spdlog::get("logger")->error("handleContactRelationDelete: Exception extracting accountId from task data: {}", e2.what());
                return;
            }
        } else {
            spdlog::get("logger")->error("handleContactRelationDelete: Cannot determine accountId from either account object or task data");
            return;
        }
    }
    
    auto existing = findContactRelation(accountId, email);
    if (existing) {
        remove(existing.get());
    }else{
        spdlog::get("logger")->error("handleContactRelationDelete: contact relation not found for email: {}", email);
    }
}

void MailStore::triggerMessagesWithSummaryUpdate(string accountId, string threadId) {
    assertCorrectThread();
    
    try {
        // 查找该Thread中的所有Message
        auto messages = findAll<Message>(Query().equal("accountId", accountId).equal("threadId", threadId));
        
        spdlog::get("logger")->info("triggerMessagesWithSummaryUpdate: Found {} messages in thread {}", messages.size(), threadId);
        
        // 触发每个Message的重新推送，使用DeltaStreamItem来包含Summary数据
        for (auto& msg : messages) {
            // 创建一个包含Summary数据的自定义JSON
            json msgWithSummary = msg->toJSONDispatchWithSummary(this);
            
            // 创建DeltaStreamItem并直接通过SharedDeltaStream推送
            vector<json> jsonArray = {msgWithSummary};
            DeltaStreamItem item(DELTA_TYPE_PERSIST, msg->tableName(), jsonArray);
            SharedDeltaStream()->emit(item, _streamMaxDelay);
            
            spdlog::get("logger")->info("triggerMessagesWithSummaryUpdate: Pushed message {} with summary data", msg->id());
        }
        
        spdlog::get("logger")->info("triggerMessagesWithSummaryUpdate: Successfully triggered {} messages with summary", messages.size());
    } catch (const std::exception& e) {
        spdlog::get("logger")->error("triggerMessagesWithSummaryUpdate: Exception: {}", e.what());
    }
}


