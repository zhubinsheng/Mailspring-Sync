#include "Summary.hpp"
#include "MailUtils.hpp"
#include "Message.hpp"

using namespace std;

string Summary::TABLE_NAME = "Summary";

Summary::Summary() : MailModel(json::object()) {
    _data["messageId"] = "";
    _data["threadId"] = "";
    _data["briefSummary"] = "";
    _data["messageSummary"] = "";
    _data["threadSummary"] = "";
    _data["important"] = false;
    _data["emergency"] = false;
    _data["urgencyStatus"] = 0;
    _data["SummaryTagStatus"] = 0;
}

Summary::Summary(string id, string accountId, int version) :
    MailModel(id, accountId, version)
{
    _data["messageId"] = "";
    _data["threadId"] = "";
    _data["briefSummary"] = "";
    _data["messageSummary"] = "";
    _data["threadSummary"] = "";
    _data["important"] = false;
    _data["emergency"] = false;
    _data["urgencyStatus"] = 0;
    _data["SummaryTagStatus"] = 0;
}

Summary::Summary(Message * msg) :
    MailModel(msg->id(), msg->accountId(), 0)
{
    _data["messageId"] = msg->id();
    _data["threadId"] = msg->threadId();
    _data["briefSummary"] = "";
    _data["messageSummary"] = "";
    _data["threadSummary"] = "";
    _data["important"] = false;
    _data["emergency"] = false;
    _data["urgencyStatus"] = 0;
    _data["SummaryTagStatus"] = 0;
}

Summary::Summary(json json) : MailModel(json) {
    if (json.contains("messageId")) _data["messageId"] = json["messageId"];
    if (json.contains("threadId")) _data["threadId"] = json["threadId"];
    if (json.contains("briefSummary")) _data["briefSummary"] = json["briefSummary"];
    if (json.contains("messageSummary")) _data["messageSummary"] = json["messageSummary"];
    if (json.contains("threadSummary")) _data["threadSummary"] = json["threadSummary"];
    if (json.contains("important")) _data["important"] = json["important"];
    if (json.contains("emergency")) _data["emergency"] = json["emergency"];
    if (json.contains("urgencyStatus")) _data["urgencyStatus"] = json["urgencyStatus"];
    if (json.contains("SummaryTagStatus")) _data["SummaryTagStatus"] = json["SummaryTagStatus"];
}

Summary::Summary(SQLite::Statement & query) :
    MailModel(query)
{
}

string Summary::constructorName() {
    return _data["__cls"].is_null() ? "" : _data["__cls"].get<string>();
}

string Summary::tableName() {
    return Summary::TABLE_NAME;
}

string Summary::messageId() {
    return _data["messageId"].is_null() ? "" : _data["messageId"].get<string>();
}

void Summary::setMessageId(string id) {
    _data["messageId"] = id;
}

string Summary::threadId() {
    return _data["threadId"].is_null() ? "" : _data["threadId"].get<string>();
}

void Summary::setThreadId(string id) {
    _data["threadId"] = id;
}

string Summary::briefSummary() {
    return _data["briefSummary"].is_null() ? "" : _data["briefSummary"].get<string>();
}

void Summary::setBriefSummary(string s) {
    _data["briefSummary"] = s;
}

string Summary::messageSummary() {
    return _data["messageSummary"].is_null() ? "" : _data["messageSummary"].get<string>();
}

void Summary::setMessageSummary(string s) {
    _data["messageSummary"] = s;
}

string Summary::threadSummary() {
    return _data["threadSummary"].is_null() ? "" : _data["threadSummary"].get<string>();
}

void Summary::setThreadSummary(string s) {
    _data["threadSummary"] = s;
}

bool Summary::isImportant() {
    return _data["important"].is_null() ? false : _data["important"].get<bool>();
}

void Summary::setImportant(bool v) {
    _data["important"] = v;
}

bool Summary::isEmergency() {
    return _data["emergency"].is_null() ? false : _data["emergency"].get<bool>();
}

void Summary::setEmergency(bool v) {
    _data["emergency"] = v;
}

int Summary::urgencyStatus() {
    return _data["urgencyStatus"].is_null() ? 0 : _data["urgencyStatus"].get<int>();
}

void Summary::setUrgencyStatus(int v) {
    _data["urgencyStatus"] = v;
}

int Summary::SummaryTagStatus() {
    return _data["SummaryTagStatus"].is_null() ? 0 : _data["SummaryTagStatus"].get<int>();
}

void Summary::setSummaryTagStatus(int v) {
    _data["SummaryTagStatus"] = v;
}

vector<string> Summary::columnsForQuery() {
    return {
        "id",
        "data", 
        "accountId",
        "version",
        "messageId",
        "threadId",
        "briefSummary",
        "messageSummary",
        "threadSummary",
        "important",
        "emergency",
        "urgencyStatus",
        "SummaryTagStatus"
    };
}

void Summary::bindToQuery(SQLite::Statement * query) {
    MailModel::bindToQuery(query);
    query->bind(":messageId", messageId());
    query->bind(":threadId", threadId());
    query->bind(":briefSummary", briefSummary());
    query->bind(":messageSummary", messageSummary());
    query->bind(":threadSummary", threadSummary());
    query->bind(":urgencyStatus", urgencyStatus());
    query->bind(":important", isImportant());
    query->bind(":emergency", isEmergency());
    query->bind(":SummaryTagStatus", SummaryTagStatus());
} 