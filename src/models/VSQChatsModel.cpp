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

void VSQChatsModel::addMessage(const Message &message)
{
    const bool isUnread = message.author == Message::Author::Contact && message.status != Message::Status::Read;
    if (isUnread)
        qCDebug(lcChatsModel) << "Received unread message from" << message.contact;

    const auto chatRow = findChatRow(message.contact);
    if (!chatRow) {
        addNewChat(message.contact, message.body, message.timestamp, isUnread ? 1 : 0);
    }
    else {
        const int row = *chatRow;
        auto &chat = m_chats[row];
        chat.lastMessageBody = message.body;
        if (chat.lastEventTimestamp < message.timestamp) {
            chat.lastEventTimestamp = message.timestamp;
        }
        QVector<int> roles { LastMessageBodyRole, LastEventTimeRole, LastEventTimestampRole };
        if (isUnread) {
            ++chat.unreadMessageCount;
            roles << UnreadMessagesCountRole;
        }
        emit dataChanged(index(row), index(row), roles);
    }
}

void VSQChatsModel::addContact(const QString &contact)
{
    if (!findChatRow(contact)) {
        addNewChat(contact, QLatin1String(), QDateTime::currentDateTime(), 0);
    }
}

void VSQChatsModel::processMessageStatus(const Message &message)
{
    const bool isUnread = message.author == Message::Author::Contact && message.status != Message::Status::Read;
    const bool isRead = message.author == Message::Author::Contact && message.status == Message::Status::Read;

    const auto chatRow = findChatRow(message.contact);
    if (!chatRow) {
        addNewChat(message.contact, message.body, message.timestamp, isUnread ? 1 : 0);
    }
    else if (isUnread || isRead) {
        const int row = *chatRow;
        auto &chat = m_chats[row];
        if (isUnread) {
            ++chat.unreadMessageCount;
            qCDebug(lcChatsModel) << "Message was marked as unread:" << message.id;
        }
        else if (isRead) {
            --chat.unreadMessageCount;
            qCDebug(lcChatsModel) << "Message was marked as read:" << message.id;
        }
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

void VSQChatsModel::setRecipient(const QString &recipient)
{
    if (m_recipient == recipient) {
        return;
    }
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

void VSQChatsModel::addFetchedChats(const QString &user, const QVector<Chat> &chats)
{
    Q_UNUSED(user);
    beginResetModel();
    m_chats = chats;
    endResetModel();
    qCDebug(lcChatsModel) << "Chats are updated";
}

void VSQChatsModel::addNewChat(const QString &contact, const QString &lastMessageBody, const QDateTime &lastEventTimestamp, int unreadMessageCount)
{
    Chat chat;
    chat.contact = contact;
    chat.lastMessageBody = lastMessageBody;
    chat.lastEventTimestamp = lastEventTimestamp;
    chat.unreadMessageCount = unreadMessageCount;
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_chats.push_back(chat);
    endInsertRows();

    if (unreadMessageCount)
        qCDebug(lcChatsModel) << "Added new unread chat for:" << contact;
}
