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

#ifndef VSQ_MESSAGESMODEL_H
#define VSQ_MESSAGESMODEL_H

#include <QAbstractListModel>

#include "VSQCommon.h"

class VSQMessagesDatabase;

Q_DECLARE_LOGGING_CATEGORY(lcMessagesModel)

class VSQMessagesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles
    {
        BodyRole = Qt::UserRole,
        TimeRole,
        NicknameRole,
        ContactRole,
        IsUserRoles,
        StatusRole,
        FailedRole,
        InRowRole,
        FirstInRowRole,
        AttachmentIdRole,
        AttachmentSizeRole,
        AttachmentDisplaySizeRole,
        AttachmentTypeRole,
        AttachmentLocalUrlRole,
        AttachmentLocalPreviewRole,
        AttachmentUploadedRole,
        AttachmentLoadingFailedRole
    };

    explicit VSQMessagesModel(VSQMessagesDatabase *messagesDatabase, QObject *parent = nullptr);
    ~VSQMessagesModel() override;

    void addMessage(const Message &message);
    void setMessageStatus(const Message &message, const Message::Status status);
    void setMessageStatusById(const QString &messageId, const Message::Status status);

    void setUploadProgress(const QString &messageId, DataSize bytesUploaded);
    void setUploadFailed(const QString &messageId, bool failed);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    QVariant data(const QModelIndex &index, int role) const override;

    void setRecipient(const QString &recipient);

signals:
    void messageStatusChanged(const Message &message);
    void recipientChanged(const QString &recipient);

private:
    void setUser(const QString &user);
    Optional<int> findMessageRow(const QString &id) const;
    void setMessageStatusByRow(int row, const Message::Status status);
    void addFetchedMessages(const QString &user, const QVector<Message> &messages);

    QString displayStatus(const Message &message) const;
    bool isInRow(const Message &message, int row) const;
    bool isFirstInRow(const Message &message, int row) const;

    VSQMessagesDatabase *m_messagesDatabase;
    QVector<Message> m_messages;
    QString m_user;
    QString m_recipient;
};

#endif // VSQ_MESSAGESMODEL_H
