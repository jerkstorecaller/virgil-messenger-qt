import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.1

import "../helpers/ui"
import "../theme"
import "./components"


Page {
    id: loginPage

    background: Rectangle {
        color: "transparent"
    }

    header: HeaderToolbar {
        title: qsTr("Sign in")
    }

    CenteredAuthLayout {
        content: ColumnLayout {

            Layout.preferredWidth: 260
            Layout.maximumWidth: 260
            Layout.alignment: Qt.AlignCenter

            spacing: 0

            Label {
                id: label
                Layout.fillWidth: true
                color: Theme.secondaryTextColor
                text: qsTr("Username")
                width: parent.width
                font.pointSize: UiHelper.fixFontSz(11)
            }

            TextField {
                id: username
                Layout.fillWidth: true
                Layout.topMargin: 10
                height: 40
                leftPadding: 15
                rightPadding: 15
                font.pointSize: UiHelper.fixFontSz(15)
                color: Theme.primaryTextColor

                background: Rectangle {
                    implicitWidth: parent.width
                    implicitHeight: parent.height
                    radius: 20
                    color: Theme.inputBackgroundColor
                }
            }

            PrimaryButton {
                text: qsTr("Sign In")
                onClicked: screenManager.signIn(username.text)
                Layout.fillWidth: true
                Layout.topMargin: 15
            }
        }
    }
}
