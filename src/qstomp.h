/*
 * This file is part of QStomp
 *
 * Copyright (C) 2009 Patrick Schneider <patrick.p2k.schneider@googlemail.com>
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; see the file
 * COPYING.LESSER.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef QSTOMP_H
#define QSTOMP_H

#include "qstomp_global.h"

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QMap>
#include <QtNetwork/QAbstractSocket>

class QTcpSocket;
class QAuthenticator;
class QTextCodec;

class QStompFramePrivate;
class QStompResponseFramePrivate;
class QStompRequestFramePrivate;
//class QStompRequestSubscribeFramePrivate;
class QStompClientPrivate;

typedef QList< QPair<QByteArray, QByteArray> > QStompHeaderListOld;
typedef QVariantMap QStompHeaderList;

namespace Stomp {
    enum RequestCommand {
        RequestInvalid = -1,
        RequestConnect,
        RequestSend,
        RequestSubscribe,
        RequestUnsubscribe,
        RequestBegin,
        RequestCommit,
        RequestAbort,
        RequestAck,
        RequestDisconnect,
        RequestNack
    };
    const QList<QString> RequestCommandList = {"CONNECT", "SEND", "SUBSCRIBE", "UNSUBSCRIBE", "BEGIN", "COMMIT", "ABORT", "ACK", "DISCONNECT", "NACK"};

    enum ResponseCommand {
        ResponseInvalid = -1,
        ResponseConnected,
        ResponseMessage,
        ResponseReceipt,
        ResponseError
    };
    const QList<QString> ResponseCommandList = {"CONNECTED", "MESSAGE", "RECEIPT", "ERROR"};

    enum AckType {
        AckAuto,
        AckClient,
        AckClientIndividual // stomp_v1.1 | stomp_v1.2
    };
    const QList<QString> AckTypeList = {"auto", "client", "client-individual"};

    const QString HeaderConnectAcceptVersion("accept-version");
    const QString HeaderConnectHost("host");
    const QString HeaderConnectHeartBeat("heart-beat");
    const QString HeaderConnectLogin("login");
    const QString HeaderConnectPassCode("passcode");

    const QString HeaderConnectedServer("server");
    const QString HeaderConnectedVersion("version");
    const QString HeaderConnectedSession("session");
    const QString HeaderConnectedHeartBeat("heart-beat");

    const QString HeaderContentType("content-type");
    const QString HeaderContentLength("content-length");
    const QString HeaderContentEncoding("content-encoding");

    const QString HeaderResponseDestination("destination");
    const QString HeaderResponseMessageID("message-id");
    const QString HeaderResponseMessage("message");

    const QByteArray PingContent(1, 0x0A);
}

class QSTOMP_SHARED_EXPORT QStompFrame
{
	P_DECLARE_PRIVATE(QStompFrame)
public:
	virtual ~QStompFrame();

	QStompFrame &operator=(const QStompFrame &other);

    void setHeaderValue(const QString &key, const QVariant &value);
	void setHeaderValues(const QStompHeaderList &values);
	QStompHeaderList header() const;
    bool headerHasKey(const QString &key) const;
    QList<QString> headerKeys() const;
    QVariant headerValue(const QString &key) const;
    void removeHeaderValue(const QString &key);
    void removeAllHeaders();

	bool hasContentLength() const;
    int contentLength() const;
	void setContentLength(uint len);
	bool hasContentType() const;
	QByteArray contentType() const;
	void setContentType(const QByteArray &type);
	bool hasContentEncoding() const;
	QByteArray contentEncoding() const;
	void setContentEncoding(const QByteArray & name);
	void setContentEncoding(const QTextCodec * codec);

	virtual QByteArray toByteArray() const;
	bool isValid() const;

	QString body() const;
	QByteArray rawBody() const;

	void setBody(const QString &body);
	void setRawBody(const QByteArray &body);

protected:
	virtual bool parseHeaderLine(const QByteArray &line, int number);
	bool parse(const QByteArray &str);
	void setValid(bool);

protected:
	QStompFrame(QStompFramePrivate * d);
	QStompFrame(const QStompFrame &other, QStompFramePrivate * d);

	QStompFramePrivate * const pd_ptr;
};

class QSTOMP_SHARED_EXPORT QStompResponseFrame : public QStompFrame
{
	P_DECLARE_PRIVATE(QStompResponseFrame)
public:
	QStompResponseFrame();
	QStompResponseFrame(const QStompResponseFrame &other);
	QStompResponseFrame(const QByteArray &frame);
    QStompResponseFrame(Stomp::ResponseCommand type);
	QStompResponseFrame &operator=(const QStompResponseFrame &other);

    void setType(Stomp::ResponseCommand type);
    Stomp::ResponseCommand type() const;

	bool hasDestination() const;
    QString destination() const;
    void setDestination(const QString &value);

	bool hasSubscriptionId() const;
    QString subscriptionId() const;
    void setSubscriptionId(const QString &value);

	bool hasMessageId() const;
    QString messageId() const;
    void setMessageId(const QString &value);

	bool hasReceiptId() const;
    QString receiptId() const;
    void setReceiptId(const QString &value);

	bool hasMessage() const;
    QString message() const;
    void setMessage(const QString &value);

	QByteArray toByteArray() const;

protected:
	bool parseHeaderLine(const QByteArray &line, int number);
};

class QSTOMP_SHARED_EXPORT QStompRequestFrame : public QStompFrame
{
	P_DECLARE_PRIVATE(QStompRequestFrame)
public:

	QStompRequestFrame();
	QStompRequestFrame(const QStompRequestFrame &other);
	QStompRequestFrame(const QByteArray &frame);
    QStompRequestFrame(Stomp::RequestCommand type);
	QStompRequestFrame &operator=(const QStompRequestFrame &other);

    void setType(Stomp::RequestCommand type);
    Stomp::RequestCommand type() const;

	bool hasDestination() const;
    QString destination() const;
    void setDestination(const QString &value);

	bool hasTransactionId() const;
    QString transactionId() const;
    void setTransactionId(const QString &value);

	bool hasMessageId() const;
    QString messageId() const;
    void setMessageId(const QString &value);

	bool hasReceiptId() const;
    QString receiptId() const;
    void setReceiptId(const QString &value);

	bool hasAckType() const;
    Stomp::AckType ackType() const;
    void setAckType(Stomp::AckType type);

	bool hasSubscriptionId() const;
    QString subscriptionId() const;
    void setSubscriptionId(const QString &value);

	QByteArray toByteArray() const;

protected:
	bool parseHeaderLine(const QByteArray &line, int number);

//    QStompRequestFrame(QStompRequestFramePrivate * d);
//    QStompRequestFrame(const QStompRequestFrame &other, QStompRequestFramePrivate * d);
};

//class QStompRequestSubscribeFrame : public QStompRequestFrame {
//    P_DECLARE_PRIVATE(QStompRequestSubscribeFrame)
//public:
//    QStompRequestSubscribeFrame();
//    QStompRequestSubscribeFrame(const QStompRequestSubscribeFrame &other);
//    QStompRequestSubscribeFrame(const QByteArray &frame);
//    QStompRequestSubscribeFrame &operator=(const QStompRequestSubscribeFrame &other);
//};

class QSTOMP_SHARED_EXPORT QStompClient : public QObject
{
	Q_OBJECT
	P_DECLARE_PRIVATE(QStompClient)
public:

	explicit QStompClient(QObject *parent = 0);
	virtual ~QStompClient();

	enum Error {
		NoError,
		UnknownError,
		HostNotFound,
		ConnectionRefused,
		UnexpectedClose
	};

	void connectToHost(const QString &hostname, quint16 port = 61613);
	void setSocket(QTcpSocket *socket);
	QTcpSocket * socket() const;

	void sendFrame(const QStompRequestFrame &frame);

    void setLogin(const QString &user = QString(), const QString &password = QString());
    void setVirtualHost(const QString &host = QString("/"));
    void setHeartBeat(const int &outgoing = 0, const int &incoming = 0);

	void logout();

    void send(const QString &destination, const QString &body, const QString &transactionId = QString(), const QStompHeaderList &headers = QStompHeaderList());
    void subscribe(const QString &destination, Stomp::AckType ack, const QStompHeaderList &headers = QStompHeaderList());
    void unsubscribe(const QString &destination, const QStompHeaderList &headers = QStompHeaderList());
    void commit(const QString &transactionId, const QStompHeaderList &headers = QStompHeaderList());
    void begin(const QString &transactionId, const QStompHeaderList &headers = QStompHeaderList());
    void abort(const QString &transactionId, const QStompHeaderList &headers = QStompHeaderList());
    void ack(const QString &messageId, const QString &transactionId = QString(), const QStompHeaderList &headers = QStompHeaderList());
    // not available for stomp v1.0
    void nack(const QString &messageId, const QString &transactionId = QString(), const QStompHeaderList &headers = QStompHeaderList());

    bool isConnected() const;
    QString getConnectedStompVersion() const;
    QString getConnectedStompServeur() const;
    QString getConnectedStompSession() const;
    int getHeartBeatPingOutGoing() const;
    int getHeartBeatPongInComming() const;

	QAbstractSocket::SocketState socketState() const;
	QAbstractSocket::SocketError socketError() const;
	QString socketErrorString() const;

	QByteArray contentEncoding();
	void setContentEncoding(const QByteArray & name);
	void setContentEncoding(const QTextCodec * codec);

public Q_SLOTS:
    void disconnectFromHost();

Q_SIGNALS:
    void socketConnected();
	void socketDisconnected();
	void socketError(QAbstractSocket::SocketError);
	void socketStateChanged(QAbstractSocket::SocketState);

    void frameConnectedReceived();
    void frameMessageReceived(QStompResponseFrame);
    void frameReceiptReceived(QStompResponseFrame);
    void frameErrorReceived(QStompResponseFrame);

protected:
    void stompConnected(QStompResponseFrame);
protected slots:
    void on_socketConnected();
    void on_socketDisconnected();


private:
	QStompClientPrivate * const pd_ptr;
    Q_PRIVATE_SLOT(pd_func(), void _q_socketReadyRead())
    Q_PRIVATE_SLOT(pd_func(), void _q_sendPing())
    Q_PRIVATE_SLOT(pd_func(), void _q_checkPong())
};

// Include private header so MOC won't complain
#ifdef QSTOMP_P_INCLUDE
#  include "qstomp_p.h"
#endif

#endif // QSTOMP_H
