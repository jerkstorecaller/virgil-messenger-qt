import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QuickFuture 1.0

import "../theme"
import "../components"

Page {

    property string username

    background: Rectangle {
        color: Theme.mainBackgroundColor
    }

    header: Header {
        title: qsTr("Download from the Cloud")
    }

    Form {
        id: form

        FormImage {
            source: "../resources/icons/Key.png"
        }

        FormSubtitle {
            text: qsTr("Your account information is securely stored in the cloud. Please enter your security word(s) to access it:")
        }

        FormInput {
            id: password
            label: qsTr("Password")
            placeholder: qsTr("Enter password")
            password: true
        }

        FormPrimaryButton {
            text: qsTr("Decrypt")
            onClicked: {
                if (password.text === '') {
                    window.showPopupError('Password can not be empty')
                    return
                }
                messenger.signInWithKey(username, password.text)
            }
        }
    }

    footer: Footer {}

    Connections {
        target: messenger
        onSignInWithKey: form.showLoading(qsTr("Downloading your private key..."))
        onSignedIn: {
            form.hideLoading()
            mainView.showContacts(true)
        }
        onSignInFailed: {
            form.hideLoading()
            showPopupError(qsTr("Private key download error"))
        }
    }
}

