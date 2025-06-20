#include "Summary.hpp"
#include "MailUtils.hpp"
#include "Message.hpp"

using namespace std;

string Summary::TABLE_NAME = "Summary";

Summary::Summary() : MailModel(json::object()) {
    _data["messageId"] = "";
    _data["accountId"] = "";
    _data["threadId"] = "";
    _data["briefSummary"] = "";
    _data["messageSummary"] = "";
    _data["threadSummary"] = "";
    _data["important"] = false;
    _data["emergency"] = false;
    _data["category"] = "";
}

Summary::Summary(Message * msg) :
    MailModel(msg->id(), msg->accountId(), 0)
{
    _data["messageId"] = msg->id();
    _data["accountId"] = msg->accountId();
    _data["threadId"] = msg->threadId();
    _data["briefSummary"] = "";
    _data["messageSummary"] = "";
    _data["threadSummary"] = "";
    _data["important"] = false;
    _data["emergency"] = false;
    _data["category"] = "";
}

Summary::Summary(json json) : MailModel(json) {
    if (json.contains("messageId")) _data["messageId"] = json["messageId"];
    if (json.contains("accountId")) _data["accountId"] = json["accountId"];
    if (json.contains("threadId")) _data["threadId"] = json["threadId"];
    if (json.contains("briefSummary")) _data["briefSummary"] = json["briefSummary"];
    if (json.contains("messageSummary")) _data["messageSummary"] = json["messageSummary"];
    if (json.contains("threadSummary")) _data["threadSummary"] = json["threadSummary"];
    if (json.contains("important")) _data["important"] = json["important"];
    if (json.contains("emergency")) _data["emergency"] = json["emergency"];
    if (json.contains("category")) _data["category"] = json["category"];
}

Summary::Summary(SQLite::Statement & query) :
    MailModel(query)
{
}

string Summary::constructorName() {
    return _data["__cls"].get<string>();
}

string Summary::tableName() {
    return Summary::TABLE_NAME;
}

string Summary::messageId() {
    return _data["messageId"].get<string>();
}

void Summary::setMessageId(string id) {
    _data["messageId"] = id;
}

string Summary::threadId() {
    return _data["threadId"].get<string>();
}

void Summary::setThreadId(string id) {
    _data["threadId"] = id;
}

string Summary::accountId() {
    return _data["accountId"].get<string>();
}

void Summary::setAccountId(string id) {
    _data["accountId"] = id;
}

string Summary::briefSummary() {
    return _data["briefSummary"].get<string>();
}

void Summary::setBriefSummary(string s) {
    _data["briefSummary"] = s;
}

string Summary::messageSummary() {
    return _data["messageSummary"].get<string>();
}

void Summary::setMessageSummary(string s) {
    _data["messageSummary"] = s;
}

string Summary::threadSummary() {
    return _data["threadSummary"].get<string>();
}

void Summary::setThreadSummary(string s) {
    _data["threadSummary"] = s;
}

bool Summary::isImportant() {
    return _data["important"].get<bool>();
}

void Summary::setImportant(bool v) {
    _data["important"] = v;
}

bool Summary::isEmergency() {
    return _data["emergency"].get<bool>();
}

void Summary::setEmergency(bool v) {
    _data["emergency"] = v;
}

string Summary::category() {
    return _data["category"].get<string>();
}

void Summary::setCategory(string s) {
    _data["category"] = s;
}

vector<string> Summary::columnsForQuery() {
    return {
        "messageId",
        "accountId",
        "threadId",
        "briefSummary",
        "messageSummary",
        "threadSummary",
        "important",
        "emergency",
        "category"
    };
}

void Summary::bindToQuery(SQLite::Statement * query) {
    query->bind(":messageId", messageId());
    query->bind(":accountId", accountId());
    query->bind(":threadId", threadId());
    query->bind(":briefSummary", briefSummary());
    query->bind(":messageSummary", messageSummary());
    query->bind(":threadSummary", threadSummary());
    query->bind(":important", isImportant());
    query->bind(":emergency", isEmergency());
    query->bind(":category", category());
} 