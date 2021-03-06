import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import "../theme"
import "../components"

Page {

    property bool showServersPanel: true

    background: Rectangle {
        color: Theme.contactsBackgroundColor
    }

    header: Header {
        title: qsTr("Settings")
    }

    Form {

        Avatar {
            Layout.alignment: Qt.AlignHCenter
            diameter: 80
            nickname: Messenger.currentUser
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 50
            font.pointSize: UiHelper.fixFontSz(18)
            color: Theme.primaryTextColor
            text: Messenger.currentUser
        }

        FormLabel {
            text: qsTr("Version: %1".arg(app.currentVersion()))
        }
/*
        FormPrimaryButton {
            text: "Software Update"
            onClicked: {
                app.checkUpdates()
            }
        }

        FormPrimaryButton {
            text: "Delete Account"
            onClicked: {
            }
        }
*/
        FormPrimaryButton {
            text: "Backup private key"
            onClicked: {
                mainView.showBackupKey()
            }
        }
/*
        FormPrimaryButton {
            text: "Send report"
            onClicked: {
                app.sendReport()
            }
        }
*/
        FormPrimaryButton {
            text: "Sign out"
            onClicked: {
                mainView.signOut()
            }
        }
    }
}
