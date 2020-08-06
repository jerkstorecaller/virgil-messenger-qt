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

#ifndef VSQ_UPLOAD_H
#define VSQ_UPLOAD_H

#include <QObject>
#include <QNetworkReply>

#include <QXmppHttpUploadIq.h>

#include "VSQCommon.h"

class QNetworkAccessManager;

Q_DECLARE_LOGGING_CATEGORY(lcUploader);

class VSQUpload : public QObject
{
    Q_OBJECT

public:
    VSQUpload(QNetworkAccessManager *networkAccessManager, const QString &messageId, const Attachment &attachment, QObject *parent);
    ~VSQUpload() override;

    QString messageId() const;
    Attachment attachment() const;

    void setSlotId(const QString &id);
    QString slotId() const;

    void start(const QXmppHttpUploadSlotIq &slot);
    void abort();

signals:
    void progressChanged(DataSize bytesReceived, DataSize bytesTotal);
    void finished();
    void failed(const QString &errorText);

private:
    void onNetworkReplyError(QNetworkReply::NetworkError error, QNetworkReply *reply);
    void cleanupReply(QNetworkReply *reply);

    QString toString() const;

    QNetworkAccessManager *m_networkAccessManager;
    QString m_messageId;
    Attachment m_attachment;

    QString m_slotId;
    bool m_running;
};

#endif // VSQ_UPLOAD_H