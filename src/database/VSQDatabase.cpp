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

#include "database/VSQDatabase.h"

#include <QSqlError>
#include <QSqlQuery>

#include "VSQSettings.h"
#include "VSQUtils.h"

Q_LOGGING_CATEGORY(lcDatabase, "database")

VSQDatabase::VSQDatabase(const VSQSettings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_connectionName(QLatin1String("VSQDatabase"))
{}

VSQDatabase::~VSQDatabase()
{
    m_db.close();
#ifdef VS_DEVMODE
    qCDebug(lcDev) << "~Database";
#endif
}

void VSQDatabase::open()
{
    qCDebug(lcDatabase) << "Database opening...";
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    if (!m_db.isValid()) {
        qFatal("Cannot add database: %s", qPrintable(m_db.lastError().text()));
        emit failed();
    }
    m_db.setDatabaseName(m_settings->databaseFileName());
    if (!m_db.open()) {
        qFatal("Cannot open database: %s", qPrintable(m_db.lastError().text()));
        emit failed();
    }
    qCDebug(lcDatabase) << "Database is opened:" << m_db.databaseName();
    emit opened();
}

void VSQDatabase::setUser(const QString &userWithEnv)
{
    qCDebug(lcDatabase) << "Set user:" << userWithEnv;
    const QString user = VSQUtils::escapedUserName(userWithEnv);
    const QString messagesTableName = QString("Messages_") + user;
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
    ).arg(messagesTableName);
    qCDebug(lcDatabase) << queryString;
    QSqlQuery query(queryString, m_db);
    if (!query.exec()) {
        qFatal("Failed to create database table %s", qPrintable(messagesTableName));
    }
}
