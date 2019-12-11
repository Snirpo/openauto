/*
*  This file is part of openauto project.
*  Copyright (C) 2018 f1x.studio (Michal Szwaj)
*
*  openauto is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 3 of the License, or
*  (at your option) any later version.

*  openauto is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with openauto. If not, see <http://www.gnu.org/licenses/>.
*/

#include <boost/algorithm/hex.hpp>
#include <f1x/openauto/Common/Log.hpp>
#include <f1x/openauto/btservice/AndroidBluetoothServer.hpp>
#include <QtCore/QDataStream>
#include <aasdk_proto/WifiInfoRequestMessage.pb.h>
#include <aasdk_proto/WifiInfoResponseMessage.pb.h>

namespace f1x {
    namespace openauto {
        namespace btservice {

            AndroidBluetoothServer::AndroidBluetoothServer()
                    : rfcommServer_(std::make_unique<QBluetoothServer>(QBluetoothServiceInfo::RfcommProtocol, this)) {
                connect(rfcommServer_.get(), &QBluetoothServer::newConnection, this,
                        &AndroidBluetoothServer::onClientConnected);
            }

            uint16_t AndroidBluetoothServer::start(const QBluetoothAddress &address) {
                if (rfcommServer_->listen(address)) {
                    return rfcommServer_->serverPort();
                }
                return 0;
            }

            void AndroidBluetoothServer::onClientConnected() {
                if (socket != nullptr) {
                    OPENAUTO_LOG(info) << "[AndroidBluetoothServer] not accepting new connections, already connected "
                                       << socket->peerName().toStdString();
                    return;
                }

                socket = rfcommServer_->nextPendingConnection();

                if (socket != nullptr) {
                    OPENAUTO_LOG(info) << "[AndroidBluetoothServer] rfcomm client connected, peer name: "
                                       << socket->peerName().toStdString();

                    connect(socket, &QBluetoothSocket::readyRead, this, &AndroidBluetoothServer::readSocket);
//                    connect(socket, &QBluetoothSocket::disconnected, this,
//                            QOverload<>::of(&ChatServer::clientDisconnected));
                } else {
                    OPENAUTO_LOG(error) << "[AndroidBluetoothServer] received null socket during client connection.";
                }
            }

            void AndroidBluetoothServer::readSocket() {
                buffer += socket->readAll();

                if (buffer.length() < 4) {
                    OPENAUTO_LOG(debug) << "Not enough data, waiting for more";
                    return;
                }

                QDataStream stream(buffer);
                uint16_t length;
                stream >> length;

                if (buffer.length() < length + 4) {
                    OPENAUTO_LOG(info) << "Not enough data, waiting for more: " << buffer.length();
                    return;
                }

                uint16_t messageId;
                stream >> messageId;

                OPENAUTO_LOG(info) << "[AndroidBluetoothServer] " << length << " " << messageId;

                switch (messageId) {
                    case 1: {
                        handleWifiInfoRequest(buffer, length);
                        break;
                    }
                    default: {
                        std::stringstream ss;
                        ss << std::hex << std::setfill('0');
                        for (auto &&val : buffer) {
                            ss << std::setw(2) << static_cast<unsigned>(val);
                        }
                        OPENAUTO_LOG(info) << "Unknown message: " << messageId;
                        OPENAUTO_LOG(info) << ss.str();
                        break;
                    }
                }

                buffer = buffer.mid(length + 4);
            }

            void AndroidBluetoothServer::handleWifiInfoRequest(QByteArray &buffer, uint16_t length) {
                f1x::aasdk::proto::data::WifiInfoRequest msg;
                msg.ParseFromArray(buffer.data(), length);
                OPENAUTO_LOG(info) << "WifiInfoRequest: " << msg.DebugString();

                f1x::aasdk::proto::data::WifiInfoResponse response;

                int byteSize = response.ByteSize();
                QByteArray out(byteSize + 4, 0);
                QDataStream ds(&out, QIODevice::ReadWrite);
                ds << (uint16_t) byteSize;
                ds << (uint16_t) 2;
                response.SerializeToArray(out.data() + 4, byteSize);

                std::stringstream ss;
                ss << std::hex << std::setfill('0');
                for (auto &&val : out) {
                    ss << std::setw(2) << static_cast<unsigned>(val);
                }
                OPENAUTO_LOG(info) << "Writing message: " << ss.str();

                //socket->write(out.data());
            }
        }
    }
}