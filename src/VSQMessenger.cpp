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

#include <android/VSQAndroid.h>
#include <cstring>
#include <cstdlib>

#include <VSQDownload.h>
#include <VSQMessenger.h>
#include <VSQPushNotifications.h>
#include <VSQSqlConversationModel.h>
#include <VSQUpload.h>
#include <VSQUtils.h>
#include <VSQSettings.h>

#include <android/VSQAndroid.h>
#include <android/VSQAndroid.h>

#include <qxmpp/QXmppMessage.h>
#include <qxmpp/QXmppUtils.h>
#include <qxmpp/QXmppPushEnableIq.h>
#include <qxmpp/QXmppMessageReceiptManager.h>
#include <qxmpp/QXmppCarbonManager.h>

#include <QtConcurrent>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlError>
#include <QtQml>
#include <QUuid>

#include <QuickFuture>
Q_DECLARE_METATYPE(VSQMessenger::EnStatus)
Q_DECLARE_METATYPE(VSQMessenger::EnResult)
Q_DECLARE_METATYPE(QFuture<VSQMessenger::EnResult>)

#ifndef USE_XMPP_LOGS
#define USE_XMPP_LOGS 0
#endif

const QString VSQMessenger::kOrganization = "VirgilSecurity";
const QString VSQMessenger::kApp = "VirgilMessenger";
const QString VSQMessenger::kUsers = "Users";
const QString VSQMessenger::kProdEnvPrefix = "prod";
const QString VSQMessenger::kStgEnvPrefix = "stg";
const QString VSQMessenger::kDevEnvPrefix = "dev";
const QString VSQMessenger::kPushNotificationsProxy = "push-notifications-proxy";
const QString VSQMessenger::kPushNotificationsNode = "node";
const QString VSQMessenger::kPushNotificationsService = "service";
const QString VSQMessenger::kPushNotificationsFCM = "fcm";
const QString VSQMessenger::kPushNotificationsDeviceID = "device_id";
const QString VSQMessenger::kPushNotificationsFormType = "FORM_TYPE";
const QString VSQMessenger::kPushNotificationsFormTypeVal = "http://jabber.org/protocol/pubsub#publish-options";
const int VSQMessenger::kConnectionWaitMs = 10000;
const int VSQMessenger::kKeepAliveTimeSec = 10;

Q_LOGGING_CATEGORY(lcMessenger, "messenger")

// Helper struct to categorize transfers
struct TransferId
{
    enum class Type
    {
        File,
        Thumbnail
    };

    TransferId() = default;
    TransferId(const QString &messageId, const Type type)
        : messageId(messageId)
        , type(type)
    {}
    ~TransferId() = default;

    static TransferId parse(const QString &rawTransferId)
    {
        const auto parts = rawTransferId.split(';');
        return TransferId(parts.front(), (parts.back() == "thumb") ? Type::Thumbnail : Type::File);
    }

    operator QString() const
    {
        return messageId + ((type == Type::Thumbnail) ? QLatin1String(";thumb") : QLatin1String(";file"));
    }

    QString messageId;
    Type type = Type::File;
};


/******************************************************************************/
VSQMessenger::VSQMessenger(QNetworkAccessManager *networkAccessManager, VSQSettings *settings)
    : QObject()
    , m_xmpp()
    , m_settings(settings)
    , m_transferManager(new VSQCryptoTransferManager(&m_xmpp, networkAccessManager, m_settings, this))
    , m_attachmentBuilder(settings, this)
{
    // Register QML typess
    qmlRegisterType<VSQMessenger>("MesResult", 1, 0, "Result");
    QuickFuture::registerType<VSQMessenger::EnResult>([](VSQMessenger::EnResult res) -> QVariant {
        return QVariant(static_cast<int>(res));
    });

    qRegisterMetaType<QXmppClient::Error>();

    // Connect to Database
    _connectToDatabase();
    m_sqlConversations = new VSQSqlConversationModel(this);
    m_sqlChatModel = new VSQSqlChatModel(this);

    // Add receipt messages extension
    m_xmppReceiptManager = new QXmppMessageReceiptManager();
    m_xmppCarbonManager = new QXmppCarbonManager();
    m_xmppDiscoveryManager = new VSQDiscoveryManager(&m_xmpp, this);

    m_xmpp.addExtension(m_xmppReceiptManager);
    m_xmpp.addExtension(m_xmppCarbonManager);

    // Signal connection
    connect(this, SIGNAL(fireReadyToAddContact(QString)), this, SLOT(onAddContactToDB(QString)));
    connect(this, SIGNAL(fireError(QString)), this, SLOT(disconnect()));
    connect(this, &VSQMessenger::downloadThumbnail, this, &VSQMessenger::onDownloadThumbnail);

    connect(m_transferManager, &VSQCryptoTransferManager::statusChanged, this, &VSQMessenger::onAttachmentStatusChanged);
    connect(m_transferManager, &VSQCryptoTransferManager::progressChanged, this, &VSQMessenger::onAttachmentProgressChanged);
    connect(m_transferManager, &VSQCryptoTransferManager::fileDecrypted, this, &VSQMessenger::onAttachmentDecrypted);

    // Connect XMPP signals
    connect(&m_xmpp, SIGNAL(connected()), this, SLOT(onConnected()), Qt::QueuedConnection);
    connect(&m_xmpp, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
    connect(&m_xmpp, &QXmppClient::error, this, &VSQMessenger::onError);
    connect(&m_xmpp, SIGNAL(messageReceived(const QXmppMessage &)), this, SLOT(onMessageReceived(const QXmppMessage &)));
    connect(&m_xmpp, SIGNAL(presenceReceived(const QXmppPresence &)), this, SLOT(onPresenceReceived(const QXmppPresence &)));
    connect(&m_xmpp, SIGNAL(iqReceived(const QXmppIq &)), this, SLOT(onIqReceived(const QXmppIq &)));
    connect(&m_xmpp, SIGNAL(sslErrors(const QList<QSslError> &)), this, SLOT(onSslErrors(const QList<QSslError> &)));
    connect(&m_xmpp, SIGNAL(stateChanged(QXmppClient::State)), this, SLOT(onStateChanged(QXmppClient::State)));

    // messages sent to our account (forwarded from another client)
    connect(m_xmppCarbonManager, &QXmppCarbonManager::messageReceived, &m_xmpp, &QXmppClient::messageReceived);
    // messages sent from our account (but another client)
    connect(m_xmppCarbonManager, &QXmppCarbonManager::messageSent, &m_xmpp, &QXmppClient::messageReceived);
    connect(m_xmppReceiptManager, &QXmppMessageReceiptManager::messageDelivered, this, &VSQMessenger::onMessageDelivered);

    // Network Analyzer
    connect(&m_networkAnalyzer, &VSQNetworkAnalyzer::fireStateChanged, this, &VSQMessenger::onProcessNetworkState, Qt::QueuedConnection);
    connect(&m_networkAnalyzer, &VSQNetworkAnalyzer::fireHeartBeat, this, &VSQMessenger::checkState, Qt::QueuedConnection);

    // Uploading
    connect(m_transferManager, &VSQCryptoTransferManager::fireReadyToUpload, this,  &VSQMessenger::onReadyToUpload, Qt::QueuedConnection);

    // Use Push notifications
#if VS_PUSHNOTIFICATIONS
    VSQPushNotifications::instance().startMessaging();
#endif
}

VSQMessenger::~VSQMessenger()
{
}

void
VSQMessenger::onMessageDelivered(const QString& to, const QString& messageId) {

    m_sqlConversations->setMessageStatus(messageId, StMessage::Status::MST_RECEIVED);

    // QMetaObject::invokeMethod(m_sqlConversations, "setMessageStatus",
    //                          Qt::QueuedConnection, Q_ARG(const QString &, messageId),
    //                          Q_ARG(const StMessage::Status, StMessage::Status::MST_RECEIVED));

    qDebug() << "Message with id: '" << messageId << "' delivered to '" << to << "'";
}

/******************************************************************************/
void
VSQMessenger::_connectToDatabase() {
    QSqlDatabase database = QSqlDatabase::database();
    if (!database.isValid()) {
        database = QSqlDatabase::addDatabase("QSQLITE");
        if (!database.isValid())
            qFatal("Cannot add database: %s", qPrintable(database.lastError().text()));
    }

    const QDir writeDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!writeDir.mkpath("."))
        qFatal("Failed to create writable directory at %s", qPrintable(writeDir.absolutePath()));

    // Ensure that we have a writable location on all devices.
    const QString fileName = writeDir.absolutePath() + "/chat-database.sqlite3";
    // When using the SQLite driver, open() will create the SQLite database if it doesn't exist.
    database.setDatabaseName(fileName);
    if (!database.open()) {
        QFile::remove(fileName);
        qFatal("Cannot open database: %s", qPrintable(database.lastError().text()));
    }
}

/******************************************************************************/
QString
VSQMessenger::_xmppPass() {
    if (!m_xmppPass.isEmpty()) {
        return m_xmppPass;
    }

    const size_t _pass_buf_sz = 512;
    char pass[_pass_buf_sz];

    // Get XMPP password
    if (VS_CODE_OK != vs_messenger_virgil_get_xmpp_pass(pass, _pass_buf_sz)) {
        emit fireError(tr("Cannot get XMPP password"));
        return "";
    }

    m_xmppPass = QString::fromLatin1(pass);

    return m_xmppPass;
}

/******************************************************************************/
Q_DECLARE_METATYPE(QXmppConfiguration)
bool
VSQMessenger::_connect(QString userWithEnv, QString deviceId, QString userId, bool forced) {
    if (forced) {
        m_connectGuard.lock();
    } else if (!m_connectGuard.tryLock()) {
        return false;
    }

    static int cnt = 0;

    const int cur_val = cnt++;

    qCDebug(lcNetwork) << ">>>>>>>>>>> _connect: START " << cur_val;

    // Update users list
    _addToUsersList(userWithEnv);

    // Connect to XMPP
    emit fireConnecting();

    QString jid = userId + "@" + _xmppURL() + "/" + deviceId;
    conf.setJid(jid);
    conf.setHost(_xmppURL());
    conf.setPassword(_xmppPass());
    conf.setAutoReconnectionEnabled(false);
#if VS_ANDROID
    conf.setKeepAliveInterval(kKeepAliveTimeSec);
    conf.setKeepAliveTimeout(kKeepAliveTimeSec - 1);
#endif
    qDebug() << "SSL: " << QSslSocket::supportsSsl();

    auto logger = QXmppLogger::getLogger();
    logger->setLoggingType(QXmppLogger::SignalLogging);
    logger->setMessageTypes(QXmppLogger::AnyMessage);

#if USE_XMPP_LOGS
    connect(logger, &QXmppLogger::message, [=](QXmppLogger::MessageType, const QString &text){
        qDebug() << text;
    });

    m_xmpp.setLogger(logger);
#endif
    if (m_xmpp.isConnected()) {
        // Wait for disconnection
        QTimer timer;
        timer.setSingleShot(true);
        QEventLoop loop;
        connect(&m_xmpp, &QXmppClient::connected, &loop, &QEventLoop::quit);
        connect(&m_xmpp, &QXmppClient::disconnected, &loop, &QEventLoop::quit);
        connect(&m_xmpp, &QXmppClient::error, &loop, &QEventLoop::quit);
        connect( &timer, &QTimer::timeout, &loop, &QEventLoop::quit);

        QMetaObject::invokeMethod(&m_xmpp, "disconnectFromServer", Qt::QueuedConnection);

        timer.start(2000);
        loop.exec();
    }

    // Wait for connection
    QTimer timer;
    timer.setSingleShot(true);
    QEventLoop loop;
    connect(&m_xmpp, &QXmppClient::connected, &loop, &QEventLoop::quit);
    connect(&m_xmpp, &QXmppClient::disconnected, &loop, &QEventLoop::quit);
    connect(&m_xmpp, &QXmppClient::error, &loop, &QEventLoop::quit);
    connect( &timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    qRegisterMetaType<QXmppConfiguration>("QXmppConfiguration");
    QMetaObject::invokeMethod(&m_xmpp, "connectToServer", Qt::QueuedConnection, Q_ARG(QXmppConfiguration, conf));

    timer.start(kConnectionWaitMs);
    loop.exec();

    const bool connected = m_xmpp.isConnected();
    qCDebug(lcNetwork) << "<<<<<<<<<<< _connect: FINISH connected = " << connected << "  " << cur_val;

    qDebug() << "Carbons " << (connected ? "enable" : "disable");
    m_xmppCarbonManager->setCarbonsEnabled(connected);
    m_connectGuard.unlock();
    return connected;
}

/******************************************************************************/
void
VSQMessenger::onProcessNetworkState(bool online) {
    if (online) {
        _reconnect();
    } else {
        emit fireError("No internet connection");
    }
}

/******************************************************************************/
QString
VSQMessenger::_caBundleFile() {
#if VS_ANDROID
    return VSQAndroid::caBundlePath();
#else
    return qgetenv("VS_CURL_CA_BUNDLE");
#endif
}

/******************************************************************************/
QString
VSQMessenger::_prepareLogin(const QString &user) {
    QString userId = user;
    m_envType = _defaultEnv;

    // NOTE(vova.y): deprecated logic, user can't contain @
    // Check required environment
    QStringList pieces = user.split("@");
    if (2 == pieces.size()) {
        userId = pieces.at(1);
        if (kProdEnvPrefix == pieces.first()) {
            m_envType = PROD;
        } else if (kStgEnvPrefix == pieces.first()) {
            m_envType = STG;
        }
        else if (kDevEnvPrefix == pieces.first()) {
            m_envType = DEV;
        }
    }

    vs_messenger_virgil_logout();
    char *cCABundle = strdup(_caBundleFile().toStdString().c_str());
    if (VS_CODE_OK != vs_messenger_virgil_init(_virgilURL().toStdString().c_str(), cCABundle)) {
        qCritical() << "Cannot initialize low level messenger";
    }
    free(cCABundle);

    m_logging->setVirgilUrl(_virgilURL());
    m_logging->setkApp(kApp);
    m_logging->setkOrganization(kOrganization);

    // Set current user
    m_user = userId;
    m_sqlConversations->setUser(userId);
    m_sqlChatModel->init(userId);

    // Inform about user activation
    emit fireCurrentUserChanged();

    return userId;
}

/******************************************************************************/
QString VSQMessenger::currentUser() const {
    return m_user;
}

/******************************************************************************/
QString VSQMessenger::currentRecipient() const {
    return m_recipient;
}

/******************************************************************************/
QFuture<VSQMessenger::EnResult>
VSQMessenger::backupUserKey(QString password) {
    // m_userId = _prepareLogin(user);
    return QtConcurrent::run([=]() -> EnResult {

        // Upload current user key to the cloud
        if (VS_CODE_OK != vs_messenger_virgil_set_sign_in_password(password.toStdString().c_str())) {
            emit fireError(tr("Cannot set sign in password"));
            return VSQMessenger::EnResult::MRES_ERR_ENCRYPTION;
        }

        return MRES_OK;
    });
}

/******************************************************************************/
QFuture<VSQMessenger::EnResult>
VSQMessenger::signInWithBackupKey(QString username, QString password) {
    m_userId = _prepareLogin(username);
    return QtConcurrent::run([=]() -> EnResult {

        vs_messenger_virgil_user_creds_t creds;
        memset(&creds, 0, sizeof (creds));

        // Download private key from the cloud
        if (VS_CODE_OK != vs_messenger_virgil_sign_in_with_password(username.toStdString().c_str(), password.toStdString().c_str(), &creds)) {
            emit fireError(tr("Cannot set sign in password"));
            return MRES_ERR_SIGNUP;
        }

        m_deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();

        // Save credentials
        _saveCredentials(username, m_deviceId, creds);

        return _connect(m_user, m_deviceId, m_userId, true) ? MRES_OK : MRES_ERR_SIGNIN;;
    });
}

/******************************************************************************/
QFuture<VSQMessenger::EnResult>
VSQMessenger::signIn(QString user) {
    m_userId = _prepareLogin(user);
    return QtConcurrent::run([=]() -> EnResult {
        qDebug() << "Trying to Sign In: " << m_userId;

        vs_messenger_virgil_user_creds_t creds;
        memset(&creds, 0, sizeof (creds));

        // Load User Credentials
        if (!_loadCredentials(m_userId, m_deviceId, creds)) {
            emit fireError(tr("Cannot load user credentials"));
            qDebug() << "Cannot load user credentials";
            return MRES_ERR_NO_CRED;
        }

        // Sign In user, using Virgil Service
        if (VS_CODE_OK != vs_messenger_virgil_sign_in(&creds)) {
            emit fireError(tr("Cannot Sign In user"));
            qDebug() << "Cannot Sign In user";
            return MRES_ERR_SIGNIN;
        }

        // Check previus run is crashed
        m_logging->checkAppCrash();

        // Connect over XMPP
        return _connect(m_user, m_deviceId, m_userId, true) ? MRES_OK : MRES_ERR_SIGNIN;
    });
}



/******************************************************************************/
QFuture<VSQMessenger::EnResult>
VSQMessenger::signUp(QString user) {
    m_userId = _prepareLogin(user);
    return QtConcurrent::run([=]() -> EnResult {
        qInfo() << "Trying to sign up: " << m_userId;

        vs_messenger_virgil_user_creds_t creds;
        memset(&creds, 0, sizeof (creds));

        VirgilIoTKit::vs_status_e status = vs_messenger_virgil_sign_up(m_userId.toStdString().c_str(), &creds);

        if (status != VS_CODE_OK) {
            emit fireError(tr("Cannot sign up user"));
            return MRES_ERR_USER_ALREADY_EXISTS;
        }

        qInfo() << "User has been successfully signed up: " << m_userId;

        m_deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();

        // Save credentials
        _saveCredentials(m_userId, m_deviceId, creds);

        // Connect over XMPP
        return _connect(m_user, m_deviceId, m_userId, true) ? MRES_OK : MRES_ERR_SIGNUP;
    });
}

/******************************************************************************/
QFuture<VSQMessenger::EnResult>
VSQMessenger::addContact(QString contact) {
    return QtConcurrent::run([=]() -> EnResult {
        // Sign Up user, using Virgil Service
        if (VS_CODE_OK != vs_messenger_virgil_search(contact.toStdString().c_str())) {
            auto errorText = tr("User is not registered : ") + contact;
            emit fireInform(errorText);
            return MRES_ERR_USER_NOT_FOUND;
        }

        emit fireReadyToAddContact(contact);
        return MRES_OK;
    });
}

/******************************************************************************/
void
VSQMessenger::onAddContactToDB(QString contact) {
    m_sqlChatModel->createPrivateChat(contact);
    // m_sqlContacts->addContact(contact);
    emit fireAddedContact(contact);
}

void VSQMessenger::onDownloadThumbnail(const StMessage message, const QString sender)
{
    qCDebug(lcTransferManager) << "Downloading of thumbnail for message:" << message.messageId;
    auto attachment = *message.attachment;
    if (attachment.thumbnailPath.isEmpty()) {
        attachment.thumbnailPath = m_attachmentBuilder.generateThumbnailFileName();
    }
    auto future = QtConcurrent::run([=]() {
        const TransferId id(message.messageId, TransferId::Type::Thumbnail);
        auto download = m_transferManager->startCryptoDownload(id, attachment.remoteThumbnailUrl, attachment.thumbnailPath, sender);
        bool success = false;
        QEventLoop loop;
        connect(download, &VSQDownload::ended, [&](bool failed) {
            success = !failed;
            loop.quit();
        });
        loop.exec();
        if(!success) {
            m_sqlConversations->setAttachmentStatus(message.messageId, Attachment::Status::Created);
        }
    });
}

void VSQMessenger::onAttachmentStatusChanged(const QString &uploadId, const Enums::AttachmentStatus status)
{
    const auto id = TransferId::parse(uploadId);
    if (id.type == TransferId::Type::File) {
        m_sqlConversations->setAttachmentStatus(id.messageId, status);
    }
}

void VSQMessenger::onAttachmentProgressChanged(const QString &uploadId, const DataSize bytesReceived, const DataSize bytesTotal)
{
    const auto id = TransferId::parse(uploadId);
    if (id.type == TransferId::Type::File) {
        m_sqlConversations->setAttachmentProgress(id.messageId, bytesReceived, bytesTotal);
    }
}

void VSQMessenger::onAttachmentDecrypted(const QString &uploadId, const QString &filePath)
{
    const auto id = TransferId::parse(uploadId);
    if (id.type == TransferId::Type::File) {
        m_sqlConversations->setAttachmentFilePath(id.messageId, filePath);
    }
    else if (id.type == TransferId::Type::Thumbnail) {
        m_sqlConversations->setAttachmentThumbnailPath(id.messageId, filePath);
    }
}

/******************************************************************************/
QString
VSQMessenger::_virgilURL() {
    QString res = qgetenv("VS_MSGR_VIRGIL");
    if (res.isEmpty()) {
        switch (m_envType) {
        case PROD:
            res = "https://messenger.virgilsecurity.com";
            break;

        case STG:
            res = "https://messenger-stg.virgilsecurity.com";
            break;

        case DEV:
            res = "https://messenger-dev.virgilsecurity.com";
            break;
        }
    }

    VS_LOG_DEBUG("Virgil URL: [%s]", qPrintable(res));
    return res;
}

/******************************************************************************/
QString
VSQMessenger::_xmppURL() {
    QString res = qgetenv("VS_MSGR_XMPP_URL");

    if (res.isEmpty()) {
        switch (m_envType) {
        case PROD:
            res = "xmpp.virgilsecurity.com";
            break;

        case STG:
            res = "xmpp-stg.virgilsecurity.com";
            break;

        case DEV:
            res = "xmpp-dev.virgilsecurity.com";
            break;
        }
    }

    VS_LOG_DEBUG("XMPP URL: %s", res.toStdString().c_str());
    return res;
}

/******************************************************************************/
uint16_t
VSQMessenger::_xmppPort() {
    uint16_t res = 5222;
    QString portStr = qgetenv("VS_MSGR_XMPP_PORT");

    if (!portStr.isEmpty()) {
        bool ok;
        int port = portStr.toInt(&ok);
        if (ok) {
            res = static_cast<uint16_t> (port);
        }
    }

    VS_LOG_DEBUG("XMPP PORT: %d", static_cast<int> (res));

    return res;
}

/******************************************************************************/

void
VSQMessenger::_addToUsersList(const QString &user) {
    // Save known users
    auto knownUsers = usersList();
    knownUsers.removeAll(user);
    knownUsers.push_front(user);
    _saveUsersList(knownUsers);
}

/******************************************************************************/
// TODO: Use SecBox
bool
VSQMessenger::_saveCredentials(const QString &user, const QString &deviceId, const vs_messenger_virgil_user_creds_t &creds) {
    // Save credentials
    QByteArray baCred(reinterpret_cast<const char*>(&creds), sizeof(creds));

    QJsonObject jsonObject;
    jsonObject.insert("device_id", deviceId);
    jsonObject.insert("creds", QJsonValue::fromVariant(baCred.toBase64()));

    QJsonDocument doc(jsonObject);
    QString json = doc.toJson(QJsonDocument::Compact);

    qInfo() << "Saving user credentails: " << json;

    QSettings settings(kOrganization, kApp);
    settings.setValue(user, json);

    return true;
}

/******************************************************************************/
bool
VSQMessenger::_loadCredentials(const QString &user, QString &deviceId, vs_messenger_virgil_user_creds_t &creds) {
    QSettings settings(kOrganization, kApp);

    auto settingsJson = settings.value(user, QString("")).toString();
    QJsonDocument json(QJsonDocument::fromJson(settingsJson.toUtf8()));
    deviceId = json["device_id"].toString();
    auto baCred = QByteArray::fromBase64(json["creds"].toString().toUtf8());

    if (baCred.size() != sizeof(creds)) {
        VS_LOG_WARNING("Cannot load credentials for : %s", user.toStdString().c_str());
        return false;
    }

    memcpy(&creds, baCred.data(), static_cast<size_t> (baCred.size()));
    return true;
}

/******************************************************************************/
void
VSQMessenger::_saveUsersList(const QStringList &users) {
    QSettings settings(kOrganization, kApp);
    settings.setValue(kUsers, users);
}

/******************************************************************************/
QStringList
VSQMessenger::usersList() {
    QSettings settings(kOrganization, kApp);
    qDebug() << settings.fileName();
    return settings.value(kUsers, QStringList()).toStringList();
}

/******************************************************************************/
QFuture<VSQMessenger::EnResult>
VSQMessenger::logout() {
    return QtConcurrent::run([=]() -> EnResult {
        qDebug() << "Logout";
        m_user = "";
        m_userId = "";
        m_xmppPass = "";
        QMetaObject::invokeMethod(this, "onSubscribePushNotifications", Qt::BlockingQueuedConnection, Q_ARG(bool, false));
        QMetaObject::invokeMethod(&m_xmpp, "disconnectFromServer", Qt::BlockingQueuedConnection);
        vs_messenger_virgil_logout();
        return MRES_OK;
    });
}

/******************************************************************************/
QFuture<VSQMessenger::EnResult> VSQMessenger::disconnect() {
    bool connected = m_xmpp.isConnected();
    return QtConcurrent::run([=]() -> EnResult {
        qDebug() << "Disconnect";
        if (connected) {
            QMetaObject::invokeMethod(&m_xmpp, "disconnectFromServer", Qt::BlockingQueuedConnection);
        }
        return MRES_OK;
    });
}

/******************************************************************************/
QFuture<VSQMessenger::EnResult>
VSQMessenger::deleteUser(QString user) {
    return QtConcurrent::run([=]() -> EnResult {
        Q_UNUSED(user)
        logout();
        return MRES_OK;
    });
}

/******************************************************************************/
VSQSqlConversationModel &
VSQMessenger::modelConversations() {
    return *m_sqlConversations;
}

VSQSqlChatModel &
VSQMessenger::getChatModel() {
    return *m_sqlChatModel;
}

void VSQMessenger::setLogging(VSQLogging *loggingPtr) {
    m_logging = loggingPtr;
}


/******************************************************************************/
void
VSQMessenger::onSubscribePushNotifications(bool enable) {
    #if VS_PUSHNOTIFICATIONS

    // Subscribe Form Type
    QXmppDataForm::Field subscribeFormType;
    subscribeFormType.setKey(kPushNotificationsFormType);
    subscribeFormType.setValue(kPushNotificationsFormTypeVal);

    // Subscribe service
    QXmppDataForm::Field subscribeService;
    subscribeService.setKey(kPushNotificationsService);
    subscribeService.setValue(kPushNotificationsFCM);

    // Subscribe device
    QXmppDataForm::Field subscribeDevice;
    subscribeDevice.setKey(kPushNotificationsDeviceID);
    subscribeDevice.setValue(VSQPushNotifications::instance().token());

    // Create a Data Form
    QList<QXmppDataForm::Field> fields;
    fields << subscribeFormType << subscribeService << subscribeDevice;

    QXmppDataForm dataForm;
    dataForm.setType(QXmppDataForm::Submit);
    dataForm.setFields(fields);

    // Create request
    QXmppPushEnableIq xmppPush;
    xmppPush.setType(QXmppIq::Set);
    xmppPush.setMode(enable ? QXmppPushEnableIq::Enable : QXmppPushEnableIq::Disable);
    xmppPush.setJid(kPushNotificationsProxy);

    // xmppPush.setNode(kPushNotificationsNode);
    xmppPush.setNode(currentUser() + "@" + _xmppURL() + "/" + m_deviceId);
    xmppPush.setDataForm(dataForm);

    m_xmpp.sendPacket(xmppPush);
#else
    Q_UNUSED(enable)
#endif // VS_PUSHNOTIFICATIONS
}

/******************************************************************************/
void
VSQMessenger::onConnected() {
    onSubscribePushNotifications(true);
}

/******************************************************************************/
void
VSQMessenger::onReadyToUpload() {
    emit fireReady();
    _sendFailedMessages();
}

/******************************************************************************/
void
VSQMessenger::_sendFailedMessages() {
   auto messages = m_sqlConversations->getMessages(m_user, StMessage::Status::MST_FAILED);
   QtConcurrent::run([=]() {
       for (int i = 0; i < messages.length(); i++) {
           const auto &msg = messages[i];
           if (MRES_OK != _sendMessageInternal(false, msg.messageId, msg.recipient, msg.message, msg.attachment)) {
               break;
           }
       }
   });
}

QString VSQMessenger::createJson(const QString &message, const OptionalAttachment &attachment)
{
    QJsonObject mainObject;
    QJsonObject payloadObject;
    if (!attachment) {
        mainObject.insert("type", "text");
        payloadObject.insert("body", message);
    }
    else if (attachment->type == Attachment::Type::Picture) {
        mainObject.insert("type", "picture");
        payloadObject.insert("url", attachment->remoteUrl.toString());
        payloadObject.insert("displayName", attachment->displayName);
        payloadObject.insert("thumbnailUrl", attachment->remoteThumbnailUrl.toString());
        payloadObject.insert("thumbnailWidth", attachment->thumbnailSize.width());
        payloadObject.insert("thumbnailHeight", attachment->thumbnailSize.height());
        payloadObject.insert("bytesTotal", attachment->bytesTotal);
    }
    else {
        mainObject.insert("type", "file");
        payloadObject.insert("url", attachment->remoteUrl.toString());
        payloadObject.insert("displayName", attachment->displayName);
        payloadObject.insert("bytesTotal", attachment->bytesTotal);
    }
    mainObject.insert("payload", payloadObject);

    QJsonDocument doc(mainObject);
    return doc.toJson(QJsonDocument::Compact);
}

StMessage VSQMessenger::parseJson(const QJsonDocument &json)
{
    const auto type = json["type"].toString();
    const auto payload = json["payload"];
    StMessage message;
    if (type == QLatin1String("text")) {
        message.message = payload["body"].toString();
        return message;
    }
    Attachment attachment;
    attachment.id = VSQUtils::createUuid();
    attachment.remoteUrl = payload["url"].toString();
    attachment.displayName = payload["displayName"].toString();
    attachment.bytesTotal = payload["bytesTotal"].toInt();
    message.message = attachment.displayName;
    if (type == QLatin1String("picture")) {
        attachment.type = Attachment::Type::Picture;
        attachment.remoteThumbnailUrl = payload["thumbnailUrl"].toString();
        attachment.thumbnailSize = QSize(payload["thumbnailWidth"].toInt(), payload["thumbnailHeight"].toInt());
    }
    else {
        attachment.type = Attachment::Type::File;
    }
    message.attachment = attachment;
    return message;
}

OptionalAttachment VSQMessenger::uploadAttachment(const QString messageId, const QString recipient, const Attachment &attachment)
{
    if (!m_transferManager->isReady()) {
        qCDebug(lcMessenger) << "Transfer manager is not ready";
        setFailedAttachmentStatus(messageId);
        return NullOptional;
    }

    Attachment uploadedAttachment = attachment;

    bool thumbnailUploadNeeded = attachment.type == Attachment::Type::Picture && attachment.remoteThumbnailUrl.isEmpty();
    if (thumbnailUploadNeeded) {
        qCDebug(lcMessenger) << "Thumbnail uploading...";
        const TransferId uploadId(messageId, TransferId::Type::Thumbnail);
        if (m_transferManager->hasTransfer(uploadId)) {
            qCCritical(lcMessenger) << "Thumbnail upload for" << messageId << "already exists";
            return NullOptional;
        }
        else if (auto upload = m_transferManager->startCryptoUpload(uploadId, attachment.thumbnailPath, recipient)) {
            m_sqlConversations->setAttachmentStatus(messageId, Attachment::Status::Loading);
            QEventLoop loop;
            QMutex guard;
            auto con = connect(upload, &VSQUpload::ended, [&](bool failed) {
                QMutexLocker l(&guard);
                if (failed) {
                    setFailedAttachmentStatus(messageId);
                }
                else {
                    m_sqlConversations->setAttachmentThumbnailRemoteUrl(messageId, *upload->remoteUrl());
                    uploadedAttachment.remoteThumbnailUrl = *upload->remoteUrl();
                    thumbnailUploadNeeded = false;
                }
                loop.quit();
            });
            connect(upload, &VSQUpload::connectionChanged, &loop, &QEventLoop::quit);
            qCDebug(lcMessenger) << "Upload waiting: start";
            loop.exec();
            QObject::disconnect(con);
            QMutexLocker l(&guard);
            qCDebug(lcMessenger) << "Upload waiting: end";
            if (!thumbnailUploadNeeded) {
                qCDebug(lcMessenger) << "Thumbnail was uploaded";
            }
        }
        else  {
            qCDebug(lcMessenger) << "Unable to start upload";
            setFailedAttachmentStatus(messageId);
            return NullOptional;
        }
    }

    bool attachmentUploadNeeded = attachment.remoteUrl.isEmpty();
    if (attachmentUploadNeeded) {
        qCDebug(lcMessenger) << "Attachment uploading...";
        const TransferId id(messageId, TransferId::Type::File);
        if (m_transferManager->hasTransfer(messageId)) {
            qCCritical(lcMessenger) << "Upload for" << messageId << "already exists";
            return NullOptional;
        }
        else if (auto upload = m_transferManager->startCryptoUpload(id, attachment.filePath, recipient)) {
            m_sqlConversations->setAttachmentStatus(messageId, Attachment::Status::Loading);
            uploadedAttachment.bytesTotal = upload->fileSize();
            m_sqlConversations->setAttachmentBytesTotal(messageId, upload->fileSize());
            QEventLoop loop;
            QMutex guard;
            auto con = connect(upload, &VSQUpload::ended, [&](bool failed) {
                QMutexLocker locker(&guard);
                if (failed) {
                    setFailedAttachmentStatus(messageId);
                }
                else {
                    m_sqlConversations->setAttachmentRemoteUrl(messageId, *upload->remoteUrl());
                    uploadedAttachment.remoteUrl = *upload->remoteUrl();
                    attachmentUploadNeeded = false;
                }
                loop.quit();
            });
            connect(upload, &VSQUpload::connectionChanged, &loop, &QEventLoop::quit);
            qCDebug(lcMessenger) << ": start";
            loop.exec();
            QObject::disconnect(con);
            QMutexLocker locker(&guard);
            qCDebug(lcMessenger) << "Upload waiting: end";
            if (!attachmentUploadNeeded) {
                qCDebug(lcMessenger) << "Attachment was uploaded";
            }
        }
        else {
            qCDebug(lcMessenger) << "Unable to start upload";
            setFailedAttachmentStatus(messageId);
            return NullOptional;
        }
    }

    if (thumbnailUploadNeeded || attachmentUploadNeeded) {
        qCDebug(lcMessenger) << "Thumbnail or/and attachment were not uploaded";
        return NullOptional;
    }
    qCDebug(lcMessenger) << "Everything was uploaded";
    m_sqlConversations->setAttachmentStatus(messageId, Attachment::Status::Loaded);
    uploadedAttachment.status = Attachment::Status::Loaded;
    return uploadedAttachment;
}

void VSQMessenger::setFailedAttachmentStatus(const QString &messageId)
{
    m_sqlConversations->setMessageStatus(messageId, StMessage::Status::MST_FAILED);
    m_sqlConversations->setAttachmentStatus(messageId, Attachment::Status::Failed);
}

/******************************************************************************/
void
VSQMessenger::checkState() {
    if (m_xmpp.state() == QXmppClient::DisconnectedState) {
        qCDebug(lcNetwork) << "We should be connected, but it's not so. Let's try to reconnect.";
#if VS_ANDROID
        emit fireError(tr("Disconnected ..."));
#endif
        _reconnect();
    } else {
#if 0
        qCDebug(lcNetwork) << "Connection is ok";
#endif
    }
}

/******************************************************************************/
void
VSQMessenger::_reconnect() {
    if (!m_user.isEmpty() && !m_userId.isEmpty() && vs_messenger_virgil_is_signed_in()) {
        QtConcurrent::run([=]() {
            _connect(m_user, m_deviceId, m_userId);
        });
    }
}

/******************************************************************************/
void
VSQMessenger::onDisconnected() {
    qDebug() << "onDisconnected  state:" << m_xmpp.state();

    qDebug() << "Carbons disable";
    m_xmppCarbonManager->setCarbonsEnabled(false);
}

/******************************************************************************/
void
VSQMessenger::onError(QXmppClient::Error err) {
    VS_LOG_DEBUG("onError");
    qDebug() << "onError : " << err << "   state:" << m_xmpp.state();
    emit fireError(tr("Connection error ..."));
}

/******************************************************************************/
Optional<StMessage> VSQMessenger::decryptMessage(const QString &sender, const QString &message) {
    static const size_t _decryptedMsgSzMax = 10 * 1024;
    uint8_t decryptedMessage[_decryptedMsgSzMax];
    size_t decryptedMessageSz = 0;

    qDebug() << "Sender            : " << sender;
    qDebug() << "Encrypted message : " << message.length() << " bytes";

    // Decrypt message
    // DECRYPTED_MESSAGE_SZ_MAX - 1  - This is required for a Zero-terminated string
    if (VS_CODE_OK != vs_messenger_virgil_decrypt_msg(
                sender.toStdString().c_str(),
                message.toStdString().c_str(),
                decryptedMessage, decryptedMessageSz - 1,
                &decryptedMessageSz)) {
        VS_LOG_WARNING("Received message cannot be decrypted");
        return NullOptional;
    }

    // Add Zero termination
    decryptedMessage[decryptedMessageSz] = 0;

    // Get message from JSON
    QByteArray baDecr(reinterpret_cast<char *> (decryptedMessage), static_cast<int> (decryptedMessageSz));
    QJsonDocument jsonMsg(QJsonDocument::fromJson(baDecr));

    qCDebug(lcMessenger) << "JSON for parsing:" << jsonMsg;
    auto msg = parseJson(jsonMsg);
    qCDebug(lcMessenger) << "Received message: " << msg.message;
    return msg;
}

/******************************************************************************/
void
VSQMessenger::onMessageReceived(const QXmppMessage &message) {

    QString sender = message.from().split("@").first();
    QString recipient = message.to().split("@").first();

    qInfo() << "Sender: " << sender << " Recipient: " << recipient;

    // Decrypt message
    auto msg = decryptMessage(sender, message.body());
    if (!msg)
        return;

    msg->messageId = message.id();
    if (sender == currentUser()) {
        QString recipient = message.to().split("@").first();
        m_sqlConversations->createMessage(recipient, msg->message, msg->messageId, msg->attachment);
        m_sqlConversations->setMessageStatus(msg->messageId, StMessage::Status::MST_SENT);
        // ensure private chat with recipient exists
        m_sqlChatModel->createPrivateChat(recipient);
        emit fireNewMessage(sender, msg->message);
        return;
    }

    // Add sender to contact
    m_sqlChatModel->createPrivateChat(sender);
    // Save message to DB
    m_sqlConversations->receiveMessage(msg->messageId, sender, msg->message, msg->attachment);
    m_sqlChatModel->updateLastMessage(sender, msg->message);
    if (sender != m_recipient) {
        m_sqlChatModel->updateUnreadMessageCount(sender);
    }

    if (msg->attachment && msg->attachment->type == Attachment::Type::Picture) {
        emit downloadThumbnail(*msg, sender, QPrivateSignal());
    }

    // Inform system about new message
    emit fireNewMessage(sender, msg->message);
}

/******************************************************************************/

VSQMessenger::EnResult
VSQMessenger::_sendMessageInternal(bool createNew, const QString &messageId, const QString &to, const QString &message, const OptionalAttachment &attachment)
{
    // Write to database
    if(createNew) {
        m_sqlConversations->createMessage(to, message, messageId, attachment);
    }
    m_sqlChatModel->updateLastMessage(to, message);

    OptionalAttachment updloadedAttacment;
    if (attachment) {
        qCDebug(lcMessenger) << "Trying to upload the attachment";
        updloadedAttacment = uploadAttachment(messageId, to, *attachment);
        if (!updloadedAttacment) {
            qCDebug(lcMessenger) << "Attachment was NOT uploaded";
            return MRES_OK; // don't send message
        }
        qCDebug(lcMessenger) << "Everything was uploaded. Continue to send message";
    }

    QMutexLocker _guard(&m_messageGuard);
    static const size_t _encryptedMsgSzMax = 20 * 1024;
    uint8_t encryptedMessage[_encryptedMsgSzMax];
    size_t encryptedMessageSz = 0;

    // Create JSON-formatted message to be sent
    const QString internalJson = createJson(message, updloadedAttacment);
    qDebug() << "Json for encryption:" << internalJson;

    // Encrypt message
    auto plaintext = internalJson.toStdString();
    if (VS_CODE_OK != vs_messenger_virgil_encrypt_msg(
                     to.toStdString().c_str(),
                     reinterpret_cast<const uint8_t*>(plaintext.c_str()),
                     plaintext.length(),
                     encryptedMessage,
                     _encryptedMsgSzMax,
                     &encryptedMessageSz)) {
        VS_LOG_WARNING("Cannot encrypt message to be sent");

        // Mark message as failed
        m_sqlConversations->setMessageStatus(messageId, StMessage::Status::MST_FAILED);
        return MRES_ERR_ENCRYPTION;
    }

    // Send encrypted message
    QString toJID = to + "@" + _xmppURL();
    QString fromJID = currentUser() + "@" + _xmppURL();
    QString encryptedStr = QString::fromLatin1(reinterpret_cast<char*>(encryptedMessage), encryptedMessageSz);

    QXmppMessage msg(fromJID, toJID, encryptedStr);
    msg.setReceiptRequested(true);
    msg.setId(messageId);

    // Send message and update status
    if (m_xmpp.sendPacket(msg)) {
        m_sqlConversations->setMessageStatus(messageId, StMessage::Status::MST_SENT);
    } else {
        m_sqlConversations->setMessageStatus(messageId, StMessage::Status::MST_FAILED);
    }
    return MRES_OK;
}

void VSQMessenger::downloadAndProcess(StMessage message, const Function &func)
{
    if (!message.attachment) {
        qCDebug(lcTransferManager) << "Message has no attachment:" << message.messageId;
        return;
    }
    auto future = QtConcurrent::run([=]() {
        auto msg = message;
        auto &attachment = *msg.attachment;
        auto &filePath = attachment.filePath;
        if (QFile::exists(filePath)) {
            func(msg);
            return;
        }
        // Update attachment filePath
        const auto downloads = m_settings->downloadsDir();
        if (filePath.isEmpty() || QFileInfo(filePath).dir() != downloads) {
            filePath = VSQUtils::findUniqueFileName(downloads.filePath(attachment.displayName));
        }
        const TransferId id(msg.messageId, TransferId::Type::File);
        auto download = m_transferManager->startCryptoDownload(id, attachment.remoteUrl, filePath, msg.sender);
        bool success = false;
        QEventLoop loop;
        QMutex guard;
        auto con = connect(download, &VSQDownload::ended, [&](bool failed) {
            QMutexLocker l(&guard);
            success = !failed;
            loop.quit();
        });
        connect(m_transferManager, &VSQCryptoTransferManager::fileDecrypted, download, [=](const QString &id, const QString &filePath) {
            qCDebug(lcTransferManager) << "Comparing of downloaded file with requested..." << filePath;
            if (TransferId::parse(id).messageId == msg.messageId) {
                auto msgWithPath = msg;
                msgWithPath.attachment->filePath = filePath;
                func(msgWithPath);
            }
        });
        loop.exec();
        QObject::disconnect(con);
        QMutexLocker l(&guard);

        if(!success) {
            m_sqlConversations->setAttachmentStatus(message.messageId, Attachment::Status::Created);
        }
    });
}

QFuture<VSQMessenger::EnResult>
VSQMessenger::createSendMessage(const QString messageId, const QString to, const QString message)
{
    return QtConcurrent::run([=]() -> EnResult {
        return _sendMessageInternal(true, messageId, to, message, NullOptional);
    });
}

QFuture<VSQMessenger::EnResult>
VSQMessenger::createSendAttachment(const QString messageId, const QString to,
                                   const QUrl url, const Enums::AttachmentType attachmentType)
{
    return QtConcurrent::run([=]() -> EnResult {
        QString warningText;
        auto attachment = m_attachmentBuilder.build(url, attachmentType, warningText);
        if (!attachment) {
            qCWarning(lcAttachment) << warningText;
            fireWarning(warningText);
            return MRES_ERR_ATTACHMENT;
        }
        return _sendMessageInternal(true, messageId, to, attachment->displayName, attachment);
    });
}

/******************************************************************************/
QFuture<VSQMessenger::EnResult>
VSQMessenger::sendMessage(const QString &to, const QString &message,
                          const QVariant &attachmentUrl, const Enums::AttachmentType attachmentType)
{
    const auto url = attachmentUrl.toUrl();
    if (VSQUtils::isValidUrl(url)) {
        return createSendAttachment(VSQUtils::createUuid(), to, url, attachmentType);
    }
    else {
        return createSendMessage(VSQUtils::createUuid(), to, message);
    }
}

/******************************************************************************/
void
VSQMessenger::setStatus(VSQMessenger::EnStatus status) {
    QXmppPresence::Type presenceType = VSQMessenger::MSTATUS_ONLINE == status ?
                QXmppPresence::Available : QXmppPresence::Unavailable;
    m_xmpp.setClientPresence(QXmppPresence(presenceType));
}

/******************************************************************************/
void VSQMessenger::setCurrentRecipient(const QString &recipient)
{
    m_recipient = recipient;
}

void VSQMessenger::saveAttachmentAs(const QString &messageId, const QVariant &fileUrl)
{
    auto message = m_sqlConversations->getMessage(messageId);
    if (!message) {
        return;
    }
    const auto dest = VSQUtils::urlToLocalFile(fileUrl.toUrl());
    qCDebug(lcTransferManager) << "Saving of attachment as" << dest;
    downloadAndProcess(*message, [=](const StMessage &msg) {
        qCDebug(lcTransferManager) << "Message attachment saved as:" << dest;
        const auto src = msg.attachment->filePath;
        QFile::copy(src, dest);
    });
}

void VSQMessenger::downloadAttachment(const QString &messageId)
{
    auto message = m_sqlConversations->getMessage(messageId);
    if (!message) {
        return;
    }
    qCDebug(lcTransferManager) << "Downloading of attachment:" << messageId;
    downloadAndProcess(*message, [this](const StMessage &msg) {
        qCDebug(lcTransferManager) << QString("Message '%1' attachment was downloaded").arg(msg.messageId);
        emit informationRequested("Saved to downloads");
    });
}

void VSQMessenger::openAttachment(const QString &messageId)
{
    auto message = m_sqlConversations->getMessage(messageId);
    if (!message) {
        return;
    }
    qCDebug(lcTransferManager) << "Opening of attachment:" << messageId;
    downloadAndProcess(*message, [this](const StMessage &msg) {
        const auto url = VSQUtils::localFileToUrl(msg.attachment->filePath);
        qCDebug(lcTransferManager) << "Opening of message attachment:" << url;
        emit openPreviewRequested(url);
    });
}

/******************************************************************************/
void
VSQMessenger::onPresenceReceived(const QXmppPresence &presence) {
    Q_UNUSED(presence)
}

/******************************************************************************/
void
VSQMessenger::onIqReceived(const QXmppIq &iq) {
    Q_UNUSED(iq)
}

/******************************************************************************/
void
VSQMessenger::onSslErrors(const QList<QSslError> &errors) {
    Q_UNUSED(errors)
    emit fireError(tr("Secure connection error ..."));
}

/******************************************************************************/
void
VSQMessenger::onStateChanged(QXmppClient::State state) {
    if (QXmppClient::ConnectingState == state) {
        emit fireConnecting();
    }
}

/******************************************************************************/
