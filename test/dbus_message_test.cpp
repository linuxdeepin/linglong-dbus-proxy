/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.  
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QDebug>

#include "message/dbus_message.h"

TEST(dbusmsg, message01)
{
    QByteArray byteArray(
        "l\x03\x01\x01\x42\x00\x00\x00\x10\x00\x00\x00g\x00\x00\x00\x04\x01s\x00(\x00\x00\x00org.freedesktop.DBus.Error.UnknownMethod\x00\x00\x00\x00\x00\x00\x00\x00\x06\x01s\x00\x06\x00\x00\x00:1.585\x00\x00\x05\x01u\x00\x02\x00\x00\x00\b\x01g\x00\x01s\x00\x00\x07\x01s\x00\x06\x00\x00\x00:1.298\x00\x00\x3D\x00\x00\x00org.freedesktop.DBus.Error.AccessDenied, dbus msg hijack test\x00",
        186);
    Header header;
    bool ret = parseHeader(byteArray, &header);
    qInfo() << "msg path:" << header.path << ", interface:" << header.interface << ",member:" << header.member
            << ", dest:" << header.destination;
    EXPECT_EQ(ret, true);
    bool isErrNameOk = (header.errorName == "org.freedesktop.DBus.Error.UnknownMethod");
    EXPECT_EQ(isErrNameOk, true);
    bool isDstOk = (header.destination == ":1.585");
    EXPECT_EQ(isDstOk, true);
    bool isSenderOk = (header.sender == ":1.298");
    EXPECT_EQ(isSenderOk, true);
}

TEST(dbusmsg, message02)
{
    QByteArray byteArray(
        "l\x01\x00\x01\x14\x00\x00\x00\x02\x00\x00\x00\x9F\x00\x00\x00\x01\x01o\x00#\x00\x00\x00/com/deepin/linglong/PackageManager\x00\x00\x00\x00\x00\x02\x01s\x00\"\x00\x00\x00"
        "com.deepin.linglong.PackageManager\x00\x00\x00\x00\x00\x00\x03\x01s\x00\x04\x00\x00\x00test\x00\x00\x00\x00\x06\x01s\x00\x1E\x00\x00\x00"
        "com.deepin.linglong.AppManager\x00\x00\b\x01g\x00\x01s\x00\x00\x0F\x00\x00\x00org.deepin.demo\x00",
        196);
    Header header;
    bool ret = parseHeader(byteArray, &header);
    qInfo() << "msg path:" << header.path << ", interface:" << header.interface << ",member:" << header.member
            << ", dest:" << header.destination;
    EXPECT_EQ(ret, true);
    bool isPathOk = (header.path == "/com/deepin/linglong/PackageManager");
    EXPECT_EQ(isPathOk, true);
    bool isInterfaceOk = (header.interface == "com.deepin.linglong.PackageManager");
    EXPECT_EQ(isInterfaceOk, true);
    bool isMemberOk = (header.member == "test");
    EXPECT_EQ(isMemberOk, true);
}

TEST(dbusmsg, message03)
{
    // ???????????? ???????????????????????????String
    // "l\x02\x01\x01\x14\x00\x00\x00\x06\x00\x00\x00""0\x00\x00\x00\x06\x01s\x00\x07\x00\x00\x00:1.5475\x00\x05\x01u\x00\x02\x00\x00\x00\b\x01g\x00\x01s\x00\x00\x07\x01s\x00\x07\x00\x00\x00:1.5447\x00\x0F\x00\x00\x00org.deepin.demo\x00"
    QByteArray byteArray(
        "l\x02\x01\x01\x14\x00\x00\x00\x06\x00\x00\x00"
        "0\x00\x00\x00\x06\x01s\x00\x07\x00\x00\x00:1.5475\x00\x05\x01u\x00\x02\x00\x00\x00\b\x01g\x00\x01s\x00\x00\x07\x01s\x00\x07\x00\x00\x00:1.5447\x00\x0F\x00\x00\x00org.deepin.demo\x00",
        84);
    Header header;
    bool ret = parseHeader(byteArray, &header);
    EXPECT_EQ(ret, true);
    bool isDstOk = (header.destination == ":1.5475");
    EXPECT_EQ(isDstOk, true);
    bool isSenderOk = (header.sender == ":1.5447");
    EXPECT_EQ(isSenderOk, true);
    qInfo() << "msg sender:" << header.sender << ", dest:" << header.destination;
}

// buffer[0] ?????????  buffer[1] Message type 1 2 3 4 ????????????METHOD_CALL METHOD_RETURN ERROR SIGNAL
// buffer[2] ???flags buffer[3] ???????????? buffer[4] ;?????????4???????????????body??????????????????59 buffer[8] ?????????4?????? ???serial???
// buffer[12] ???????????????  \x06 ??????HeadType ??????DESTINATION x01s\x00 ???????????? \x07?????????4?????? ????????????  \x04 ???errname
// (\x00\x00\x00 ?????????40  \x05 ??? REPLY_SERIAL???2 \b ??????8?????????SIGNATURE  \x07 ??????????????? \x14\x00\x00\x00
// ???????????????20     org????????????????????????     he body is made up of arguments. "l\x03\x01\x01 ;\x00\x00\x00
// \x03\x00\x00\x00  u\x00\x00\x00 \x06\x01s\x00  \x07\x00\x00\x00  :1.6957\x00 \x04\x01s\x00 (\x00\x00\x00 org. free
// desk top. DBus .Err or.U nkno wnMe thod \x00\x00\x00\x00 \x00\x00\x00\x00 \x05\x01u\x00 \x02\x00\x00\x00  \b\x01g\x00
// \x01s\x00\x00 \x07\x01s\x00 \x14\x00\x00\x00 org. free desk top. DBus \x00\x00\x00\x00 ""6\x00\x00\x00
// org.freedesktop.DBus does not understand message Ping1\x00"
TEST(dbusmsg, message04)
{
    QByteArray byteArray(
        "l\x01\x00\x01\x00\x00\x00\x00\x01\x00\x00\x00n\x00\x00\x00\x01\x01o\x00\x15\x00\x00\x00/org/freedesktop/DBus\x00\x00\x00\x06\x01s\x00\x14\x00\x00\x00org.freedesktop.DBus\x00\x00\x00\x00\x02\x01s\x00\x14\x00\x00\x00org.freedesktop.DBus\x00\x00\x00\x00\x03\x01s\x00\x05\x00\x00\x00Hello\x00\x00\x00",
        128);
    qInfo() << byteArray;
    Header header;
    bool ret = parseHeader(byteArray, &header);
    qInfo() << "msg path:" << header.path << ", interface:" << header.interface << ",member:" << header.member
            << ", dest:" << header.destination;
    EXPECT_EQ(ret, true);
    bool isMemberOk = (header.member == "Hello");
    EXPECT_EQ(isMemberOk, true);
}

TEST(dbusmsg, message05)
{
    QByteArray byteArray(
        "l\x01\x00\x01\xA2\x00\x00\x00\x07\x00\x00\x00y\x00\x00\x00\x01\x01o\x00\x15\x00\x00\x00/org/freedesktop/DBus\x00\x00\x00\x02\x01s\x00\x14\x00\x00\x00org.freedesktop.DBus\x00\x00\x00\x00\x06\x01s\x00\x14\x00\x00\x00org.freedesktop.DBus\x00\x00\x00\x00\b\x01g\x00\x01s\x00\x00\x03\x01s\x00\b\x00\x00\x00""AddMatch\x00\x00\x00\x00\x00\x00\x00\x00\x9D\x00\x00\x00type='signal',sender='org.freedesktop.DBus',interface='org.freedesktop.DBus',member='NameOwnerChanged',path='/org/freedesktop/DBus',arg0='org.gtk.vfs.Daemon'\x00l\x01\x00\x01\x1C\x00\x00\x00\b\x00\x00\x00\x83\x00\x00\x00\x01\x01o\x00\x15\x00\x00\x00/org/freedesktop/DBus\x00\x00\x00\x02\x01s\x00\x14\x00\x00\x00org.freedesktop.DBus\x00\x00\x00\x00\x06\x01s\x00\x14\x00\x00\x00org.freedesktop.DBus\x00\x00\x00\x00\b\x01g\x00\x02su\x00\x03\x01s\x00\x12\x00\x00\x00StartServiceByName\x00\x00\x00\x00\x00\x00\x12\x00\x00\x00org.gtk.vfs.Daemon\x00\x00\x00\x00\x00\x00",
        486);
    qInfo() << byteArray;
    QList<QByteArray> out;
    splitDBusMsg(byteArray, out);
    EXPECT_EQ(out.size(), 2);
}

TEST(dbusmsg, message06)
{
    QByteArray byteArray(
        "BEGIN\r\nl\x01\x00\x01\x00\x00\x00\x00\x01\x00\x00\x00n\x00\x00\x00\x01\x01o\x00\x15\x00\x00\x00/org/freedesktop/DBus\x00\x00\x00\x06\x01s\x00\x14\x00\x00\x00org.freedesktop.DBus\x00\x00\x00\x00\x02\x01s\x00\x14\x00\x00\x00org.freedesktop.DBus\x00\x00\x00\x00\x03\x01s\x00\x05\x00\x00\x00Hello\x00\x00\x00",
        135);
    qInfo() << byteArray;
    QList<QByteArray> out;
    splitDBusMsg(byteArray, out);
    EXPECT_EQ(out.size(), 2);
}

TEST(dbusmsg, message07)
{
    QByteArray byteArray(
        "l\x01\x00\x01\x00\x00\x00\x00\x01\x00\x00\x00n\x00\x00\x00\x01\x01o\x00\x15\x00\x00\x00/org/freedesktop/DBus\x00\x00\x00\x06\x01s\x00\x14\x00\x00\x00org.freedesktop.DBus\x00\x00\x00\x00\x02\x01s\x00\x14\x00\x00\x00org.freedesktop.DBus\x00\x00\x00\x00\x03\x01s\x00\x05\x00\x00\x00Hello\x00\x00\x00",
        128);
    qInfo() << byteArray;
    Header header;
    bool ret = parseDBusMsg(byteArray, &header);
    qInfo() << "msg path:" << header.path << ", interface:" << header.interface << ",member:" << header.member
            << ", dest:" << header.destination;
    EXPECT_EQ(ret, true);
    bool isMemberOk = (header.member == "Hello");
    EXPECT_EQ(isMemberOk, true);
}