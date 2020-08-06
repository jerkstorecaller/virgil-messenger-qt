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

#include "models/VSQChatsModel.h"

#include "database/VSQMessagesDatabase.h"
#include "VSQUtils.h"

Q_LOGGING_CATEGORY(lcChatsModel, "chatsModel")

VSQChatsModel::VSQChatsModel(VSQMessagesDatabase *messagesDatabase, QObject *parent)
    : QAbstractListModel(parent)
    , m_messagesDatabase(messagesDatabase)
{
    connect(m_messagesDatabase, &VSQMessagesDatabase::chatsFetched, this, &VSQChatsModel::addFetchedChats);
}

VSQChatsModel::~VSQChatsModel()
{}

void VSQChatsModel::processMessage(const Message &message)
{
    updateChat(message.contact, message.body, message.timestamp, message.status);
}

void VSQChatsModel::processContact(const QString &contact)
{
    if (!findChatRow(contact))
        addChat(contact, QLatin1String(), QDateTime::currentDateTime(), NullOptional);
}

void VSQChatsModel::updateMessageStatus(const Message &message)
{
    auto chatRow = findChatRow(message.contact);
    if (!chatRow) {
        qCritical() << "Message status changed but chat doesn't exist:" << message.contact;
    }
    else if (message.status == Message::Status::Read) {
        const int row = *chatRow;
        m_chats[row].unreadMessageCount--;
        emit dataChanged(index(row), index(row), { UnreadMessagesCountRole });
    }
}

int VSQChatsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_chats.size();
}

QHash<int, QByteArray> VSQChatsModel::roleNames() const
{
    QHash<int, QByteArray> names;
    names[NicknameRole] = "nickname";
    names[LastMessageBodyRole] = "lastMessageBody";
    names[LastEventTimestampRole] = "lastEventTimestamp";
    names[LastEventTimeRole] = "lastEventTime";
    names[UnreadMessagesCountRole] = "unreadMessagesCount";
    return names;
}

QVariant VSQChatsModel::data(const QModelIndex &index, int role) const
{
    const auto &chat = m_chats[index.row()];
    switch (role) {
    case NicknameRole:
        return chat.contact;
    case LastMessageBodyRole:
        return chat.lastMessageBody.left(30);
    case LastEventTimestampRole:
        return chat.lastEventTimestamp;
    case LastEventTimeRole:
        return chat.lastEventTimestamp.toString("hh:mm");
    case UnreadMessagesCountRole:
        return chat.unreadMessageCount;
    default:
        return QVariant();
    }
}

void VSQChatsModel::setUser(const QString &user)
{
    if (user.isEmpty())
        return;
    qCDebug(lcChatsModel) << "Chats user:" << user;
}

void VSQChatsModel::setRecipient(const QString &recipient)
{
    if (m_recipient == recipient || recipient.isEmpty())
        return;
    qCDebug(lcChatsModel) << "Chats recipient:" << recipient;
    m_recipient = recipient;
}

Optional<int> VSQChatsModel::findChatRow(const QString &contact) const
{
    // TODO(fpohtmeh): add caching
    int row = -1;
    for (auto &chat : m_chats) {
        ++row;
        if (chat.contact == contact)
            return row;
    }
    return NullOptional;
}

void VSQChatsModel::addChat(const QString &contact, const QString &messageBody, const QDateTime &eventTimestamp,
                            const Optional<Message::Status> status)
{
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    Chat chat;
    chat.contact = contact;
    chat.lastMessageBody = messageBody;
    chat.lastEventTimestamp = eventTimestamp;
    chat.unreadMessageCount = (status && *status == Message::Status::Received) ? 1 : 0;
    m_chats.push_back(chat);
    endInsertRows();
}

void VSQChatsModel::updateChat(const QString &contact, const QString &messageBody, const QDateTime &eventTimestamp,
                            const Optional<Message::Status> status)
{
    const auto chatRow = findChatRow(contact);
    if (!chatRow) {
        addChat(contact, messageBody, eventTimestamp, status);
    }
    else {
        const int row = *chatRow;
        auto &chat = m_chats[row];
        if (chat.lastEventTimestamp > eventTimestamp)
            return;
        chat.lastMessageBody = messageBody;
        chat.lastEventTimestamp = eventTimestamp;
        QVector<int> roles { LastMessageBodyRole, LastEventTimeRole, LastEventTimestampRole };
        if (status && *status == Message::Status::Received && m_recipient != contact) {
            ++chat.unreadMessageCount;
            roles << UnreadMessagesCountRole;
        }
        emit dataChanged(index(row), index(row), roles);
    }
}

void VSQChatsModel::addFetchedChats(const QVector<Chat> &chats)
{
    beginResetModel();
    m_chats = chats;
    endResetModel();
    qCDebug(lcChatsModel) << "Chats are updated";
}
