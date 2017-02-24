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

#include "qstomp.h"

#include <QtCore/QStringList>
#include <QtCore/QSet>
#include <QtCore/QTextCodec>
#include <QtNetwork/QTcpSocket>

static const QList<QByteArray> VALID_COMMANDS = QList<QByteArray>() << "ABORT" << "ACK" << "BEGIN" << "COMMIT" << "CONNECT" << "DISCONNECT"
												<< "CONNECTED" << "MESSAGE" << "SEND" << "SUBSCRIBE" << "UNSUBSCRIBE" << "RECEIPT" << "ERROR";

QStompFrame::QStompFrame(QStompFramePrivate * d) : pd_ptr(d)
{
	d->m_valid = true;
	d->m_textCodec = QTextCodec::codecForName("utf-8");
}

QStompFrame::QStompFrame(const QStompFrame &other, QStompFramePrivate * d) : pd_ptr(d)
{
	d->m_valid = other.pd_ptr->m_valid;
	d->m_header = other.pd_ptr->m_header;
	d->m_body = other.pd_ptr->m_body;
	d->m_textCodec = other.pd_ptr->m_textCodec;
}

QStompFrame::~QStompFrame()
{
	delete this->pd_ptr;
}

QStompFrame & QStompFrame::operator=(const QStompFrame &other)
{
	P_D(QStompFrame);
	d->m_valid = other.pd_ptr->m_valid;
	d->m_header = other.pd_ptr->m_header;
	d->m_body = other.pd_ptr->m_body;
	d->m_textCodec = other.pd_ptr->m_textCodec;
	return *this;
}

void QStompFrame::setHeaderValue(const QString &key, const QVariant &value)
{
	P_D(QStompFrame);
    d->m_header.insert(key.toLower(), value);
}

void QStompFrame::setHeaderValues(const QStompHeaderList &values)
{
	P_D(QStompFrame);
	d->m_header = values;
}

QStompHeaderList QStompFrame::header() const
{
	const P_D(QStompFrame);
	return d->m_header;
}

bool QStompFrame::headerHasKey(const QString &key) const
{
	const P_D(QStompFrame);
    return d->m_header.contains(key.toLower());
}

QList<QString> QStompFrame::headerKeys() const
{
	const P_D(QStompFrame);
    return d->m_header.keys();
}

QVariant QStompFrame::headerValue(const QString &key) const
{
	const P_D(QStompFrame);
    return d->m_header.value(key.toLower());
}

void QStompFrame::removeHeaderValue(const QString &key)
{
	P_D(QStompFrame);
    d->m_header.remove(key.toLower());
}

void QStompFrame::removeAllHeaders()
{
    P_D(QStompFrame);
    d->m_header.clear();
}

bool QStompFrame::hasContentLength() const
{
	return this->headerHasKey("content-length");
}

int QStompFrame::contentLength() const
{
    return this->headerValue("content-length").toInt();
}

void QStompFrame::setContentLength(uint len)
{
	this->setHeaderValue("content-length", QByteArray::number(len));
}

bool QStompFrame::hasContentType() const
{
	return this->headerHasKey("content-type");
}

QByteArray QStompFrame::contentType() const
{
    QByteArray type = this->headerValue("content-type").toByteArray();
	if (type.isEmpty())
		return QByteArray();

	int pos = type.indexOf(';');
	if (pos == -1)
		return type;

	return type.left(pos).trimmed();
}

void QStompFrame::setContentType(const QByteArray &type)
{
	this->setHeaderValue("content-type", type);
}

bool QStompFrame::hasContentEncoding() const
{
	return this->headerHasKey("content-length");
}

QByteArray QStompFrame::contentEncoding() const
{
    return this->headerValue("content-encoding").toByteArray();
}

void QStompFrame::setContentEncoding(const QByteArray &name)
{
	P_D(QStompFrame);
	this->setHeaderValue("content-encoding", name);
	d->m_textCodec = QTextCodec::codecForName(name);
}

void QStompFrame::setContentEncoding(const QTextCodec * codec)
{
	P_D(QStompFrame);
	this->setHeaderValue("content-encoding", codec->name());
	d->m_textCodec = codec;
}

QByteArray QStompFrame::toByteArray() const
{
	const P_D(QStompFrame);
	if (!this->isValid())
		return QByteArray("");

	QByteArray ret = QByteArray("");

	QStompHeaderList::ConstIterator it = d->m_header.constBegin();
	while (it != d->m_header.constEnd()) {
        QByteArray key = it.key().toLatin1();
        if (key == Stomp::HeaderConnectHost || key == Stomp::HeaderConnectHeartBeat ||
                key == Stomp::HeaderConnectLogin || key == Stomp::HeaderConnectPassCode)
            ret += key + ":" + it.value().toString().toLatin1() + "\n";
		else
            ret += key + ": " + it.value().toString().toLatin1() + "\n";
		++it;
	}
	ret.append('\n');
	return ret + d->m_body;
}

bool QStompFrame::isValid() const
{
	const P_D(QStompFrame);
	return d->m_valid;
}

bool QStompFrame::parseHeaderLine(const QByteArray &line, int)
{
	int i = line.indexOf(':');
	if (i == -1)
		return false;

    QByteArray key = line.left(i).trimmed().toLower();
    if (key == Stomp::HeaderConnectHost || key == Stomp::HeaderConnectHeartBeat ||
            key == Stomp::HeaderConnectLogin || key == Stomp::HeaderConnectPassCode)
        this->setHeaderValue(key, line.mid(i + 1));
	else
        this->setHeaderValue(key, line.mid(i + 1).trimmed());

	return true;
}

bool QStompFrame::parse(const QByteArray &frame)
{
	P_D(QStompFrame);
	int headerEnd = frame.indexOf("\n\n");
	if (headerEnd == -1)
		return false;

	d->m_body = frame.mid(headerEnd+2);

	QList<QByteArray> lines = frame.left(headerEnd).split('\n');

	if (lines.isEmpty())
		return false;

	for (int i = 0; i < lines.size(); i++) {
		if (!this->parseHeaderLine(lines.at(i), i))
			return false;
	}
	if (this->hasContentLength())
		d->m_body.resize(this->contentLength());
	else if (d->m_body.endsWith(QByteArray("\0\n", 2)))
		d->m_body.chop(2);

	return true;
}

void QStompFrame::setValid(bool v)
{
	P_D(QStompFrame);
	d->m_valid = v;
}

QString QStompFrame::body() const
{
	const P_D(QStompFrame);
	return d->m_textCodec->toUnicode(d->m_body);
}

QByteArray QStompFrame::rawBody() const
{
	const P_D(QStompFrame);
	return d->m_body;
}

void QStompFrame::setBody(const QString &body)
{
	P_D(QStompFrame);
	d->m_body = d->m_textCodec->fromUnicode(body);
}

void QStompFrame::setRawBody(const QByteArray &body)
{
	P_D(QStompFrame);
	d->m_body = body;
}


QStompResponseFrame::QStompResponseFrame() : QStompFrame(new QStompResponseFramePrivate)
{
    this->setType(Stomp::ResponseInvalid);
}

QStompResponseFrame::QStompResponseFrame(const QStompResponseFrame &other) : QStompFrame(other, new QStompResponseFramePrivate)
{
	P_D(QStompResponseFrame);
	d->m_type = other.pd_func()->m_type;
}

QStompResponseFrame::QStompResponseFrame(const QByteArray &frame) : QStompFrame(new QStompResponseFramePrivate)
{
	this->setValid(this->parse(frame));
}

QStompResponseFrame::QStompResponseFrame(Stomp::ResponseCommand type) : QStompFrame(new QStompResponseFramePrivate)
{
	this->setType(type);
}

QStompResponseFrame & QStompResponseFrame::operator=(const QStompResponseFrame &other)
{
	QStompFrame::operator=(other);
	P_D(QStompResponseFrame);
	d->m_type = other.pd_func()->m_type;
	return *this;
}

void QStompResponseFrame::setType(Stomp::ResponseCommand type)
{
	P_D(QStompResponseFrame);
    this->setValid(type != Stomp::ResponseInvalid);
	d->m_type = type;
}

Stomp::ResponseCommand QStompResponseFrame::type() const
{
	const P_D(QStompResponseFrame);
	return d->m_type;
}

bool QStompResponseFrame::parseHeaderLine(const QByteArray &line, int number)
{
	P_D(QStompResponseFrame);
	if (number != 0)
		return QStompFrame::parseHeaderLine(line, number);
    int reponseCommandIdx = Stomp::ResponseCommandList.indexOf(line);
    if(reponseCommandIdx >= 0 && reponseCommandIdx < Stomp::ResponseCommandList.size())
        d->m_type = static_cast<Stomp::ResponseCommand>(reponseCommandIdx);
    else
        d->m_type = Stomp::ResponseInvalid;

    return d->m_type != Stomp::ResponseInvalid;
}

QByteArray QStompResponseFrame::toByteArray() const
{
	const P_D(QStompResponseFrame);
	if (!this->isValid())
		return QByteArray("");

	QByteArray ret;
    int reponseCommandIdx = int(d->m_type);
    if(reponseCommandIdx >= 0 && reponseCommandIdx < Stomp::ResponseCommandList.size())
        ret = Stomp::ResponseCommandList.at(reponseCommandIdx).toLatin1() + "\n";
    else
        return QByteArray("");

	return ret + QStompFrame::toByteArray();
}

bool QStompResponseFrame::hasDestination() const
{
	return this->headerHasKey("destination");
}

QString QStompResponseFrame::destination() const
{
    return this->headerValue("destination").toString();
}

void QStompResponseFrame::setDestination(const QString &value)
{
	this->setHeaderValue("destination", value);
}

bool QStompResponseFrame::hasSubscriptionId() const
{
	return this->headerHasKey("subscription");
}

QString QStompResponseFrame::subscriptionId() const
{
    return this->headerValue("subscription").toString();
}

void QStompResponseFrame::setSubscriptionId(const QString &value)
{
	this->setHeaderValue("subscription", value);
}

bool QStompResponseFrame::hasMessageId() const
{
	return this->headerHasKey("message-id");
}

QString QStompResponseFrame::messageId() const
{
    return this->headerValue("message-id").toString();
}

void QStompResponseFrame::setMessageId(const QString &value)
{
	this->setHeaderValue("message-id", value);
}

bool QStompResponseFrame::hasReceiptId() const
{
	return this->headerHasKey("receipt-id");
}

QString QStompResponseFrame::receiptId() const
{
    return this->headerValue("receipt-id").toString();
}

void QStompResponseFrame::setReceiptId(const QString &value)
{
	this->setHeaderValue("receipt-id", value);
}

bool QStompResponseFrame::hasMessage() const
{
	return this->headerHasKey("message");
}

QString QStompResponseFrame::message() const
{
    return this->headerValue("message").toString();
}

void QStompResponseFrame::setMessage(const QString &value)
{
	this->setHeaderValue("message", value);
}


QStompRequestFrame::QStompRequestFrame() : QStompFrame(new QStompRequestFramePrivate)
{
    this->setType(Stomp::RequestInvalid);
}

QStompRequestFrame::QStompRequestFrame(const QStompRequestFrame &other) : QStompFrame(other, new QStompRequestFramePrivate)
{
	P_D(QStompRequestFrame);
	d->m_type = other.pd_func()->m_type;
}

QStompRequestFrame::QStompRequestFrame(const QByteArray &frame) : QStompFrame(new QStompRequestFramePrivate)
{
	this->setValid(this->parse(frame));
}

QStompRequestFrame::QStompRequestFrame(Stomp::RequestCommand type) : QStompFrame(new QStompRequestFramePrivate)
{
	this->setType(type);
}

QStompRequestFrame & QStompRequestFrame::operator=(const QStompRequestFrame &other)
{
	QStompFrame::operator=(other);
	P_D(QStompRequestFrame);
	d->m_type = other.pd_func()->m_type;
	return *this;
}

void QStompRequestFrame::setType(Stomp::RequestCommand type)
{
	P_D(QStompRequestFrame);
    this->setValid(type != Stomp::RequestInvalid);
	d->m_type = type;
}

Stomp::RequestCommand QStompRequestFrame::type() const
{
    const P_D(QStompRequestFrame);
    return d->m_type;
}

bool QStompRequestFrame::parseHeaderLine(const QByteArray &line, int number)
{
	P_D(QStompRequestFrame);
	if (number != 0)
		return QStompFrame::parseHeaderLine(line, number);

    int requestCommandIdx = Stomp::RequestCommandList.indexOf(line);

    d->m_type = static_cast<Stomp::RequestCommand>(requestCommandIdx);
    return d->m_type != Stomp::RequestInvalid;
}

//QStompRequestFrame::QStompRequestFrame(QStompRequestFramePrivate *d) : QStompFrame(d) {
//}

//QStompRequestFrame::QStompRequestFrame(const QStompRequestFrame &other, QStompRequestFramePrivate *d) : QStompFrame(other, d) {
//}

QByteArray QStompRequestFrame::toByteArray() const
{
	const P_D(QStompRequestFrame);
    if (!this->isValid())
		return QByteArray("");

    QByteArray ret;
    int requestCommandIdx = int(d->m_type);
    if(requestCommandIdx >= 0 && requestCommandIdx < Stomp::RequestCommandList.size()){
        ret = Stomp::RequestCommandList.at(requestCommandIdx).toLatin1()+"\n";
    }else{
//        qWarning() << "The request to send is invalid";
        return ret;
    }

	return ret + QStompFrame::toByteArray();
}

bool QStompRequestFrame::hasDestination() const
{
	return this->headerHasKey("destination");
}

QString QStompRequestFrame::destination() const
{
    return this->headerValue("destination").toString();
}

void QStompRequestFrame::setDestination(const QString &value)
{
	this->setHeaderValue("destination", value);
}

bool QStompRequestFrame::hasTransactionId() const
{
	return this->headerHasKey("transaction");
}

QString QStompRequestFrame::transactionId() const
{
    return this->headerValue("transaction").toString();
}

void QStompRequestFrame::setTransactionId(const QString &value)
{
	this->setHeaderValue("transaction", value);
}

bool QStompRequestFrame::hasMessageId() const
{
	return this->headerHasKey("message-id");
}

QString QStompRequestFrame::messageId() const
{
    return this->headerValue("message-id").toString();
}

void QStompRequestFrame::setMessageId(const QString &value)
{
	this->setHeaderValue("message-id", value);
}

bool QStompRequestFrame::hasReceiptId() const
{
	return this->headerHasKey("receipt");
}

QString QStompRequestFrame::receiptId() const
{
    return this->headerValue("receipt").toString();
}

void QStompRequestFrame::setReceiptId(const QString &value)
{
	this->setHeaderValue("receipt", value);
}

bool QStompRequestFrame::hasAckType() const
{
	return this->headerHasKey("ack");
}

Stomp::AckType QStompRequestFrame::ackType() const
{
    int ackTypeIdx = Stomp::AckTypeList.indexOf(this->headerValue("ack").toString());
    if(ackTypeIdx >= 0 && ackTypeIdx < Stomp::AckTypeList.size())
        return static_cast<Stomp::AckType>(ackTypeIdx);

    return Stomp::AckAuto;
}

void QStompRequestFrame::setAckType(Stomp::AckType type)
{
    this->setHeaderValue("ack", Stomp::AckTypeList.at( int(type) ));
}

bool QStompRequestFrame::hasSubscriptionId() const
{
	return this->headerHasKey("id");
}

QString QStompRequestFrame::subscriptionId() const
{
    return this->headerValue("id").toString();
}

void QStompRequestFrame::setSubscriptionId(const QString &value)
{
	this->setHeaderValue("id", value);
}


QStompClient::QStompClient(QObject *parent) : QObject(parent), pd_ptr(new QStompClientPrivate(this))
{
	P_D(QStompClient);
	d->m_socket = NULL;
	d->m_textCodec = QTextCodec::codecForName("utf-8");
    d->m_connectionFrame.setHeaderValue(Stomp::HeaderConnectAcceptVersion, "1.2,1.1,1.0");
    d->m_connectionFrame.setHeaderValue(Stomp::HeaderConnectHost, "/");
    connect(&d->m_pingTimer, SIGNAL(timeout()), this, SLOT(_q_sendPing()));
    connect(&d->m_pongTimer, SIGNAL(timeout()), this, SLOT(_q_checkPong()));
}

QStompClient::~QStompClient()
{
	delete this->pd_ptr;
}

void QStompClient::connectToHost(const QString &hostname, quint16 port)
{
	P_D(QStompClient);
	if (d->m_socket != NULL && d->m_socket->parent() == this)
		delete d->m_socket;
	d->m_socket = new QTcpSocket(this);
    connect(d->m_socket, SIGNAL(connected()), this, SLOT(on_socketConnected()));
    connect(d->m_socket, SIGNAL(disconnected()), this, SLOT(on_socketDisconnected()));
	connect(d->m_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SIGNAL(socketStateChanged(QAbstractSocket::SocketState)));
	connect(d->m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SIGNAL(socketError(QAbstractSocket::SocketError)));
	connect(d->m_socket, SIGNAL(readyRead()), this, SLOT(_q_socketReadyRead()));
	d->m_socket->connectToHost(hostname, port);
}

void QStompClient::setSocket(QTcpSocket *socket)
{
	P_D(QStompClient);
	if (d->m_socket != NULL && d->m_socket->parent() == this)
		delete d->m_socket;
	d->m_socket = socket;
	connect(d->m_socket, SIGNAL(connected()), this, SLOT(on_socketConnected()));
	connect(d->m_socket, SIGNAL(disconnected()), this, SLOT(on_socketDisconnected()));
	connect(d->m_socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(on_socketStateChanged(QAbstractSocket::SocketState)));
	connect(d->m_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(on_socketError(QAbstractSocket::SocketError)));
	connect(d->m_socket, SIGNAL(readyRead()), this, SLOT(on_socketReadyRead()));
}

QTcpSocket * QStompClient::socket() const
{
	const P_D(QStompClient);
	return d->m_socket;
}

void QStompClient::sendFrame(const QStompRequestFrame &frame)
{
	P_D(QStompClient);
	if (d->m_socket == NULL || d->m_socket->state() != QAbstractSocket::ConnectedState)
		return;
	QByteArray serialized = frame.toByteArray();
    qDebug() << serialized;
	serialized.append('\0');
	serialized.append('\n');
	d->m_socket->write(serialized);
}

void QStompClient::setLogin(const QString &user, const QString &password)
{
    P_D(QStompClient);

    if(user.isEmpty())
        d->m_connectionFrame.removeHeaderValue(Stomp::HeaderConnectLogin);
    else
        d->m_connectionFrame.setHeaderValue(Stomp::HeaderConnectLogin, user);

    if(password.isEmpty())
        d->m_connectionFrame.removeHeaderValue(Stomp::HeaderConnectPassCode);
    else
        d->m_connectionFrame.setHeaderValue(Stomp::HeaderConnectPassCode, password);
}

void QStompClient::setVirtualHost(const QString &host) {
    P_D(QStompClient);
    if(host.isEmpty())
        d->m_connectionFrame.removeHeaderValue(Stomp::HeaderConnectHost);
    else
        d->m_connectionFrame.setHeaderValue(Stomp::HeaderConnectHost, host);
}

void QStompClient::setHeartBeat(const int &outgoing, const int &incoming)
{
    P_D(QStompClient);
    if(incoming == 0 && outgoing == 0)
        d->m_connectionFrame.removeHeaderValue(Stomp::HeaderConnectHeartBeat);
    else
        d->m_connectionFrame.setHeaderValue(Stomp::HeaderConnectHeartBeat, QString("%1,%2").arg(outgoing).arg(incoming));
}

void QStompClient::logout()
{
    this->sendFrame(QStompRequestFrame(Stomp::RequestDisconnect));
}

void QStompClient::send(const QString &destination, const QString &body, const QString &transactionId, const QStompHeaderList &headers)
{
	P_D(QStompClient);
    QStompRequestFrame frame(Stomp::RequestSend);
	frame.setHeaderValues(headers);
	frame.setContentEncoding(d->m_textCodec);
	frame.setDestination(destination);
	frame.setBody(body);
	if (!transactionId.isNull())
		frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::subscribe(const QString &destination, Stomp::AckType ack, const QStompHeaderList &headers)
{
    QStompRequestFrame frame(Stomp::RequestSubscribe);
	frame.setHeaderValues(headers);
    frame.setDestination(destination);
    frame.setAckType(ack);
	this->sendFrame(frame);
}

void QStompClient::unsubscribe(const QString &destination, const QStompHeaderList &headers)
{
    QStompRequestFrame frame(Stomp::RequestUnsubscribe);
	frame.setHeaderValues(headers);
	frame.setDestination(destination);
	this->sendFrame(frame);
}

void QStompClient::commit(const QString &transactionId, const QStompHeaderList &headers)
{
    QStompRequestFrame frame(Stomp::RequestCommit);
	frame.setHeaderValues(headers);
	frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::begin(const QString &transactionId, const QStompHeaderList &headers)
{
    QStompRequestFrame frame(Stomp::RequestBegin);
	frame.setHeaderValues(headers);
	frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::abort(const QString &transactionId, const QStompHeaderList &headers)
{
    QStompRequestFrame frame(Stomp::RequestAbort);
	frame.setHeaderValues(headers);
	frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::ack(const QString &messageId, const QString &transactionId, const QStompHeaderList &headers)
{
    QStompRequestFrame frame(Stomp::RequestAck);
	frame.setHeaderValues(headers);
	frame.setMessageId(messageId);
	if (!transactionId.isNull())
		frame.setTransactionId(transactionId);
    this->sendFrame(frame);
}

void QStompClient::nack(const QString &messageId, const QString &transactionId, const QStompHeaderList &headers)
{
    QStompRequestFrame frame(Stomp::RequestNack);
    frame.setHeaderValues(headers);
    frame.setMessageId(messageId);
    if (!transactionId.isNull())
        frame.setTransactionId(transactionId);
    this->sendFrame(frame);
}

bool QStompClient::isConnected() const
{
    const P_D(QStompClient);
    return !d->m_connectedHeaders.isEmpty();
}

QString QStompClient::getConnectedStompVersion() const
{
    const P_D(QStompClient);
    return d->m_connectedHeaders.value(Stomp::HeaderConnectedVersion).toString();
}

QString QStompClient::getConnectedStompServeur() const
{
    const P_D(QStompClient);
    return d->m_connectedHeaders.value(Stomp::HeaderConnectedServer).toString();
}

QString QStompClient::getConnectedStompSession() const
{
    const P_D(QStompClient);
    return d->m_connectedHeaders.value(Stomp::HeaderConnectedSession).toString();
}

int QStompClient::getHeartBeatPingOutGoing() const
{
    const P_D(QStompClient);
    return d->m_outgoingPingInternal;
}

int QStompClient::getHeartBeatPongInComming() const
{
    const P_D(QStompClient);
    return d->m_incomingPongInternal;
}

QAbstractSocket::SocketState QStompClient::socketState() const
{
	const P_D(QStompClient);
	if (d->m_socket == NULL)
		return QAbstractSocket::UnconnectedState;
	return d->m_socket->state();
}

QAbstractSocket::SocketError QStompClient::socketError() const
{
	const P_D(QStompClient);
	if (d->m_socket == NULL)
		return QAbstractSocket::UnknownSocketError;
	return d->m_socket->error();
}

QString QStompClient::socketErrorString() const
{
	const P_D(QStompClient);
	if (d->m_socket == NULL)
		return QLatin1String("No socket");
	return d->m_socket->errorString();
}

QByteArray QStompClient::contentEncoding()
{
	P_D(QStompClient);
	return d->m_textCodec->name();
}

void QStompClient::setContentEncoding(const QByteArray & name)
{
	P_D(QStompClient);
	d->m_textCodec = QTextCodec::codecForName(name);
}

void QStompClient::setContentEncoding(const QTextCodec * codec)
{
	P_D(QStompClient);
	d->m_textCodec = codec;
}

void QStompClient::disconnectFromHost()
{
	P_D(QStompClient);
    d->m_connectedHeaders.clear();
	if (d->m_socket != NULL)
        d->m_socket->disconnectFromHost();
}

void QStompClient::stompConnected(QStompResponseFrame frame) {
    P_D(QStompClient);
    d->m_connectedHeaders = frame.header();
    d->m_outgoingPingInternal = d->m_incomingPongInternal = 0;
    QStringList heartBeat = d->m_connectedHeaders[Stomp::HeaderConnectedHeartBeat].toString().split(",");
    if(heartBeat.size() == 2){
        // In server's response heartBeat parameters are in server context
        // Therefore, these parameters are inverted in relation to the client's 'CONNECT' request
        d->m_outgoingPingInternal = heartBeat[1].toInt();
        d->m_incomingPongInternal = heartBeat[0].toInt();
    }
    if(d->m_outgoingPingInternal > 0){
        qDebug() << "heartBeat outgoing:" << d->m_outgoingPingInternal << "(must send PING to server)";
        d->m_pongTimer.setInterval(d->m_outgoingPingInternal);
        d->m_pingTimer.setSingleShot(true);
        d->m_pingTimer.start(d->m_outgoingPingInternal);
    }
    if(d->m_incomingPongInternal > 0) {
        qDebug() << "heartBeat incoming:" << d->m_incomingPongInternal << "(must receive PING from server)";
        d->m_pongTimer.setInterval(d->m_incomingPongInternal);
        d->m_pongTimer.setSingleShot(false);
        d->m_lastPong = QDateTime::currentDateTime();
        d->m_pongTimer.start();
    }
    // TODO subscriptions
    emit frameConnectedReceived();
}

void QStompClient::on_socketConnected() {
    // do login
    P_D(QStompClient);
    d->m_connectionFrame.setHeaderValue(Stomp::HeaderConnectAcceptVersion, "1.2,1.1,1.0");
    // TODO check required headers
    this->sendFrame(d->m_connectionFrame);

    emit socketConnected();
}

void QStompClient::on_socketDisconnected() {
    P_D(QStompClient);
    d->m_connectedHeaders.clear();
    d->m_pongTimer.stop();
    d->m_pingTimer.stop();
    d->m_incomingPongInternal = d->m_outgoingPingInternal = 0;

    emit socketDisconnected();
}

void QStompClientPrivate::_q_checkPong(){
    if(this->m_socket && this->m_socket->isValid() && this->m_incomingPongInternal > 0){
        qint64 elapted = this->m_lastPong.msecsTo(QDateTime::currentDateTime());
        if(elapted > this->m_incomingPongInternal*2) {
            qWarning() << "Connexion with server too long time without PING";
            this->m_socket->close();
        }
    }else{
        // ensure pong Timer is stopped
        this->m_pongTimer.stop();
    }
}

void QStompClientPrivate::_q_sendPing(){
    this->m_pingTimer.stop();
    if(this->m_socket && this->m_socket->isValid() && m_outgoingPingInternal > 0) {
        qDebug() << "<<< PING";
        this->m_socket->write(Stomp::PingContent);
        this->m_pingTimer.start(m_outgoingPingInternal);
    }
}


void QStompClientPrivate::_q_socketReadyRead()
{
	P_Q(QStompClient);
	QByteArray data = this->m_socket->readAll();

    if(this->m_incomingPongInternal>0 && this->m_buffer.isEmpty() && data == Stomp::PingContent){
        qDebug() << ">>> PONG";
        this->m_lastPong = QDateTime::currentDateTime();
        return;
    }

	this->m_buffer.append(data);

	quint32 length;
	while ((length = this->findMessageBytes())) {
		QStompResponseFrame frame(this->m_buffer.left(length));
		if (frame.isValid()) {
            qDebug() << frame.toByteArray();
            switch(frame.type()) {
            case Stomp::ResponseConnected :
                q->stompConnected(frame);
                break;
            case Stomp::ResponseMessage :
                emit q->frameMessageReceived(frame);
                break;
            case Stomp::ResponseReceipt :
                emit q->frameReceiptReceived(frame);
                break;
            case Stomp::ResponseError :
                emit q->frameErrorReceived(frame);
                break;
            default:
                break;
            }
		}
		else
			qDebug("QStomp: Invalid frame received!");
		this->m_buffer.remove(0, length);
	}
}


quint32 QStompClientPrivate::findMessageBytes()
{
	// Buffer sanity check
	forever {
		if (this->m_buffer.isEmpty())
			return 0;
		int nl = this->m_buffer.indexOf('\n');
		if (nl == -1)
			break;
		QByteArray cmd = this->m_buffer.left(nl);
		if (VALID_COMMANDS.contains(cmd))
			break;
		else {
			qDebug("QStomp: Framebuffer corrupted, repairing...");
			int syncPos = this->m_buffer.indexOf(QByteArray("\0\n", 2));
			if (syncPos != -1)
				this->m_buffer.remove(0, syncPos+2);
			else {
				syncPos = this->m_buffer.indexOf(QByteArray("\0", 1));
				if (syncPos != -1)
					this->m_buffer.remove(0, syncPos+1);
				else
					this->m_buffer.clear();
			}
		}
	}

	// Look for content-length
	int headerEnd = this->m_buffer.indexOf("\n\n");
	int clPos = this->m_buffer.indexOf("\ncontent-length");
	if (clPos != -1 && headerEnd != -1 && clPos < headerEnd) {
		int colon = this->m_buffer.indexOf(':', clPos);
		int nl = this->m_buffer.indexOf('\n', clPos);
		if (colon != -1 && nl != -1 && nl > colon) {
			bool ok = false;
			quint32 cl = this->m_buffer.mid(colon, nl-colon).toUInt(&ok) + headerEnd+2;
			if (ok) {
				if ((quint32)this->m_buffer.size() >= cl)
					return cl;
				else
					return 0;
			}
		}
	}

	// No content-length, look for \0\n
	int end = this->m_buffer.indexOf(QByteArray("\0\n", 2));
	if (end == -1) {
		// look for \0
		end = this->m_buffer.indexOf(QByteArray("\0", 1));
		if (end == -1)
			return 0;
		else
			return (quint32) end+1;
	}
	else
		return (quint32) end+2;
}
