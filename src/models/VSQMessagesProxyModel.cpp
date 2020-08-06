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

#include "models/VSQMessagesProxyModel.h"

#include "models/VSQMessagesModel.h"

VSQMessagesProxyModel::VSQMessagesProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setFilterKeyColumn(0);
    setFilterRole(VSQMessagesModel::ContactRole);
}

VSQMessagesProxyModel::~VSQMessagesProxyModel()
{}

void VSQMessagesProxyModel::setSourceModel(QAbstractItemModel *model)
{
    if (sourceModel()) {
        qFatal("Messages proxy model already has source model");
    }
    auto messagesModel = qobject_cast<VSQMessagesModel *>(model);
    if (!messagesModel) {
        qFatal("Only VSQMessagesModel cant be set as proxy source model");
    }

    connect(messagesModel, &VSQMessagesModel::recipientChanged, this, &VSQMessagesProxyModel::setRecipient);
    QSortFilterProxyModel::setSourceModel(model);
    invalidate();
}

void VSQMessagesProxyModel::setRecipient(const QString &recipient)
{
    if (m_recipient == recipient || recipient.isEmpty())
        return;
    m_recipient = recipient;
    qCDebug(lcMessagesModel) << "New recipient for messages filtering:" << recipient;
    setFilterFixedString(recipient);
    invalidate();
}
