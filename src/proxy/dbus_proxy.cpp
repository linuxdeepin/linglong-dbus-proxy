/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dbus_proxy.h"

#include <unistd.h>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>

#include <QEventLoop>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

// 存储dbus信息服务器配置文件
const QString serverConfigPath = "/deepin/linglong/config/dbus_proxy_config";

DbusProxy::DbusProxy()
    : serverProxy(new QLocalServer())
{
    connect(serverProxy.get(), SIGNAL(newConnection()), this, SLOT(onNewConnection()));
}

DbusProxy::~DbusProxy()
{
    if (serverProxy) {
        serverProxy->close();
        // delete serverProxy;
    }

    for (auto client : relations.keys()) {
        if (relations[client]) {
            delete relations[client];
            client->close();
        }
    }
}

/*
 * 将应用访问的dbus信息发送到服务端
 *
 * @param appId: 应用的appId
 * @param name: dbus 对象名字
 * @param path: dbus 对象路径
 * @param interface: dbus 对象接口
 */
void DbusProxy::sendDataToServer(const QString &appId, const QString &name, const QString &path,
                                 const QString &interface)
{
    if (name.isEmpty() && path.isEmpty() && interface.isEmpty()) {
        return;
    }

    QFile dbFile(serverConfigPath);
    auto ret = dbFile.open(QIODevice::ReadOnly);
    if (!ret) {
        qWarning() << "open config file err";
        return;
    }
    QString qValue = dbFile.readAll();
    dbFile.close();
    QJsonParseError parseJsonErr;
    QJsonDocument document = QJsonDocument::fromJson(qValue.toUtf8(), &parseJsonErr);
    if (QJsonParseError::NoError != parseJsonErr.error) {
        qWarning() << "parse config file err";
        return;
    }
    QJsonObject dataObject = document.object();
    if (!dataObject.contains("dbusDbUrl")) {
        qWarning() << "dbusDbUrl not found in config";
        return;
    }
    const QString configValue = dataObject["dbusDbUrl"].toString();
    QNetworkAccessManager mgr;
    const QUrl url(configValue + "/apps/adddbusproxy");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject obj;
    obj["appId"] = appId;
    obj["name"] = name;
    obj["path"] = path;
    obj["interface"] = interface;
    QJsonDocument doc(obj);
    QByteArray data = doc.toJson();
    mgr.post(request, data);
    qInfo().noquote() << "send data to test server:";
    qInfo().noquote() << data;
}

/*
 * 启动监听
 *
 * @param socketPath: socket监听地址
 *
 * @return bool: true:成功 其它:失败
 */
bool DbusProxy::startListenBoxClient(const QString &socketPath)
{
    if (socketPath.isEmpty()) {
        qCritical() << "socketPath not exist";
        return false;
    }
    QLocalServer::removeServer(socketPath);
    serverProxy->setSocketOptions(QLocalServer::UserAccessOption);
    bool ret = serverProxy->listen(socketPath);
    if (!ret) {
        qCritical() << "listen box dbus client error";
        return false;
    }
    qInfo() << "startListenBoxClient ret:" << ret;
    return ret;
}

/*
 * 连接dbus-daemon
 *
 * @param localProxy: 请求与dbus-daemon连接的客户端
 * @param daemonPath: dbus-daemon地址
 *
 * @return bool: true:成功 其它:失败
 */
bool DbusProxy::startConnectDbusDaemon(QLocalSocket *localProxy, const QString &daemonPath)
{
    if (daemonPath.isEmpty()) {
        qCritical() << "daemonPath is empty";
        return false;
    }
    // bind clientProxy to dbus daemon
    connect(localProxy, SIGNAL(connected()), this, SLOT(onConnectedServer()));
    connect(localProxy, SIGNAL(disconnected()), this, SLOT(onDisconnectedServer()));
    connect(localProxy, SIGNAL(readyRead()), this, SLOT(onReadyReadServer()));
    qInfo() << "proxy client:" << localProxy << " start connect dbus-daemon...";
    localProxy->connectToServer(daemonPath);
    // 等待3s
    if (!localProxy->waitForConnected(3000)) {
        qCritical() << "connect dbus-daemon error, msg:" << localProxy->errorString();
        return false;
    }
    return true;
}

void DbusProxy::onNewConnection()
{
    QLocalSocket *client = serverProxy->nextPendingConnection();
    qInfo() << "onNewConnection called, client:" << client;
    connect(client, SIGNAL(readyRead()), this, SLOT(onReadyReadClient()));
    connect(client, SIGNAL(disconnected()), this, SLOT(onDisconnectedClient()));

    QLocalSocket *proxyClient = new QLocalSocket();
    bool ret = startConnectDbusDaemon(proxyClient, daemonPath);
    relations.insert(client, proxyClient);
    qInfo() << "onNewConnection create: " << client << "<===>" << proxyClient << " relation, ret:" << ret;
}


int requestPermission(const QString &appId)
{
    QDBusInterface interface("org.desktopspec.permission", "/org/desktopspec/permission", "org.desktopspec.permission",
                             QDBusConnection::sessionBus());
    // 25s dbus 客户端默认25s必须回
    interface.setTimeout(1000 * 60 * 25);
    QDBusPendingReply<int> reply = interface.call("request", appId, "org.desktopspec.permission.Account");
    reply.waitForFinished();
    int ret = -1;
    if (reply.isValid()) {
        ret = reply.value();
    }
    qInfo() << "requestPermission ret:" << ret;
    return ret;
}

void DbusProxy::onReadyReadClient()
{
    // box client socket address
    QLocalSocket *boxClient = static_cast<QLocalSocket *>(sender());
    qInfo() << "onReadyReadClient called, boxClient:" << boxClient;
    if (boxClient) {
        QByteArray data = boxClient->readAll();
        qInfo() << "Read Data From Client size:" << data.size();
        qInfo() << "Read Data From Client:" << data;
        QByteArray helloData;
        bool isHelloMsg = data.contains("BEGIN");
        if (isHelloMsg) {
            // auth begin msg is not normal header
            qWarning() << "get client hello msg";
            helloData = data.mid(7);
        }

        Header header;
        bool ret = false;
        if (isHelloMsg) {
            ret = parseHeader(helloData, &header);
        } else {
            ret = parseHeader(data, &header);
        }
        if (!ret) {
            qWarning() << "parseHeader is not a normal msg";
        }

        QLocalSocket *proxyClient = nullptr;
        if (relations.contains(boxClient)) {
            proxyClient = relations[boxClient];
        } else {
            qCritical() << "boxClient:" << boxClient << " related proxyClient not found";
        }
        // 代理未连接上dbus daemon，尝试重新连接dbus daemon一次
        if (!proxyClient && !connStatus.contains(proxyClient)) {
            ret = startConnectDbusDaemon(proxyClient, daemonPath);
            qInfo() << proxyClient << " start reconnect dbus-daemon ret:" << ret;
        }

        // 判断是否满足过滤规则
        bool isMatch = filter.isMessageMatch(header.destination, header.path, header.interface);
        qInfo() << "msg destination:" << header.destination << ", header.path:" << header.path
                << ", header.interface:" << header.interface << ", header.member:" << header.member
                << ", dbus msg match filter ret:" << isMatch;
        if (!isDbusAuthMsg(data)) {
            sendDataToServer(appId, header.destination, header.path, header.interface);
        }
        // 握手信息不拦截
        if (!isDbusAuthMsg(data) && !isMatch) {
            // 未配置权限申请用户授权
            int result = Allow;
            if (!qgetenv("DBUS_PROXY_INTERCEPT").isNull()) {
                result = requestPermission(appId);
            }
            // 记录应用通过dbus访问的宿主机资源
            if (result != Allow) {
                if (isNeedReply(&header)) {
                    QByteArray reply = createFakeReplyMsg(
                        data, header.serial + 1, boxClientAddr, "org.freedesktop.DBus.Error.AccessDenied",
                        "org.freedesktop.DBus.Error.AccessDenied, please config permission first!");
                    // 伪造 错误消息格式给客户端
                    // 将消息发送方 header中的serial 填充到 reply_serial
                    // 填写消息类型 flags(是否需要回复) 消息body 需要修改消息body长度
                    // 生成一个惟一的序列号
                    boxClient->write(reply);
                    boxClient->waitForBytesWritten(3000);
                    qInfo() << "reply size:" << reply.size();
                    qInfo() << reply;
                }
                return;
            }
        }
        if (!connStatus.contains(proxyClient)) {
            qCritical() << proxyClient << " not connect to dbus-daemon";
            return;
        }
        // 查找对应的 dbus 代理转发
        proxyClient->write(data);
        proxyClient->waitForBytesWritten(3000);
        qInfo() << proxyClient << " send data to dbus-daemon done";
    }
}

void DbusProxy::onDisconnectedClient()
{
    QLocalSocket *sender = static_cast<QLocalSocket *>(QObject::sender());
    if (sender) {
        sender->disconnectFromServer();
    }
    qInfo() << "onDisconnectedClient called, sender:" << sender;
    QLocalSocket *proxyClient = relations[sender];
    // box 客户端断开连接时，断开代理与dbus daemon的连接
    if (!proxyClient) {
        qCritical() << "onDisconnectedClient box client: " << sender << " related proxyClient not found";
        return;
    }
    proxyClient->disconnectFromServer();
    relations.remove(sender);
    delete proxyClient;
}

// dbus-daemon 服务端回调函数
void DbusProxy::onConnectedServer()
{
    QLocalSocket *proxyClient = static_cast<QLocalSocket *>(QObject::sender());
    qInfo() << proxyClient << " connected to dbus-daemon success";
    connStatus.insert(proxyClient, true);
}

void DbusProxy::onReadyReadServer()
{
    QLocalSocket *daemonClient = static_cast<QLocalSocket *>(QObject::sender());
    QByteArray receiveDta = daemonClient->readAll();
    qInfo() << "receive from dbus-daemon, data size:" << receiveDta.size();
    qInfo() << receiveDta;
    // is a right way to judge?
    bool isHelloReply = receiveDta.contains("NameAcquired");
    if (isHelloReply) {
        qInfo() << "parse msg header from dbus-daemon";
        Header header;
        bool ret = parseHeader(receiveDta, &header);
        if (!ret) {
            qCritical() << "dbus-daemon msg parseHeader err";
        }
        boxClientAddr = header.destination;
        qInfo() << "boxClientAddr:" << boxClientAddr;
    }

    // 查找代理对应的客户端
    QLocalSocket *boxClient = nullptr;
    for (auto client : relations.keys()) {
        if (relations[client] == daemonClient) {
            boxClient = client;
            break;
        }
    }
    // 将消息转发给客户端
    if (boxClient) {
        boxClient->write(receiveDta);
        boxClient->waitForBytesWritten(3000);
        qInfo() << boxClient << " send data to box dbus client done, data size:" << receiveDta.size();
    } else {
        qCritical() << daemonClient << " related boxClient not found";
    }
}

// 与dbus-daemon 断开连接
void DbusProxy::onDisconnectedServer()
{
    QLocalSocket *sender = static_cast<QLocalSocket *>(QObject::sender());
    if (sender) {
        sender->disconnectFromServer();
    }

    QLocalSocket *boxClient = nullptr;
    for (auto client : relations.keys()) {
        if (relations[client] == sender) {
            boxClient = client;
            break;
        }
    }
    if (boxClient) {
        boxClient->disconnectFromServer();
    } else {
        qCritical() << "onDisconnectedServer " << sender << " related boxClient not found";
    }

    // 更新代理与dbus daemon连接关系
    if (connStatus.contains(sender)) {
        connStatus.remove(sender);
    }
    qInfo() << "onDisconnectedServer called sender:" << sender;
}

QByteArray DbusProxy::createFakeReplyMsg(const QByteArray &byteMsg, quint32 serial, const QString &dst,
                                         const QString &errorName, const QString &errorMsg)
{
    DBusError dbErr;
    dbus_error_init(&dbErr);
    DBusMessage *receiveMsg = dbus_message_demarshal(byteMsg.constData(), byteMsg.size(), &dbErr);
    if (!receiveMsg) {
        qCritical() << "dbus_message_demarshal failed";
        if (dbus_error_is_set(&dbErr)) {
            qCritical() << "dbus_message_demarshal err msg:" << dbErr.message;
            dbus_error_free(&dbErr);
        }
        return nullptr;
    }

    std::string nameString = errorName.toStdString();
    std::string msgString = errorMsg.toStdString();
    DBusMessage *reply = dbus_message_new_error(receiveMsg, nameString.c_str(), msgString.c_str());
    std::string destination = dst.toStdString();
    auto ret = dbus_message_set_destination(reply, destination.c_str());
    if (!ret) {
        // dbus_message_unref(receiveMsg);
        qCritical() << "createFakeReplyMsg set destination failed";
    }
    dbus_message_set_serial(reply, serial);

    char *replyAsc;
    int len = 0;
    ret = dbus_message_marshal(reply, &replyAsc, &len);
    if (!ret) {
        // dbus_message_unref(receiveMsg);
        qCritical() << "createFakeReplyMsg dbus_message_marshal failed";
    }
    QByteArray data(replyAsc, len);
    dbus_free(replyAsc);
    dbus_message_unref(receiveMsg);
    dbus_message_unref(reply);
    return data;
}