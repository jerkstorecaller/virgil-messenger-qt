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


#ifndef VSQLOGGING_H
#define VSQLOGGING_H

#include <QObject>

#include "Common.h"

class QNetworkAccessManager;

class VSQLogging : public QObject
{
    Q_OBJECT

public:
    explicit VSQLogging(QObject *parent);
    virtual ~VSQLogging();

    void checkAppCrash();
    void resetRunFlag();
    bool sendLogFiles();
    void setVirgilUrl(QString VirgilUrl);
    void setkVersion(QString AppVersion);
    void setkOrganization(QString strkOrganization);
    void setkApp(QString strkApp);

signals:
    void crashReportRequested();

private:
    static const QString endpointSendReport;

    static void logger_qt_redir(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    bool checkRunFlag();
    bool sendFileToBackendRequest(QByteArray fileData);
    void setRunFlag(bool runState);

    QString currentVirgilUrl;
    QString kVersion;
    QString kOrganization;
    QString kApp;
    QNetworkAccessManager *manager;

private slots:
    void endpointReply();
};

#endif // VSQLOGGING_H
