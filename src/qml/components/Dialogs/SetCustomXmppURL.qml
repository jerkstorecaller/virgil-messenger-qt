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

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QuickFuture 1.0
import MesResult 1.0

import ".."
import "../../base"

Dialog {
    id: root
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    visible: true
    modal: true
    title: qsTr("Set custom URLs")
    standardButtons: Dialog.Apply | Dialog.Discard
    focus: true

    property var closed

    contentItem: Rectangle {
        implicitWidth: 400
        implicitHeight: 100

        TextField {
            id: xmppTextField
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 3
            placeholderText: "XMPP URL"
            color: "black"
            focus: true

            Keys.onPressed: {
                if (Platform.isDesktop) {
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return)
                        root.accept();
                    else if (event.key === Qt.Key_Escape)
                        root.reject();
                }
            }
        }

        TextField {
            id: msgrTextField
            anchors.top: xmppTextField.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 3
            placeholderText: "MSGR URL"
            color: "black"
            focus: true

            Keys.onPressed: {
                if (Platform.isDesktop) {
                    if (event.key === Qt.Key_Enter || event.key === Qt.Key_Return)
                        root.accept();
                    else if (event.key === Qt.Key_Escape)
                        root.reject();
                }
            }
        }
    }

    function openMe(closed) {
        this.closed = closed
        open()
    }

    function apply() {
        try {
            const xmppURL = xmppTextField.text.toLocaleLowerCase()
            const msngURL = msgrTextField.text.toLocaleLowerCase()

            if (xmppURL && msngURL) {
                Messenger.setCustomURLs(xmppURL, msngURL);
            }
            console.log(xmppURL)
            console.log(msngURL)
            root.close()
            closed()
        } catch (error) {
            console.error(error)
        }
    }

    Component.onCompleted: {
        applied.connect(apply)
        accepted.connect(apply)
        discarded.connect(() => {
                          root.close()
                          closed()
                          })
    }
}
