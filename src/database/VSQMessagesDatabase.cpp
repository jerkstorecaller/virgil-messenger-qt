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
    connect(this, &VSQMessagesDatabase::insert, this, &VSQMessagesDatabase::onInsert);
    connect(this, &VSQMessagesDatabase::updateStatus, this, &VSQMessagesDatabase::onUpdateStatus);
}

VSQMessagesDatabase::~VSQMessagesDatabase()
{}

void VSQMessagesDatabase::setUser(const QString &userWithEnv)
{
    const QString user = VSQUtils::escapedUserName(userWithEnv);
    m_tableName = QString("Messages_") + user;
    emit reset();
    createTables();
    emit fetch();
}

void VSQMessagesDatabase::createTables()
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

void VSQMessagesDatabase::onFetch()
{
    qCDebug(lcDatabase) << "Fetching:" << m_tableName;
    const QString queryString = QString("SELECT * FROM %1 ORDER BY timestamp").arg(m_tableName);
    QSqlQuery query(queryString, *m_database);
    if (!query.exec()) {
        qFatal("Failed to fetch messages: %s", qPrintable(query.lastError().text()));
    }
    QVector<Message> messages;
    while (query.next()) {
        Message message;
        message.id = query.value("id").toString();
        message.timestamp = query.value("timestamp").toDateTime();
        message.body = query.value("body").toString();
        message.contact = query.value("contact").toString();
        message.author = static_cast<Message::Author>(query.value("author").toInt());
        message.status = static_cast<Message::Status>(query.value("status").toInt());
        auto attachment_id = query.value("attachment_id").toString();
        if (!attachment_id.isEmpty()) {
            Attachment attachment;
            attachment.id = attachment_id;
            attachment.type = static_cast<Attachment::Type>(query.value("attachment_type").toInt());
            attachment.local_url = query.value("local_url").toString();
            attachment.size = query.value("size").toInt();
            message.attachment = attachment;
        }
        messages.push_back(message);
    }
    emit fetched(messages, true); // TODO(fpohtmeh): fetch paritally
}

void VSQMessagesDatabase::onInsert(const Message &message)
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

void VSQMessagesDatabase::onUpdateStatus(const Message &message)
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
