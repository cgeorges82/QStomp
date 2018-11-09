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
#include <QMetaMethod>

static const QList<QByteArray> VALID_COMMANDS = QList<QByteArray>() << "ABORT" << "ACK" << "BEGIN" << "COMMIT" << "CONNECT" << "DISCONNECT"
												<< "CONNECTED" << "MESSAGE" << "SEND" << "SUBSCRIBE" << "UNSUBSCRIBE" << "RECEIPT" << "ERROR";

static const int qStompResponseFrameMetaTypeId = qRegisterMetaType<QStompResponseFrame>();

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

void QStompFrame::setHeader(const QString &key, const QVariant &value)
{
	P_D(QStompFrame);
    d->m_header.insert(key.toLower(), value);
}

void QStompFrame::setHeader(const QVariantMap &values)
{
	P_D(QStompFrame);
	d->m_header = values;
}

QVariantMap QStompFrame::header() const
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

void QStompFrame::removeHeader(const QString &key)
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
    this->setHeader("content-length", QByteArray::number(len));
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
    this->setHeader("content-type", type);
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
    this->setHeader("content-encoding", name);
	d->m_textCodec = QTextCodec::codecForName(name);
}

void QStompFrame::setContentEncoding(const QTextCodec * codec)
{
	P_D(QStompFrame);
    this->setHeader("content-encoding", codec->name());
	d->m_textCodec = codec;
}

QByteArray QStompFrame::toByteArray() const
{
	const P_D(QStompFrame);
	if (!this->isValid())
		return QByteArray("");

	QByteArray ret = QByteArray("");

    QVariantMap::ConstIterator it = d->m_header.constBegin();
	while (it != d->m_header.constEnd()) {
        QByteArray key = it.key().toLatin1();
//        if (key == Stomp::HeaderConnectHost || key == Stomp::HeaderConnectHeartBeat ||
//                key == Stomp::HeaderConnectLogin || key == Stomp::HeaderConnectPassCode)
            ret += key + ":" + it.value().toString().toLatin1() + "\n";
//		else
//            ret += key + ": " + it.value().toString().toLatin1() + "\n";
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
        this->setHeader(key, line.mid(i + 1));
    else
        this->setHeader(key, line.mid(i + 1).trimmed());

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
    this->setHeader("destination", value);
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
    this->setHeader("subscription", value);
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
    this->setHeader("message-id", value);
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
    this->setHeader("receipt-id", value);
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
    this->setHeader("message", value);
}

bool QStompResponseFrame::isSelfSent() const
{
    return this->headerValue(Stomp::HeaderResponseSelfSent).toBool();
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
        qWarning() << "The request to send is invalid";
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
    this->setHeader("destination", value);
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
    this->setHeader("transaction", value);
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
    this->setHeader("message-id", value);
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
    this->setHeader("receipt", value);
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
    this->setHeader("ack", Stomp::AckTypeList.at( int(type) ));
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
    this->setHeader("id", value);
}


QStompClient::QStompClient(QObject *parent) : QObject(parent), pd_ptr(new QStompClientPrivate(this))
{
	P_D(QStompClient);
    d->m_socket = nullptr;
	d->m_textCodec = QTextCodec::codecForName("utf-8");
    d->m_connectionFrame.setHeader(Stomp::HeaderConnectAcceptVersion, Stomp::ProtocolList.join(','));
    d->m_connectionFrame.setHeader(Stomp::HeaderConnectHost, "/");
    connect(&d->m_pingTimer, SIGNAL(timeout()), this, SLOT(_q_sendPing()));
    connect(&d->m_pongTimer, SIGNAL(timeout()), this, SLOT(_q_checkPong()));
}

QStompClient::~QStompClient()
{
    qDebug();
    QStompSubscription dummy(nullptr, "*");
    unregisterSubscription(dummy);
    logout();
	delete this->pd_ptr;
}

void QStompClient::connectToHost(const QString &hostname, quint16 port)
{
	P_D(QStompClient);
    if (d->m_socket != nullptr && d->m_socket->parent() == this)
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
    if (d->m_socket != nullptr && d->m_socket->parent() == this)
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
    if(frame.type() == Stomp::RequestSubscribe || frame.type() == Stomp::RequestUnsubscribe){
        qCritical() << "Please use registerSubcription and unregisterSubcription";
        return;
    }
	P_D(QStompClient);
    QStompRequestFrame msg = frame;
    if(d->m_selfSendFeature){
        msg.setHeader(d->m_selfSendKey, getConnectedStompSession());
    }
    QByteArray serialized = msg.toByteArray().append(Stomp::EndFrame);
    qDebug() << "Send" << Stomp::RequestCommandList.at(msg.type())
             << "of" << serialized.size() << "bytes";
    d->send(serialized);
}

void QStompClient::setLogin(const QString &user, const QString &password)
{
    P_D(QStompClient);

    if(user.isEmpty())
        d->m_connectionFrame.removeHeader(Stomp::HeaderConnectLogin);
    else
        d->m_connectionFrame.setHeader(Stomp::HeaderConnectLogin, user);

    if(password.isEmpty())
        d->m_connectionFrame.removeHeader(Stomp::HeaderConnectPassCode);
    else
        d->m_connectionFrame.setHeader(Stomp::HeaderConnectPassCode, password);
}

void QStompClient::setSelfSentFeature(bool b, const QString &headerKey)
{
    P_D(QStompClient);
    d->m_selfSendFeature = b;
    d->m_selfSendKey = headerKey;
}

void QStompClient::setVirtualHost(const QString &host) {
    P_D(QStompClient);
    if(host.isEmpty())
        d->m_connectionFrame.removeHeader(Stomp::HeaderConnectHost);
    else
        d->m_connectionFrame.setHeader(Stomp::HeaderConnectHost, host);
}

void QStompClient::setHeartBeat(const int &outgoing, const int &incoming)
{
    P_D(QStompClient);
    if(incoming == 0 && outgoing == 0)
        d->m_connectionFrame.removeHeader(Stomp::HeaderConnectHeartBeat);
    else
        d->m_connectionFrame.setHeader(Stomp::HeaderConnectHeartBeat, QString("%1,%2").arg(outgoing).arg(incoming));
}

QStompSubscription QStompClient::createSubscription(QObject *subcriber, const char *subcriberSlot, const QString &destination, const QString& ack, const QVariantMap &headers) const
{
    return QStompSubscription(subcriber, subcriberSlot, destination, ack, headers);
}

void QStompClient::registerSubscription(QStompSubscription & sub)
{
    P_D(QStompClient);
    if(!sub.isValid())
        return;

    if(!containsSubcription(sub)){
        connect(sub.d->m_subcriber.data(), &QObject::destroyed,
                this, &QStompClient::on_subcriberDestroyed, Qt::UniqueConnection);

        d->m_subscriptions << sub;
        doSubcription(sub);
    }else{
        qWarning() << "Subscription for topic" << sub.d->m_subcribRequestFrame.destination() << "already exist with the same subscriber";
    }
}

void QStompClient::unregisterSubscription(QStompSubscription & sub)
{
    P_D(QStompClient);

    // TODO disconnect &QObject::destroyed if no more subcription with sub.m_subcriber
    int destinationSubcribed = 0; //(by the subscriber)
    int subcriptionRemoved = 0;

    // remove all subscription where :
    //        elemSub.m_subcriber.isNULL || elemSub.m_subcriber==sub.m_subcriber
    //  OR    sub.destination=="*" || elemSub.destination==sub.destination
    for(int i = d->m_subscriptions.size()-1; i>=0; --i){
        QStompSubscription elemSub = d->m_subscriptions[i];
        if(elemSub.d->m_subcriber==sub.d->m_subcriber && !sub.d->m_subcriber.isNull())
            destinationSubcribed++;
        if(elemSub.d->m_subcriber.isNull() || elemSub.d->m_subcriber==sub.d->m_subcriber ||
                sub.d->m_subcribRequestFrame.destination()=="*" || sub.d->m_subcribRequestFrame.destination()==elemSub.d->m_subcribRequestFrame.destination()){
            doUnSubcription(elemSub);
            d->m_subscriptions.takeAt(i);
            subcriptionRemoved++;
        }
    }
    if(!sub.d->m_subcriber.isNull() && destinationSubcribed!=0 && destinationSubcribed==subcriptionRemoved){
        disconnect(sub.d->m_subcriber, &QObject::destroyed,
                   this, &QStompClient::on_subcriberDestroyed);
    }
}

void QStompClient::unregisterSubscription(QObject *subcriber, const QString &destination) {
    QStompSubscription dummy(subcriber, destination);
    unregisterSubscription(dummy);
}

void QStompClient::logout()
{
    qDebug();
    doUnSubcriptions();
    this->sendFrame(QStompRequestFrame(Stomp::RequestDisconnect));
}

void QStompClient::send(const QString &destination, const QString &body, const QString &transactionId, const QVariantMap &headers)
{
	P_D(QStompClient);
    QStompRequestFrame frame(Stomp::RequestSend);
    frame.setHeader(headers);
	frame.setContentEncoding(d->m_textCodec);
	frame.setDestination(destination);
	frame.setBody(body);
	if (!transactionId.isNull())
		frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::commit(const QString &transactionId, const QVariantMap &headers)
{
    QStompRequestFrame frame(Stomp::RequestCommit);
    frame.setHeader(headers);
	frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::begin(const QString &transactionId, const QVariantMap &headers)
{
    QStompRequestFrame frame(Stomp::RequestBegin);
    frame.setHeader(headers);
	frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::abort(const QString &transactionId, const QVariantMap &headers)
{
    QStompRequestFrame frame(Stomp::RequestAbort);
    frame.setHeader(headers);
	frame.setTransactionId(transactionId);
	this->sendFrame(frame);
}

void QStompClient::ack(const QString &messageId, const QString &transactionId, const QVariantMap &headers)
{
    QStompRequestFrame frame(Stomp::RequestAck);
    frame.setHeader(headers);
	frame.setMessageId(messageId);
	if (!transactionId.isNull())
		frame.setTransactionId(transactionId);
    this->sendFrame(frame);
}

void QStompClient::nack(const QString &messageId, const QString &transactionId, const QVariantMap &headers)
{
    QStompRequestFrame frame(Stomp::RequestNack);
    frame.setHeader(headers);
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

bool QStompClient::selfSentFeatureEnabled() const
{
    const P_D(QStompClient);
    return d->m_selfSendFeature;
}

QString QStompClient::selfSentHeaderKey() const
{
    const P_D(QStompClient);
    return d->m_selfSendKey;
}

QAbstractSocket::SocketState QStompClient::socketState() const
{
	const P_D(QStompClient);
    if (d->m_socket == nullptr)
		return QAbstractSocket::UnconnectedState;
	return d->m_socket->state();
}

QAbstractSocket::SocketError QStompClient::socketError() const
{
	const P_D(QStompClient);
    if (d->m_socket == nullptr)
		return QAbstractSocket::UnknownSocketError;
	return d->m_socket->error();
}

QString QStompClient::socketErrorString() const
{
	const P_D(QStompClient);
    if (d->m_socket == nullptr)
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
    if (d->m_socket != nullptr)
        d->m_socket->disconnectFromHost();
}

void QStompClient::stompConnected(QStompResponseFrame frame) {
    P_D(QStompClient);
    d->m_connectedHeaders = frame.header();
    d->m_outgoingPingInternal = d->m_incomingPongInternal = 0;
    d->m_stompVersion = static_cast<Stomp::Protocol>( Stomp::ProtocolList.indexOf( frame.headerValue(Stomp::HeaderConnectedVersion).toString() ));

    QStringList heartBeat = frame.headerValue(Stomp::HeaderConnectedHeartBeat).toString().split(",");
    if(heartBeat.size() == 2){
        // In server's response heartBeat parameters are in server context
        // Therefore, these parameters are inverted in relation to the client's 'CONNECT' request
        d->m_outgoingPingInternal = heartBeat[1].toInt();
        d->m_incomingPongInternal = heartBeat[0].toInt();
    }
    if(d->m_outgoingPingInternal > 0){
        qDebug() << "heartBeat outgoing:" << d->m_outgoingPingInternal << "(must send PING to server)";
        d->m_pingTimer.setInterval(d->m_outgoingPingInternal);
        d->m_pingTimer.setSingleShot(true);
        d->m_pingTimer.start(d->m_outgoingPingInternal);
    }
    if(d->m_incomingPongInternal > 0) {
        qDebug() << "heartBeat incoming:" << d->m_incomingPongInternal << "(must receive PING from server)";
        d->m_pongTimer.setInterval(d->m_incomingPongInternal);
        d->m_pongTimer.setSingleShot(false);
        d->m_lastReceivedPing = QDateTime::currentDateTime();
        d->m_pongTimer.start();
    }

    doSubcriptions();
    emit frameConnectedReceived();
}

void QStompClient::stompMessageReceived(const QStompResponseFrame& frame)
{
    P_D(QStompClient);
    int fireCount = 0;
    if(frame.hasSubscriptionId()){
        QString sub_id = frame.subscriptionId();
        for(QStompSubscription sub : d->m_subscriptions){
            if(sub.subscriptionFrame().subscriptionId() == sub_id){
                fireCount++;
                sub.fireFrameMessage(frame);
            }
        }
    }
    if(!frame.hasSubscriptionId() || fireCount==0){
        qDebug() << "Unable to match subcription. Transmit message to Stomp client";
        QMetaObject::invokeMethod(this, "frameMessageReceived", Qt::QueuedConnection, Q_ARG(QStompResponseFrame,frame));
    }

}

bool QStompClient::containsSubcription(const QStompSubscription & sub) const
{
    const P_D(QStompClient);
    for(QStompSubscription subElem : d->m_subscriptions){
        if((sub.d->m_subcriber.isNull() || subElem.d->m_subcriber == sub.d->m_subcriber) &&
                (sub.d->m_subcribRequestFrame.destination()=="*" || subElem.d->m_subcribRequestFrame.destination()==sub.d->m_subcribRequestFrame.destination()))
            return true;
    }
    return false;
}

bool QStompClient::containsSubcription(QObject *subcriber, const QString &destination) const
{
    QStompSubscription sub(subcriber, destination);
    return containsSubcription(sub);
}

void QStompClient::doSubcriptions()
{
    P_D(QStompClient);
    for(QStompSubscription sub : d->m_subscriptions){
        doSubcription(sub);
    }
}

void QStompClient::doSubcription(QStompSubscription & sub)
{
    P_D(QStompClient);
    if(!d->m_connectedHeaders.isEmpty()){
        if(d->m_stompVersion != Stomp::ProtocolStomp_1_0){
            QString sub_id = QString("sub-%1").arg(d->counter++);
            sub.d->m_subcribRequestFrame.setSubscriptionId(sub_id);
//            sub.d->m_welcomeMessage.setSubscriptionId(sub_id);
        }
        d->send( sub.d->m_subcribRequestFrame.toByteArray().append(Stomp::EndFrame) );
        if(sub.d->m_welcomeMessage.isValid()){
            qDebug() << "Send Welcome MSG";
            sendFrame(sub.d->m_welcomeMessage);
        }
    }
}

void QStompClient::doUnSubcriptions()
{
    P_D(QStompClient);
    for(QStompSubscription sub : d->m_subscriptions){
        doUnSubcription(sub);
    }
}

void QStompClient::doUnSubcription(QStompSubscription &sub)
{
    P_D(QStompClient);
    if(!d->m_connectedHeaders.isEmpty() && sub.subscriptionFrame().hasSubscriptionId()) {
        QStompRequestFrame reqSub = sub.subscriptionFrame();
        QStompRequestFrame reqUnSub(Stomp::RequestUnsubscribe);
//        if(d->m_stompVersion == Stomp::ProtocolStomp_1_0){
            reqUnSub.setDestination(reqSub.destination());
//        }else{
//            reqUnSub.setSubscriptionId(reqSub.subscriptionId());
//        }
        if(sub.d->m_goodbyeMessage.isValid()) {
            qDebug() << "Send GoodBye MSG";
            sendFrame(sub.d->m_goodbyeMessage);
        }
        QByteArray serialized = reqUnSub.toByteArray().append(Stomp::EndFrame);
        qDebug() << "Send" << Stomp::RequestCommandList.at(reqUnSub.type())
                 << "of" << serialized.size() << "bytes";
        qDebug() << serialized;
        if(d->send( serialized ) != -1){
            d->m_socket->flush();
        }


        reqSub.removeHeader(Stomp::HeaderRequestSubscription);
        sub.d->m_subcribRequestFrame = reqSub;
    }
}

void QStompClient::on_socketConnected() {
    P_D(QStompClient);
    // do login

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

void QStompClient::on_subcriberDestroyed(QObject * subscriber)
{
    if(subscriber == nullptr)
        subscriber = sender();
    QStompSubscription sub(subscriber, "*");
    unregisterSubscription(sub);
}

void QStompClientPrivate::_q_checkPong(){
    if(this->m_socket && this->m_socket->isValid() && this->m_incomingPongInternal > 0){
        qint64 elapted = this->m_lastReceivedPing.msecsTo(QDateTime::currentDateTime());
        if(elapted > this->m_incomingPongInternal*2) {
            qWarning() << "Connexion with server too long time without PING";
            this->m_socket->disconnectFromHost();
//            this->m_socket->close();
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
        this->send(Stomp::PingContent);
        this->m_pingTimer.start(m_outgoingPingInternal);
    }
}

qint64 QStompClientPrivate::send(const QByteArray& serialized){
    if (this->m_socket == nullptr || this->m_socket->state() != QAbstractSocket::ConnectedState)
        return -1;
    qint64 bytes = this->m_socket->write(serialized);
    qDebug() << "Written" << bytes << "bytes";
    return bytes;
}

void QStompClientPrivate::_q_socketReadyRead()
{
	P_Q(QStompClient);
	QByteArray data = this->m_socket->readAll();

    if(this->m_incomingPongInternal>0 && this->m_buffer.isEmpty() && data == Stomp::PingContent){
        qDebug() << ">>> PONG";
        this->m_lastReceivedPing = QDateTime::currentDateTime();
        return;
    }

	this->m_buffer.append(data);

	quint32 length;
	while ((length = this->findMessageBytes())) {
		QStompResponseFrame frame(this->m_buffer.left(length));
		if (frame.isValid()) {
            if(this->m_selfSendFeature){
                frame.setHeader(Stomp::HeaderResponseSelfSent, frame.headerValue(this->m_selfSendKey) == q->getConnectedStompSession());
//                frame.removeHeader(this->m_selfSendKey);
            }
            switch(frame.type()) {
            case Stomp::ResponseConnected :
                q->stompConnected(frame);
                break;
            case Stomp::ResponseMessage :
                q->stompMessageReceived(frame);
                break;
            case Stomp::ResponseReceipt :
                qDebug() << frame.toByteArray();
                emit q->frameReceiptReceived(frame);
                break;
            case Stomp::ResponseError :
                qCritical() << frame.toByteArray();
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


QStompSubscription::QStompSubscription(QObject *subcriber, const QString &destination, const QVariantMap &headers)
    : d(new QStompSubScriptionData)
{
    d->m_subcriber = subcriber;
    d->m_subcribRequestFrame = QStompRequestFrame(Stomp::RequestSubscribe);
    d->m_subcribRequestFrame.setHeader(headers);
    d->m_subcribRequestFrame.setDestination(destination);
}

QStompSubscription::QStompSubscription(QObject *subcriber, const char *subcriberSlot, const QString &destination, const QString &ack, const QVariantMap &headers)
    : d(new QStompSubScriptionData)
{
    d->m_subcriber = subcriber;
    d->m_subcribRequestFrame = QStompRequestFrame(Stomp::RequestSubscribe);
    d->m_subcribRequestFrame.setHeader(headers);
    d->m_subcribRequestFrame.setDestination(destination);
    int ackIdx = Stomp::AckTypeList.indexOf(ack);
    d->m_subcribRequestFrame.setAckType( ackIdx>=0 ? static_cast<Stomp::AckType>(ackIdx) : Stomp::AckAuto );
    assignMethodSlot(subcriberSlot);
}

QStompSubscription::QStompSubscription( QObject *subcriber, const char* subcriberSlot, const QString &destination, Stomp::AckType ack, const QVariantMap &headers)
    : d(new QStompSubScriptionData)
{
    d->m_subcriber = subcriber;
    d->m_subcribRequestFrame = QStompRequestFrame(Stomp::RequestSubscribe);
    d->m_subcribRequestFrame.setHeader(headers);
    d->m_subcribRequestFrame.setDestination(destination);
    d->m_subcribRequestFrame.setAckType(ack);

    assignMethodSlot(subcriberSlot);
}

QStompSubscription::QStompSubscription(const QStompSubscription &other)
    : d(other.d) {
}

QStompSubscription &QStompSubscription::operator=(const QStompSubscription &other) {
    if (this != &other)
        d.operator=(other.d);
    return *this;
}

QStompSubscription::~QStompSubscription() {
}

void QStompSubscription::setWelcomeMessage(const QString &body, const QVariantMap &headers)
{
    d->m_welcomeMessage = QStompRequestFrame(Stomp::RequestSend);
    d->m_welcomeMessage.setHeader(headers);
    d->m_welcomeMessage.setDestination(d->m_subcribRequestFrame.destination());
    d->m_welcomeMessage.setBody(body);
}

void QStompSubscription::resetWelcomeMessage()
{
    d->m_welcomeMessage = QStompRequestFrame();
}

void QStompSubscription::setGoodByeMessage(const QString &body, const QVariantMap &headers)
{
    d->m_goodbyeMessage = QStompRequestFrame(Stomp::RequestSend);
    d->m_goodbyeMessage.setHeader(headers);
    d->m_goodbyeMessage.setDestination(d->m_subcribRequestFrame.destination());
    d->m_goodbyeMessage.setBody(body);
}

void QStompSubscription::resetGoodByeMessage()
{
    d->m_goodbyeMessage = QStompRequestFrame();
}

bool QStompSubscription::isValid() const
{
    return d->m_subcriber && d->m_slotMethod.isValid();
}

QStompRequestFrame QStompSubscription::subscriptionFrame() const
{
    return d->m_subcribRequestFrame;
}


void QStompSubscription::fireFrameMessage(QStompResponseFrame frame)
{
    if(isValid()){
        if(d->m_slotMethod.parameterType(0) == QMetaType::QVariantMap) {
            QVariantMap msg = {
                { "header", frame.header() },
                { "body", frame.body() }
            };
            d->m_slotMethod.invoke(d->m_subcriber, Qt::QueuedConnection, Q_ARG(QVariantMap, msg));
        }else{
            d->m_slotMethod.invoke(d->m_subcriber, Qt::QueuedConnection, Q_ARG(QStompResponseFrame, frame));
        }
    }
}

void QStompSubscription::assignMethodSlot(const char *subcriberSlot) {
    if(d->m_subcriber) {
        int methodIdx = d->m_subcriber->metaObject()->indexOfSlot(subcriberSlot);
        if(methodIdx != -1){
            QMetaMethod method = d->m_subcriber->metaObject()->method(methodIdx);
            if(method.parameterCount() == 1 &&
                    (method.parameterType(0)==QMetaType::QVariantMap || method.parameterType(0) == qStompResponseFrameMetaTypeId)){
                d->m_slotMethod = method;
            }else{
                qCritical() << "Filled slot method don't have good signature" << method.methodSignature() << endl
                            << "That must be 'void slotMethod(QVariantMap)'";
            }
        }else{
            qCritical() << "method " << subcriberSlot << "is not a slot";
        }
    }
}
