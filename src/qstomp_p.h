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

#ifndef QSTOMP_P_H
#define QSTOMP_P_H

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QSharedData>
#include <QtCore/QMetaMethod>
#include <QtCore/QDateTime>

class QStompFramePrivate
{
public:
    QVariantMap m_header;
    bool m_valid;
    QByteArray m_body;
    const QTextCodec * m_textCodec;
};

class QStompResponseFramePrivate : public QStompFramePrivate
{
public:
    Stomp::ResponseCommand m_type;
};

class QStompRequestFramePrivate : public QStompFramePrivate
{
public:
    Stomp::RequestCommand m_type;
};

class QStompSubScriptionData : public QSharedData
{
public:
    QPointer<QObject> m_subcriber;
    QMetaMethod m_slotMethod;
    QStompRequestFrame m_subcribRequestFrame;
    QStompRequestFrame m_welcomeMessage;
    QStompRequestFrame m_goodbyeMessage;
};

class QStompClientPrivate
{
    P_DECLARE_PUBLIC(QStompClient);
public:
    QStompClientPrivate(QStompClient * q) : m_connectionFrame(Stomp::RequestConnect),
        m_stompVersion(Stomp::ProtocolInvalid),
        m_outgoingPingInternal(0), m_incomingPongInternal(0), m_selfSendFeature(false), counter(0),
        pq_ptr(q) { }
    QTimer m_pingTimer, m_pongTimer;
    QTcpSocket * m_socket;
    const QTextCodec * m_textCodec;

    QByteArray m_buffer;
//	QList<QStompResponseFrame> m_framebuffer;

    QStompRequestFrame m_connectionFrame;
    QVariantMap m_connectedHeaders;
    Stomp::Protocol m_stompVersion;
    int m_outgoingPingInternal; // PING emission
    int m_incomingPongInternal; // PING receive from server
	QDateTime m_lastReceivedPing;

    bool m_selfSendFeature;
    QString m_selfSendKey;

    int counter;

    QList<QStompSubscription> m_subscriptions;

    int findMessageBytes();
    qint64 send(const QByteArray&);

    void _q_socketReadyRead();
    void _q_sendPing();
    void _q_checkPong();
private:
    QStompClient * const pq_ptr;
};

#endif // QSTOMP_P_H
