import QtQuick 2.12
import QtQuick.Controls 2.12

import "../base"

TextField {
    inputMethodHints: Qt.ImhLowercaseOnly
    // HACK(fpohtmeh): lookbehind doesn't work in RegExpValidator.
    // Original regexp (/(?!_)[a-z0-9_]{1,20}(?<!_)/) is replaced with equivalent.
    // Also issue can be fixed by using RegularExpressionValidator from QtQuick 2.15
    validator: RegExpValidator { regExp: /(?!_)[a-z0-9_]{0,19}[a-z0-9]/ }
    selectByMouse: true

    signal accepted
    signal rejected

    TextInputMouseArea {}

    Keys.onPressed: {
        if (!Platform.isDesktop) {
            return
        }
        else if (event.key == Qt.Key_Enter || event.key == Qt.Key_Return) {
            accepted()
        }
        else if (event.key == Qt.Key_Escape) {
            rejected()
        }
    }
}
