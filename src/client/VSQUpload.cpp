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

#include "client/VSQUpload.h"

#include <QFile>
#include <QMimeDatabase>
#include <QNetworkAccessManager>

VSQUpload::VSQUpload(QNetworkAccessManager *networkAccessManager, const QString &messageId, const Attachment &attachment, QObject *parent)
    : QObject(parent)
    , m_networkAccessManager(networkAccessManager)
    , m_messageId(messageId)
    , m_attachment(attachment)
    , m_running(false)
{
    qCDebug(lcUploader) << "Created upload. File url:" << m_attachment.local_url;
}

VSQUpload::~VSQUpload()
{
#ifdef VS_DEVMODE
    qCDebug(lcDev) << "~Upload";
#endif
}

QString VSQUpload::messageId() const
{
    return m_messageId;
}

Attachment VSQUpload::attachment() const
{
    return m_attachment;
}

void VSQUpload::setSlotId(const QString &id)
{
    m_slotId = id;
}

QString VSQUpload::slotId() const
{
    return m_slotId;
}

void VSQUpload::start(const QXmppHttpUploadSlotIq &slot)
{
    qCDebug(lcUploader) << QString("Started %1. Put url: %2").arg(toString(), slot.putUrl().toString());

    // Open attachment file
    auto file = new QFile(m_attachment.filePath(), this);
    connect(this, &VSQUpload::finished, file, &QFile::deleteLater);
    connect(this, &VSQUpload::failed, file, &QFile::deleteLater);
    if (!file->open(QFile::ReadOnly)) {
        emit failed(QString("Unable to open for reading:").arg(m_attachment.filePath()));
        return;
    }

    m_running = true;
    // Create request
    QNetworkRequest request(slot.putUrl());
    auto mimeType = QMimeDatabase().mimeTypeForUrl(m_attachment.local_url);
    if (mimeType.isValid())
        request.setHeader(QNetworkRequest::ContentTypeHeader, mimeType.name());
    request.setHeader(QNetworkRequest::ContentLengthHeader, m_attachment.size);
    // Create & connect reply
    auto reply = m_networkAccessManager->put(request, file);
#ifdef VS_DEVMODE
    qCDebug(lcDev) << "Created reply:" << reply;
#endif
    connect(reply, &QNetworkReply::uploadProgress, this, &VSQUpload::progressChanged);
    connect(reply, &QNetworkReply::finished, this, &VSQUpload::finished);
    connect(reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::error),
            this, std::bind(&VSQUpload::onNetworkReplyError, this, args::_1, reply));
    connect(this, &VSQUpload::finished, reply, std::bind(&VSQUpload::cleanupReply, this, reply));
    connect(this, &VSQUpload::failed, reply, std::bind(&VSQUpload::cleanupReply, this, reply));
}

void VSQUpload::abort()
{
    if (!m_running) {
        qCWarning(lcUploader) << "Stopped not-running upload";
    }
    auto message = QString("Aborted %1").arg(toString());
    qCDebug(lcUploader) << message;
    emit failed(message);
}

void VSQUpload::onNetworkReplyError(QNetworkReply::NetworkError error, QNetworkReply *reply)
{
    emit failed(QString("Network error (code %1): %2").arg(error).arg(reply->errorString()));
}

void VSQUpload::cleanupReply(QNetworkReply *reply)
{
    m_running = false;
    reply->deleteLater();
#ifdef VS_DEVMODE
    qCDebug(lcDev) << "Cleanuped reply:" << reply;
#endif
}

QString VSQUpload::toString() const
{
    return QString("Upload (Message id: %1, file: %2)").arg(m_messageId, m_attachment.local_url.toString());
}
