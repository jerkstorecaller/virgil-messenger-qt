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

#ifndef VIRGIL_IOTKIT_QT_FILE_VERSION_H
#define VIRGIL_IOTKIT_QT_FILE_VERSION_H

#include <QtCore>
#include <virgil/iot/protocols/snap/snap-structs.h>
#include <virgil/iot/protocols/snap/info/info-structs.h>

class VSQFileVersion {
public:
    VSQFileVersion();
    VSQFileVersion(const VirgilIoTKit::vs_file_version_unpacked_t &fileVersion) {
        set(fileVersion);
    }
    VSQFileVersion(const VirgilIoTKit::vs_file_version_t &fileVersion) {
        set(fileVersion);
    }

    VSQFileVersion &
    set(const VirgilIoTKit::vs_file_version_unpacked_t &fileVersion);

    VSQFileVersion &
    set(const VirgilIoTKit::vs_file_version_t &fileVersion);

    QString
    description() const;

    operator QString() const {
        return description();
    }
    VSQFileVersion &
    operator=(const VirgilIoTKit::vs_file_version_unpacked_t &fileVersion) {
        return set(fileVersion);
    }

    VSQFileVersion &
    operator=(const VirgilIoTKit::vs_file_version_t &fileVersion) {
        return set(fileVersion);
    }

    bool
    equal(const VSQFileVersion &fileVersion) const;

    bool
    operator==(const VSQFileVersion &fileVersion) const {
        return equal(fileVersion);
    }

    bool
    operator!=(const VSQFileVersion &fileVersion) const {
        return !equal(fileVersion);
    }

private:
    quint8 m_major;
    quint8 m_minor;
    quint8 m_patch;
    quint32 m_build;
    QDateTime m_timestamp;
};


#endif // VIRGIL_IOTKIT_QT_FILE_VERSION_H
