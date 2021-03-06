/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QuickFuture 1.0
import MesResult 1.0

import "../theme"
import "../components"
import "../components/CommonHelpers"

Page {

    property bool showServersPanel: true

    background: Rectangle {
        color: Theme.contactsBackgroundColor
    }

    header: ContactsHeader {
        description: "Virgil Server"
        objectName: "hdrDefaultServer"
        id: contactsHeaderId
        title: "Virgil"
        searchPlaceholder: "Search conversation"

        onIsSearchOpenChanged: {
            contactsHeaderId.isSearchOpen ? ChatModel.applyFilter('') : ChatModel.clearFilter()
        }

        onSearchChanged: {
            ChatModel.applyFilter(contactsHeaderId.search)
        }

        Action {
            text: qsTr("New Chat")
            onTriggered: addContact()
        }
/*
        Action {
            text: qsTr("New Group")
            // onTriggered: addContact()
        }

        Action {
            text: qsTr("Send Invite")
            // onTriggered: addContact()
        }
*/
    }

    ListView {
        id: listView
        anchors.fill: parent
        model: ChatModel
        delegate: ItemDelegate {
            id: listItem
            width: listView.width
            leftInset: 8
            rightInset: 8
            background: Rectangle {
                color: listItem.down ? Theme.contactPressedColor : "Transparent"
                radius: 6
            }
            contentItem: RowLayout {
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                anchors.fill: parent
                height: avatar.height
                spacing: 10

                Avatar {
                    id: avatar
                    nickname: model.name
                }

                Column {
                    Layout.fillWidth: true

                    Text {
                        color: Theme.primaryTextColor
                        font.pointSize: UiHelper.fixFontSz(15)
                        text: model.name
                    }

                    Text {
                        color: Theme.secondaryTextColor
                        font.pointSize: UiHelper.fixFontSz(12)
                        text: model.lastMessage ? model.lastMessage.substring(0, 30) : "..."
                        width: parent.width
                        elide: Text.ElideRight
                        textFormat: Text.RichText
                    }
                }

                Column {
                    width: 30
                    spacing: 5

                    MessageCounter {
                       count: model.unreadMessageCount
                       anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: model.lastMessageTime ? Qt.formatDateTime(model.lastMessageTime, "hh:mm")   : ""
                        color: Theme.secondaryTextColor
                        font.pointSize: UiHelper.fixFontSz(9)
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            onClicked: {
                mainView.showChatWith(model.name)
            }
        }

        IconWithText {
            id: emptyListPlaceholderId
            property url conversationIcon: "../resources/icons/Chats.png"
            property url searchIcon: "../resources/icons/Search_Big.png"
            property string conversationText: qsTr("Create your first chat<br />by pressing the dots<br />button above")
            property string searchText: qsTr("Search results<br />will appear here")
            property string searchEmptyText: qsTr("Nothing found")
            visible: !listView.contentItem.children.length
            image {
                source: contactsHeaderId.isSearchOpen ? searchIcon : conversationIcon
                width: 48
                height: 48
            }
            label {
                text: getSearchText()
                color: Theme.labelColor
            }

            function getSearchText() {
                if (!contactsHeaderId.isSearchOpen) return conversationText;
                if (contactsHeaderId.search !== '') return searchEmptyText;
                return searchText;
            }
        }

        MouseArea {
            visible: !listView.contentItem.children.length
            anchors.fill: parent
            onClicked: {
                if (!listView.contentItem.children.length) {
                    addContact()
                    mouse.accepted = false
                }
            }
        }
    }


    //
    //  Functions
    //
    function setAsRead(user) {
        // ConversationsModel.setAsRead(user);
        console.log("setAsRead func");
    }

    function addContact() {
        var component = Qt.createComponent("../components/Dialogs/AddContactDialog.qml")
        if (component.status === Component.Ready) {
            var dialog = component.createObject(root)
            var apply = function() {
                try {
                    var future = Messenger.addContact(dialog.contact.toLowerCase())
                    Future.onFinished(future, function(value) {
                        var res = Future.result(future)
                        if (res === Result.MRES_OK) {
                            mainView.showChatWith(dialog.contact)
                            return
                        }

                        root.showPopupError(qsTr("User not found"))
                    })
                } catch (error) {
                    console.error("Cannot start initialization of device")
                }
                dialog.close()
            }
            dialog.applied.connect(apply)
            dialog.accepted.connect(apply)
            dialog.open()
            return dialog
        }
        console.error(component.errorString())
        return null
    }
}
