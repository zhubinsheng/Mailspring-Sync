// 核心表
Table Account {
  id varchar(40) [pk]
  data blob
  accountId varchar(8)
  email_address text
}

Table Message {
  id varchar(40) [pk]
  accountId varchar(8)
  version integer
  data text
  headerMessageId varchar(255)
  gMsgId varchar(255)
  gThrId varchar(255)
  subject varchar(500)
  date datetime
  draft boolean
  unread boolean
  starred boolean
  remoteUID integer
  remoteXGMLabels text
  remoteFolderId varchar(40)
  replyToHeaderMessageId varchar(255)
  threadId varchar(40)
}

Table Thread {
  id varchar(42) [pk]
  accountId varchar(8)
  version integer
  data text
  gThrId varchar(20)
  subject varchar(500)
  snippet varchar(255)
  unread integer
  starred integer
  firstMessageTimestamp datetime
  lastMessageTimestamp datetime
  lastMessageReceivedTimestamp datetime
  lastMessageSentTimestamp datetime
  inAllMail boolean
  isSearchIndexed boolean
  participants text
  hasAttachments integer
}

Table Folder {
  id varchar(40) [pk]
  accountId varchar(8)
  version integer
  data text
  path varchar(255)
  role varchar(255)
  createdAt datetime
  updatedAt datetime
}

Table Label {
  id varchar(40) [pk]
  accountId varchar(8)
  version integer
  data text
  path varchar(255)
  role varchar(255)
  createdAt datetime
  updatedAt datetime
}

// 消息相关表
Table MessageBody {
  id varchar(40) [pk]
  value text
  fetchedAt datetime
}

Table ThreadReference {
  threadId varchar(42)
  accountId varchar(8)
  headerMessageId varchar(255)
  
  indexes {
    (threadId, accountId, headerMessageId) [pk]
  }
}

Table ThreadCategory {
  id varchar(40)
  value varchar(40)
  inAllMail boolean
  unread boolean
  lastMessageReceivedTimestamp datetime
  lastMessageSentTimestamp datetime
  
  indexes {
    (id, value) [pk]
  }
}

Table ThreadCounts {
  categoryId text [pk]
  unread integer
  total integer
}

// 搜索相关表
Table ThreadSearch {
  content_id varchar [note: 'UNINDEXED']
  subject text
  to_ text
  from_ text
  categories text
  body text
}

Table ContactSearch {
  content_id varchar [note: 'UNINDEXED']
  content text
}

// 联系人相关表
Table Contact {
  id varchar(40) [pk]
  data blob
  accountId varchar(8)
  email text
  version integer
  refs integer [default: 0]
  hidden boolean [default: false]
  source varchar(10) [default: 'mail']
  bookId varchar(40)
  etag varchar(40)
}

Table ContactBook {
  id varchar(40) [pk]
  accountId varchar(40)
  data blob
  version integer
}

Table ContactGroup {
  id varchar(40) [pk]
  accountId varchar(40)
  bookId varchar(40)
  data blob
  version integer
  name varchar(300)
}

Table ContactContactGroup {
  id varchar(40)
  value varchar(40)
  
  indexes {
    (id, value) [pk]
  }
}

Table ContactRelation {
  id varchar(40) [pk]
  accountId varchar(8)
  email text
  relation text
}

// 日历相关表
Table Calendar {
  id varchar(40) [pk]
  data blob
  accountId varchar(8)
}

Table Event {
  id varchar(40) [pk]
  data blob
  accountId varchar(8)
  etag varchar(40)
  calendarId varchar(40)
  recurrenceStart integer
  recurrenceEnd integer
  icsuid varchar(150)
}

// 任务相关表
Table Task {
  id varchar(40) [pk]
  version integer
  data blob
  accountId varchar(8)
  status varchar(255)
}

// 元数据相关表
Table ModelPluginMetadata {
  id varchar(40)
  accountId varchar(8)
  objectType varchar(15)
  value text
  expiration datetime
  
  indexes {
    (value, id) [pk]
  }
}

Table DetatchedPluginMetadata {
  objectId varchar(40)
  objectType varchar(15)
  accountId varchar(8)
  pluginId varchar(40)
  value blob
  version integer
  
  indexes {
    (objectId, accountId, pluginId) [pk]
  }
}

// 状态表
Table _State {
  id varchar(40) [pk]
  value text
}


// 补充 Summary 表
Table Summary {
  messageId varchar(40) [pk]
  accountId varchar(8)
  threadId varchar(40)
  briefSummary text
  messageSummary text
  threadSummary text
  important integer [default: 0]
  emergency integer [default: 0]
  category text
}

// 补充 File 表（之前有但重新完整列出）
Table File {
  id varchar(40) [pk]
  version integer
  data blob
  accountId varchar(8)
  filename text
  size integer [default: 0]
  contentType text
  messageId varchar(40)
  updateTime datetime
}

// 补充关系
Ref: Summary.messageId > Message.id
Ref: Summary.accountId > Account.id
Ref: Summary.threadId > Thread.id
Ref: File.accountId > Account.id

// 关系定义
Ref: Message.accountId > Account.id
Ref: Thread.accountId > Account.id
Ref: Folder.accountId > Account.id
Ref: Label.accountId > Account.id
Ref: Message.threadId > Thread.id
Ref: ThreadReference.threadId > Thread.id
Ref: ThreadCategory.id > Thread.id
Ref: MessageBody.id > Message.id
Ref: File.messageId > Message.id
Ref: Contact.accountId > Account.id
Ref: ContactGroup.accountId > Account.id
Ref: ContactBook.accountId > Account.id
Ref: ContactContactGroup.id > ContactGroup.id
Ref: ContactContactGroup.value > Contact.id
Ref: Calendar.accountId > Account.id
Ref: Event.calendarId > Calendar.id
Ref: Task.accountId > Account.id