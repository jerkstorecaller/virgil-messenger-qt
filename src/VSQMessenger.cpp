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

#include "VSQMessenger.h"

#include <QThread>

#include "VSQCrashReporter.h"
#include "VSQPushNotifications.h"
#include "VSQSettings.h"
#include "VSQUtils.h"
#include "client/VSQCore.h"
#include "database/VSQDatabase.h"

Q_LOGGING_CATEGORY(lcMessenger, "messenger");

VSQMessenger::VSQMessenger(VSQSettings *settings, QNetworkAccessManager *networkAccessManager, VSQCrashReporter *crashReporter, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
    , m_networkAccessManager(networkAccessManager)
    , m_crashReporter(crashReporter)
    , m_database(new VSQDatabase(settings, nullptr))
    , m_databaseThread(new QThread())
    , m_client(new VSQClient(settings, networkAccessManager, nullptr))
    , m_clientThread(new QThread())
    , m_attachmentBuilder(settings)
    , m_messagesModel(m_database->messages(), this)
    , m_chatsModel(m_database->messages(), this)
{
    m_database->moveToThread(m_databaseThread);
    connect(m_databaseThread, &QThread::started, m_database, &VSQDatabase::open);
    connect(m_database, &VSQDatabase::failed, this, &VSQMessenger::quitRequested);
    connect(m_database, &VSQDatabase::opened, m_clientThread, std::bind(&QThread::start, m_clientThread, QThread::InheritPriority));

    m_client->moveToThread(m_clientThread);
    connect(m_clientThread, &QThread::started, m_client, &VSQClient::start);

    m_databaseThread->start();

#if VS_PUSHNOTIFICATIONS
    VSQPushNotifications::instance().startMessaging();
#endif
}

VSQMessenger::~VSQMessenger()
{
    m_clientThread->quit();
    m_clientThread->wait();
    delete m_client;
    delete m_clientThread;

    m_databaseThread->quit();
    m_databaseThread->wait();
    delete m_database;
    delete m_databaseThread;

#ifdef VS_DEVMODE
    qCDebug(lcDev) << "~Messenger";
#endif
}

void VSQMessenger::start()
{
    if (!m_databaseThread->isRunning())
        qFatal("Database thread isn't running");
    if (!m_clientThread->isRunning())
        qFatal("Client thread isn't running");

    setupConnections();

    // Sign-in or request credentials
    if (m_settings->lastSignedInUser().isEmpty())
        emit credentialsRequested(false);
    else
        signIn(m_settings->lastSignedInUser());
}

void VSQMessenger::setupConnections()
{
    // Messenger
    connect(this, &VSQMessenger::createSendMessage, this, &VSQMessenger::onCreateSendMessage);
    connect(this, &VSQMessenger::sendMessage, this, &VSQMessenger::addMessage);

    // Messenger to client
    connect(this, &VSQMessenger::signIn, m_client, &VSQClient::signIn);
    connect(this, &VSQMessenger::signInWithKey, m_client, &VSQClient::signInWithKey);
    connect(this, &VSQMessenger::signOut, m_client, &VSQClient::signOut);
    connect(this, &VSQMessenger::signUp, m_client, &VSQClient::signUp);
    connect(this, &VSQMessenger::backupKey, m_client, &VSQClient::backupKey);

    connect(this, &VSQMessenger::addContact, m_client, &VSQClient::addContact);

    connect(this, &VSQMessenger::sendMessage, m_client, &VSQClient::sendMessage);

    connect(this, &VSQMessenger::checkConnectionState, m_client, &VSQClient::checkConnectionState);
    connect(this, &VSQMessenger::setOnlineStatus, m_client, &VSQClient::setOnlineStatus);

    // Client to messenger
    connect(m_client, &VSQClient::signedIn, this, &VSQMessenger::signedIn);
    connect(m_client, &VSQClient::signInFailed, this, &VSQMessenger::signInFailed);
    connect(m_client, &VSQClient::signedOut, this, std::bind(&VSQMessenger::credentialsRequested, this, true));
    connect(m_client, &VSQClient::signedOut, this, &VSQMessenger::signedOut);
    connect(m_client, &VSQClient::keyBackuped, this, &VSQMessenger::keyBackuped);
    connect(m_client, &VSQClient::backupKeyFailed, this, &VSQMessenger::backupKeyFailed);

    connect(m_client, &VSQClient::contactAdded, this, &VSQMessenger::contactAdded);
    connect(m_client, &VSQClient::addContactFailed, this, &VSQMessenger::addContactFailed);

    connect(m_client, &VSQClient::messageSent, this, &VSQMessenger::messageSent);
    connect(m_client, &VSQClient::sendMessageFailed, this, &VSQMessenger::sendMessageFailed);

    connect(m_client, &VSQClient::messageReceived, this, &VSQMessenger::addMessage);

    connect(m_client, &VSQClient::signedIn, this, &VSQMessenger::setUser);
    connect(m_client, &VSQClient::signedOut, this, std::bind(&VSQMessenger::setUser, this, QLatin1String()));

    // Messages model: signals from client
    connect(m_client, &VSQClient::messageSent, &m_messagesModel,
        std::bind(&VSQMessagesModel::setMessageStatusById, &m_messagesModel, args::_1, Message::Status::Sent));
    connect(m_client, &VSQClient::sendMessageFailed, &m_messagesModel,
        std::bind(&VSQMessagesModel::setMessageStatusById, &m_messagesModel, args::_1, Message::Status::Failed));
    connect(m_client, &VSQClient::messageDelivered, &m_messagesModel,
        std::bind(&VSQMessagesModel::setMessageStatusById, &m_messagesModel, args::_1, Message::Status::Received));

    connect(m_client, &VSQClient::uploadStarted, &m_messagesModel,
        std::bind(&VSQMessagesModel::setUploadFailed, &m_messagesModel, args::_1, false));
    connect(m_client, &VSQClient::uploadProgressChanged, &m_messagesModel, &VSQMessagesModel::setUploadProgress);
    connect(m_client, &VSQClient::uploadFinished, &m_messagesModel,
        std::bind(&VSQMessagesModel::setUploadFailed, &m_messagesModel, args::_1, false));
    connect(m_client, &VSQClient::uploadFailed, &m_messagesModel,
        std::bind(&VSQMessagesModel::setUploadFailed, &m_messagesModel, args::_1, true));

    // Chats model: signals from client
    connect(m_client, &VSQClient::contactAdded, &m_chatsModel, &VSQChatsModel::addContact);
    // Chats model: signals from messages model
    connect(&m_messagesModel, &VSQMessagesModel::messageStatusChanged, &m_chatsModel, &VSQChatsModel::processMessageStatus);

    // Settings
    connect(m_client, &VSQClient::signedIn, m_settings, &VSQSettings::addUserToList);
    connect(m_client, &VSQClient::signedIn, m_settings, &VSQSettings::setLastSignedInUser);

    // Crash reporter
    connect(m_client, &VSQClient::virgilUrlChanged, m_crashReporter, &VSQCrashReporter::setUrl);
}

void VSQMessenger::setUser(const QString &user)
{
    if (user == m_user)
        return;
    m_user = user;
    qCDebug(lcMessenger) << "Messenger user:" << user;
    emit userChanged(user);

    m_database->fetch(user);
    setRecipient(QString());
}

void VSQMessenger::setRecipient(const QString &recipient)
{
    if (recipient == m_recipient)
        return;
    m_recipient = recipient;
    qCDebug(lcMessenger) << "Messenger recipient:" << recipient;
    emit recipientChanged(recipient);

    m_chatsModel.setRecipient(recipient);
    m_messagesModel.setRecipient(recipient);
}

VSQMessagesModel *VSQMessenger::messageModel()
{
    return &m_messagesModel;
}

VSQChatsModel *VSQMessenger::chatsModel()
{
    return &m_chatsModel;
}

void VSQMessenger::addMessage(const Message &message)
{
    auto msg = message;
    if (msg.author == Message::Author::Contact && msg.contact == m_recipient && msg.status != Message::Status::Read) {
        qCDebug(lcMessenger) << "Message" << msg.id << "was marked as read";
        msg.status = Message::Status::Read;
    }
    m_messagesModel.addMessage(msg);
    m_chatsModel.addMessage(msg);
    if (msg.contact == m_recipient)
        emit recipientMessageAdded();
}

void VSQMessenger::onCreateSendMessage(const QString &text, const QVariant &attachmentUrl, const Enums::AttachmentType attachmentType)
{
    const QString uuid = VSQUtils::createUuid();
    QString messageText = text;
    const auto attachment = m_attachmentBuilder.build(attachmentUrl.toUrl(), attachmentType);
    const Message message {
        uuid,
        QDateTime::currentDateTime(),
        messageText,
        m_recipient,
        Message::Author::User,
        attachment,
        Message::Status::Created
    };

    emit sendMessage(message);
}
