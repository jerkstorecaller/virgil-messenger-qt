//  Copyright (C) 2015-2020 Virgil Security, Inc.
//
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//      (1) Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//      (2) Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in
//      the documentation and/or other materials provided with the
//      distribution.
//
//      (3) Neither the name of the copyright holder nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
//  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
//  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
//  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
//  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.
//
//  Lead Maintainer: Virgil Security Inc. <support@virgilsecurity.com>

#include "database/VSQMessagesDatabase.h"

#include <QSqlError>
#include <QSqlQuery>

#include "VSQUtils.h"

VSQMessagesDatabase::VSQMessagesDatabase(QSqlDatabase *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
    connect(this, &VSQMessagesDatabase::fetch, this, &VSQMessagesDatabase::onFetch);
    connect(this, &VSQMessagesDatabase::insertMessage, this, &VSQMessagesDatabase::onInsertMessage);
    connect(this, &VSQMessagesDatabase::updateMessageStatus, this, &VSQMessagesDatabase::onUpdateMessageStatus);
}

VSQMessagesDatabase::~VSQMessagesDatabase()
{}

void VSQMessagesDatabase::createTablesIfDontExist()
{
    qCDebug(lcDatabase) << "Creation of table:" << m_tableName;
    const QString queryString = QString(
        "CREATE TABLE IF NOT EXISTS %1 ("
        "  id TEXT NOT NULL UNIQUE,"
        "  timestamp TEXT NOT NULL,"
        "  body TEXT NOT NULL,"
        "  contact TEXT NOT NULL,"
        "  author INTEGER NOT NULL,"
        "  status INTEGER NOT NULL,"
        "  attachment_id TEXT,"
        "  attachment_type INTEGER,"
        "  attachment_local_url TEXT,"
        "  attachment_size INTEGER"
        ")"
    ).arg(m_tableName);
    qCDebug(lcDatabase) << queryString;
    QSqlQuery query(queryString, *m_database);
    if (!query.exec()) {
        qFatal("Failed to create table: %s", qPrintable(query.lastError().text()));
    }

    const QString indexQueryString = QString(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_%1_id ON %1 (id);"
    ).arg(m_tableName);
    QSqlQuery indexQuery(indexQueryString, *m_database);
    if (!indexQuery.exec()) {
        qFatal("Failed to create table indices: %s", qPrintable(query.lastError().text()));
    }
}

void VSQMessagesDatabase::onFetch(const QString &user)
{
    if (user.isEmpty())
        return;

    // Create table
    m_tableName = QString("Messages_") + VSQUtils::escapedUserName(user);
    createTablesIfDontExist();

    // Fetch chats
    qCDebug(lcDatabase) << "Fetching chats:" << m_tableName;
    const QString queryString = QString(
        "SELECT contact, body, max(timestamp) AS timestamp, "
        "SUM(CASE author "
        "  WHEN 0 THEN 0 "
        "  ELSE "
        "    CASE status "
        "      WHEN 3 THEN 0"
        "      ELSE 1"
        "    END "
        "END) AS unread "
        "FROM %1 "
        "GROUP BY contact "
        "ORDER BY max(timestamp) DESC"
    ).arg(m_tableName);
    QSqlQuery query(queryString, *m_database);
    if (!query.exec()) {
        qFatal("Failed to fetch chats: %s", qPrintable(query.lastError().text()));
    }
    QVector<Chat> chats;
    while (query.next()) {
        Chat chat;
        chat.contact = query.value("contact").toString();
        chat.lastMessageBody = query.value("body").toString();
        chat.lastEventTimestamp = query.value("timestamp").toDateTime();
        chat.unreadMessageCount = query.value("unread").toInt();
        chats.push_back(chat);
    }
    emit chatsFetched(user, chats);

    // Fetch messages
    qCDebug(lcDatabase) << "Fetching messages:" << m_tableName;
    const QString queryString2 = QString(
        "SELECT * "
        "FROM %1 "
        "ORDER BY timestamp "
    ).arg(m_tableName);
    QSqlQuery query2(queryString2, *m_database);
    if (!query2.exec()) {
        qFatal("Failed to fetch messages: %s", qPrintable(query2.lastError().text()));
    }
    QVector<Message> messages;
    while (query2.next()) {
        Message message;
        message.id = query2.value("id").toString();
        message.timestamp = query2.value("timestamp").toDateTime();
        message.body = query2.value("body").toString();
        message.contact = query2.value("contact").toString();
        message.author = static_cast<Message::Author>(query2.value("author").toInt());
        message.status = static_cast<Message::Status>(query2.value("status").toInt());
        auto attachment_id = query2.value("attachment_id").toString();
        if (!attachment_id.isEmpty()) {
            Attachment attachment;
            attachment.id = attachment_id;
            attachment.type = static_cast<Attachment::Type>(query2.value("attachment_type").toInt());
            attachment.local_url = query2.value("local_url").toString();
            attachment.size = query2.value("size").toInt();
            message.attachment = attachment;
        }
        messages.push_back(message);
    }
    emit messagesFetched(user, messages);
}

void VSQMessagesDatabase::onInsertMessage(const Message &message)
{
    qCDebug(lcDatabase) << "Insertion of message:" << message.id;
    const QString queryString = QString(
        "INSERT INTO %1 (id, timestamp, body, contact, author, status, attachment_id, attachment_type, attachment_local_url, attachment_size)"
        "VALUES (:id, :timestamp, :body, :contact, :author, :status, :attachment_id, :attachment_type, :attachment_local_url, :attachment_size)"
    ).arg(m_tableName);
    QSqlQuery query(*m_database);
    query.prepare(queryString);
    query.bindValue(QLatin1String(":id"), message.id);
    query.bindValue(QLatin1String(":timestamp"), message.timestamp);
    query.bindValue(QLatin1String(":body"), message.body);
    query.bindValue(QLatin1String(":contact"), message.contact);
    query.bindValue(QLatin1String(":author"), static_cast<int>(message.author));
    query.bindValue(QLatin1String(":status"), static_cast<int>(message.status));
    query.bindValue(QLatin1String(":attachment_id"), message.attachment ? message.attachment->id : QString());
    query.bindValue(QLatin1String(":attachment_type"), message.attachment ? static_cast<int>(message.attachment->type) : 0);
    query.bindValue(QLatin1String(":attachment_local_url"), message.attachment ? message.attachment->local_url : QString());
    query.bindValue(QLatin1String(":attachment_size"), message.attachment ? message.attachment->size : 0);
    if (!query.exec()) {
        qFatal("Failed to insert message: %s", qPrintable(query.lastError().text()));
    }
}

void VSQMessagesDatabase::onUpdateMessageStatus(const Message &message)
{
    qCDebug(lcDatabase) << "Updating of message status:" << message.id << message.status;
    const QString queryString = QString(
        "UPDATE %1 "
        "SET status = %2 "
        "WHERE id = '%3'"
    ).arg(m_tableName).arg(static_cast<int>(message.status)).arg(message.id);
    QSqlQuery query(queryString, *m_database);
    if (!query.exec()) {
        qFatal("Failed to set message status: %s", qPrintable(message.id));
        return;
    }
}
