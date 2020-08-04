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

#ifndef VSQ_CLIENT_H
#define VSQ_CLIENT_H

#include <QObject>

#include <QXmppClient.h>
#include <QXmppMessage.h>

#include "VSQCore.h"
#include "VSQUploader.h"

class QNetworkAccessManager;

class QXmppCarbonManager;

Q_DECLARE_LOGGING_CATEGORY(lcClient)
Q_DECLARE_LOGGING_CATEGORY(lcXmpp)

class VSQClient : public QObject
{
    Q_OBJECT

public:
    VSQClient(VSQSettings *settings, QNetworkAccessManager *networkAccessManager, QObject *parent);
    ~VSQClient() override;

    void start();

signals:
    void signIn(const QString &userWithEnv);
    void signOut();
    void signUp(const QString &user);
    void backupKey(const QString &password);
    void signInWithKey(const QString &userWithEnv, const QString &password);

    void signedIn(const QString &userWithEnv);
    void signInFailed(const QString &userWithEnv, const QString &errorText);
    void signedOut();
    void signedUp(const QString &userWithEnv);
    void signUpFailed(const QString &userWithEnv, const QString &errorText);
    void keyBackuped(const QString &password);
    void backupKeyFailed(const QString &password, const QString &errorText);

    void addContact(const QString &contact);
    void sendMessage(const Message &message);
    void checkConnectionState();
    void setOnlineStatus(bool online);
    void virgilUrlChanged(const QString &url);

    void contactAdded(const QString &contact);
    void addContactFailed(const QString &contact, const QString &errorText);
    void messageSent(const QString &messageId);
    void sendMessageFailed(const QString &messageId, const QString &errorText);
    void messageReceived(const Message &message);
    void receiveMessageFailed(const QString &messageId, const QString &errorText);
    void messageDelivered(const QString &messageId);

    void uploadStarted(const QString &messageId);
    void uploadProgressChanged(const QString &messageId, DataSize bytesReceived, DataSize bytesTotal);
    void uploadFinished(const QString &messageId);
    void uploadFailed(const QString &messageId, const QString &errorText);

private:
    bool xmppConnect();
    void xmppDisconnect();
    void xmppReconnect();

    void waitForConnection();
    void stopWaitForConnection();
    void subscribeOnPushNotifications(bool enable);

    Optional<QXmppMessage> createXmppMessage(const Message &message);
    Optional<Message> createMessage(const QXmppMessage &xmppMessage);

    void onSignIn(const QString &userWithEnv);
    void onSignOut();
    void onSignUp(const QString &userWithEnv);
    void onBackupKey(const QString &password);
    void onSignInWithKey(const QString &userWithEnv, const QString &password);
    void onAddContact(const QString &contact);

    void onConnected();
    void onDisconnected();
    void onError(QXmppClient::Error error);
    void onMessageReceived(const QXmppMessage &xmppMessage);
    void onPresenceReceived(const QXmppPresence &presence);
    void onIqReceived(const QXmppIq &iq);
    void onStateChanged(QXmppClient::State state);

    void onSendMessage(const Message &message);
    void onCheckConnectionState();
    void onSetOnlineStatus(bool online);
    void onMessageDelivered(const QString &jid, const QString &messageId);
    void onDiscoveryInfoReceived(const QXmppDiscoveryIq &info);
    void onXmppLoggerMessage(QXmppLogger::MessageType type, const QString &message);
    void onSslErrors(const QList<QSslError> &errors);
    void updateVirgilUrl();

    VSQCore m_core;
    QXmppClient m_client;
    QXmppCarbonManager *m_carbonManager;
    VSQUploader m_uploader;

    QString m_lastErrorText;
    bool m_waitingForConnection;
    QString m_virgilUrl;
};

#endif // VSQ_CLIENT_H
