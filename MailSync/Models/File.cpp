//
//  Folder.cpp
//  MailSync
//
//  Created by Ben Gotow on 6/17/17.
//  Copyright © 2017 Foundry 376. All rights reserved.
//
//  Use of this file is subject to the terms and conditions defined
//  in 'LICENSE.md', which is part of the Mailspring-Sync package.
//
#include "File.hpp"
#include "MailUtils.hpp"
#include "Thread.hpp"
#include "Message.hpp"
#include <nlohmann/json.hpp>
#include <SQLiteCpp/SQLiteCpp.h>
#include <regex>

using namespace std;
using namespace mailcore;
using json = nlohmann::json;

string File::TABLE_NAME = "File";

File::File(Message * msg, Attachment * a) :
    MailModel(MailUtils::idForFile(msg, a), msg->accountId(), 0)
{
    _data["messageId"] = msg->id();
    _data["partId"] = a->partID()->UTF8Characters();
    
    if (a->isInlineAttachment() && a->contentID()) {
        _data["contentId"] = a->contentID()->UTF8Characters();
    }
    if (a->mimeType()) {
        _data["contentType"] = a->mimeType()->UTF8Characters();
    }
    
    string name = "";
    if (a->filename()) {
        name = a->filename()->UTF8Characters();
    }
    if (name == "") {
        name = "Unnamed Attachment";

        string type = _data["contentType"];
        if (type == "text/calendar") {
            name = "Event.ics";
        }
        if (type == "image/png" || type == "image/x-png") {
            name = "Unnamed Image.png";
        }
        if (type == "image/jpg") {
            name = "Unnamed Image.jpg";
        }
        if (type == "image/jpeg") {
            name = "Unnamed Image.jpg";
        }
        if (type == "image/gif") {
            name = "Unnamed Image.gif";
        }
        if (type == "message/delivery-status") {
            name = "Delivery Status.txt";
        }
        if (type == "message/feedback-report") {
            name = "Feedback Report.txt";
        }
    }
    
    _data["filename"] = name;
    _data["size"] = a->data()->length();
    _data["updateTime"] = MailUtils::iso8601StringFromTime(msg->date());
}

File::File(json json) : MailModel(json) {
    
}

File::File(SQLite::Statement & query) :
    MailModel(query)
{
}

string File::constructorName() {
    return _data["__cls"].get<string>();
}

string File::tableName() {
    return File::TABLE_NAME;
}

string File::filename() {
    return _data["filename"].get<string>();
}

string File::safeFilename() {
    regex e ("[\\/:|?*><\"#]");
    return regex_replace (filename(), e, "-");
}

string File::partId() {
    return _data["partId"].get<string>();
}

json & File::contentId() {
    return _data["contentId"];
}

void File::setContentId(string s) {
    _data["contentId"] = s;
}

string File::contentType() {
    return _data["contentType"].get<string>();
}

int File::size() {
    return _data["size"].get<int>();
}

void File::setSize(int s) {
    _data["size"] = s;
}

string File::messageId() {
    return _data["messageId"].get<string>();
}

void File::setMessageId(string s) {
    _data["messageId"] = s;
}

string File::updateTime() {
    return _data["updateTime"].get<string>();
}

void File::setUpdateTime(string s) {
    _data["updateTime"] = s;
}

vector<string> File::columnsForQuery() {
    return vector<string>{"id", "data", "accountId", "version", "filename", "size", "contentType", "messageId", "updateTime"};
}

void File::bindToQuery(SQLite::Statement * query) {
    MailModel::bindToQuery(query);
    query->bind(":filename", filename());
    query->bind(":size", size());
    query->bind(":contentType", contentType());
    query->bind(":messageId", messageId());
    query->bind(":updateTime", updateTime());
}
