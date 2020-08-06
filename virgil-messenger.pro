#  Copyright (C) 2015-2020 Virgil Security, Inc.
#
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#      (1) Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#
#      (2) Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#
#      (3) Neither the name of the copyright holder nor the names of its
#      contributors may be used to endorse or promote products derived from
#      this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
#  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
#  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
#  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
#  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
#  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
#  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#
#  Lead Maintainer: Virgil Security Inc. <support@virgilsecurity.com>

QT += core network qml quick bluetooth sql xml concurrent

CONFIG += c++14

TARGET = virgil-messenger

QMAKE_TARGET_BUNDLE_PREFIX = com.virgilsecurity

#
#   Set version
#
isEmpty(VERSION) {
    VERSION = $$cat($$PWD/VERSION_MESSENGER).0
}
message("VERSION = $$VERSION")

#
#   Include IoTKit Qt wrapper
#
PREBUILT_PATH = $$PWD/ext/prebuilt
include($${PREBUILT_PATH}/qt/iotkit.pri)

#
#   Include QML QFuture
#
include($$PWD/ext/quickfuture/quickfuture.pri)
#include($$PWD/ext/quickpromise/quickpromise.pri)

#
#   QXMPP
#
QXMPP_BUILD_PATH = $$PREBUILT_SYSROOT
message("QXMPP location : $${QXMPP_BUILD_PATH}")

#
#   Defines
#

DEFINES += QT_DEPRECATED_WARNINGS \
        INFO_CLIENT=1 \
        CFG_CLIENT=1 \
        VERSION="$$VERSION"

CONFIG(iphoneos, iphoneos | iphonesimulator) {
    # FIXME(fpohtmeh): remove? We already have IOS section
    DEFINES += VS_IOS=1
}

#
#   Headers
#

HEADERS += \
        include/VSQApplication.h \
        include/VSQClipboardProxy.h \
        include/VSQCommon.h \
        include/VSQCrashReporter.h \
        include/VSQLogging.h \
        include/VSQMessenger.h \
        include/VSQQmlEngine.h \
        include/VSQSettings.h \
        include/VSQUtils.h \
        include/ui/VSQUiHelper.h \
        # Client
        include/client/VSQClient.h \
        include/client/VSQCore.h \
        include/client/VSQUpload.h \
        include/client/VSQUploader.h \
        # Models
        include/models/VSQAttachmentBuilder.h \
        include/models/VSQChatsModel.h \
        include/models/VSQMessagesModel.h \
        # Database
        include/database/VSQDatabase.h \
        include/database/VSQMessagesDatabase.h \
        # Platforms
        include/android/VSQAndroid.h \
        include/macos/VSQMacos.h \
        # Thirdparty
        include/thirdparty/optional/optional.hpp

#
#   Sources
#

SOURCES += \
        src/main.cpp \
        src/VSQApplication.cpp \
        src/VSQClipboardProxy.cpp \
        src/VSQCommon.cpp \
        src/VSQCrashReporter.cpp \
        src/VSQLogging.cpp \
        src/VSQMessenger.cpp \
        src/VSQQmlEngine.cpp \
        src/VSQSettings.cpp \
        src/VSQUtils.cpp \
        src/ui/VSQUiHelper.cpp \
        # Client
        src/client/VSQClient.cpp \
        src/client/VSQCore.cpp \
        src/client/VSQUpload.cpp \
        src/client/VSQUploader.cpp \
        # Models
        src/models/VSQAttachmentBuilder.cpp \
        src/models/VSQChatsModel.cpp \
        src/models/VSQMessagesModel.cpp \
        # Database
        src/database/VSQDatabase.cpp \
        src/database/VSQMessagesDatabase.cpp \
        # Platforms
        src/android/VSQAndroid.cpp

#
#   Resources
#

RESOURCES += src/resources.qrc

#
#   Include path
#


INCLUDEPATH += \
    include \
    include/thirdparty \
    $${QXMPP_BUILD_PATH}/include \
    $${QXMPP_BUILD_PATH}/include/qxmpp

#
#   Sparkle framework
#
unix:mac:!ios {
    OBJECTIVE_SOURCES += src/macos/VSQMacos.mm
    DEFINES += MACOS=1
    SPARKLE_LOCATION=$$PREBUILT_PATH/$${OS_NAME}/sparkle
    message("SPARKLE LOCATION = $$SPARKLE_LOCATION")
    QMAKE_LFLAGS  += -F$$SPARKLE_LOCATION
    LIBS += -framework Sparkle -framework CoreFoundation -framework Foundation
    INCLUDEPATH += $$SPARKLE_LOCATION/Sparkle.framework/Headers

    sparkle.path = Contents/Frameworks
    sparkle.files = $$SPARKLE_LOCATION/Sparkle.framework
    QMAKE_BUNDLE_DATA += sparkle
}

#
#   Qt Web Driver
#
isEmpty(WEBDRIVER) {
    message("Web Driver is disabled")
} else {
    message("Web Driver is enabled")
    QT += widgets
    DEFINES += WD_ENABLE_WEB_VIEW=0 \
           WD_ENABLE_PLAYER=0 \
           QT_NO_SAMPLES=1 \
           VSQ_WEBDRIVER_DEBUG=1
    release:QTWEBDRIVER_LOCATION=$$PWD/ext/prebuilt/$${OS_NAME}/release/installed/usr/local/include/qtwebdriver
    debug:QTWEBDRIVER_LOCATION=$$PWD/ext/prebuilt/$${OS_NAME}/debug/installed/usr/local/include/qtwebdriver
    HEADERS += $$QTWEBDRIVER_LOCATION/src/Test/Headers.h
    INCLUDEPATH +=  $$QTWEBDRIVER_LOCATION $$QTWEBDRIVER_LOCATION/src
    linux:!android: {
        LIBS += -ldl -Wl,--start-group -lchromium_base -lWebDriver_core -lWebDriver_extension_qt_base -lWebDriver_extension_qt_quick -Wl,--end-group
    }
    macx: {
        LIBS += -lchromium_base -lWebDriver_core -lWebDriver_extension_qt_base -lWebDriver_extension_qt_quick
        LIBS += -framework Foundation
        LIBS += -framework CoreFoundation
        LIBS += -framework ApplicationServices
        LIBS += -framework Security
    }
}


#
#   Libraries
#
LIBS += $${QXMPP_BUILD_PATH}/lib/libqxmpp.a

#
#   Default rules for deployment
#
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#
#   Depencies
#

DEPENDPATH += $${INCLUDEPATH}

message("ANDROID_TARGET_ARCH = $$ANDROID_TARGET_ARCH")

#
#   Linux specific
#
linux {
    DEFINES += VS_LINUX=1 VS_DESKTOP=1
    QT += widgets
}

#
#   Windows specific
#

win32|win64 {
    DEFINES += VS_WINDOWS=1 VS_DESKTOP=1
    QT += widgets
    RC_ICONS = platforms/windows/Virgil.ico
    DISTFILES += \
        platforms/windows/Virgil.ico
}

#
#   macOS specific
#

macx {
    DEFINES += VS_MACOS=1 VS_DESKTOP=1
    QT += widgets
    ICON = $$PWD/scripts/macos/pkg_resources/MyIcon.icns
    QMAKE_INFO_PLIST = $$PWD/platforms/macos/virgil-messenger.plist
    DISTFILES += \
        platforms/macos/virgil-messenger.plist.in
}

#
#   iOS specific
#

ios {
    DEFINES += VS_IOS=1 VS_MOBILE=1
#    OBJECTIVE_SOURCES += \
#        src/ios/APNSApplicationDelegate.mm

#    #IOS_ENTITLEMENTS.name = CODE_SIGN_ENTITLEMENTS
#    #IOS_ENTITLEMENTS.value = ios/pushnotifications.entitlements
#    QMAKE_MAC_XCODE_SETTINGS += IOS_ENTITLEMENTS
}

isEqual(OS_NAME, "ios")|isEqual(OS_NAME, "ios-sim"): {
    Q_ENABLE_BITCODE.name = ENABLE_BITCODE
    Q_ENABLE_BITCODE.value = NO
    QMAKE_MAC_XCODE_SETTINGS += Q_ENABLE_BITCODE

    LIBS_DIR = $$PWD/ext/prebuilt/$$OS_NAME/release/installed/usr/local/lib
    QMAKE_RPATHDIR = @executable_path/Frameworks

    IOS_DYLIBS.files = \
    $$LIBS_DIR/libvs-messenger-internal.dylib
    IOS_DYLIBS.path = Frameworks
    QMAKE_BUNDLE_DATA += IOS_DYLIBS
}


#
#   Android specific
#

defineReplace(AndroidVersionCode) {
        segments = $$split(1, ".")
        vCode = "$$first(vCode)$$format_number($$member(segments,0,0), width=3 zeropad)"
        vCode = "$$first(vCode)$$format_number($$member(segments,1,1), width=3 zeropad)"
        vCode = "$$first(vCode)$$format_number($$member(segments,2,2), width=3 zeropad)"
        vCode = "$$first(vCode)$$format_number($$member(segments,3,3), width=5 zeropad)"
        return($$first(vCode))
}

android {
    QT += androidextras
    DEFINES += VS_ANDROID=1 VS_PUSHNOTIFICATIONS=1 VS_MOBILE=1

    ANDROID_VERSION_CODE = $$AndroidVersionCode($$VERSION)
    ANDROID_VERSION_NAME = $$VERSION

    INCLUDEPATH +=  $$PWD/ext/prebuilt/firebase_cpp_sdk/include

    HEADERS += \
         include/VSQPushNotifications.h \
         include/android/VSQFirebaseListener.h

    SOURCES += \
        src/VSQPushNotifications.cpp \
        src/android/VSQFirebaseListener.cpp

    release:LIBS_DIR = $$PWD/ext/prebuilt/$${OS_NAME}/release/installed/usr/local/lib
    debug:LIBS_DIR = $$PWD/ext/prebuilt/$${OS_NAME}/release/installed/usr/local/lib # TODO(fpohtmeh): debug folder must be specified
    FIREBASE_LIBS_DIR = $$PWD/ext/prebuilt/firebase_cpp_sdk/libs/android/$$ANDROID_TARGET_ARCH/c++
    ANDROID_EXTRA_LIBS = \
        $$LIBS_DIR/libvs-messenger-internal.so \
        $$LIBS_DIR/libcrypto_1_1.so \
        $$LIBS_DIR/libssl_1_1.so

    LIBS += $${FIREBASE_LIBS_DIR}/libfirebase_messaging.a \
        $${FIREBASE_LIBS_DIR}/libfirebase_app.a \
        $${FIREBASE_LIBS_DIR}/libfirebase_auth.a

    ANDROID_PACKAGE_SOURCE_DIR = \
        $$PWD/platforms/android

    DISTFILES += \
        platforms/android/gradle.properties \
        platforms/android/google-services.json \
        platforms/android/AndroidManifest.xml \
        platforms/android/build.gradle \
        platforms/android/gradle/wrapper/gradle-wrapper.jar \
        platforms/android/gradle/wrapper/gradle-wrapper.properties \
        platforms/android/gradlew \
        platforms/android/gradlew.bat \
        platforms/android/res/values/libs.xml \
        platforms/android/src/org/virgil/notification/NotificationClient.java \
        # Resources
        platforms/android/res/drawable-hdpi/icon.png \
        platforms/android/res/drawable-hdpi/icon_round.png \
        platforms/android/res/drawable-ldpi/icon.png \
        platforms/android/res/drawable-ldpi/icon_round.png \
        platforms/android/res/drawable-mdpi/icon.png \
        platforms/android/res/drawable-mdpi/icon_round.png \
        platforms/android/res/drawable-xhdpi/icon.png \
        platforms/android/res/drawable-xhdpi/icon_round.png \
        platforms/android/res/drawable-xxhdpi/icon.png \
        platforms/android/res/drawable-xxhdpi/icon_round.png \
        platforms/android/res/drawable-xxxhdpi/icon.png \
        platforms/android/res/drawable-xxxhdpi/icon_round.png \
        platforms/android/res/mipmap-hdpi/ic_launcher_round.png \
        platforms/android/res/mipmap-mdpi/ic_launcher.png \
        platforms/android/res/mipmap-mdpi/ic_launcher_round.png \
        platforms/android/res/mipmap-xhdpi/ic_launcher.png \
        platforms/android/res/mipmap-xhdpi/ic_launcher_round.png \
        platforms/android/res/mipmap-xxhdpi/ic_launcher.png \
        platforms/android/res/mipmap-xxhdpi/ic_launcher_round.png \
        platforms/android/res/mipmap-xxxhdpi/ic_launcher.png \
        platforms/android/res/mipmap-xxxhdpi/ic_launcher_round.png
}
