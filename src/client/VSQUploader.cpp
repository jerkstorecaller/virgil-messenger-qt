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

#include "client/VSQUploader.h"

#include <QFileInfo>

#include <QXmppClient.h>
#include <QXmppUploadRequestManager.h>

#include "client/VSQUpload.h"

Q_LOGGING_CATEGORY(lcUploader, "uploader");

VSQUploader::VSQUploader(QXmppClient *client, QNetworkAccessManager *networkAccessManager, QObject *parent)
    : QObject(parent)
    , m_networkAccessManager(networkAccessManager)
    , m_xmppManager(new QXmppUploadRequestManager())
    , m_activeUploadsCount(0)
{
    client->addExtension(m_xmppManager.get());
    connect(m_xmppManager.get(), &QXmppUploadRequestManager::slotReceived, this, &VSQUploader::onSlotReceived);
    connect(m_xmppManager.get(), &QXmppUploadRequestManager::requestFailed, this, &VSQUploader::onRequestFailed);
}

VSQUploader::~VSQUploader()
{
    for (auto upload : m_uploads) {
        upload->abort();
    }
#ifdef VS_DEVMODE
    qCDebug(lcDev) << "~Uploader";
#endif
}

void VSQUploader::addUpload(const QString &messageId, const Attachment &attachment)
{
    qCDebug(lcUploader) << "Adding upload for message:" << messageId;
    auto upload = new VSQUpload(m_networkAccessManager, messageId, attachment, this);
    m_uploads.push_back(upload);
    connect(upload, &VSQUpload::progressChanged, this, std::bind(&VSQUploader::onUploadProgressChanged, this, upload, args::_1, args::_2));
    connect(upload, &VSQUpload::finished, this, std::bind(&VSQUploader::onUploadFinished, this, upload));
    connect(upload, &VSQUpload::failed, this, std::bind(&VSQUploader::onUploadFailed, this, upload, args::_1));
    startNextUpload();
}

void VSQUploader::onSlotReceived(const QXmppHttpUploadSlotIq &slot)
{
    qCDebug(lcUploader) << "onSlotReceived";
    for (auto upload : m_uploads) {
        if (slot.id() != upload->slotId())
            continue;
        upload->start(slot);
    }
}

void VSQUploader::onRequestFailed(const QXmppHttpUploadRequestIq &request)
{
    Q_UNUSED(request);
    qCDebug(lcUploader) << "onRequestFailed";
}

void VSQUploader::onUploadProgressChanged(VSQUpload *upload, DataSize bytesReceived, DataSize bytesTotal)
{
    qCDebug(lcUploader) << "Upload progress:" << bytesReceived << "/" << bytesTotal;
    emit uploadProgressChanged(upload->messageId(), bytesReceived, bytesTotal);
}

void VSQUploader::onUploadFinished(VSQUpload *upload)
{
    --m_activeUploadsCount;
    emit uploadFinished(upload->messageId());
    startNextUpload();
}

void VSQUploader::onUploadFailed(VSQUpload *upload, const QString &errorText)
{
    --m_activeUploadsCount;
    emit uploadFailed(upload->messageId(), errorText);
    m_uploads.push_back(upload); // return upload to queue for future processing
}

void VSQUploader::startNextUpload()
{
    if (m_uploads.empty())
        return;
    if (m_activeUploadsCount > 0)
        return; // TODO(fpohtmeh): add parallel uploads

    // TODO(fpohtmeh): check connection
    ++m_activeUploadsCount;
    auto upload = m_uploads.front();
    m_uploads.erase(m_uploads.begin());

    const auto attachment = upload->attachment();
    auto slotId = m_xmppManager->requestUploadSlot(QFileInfo(attachment.filePath()), attachment.fileName(), QString());
    upload->setSlotId(slotId);

    if (!slotId.isEmpty()) {
        qCDebug(lcUploader) << "Slot id:" << slotId;
    }
    else {
        const QString errorText("Slot id is empty");
        qCWarning(lcUploader) << errorText;
        onUploadFailed(upload, errorText);
        //startNextUpload(); // TODO(fpohtmeh): remove
    }
}
