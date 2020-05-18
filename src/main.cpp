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

#include <VSQApplication.h>
#include <iostream>

#include <android/VSQAndroid.h>

#if (VSQ_WEBDRIVER_DEBUG)
#include "Test/Headers.h"
#endif

int
main(int argc, char *argv[]) {

#if (ANDROID)
    VSQAndroid::prepare();
#endif

#if (VSQ_WEBDRIVER_DEBUG)
    wd_setup(argc, argv);
#endif

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication a(argc, argv);
    a.setOrganizationName("Virgil Security");
    a.setOrganizationDomain("virgil.net");

    QString baseUrl;
    if (2 == argc && argv[1] && argv[1][0]) {
        baseUrl = QString::fromLocal8Bit(argv[1]);
        qDebug() << "QML URL: " << baseUrl;
    }

    return VSQApplication().run(baseUrl);
}