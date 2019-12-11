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

#include <QApplication>
#include <f1x/openauto/Common/Log.hpp>
#include <f1x/openauto/autoapp/Projection/LocalBluetoothDevice.hpp>
#include <QBluetoothUuid>
#include <QtBluetooth>

namespace f1x
{
namespace openauto
{
namespace autoapp
{
namespace projection
{

LocalBluetoothDevice::LocalBluetoothDevice()
{
    qRegisterMetaType<IBluetoothDevice::PairingPromise::Pointer>("PairingPromise::Pointer");

    this->moveToThread(QApplication::instance()->thread());
    connect(this, &LocalBluetoothDevice::startPairing, this, &LocalBluetoothDevice::onStartPairing, Qt::QueuedConnection);
    QMetaObject::invokeMethod(this, "createBluetoothLocalDevice", Qt::BlockingQueuedConnection);
}

void LocalBluetoothDevice::createBluetoothLocalDevice()
{
    OPENAUTO_LOG(debug) << "[LocalBluetoothDevice] create.";

    localDevice_ = std::make_unique<QBluetoothLocalDevice>(QBluetoothAddress());

    connect(localDevice_.get(), &QBluetoothLocalDevice::pairingDisplayConfirmation, this, &LocalBluetoothDevice::onPairingDisplayConfirmation);
    connect(localDevice_.get(), &QBluetoothLocalDevice::pairingDisplayPinCode, this, &LocalBluetoothDevice::onPairingDisplayPinCode);
    connect(localDevice_.get(), &QBluetoothLocalDevice::pairingFinished, this, &LocalBluetoothDevice::onPairingFinished);
    connect(localDevice_.get(), &QBluetoothLocalDevice::error, this, &LocalBluetoothDevice::onError);
    connect(localDevice_.get(), &QBluetoothLocalDevice::hostModeStateChanged, this, &LocalBluetoothDevice::onHostModeStateChanged);

    localDevice_->powerOn();
    localDevice_->setHostMode(QBluetoothLocalDevice::HostDiscoverable);
//
//    rfcommServer_ = std::make_unique<QBluetoothServer>(QBluetoothServiceInfo::RfcommProtocol, this);
//    connect(rfcommServer_.get(), &QBluetoothServer::newConnection, this, &LocalBluetoothDevice::onClientConnected, Qt::QueuedConnection);
//    if (rfcommServer_->listen(localDevice_->address())) {
//        OPENAUTO_LOG(debug) << "Listening for rfcomm connections on port " << rfcommServer_->serverPort();
//    }
//    else {
//        OPENAUTO_LOG(debug) << "Could not start rfcomm";
//    }
//
//    //"4de17a00-52cb-11e6-bdf4-0800200c9a66";
//    //"669a0c20-0008-f4bd-e611-cb52007ae14d";
//    const QBluetoothUuid serviceUuid(QLatin1String("4de17a00-52cb-11e6-bdf4-0800200c9a66"));
//
//    QBluetoothServiceInfo::Sequence classId;
//    classId << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::SerialPort));
//    serviceInfo_.setAttribute(QBluetoothServiceInfo::BluetoothProfileDescriptorList, classId);
//    classId.prepend(QVariant::fromValue(serviceUuid));
//    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceClassIds, classId);
//    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceName, "OpenAuto Bluetooth Service");
//    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceDescription, "AndroidAuto WiFi projection automatic setup");
//    serviceInfo_.setAttribute(QBluetoothServiceInfo::ServiceProvider, "f1xstudio.com");
//    serviceInfo_.setServiceUuid(serviceUuid);
//
//    QBluetoothServiceInfo::Sequence publicBrowse;
//    publicBrowse << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));
//    serviceInfo_.setAttribute(QBluetoothServiceInfo::BrowseGroupList, publicBrowse);
//
//    QBluetoothServiceInfo::Sequence protocolDescriptorList;
//    QBluetoothServiceInfo::Sequence protocol;
//    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
//    protocolDescriptorList.append(QVariant::fromValue(protocol));
//    protocol.clear();
//    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm))
//             << QVariant::fromValue(quint8(rfcommServer_->serverPort()));
//    protocolDescriptorList.append(QVariant::fromValue(protocol));
//    serviceInfo_.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList, protocolDescriptorList);
//    serviceInfo_.registerService(localDevice_->address());
}

void LocalBluetoothDevice::stop()
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    if(pairingPromise_ != nullptr)
    {
        pairingPromise_->reject();
        pairingPromise_.reset();
        pairingAddress_ = QBluetoothAddress();
    }
}

bool LocalBluetoothDevice::isPaired(const std::string& address) const
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    return localDevice_->pairingStatus(QBluetoothAddress(QString::fromStdString(address))) != QBluetoothLocalDevice::Unpaired;
}

void LocalBluetoothDevice::pair(const std::string& address, PairingPromise::Pointer promise)
{
    emit startPairing(QString::fromStdString(address), std::move(promise));
}

std::string LocalBluetoothDevice::getLocalAddress() const
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    return localDevice_->isValid() ? localDevice_->address().toString().toStdString() : "";
}

bool LocalBluetoothDevice::isAvailable() const
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    return localDevice_->isValid();
}

void LocalBluetoothDevice::onStartPairing(const QString& address, PairingPromise::Pointer promise)
{
    OPENAUTO_LOG(debug) << "[LocalBluetoothDevice] onStartPairing, address: " << address.toStdString();

    std::lock_guard<decltype(mutex_)> lock(mutex_);

    if(!localDevice_->isValid())
    {
        promise->reject();
    }
    else
    {
        if(pairingPromise_ != nullptr)
        {
            pairingPromise_->reject();
        }

        pairingAddress_ = QBluetoothAddress(address);
        pairingPromise_ = std::move(promise);
        localDevice_->requestPairing(pairingAddress_, QBluetoothLocalDevice::AuthorizedPaired);
    }
}

void LocalBluetoothDevice::onPairingDisplayConfirmation(const QBluetoothAddress &address, QString pin)
{
    OPENAUTO_LOG(debug) << "[LocalBluetoothDevice] onPairingDisplayConfirmation, address: " << address.toString().toStdString()
                           << ", pin: " << pin.toStdString();

    std::lock_guard<decltype(mutex_)> lock(mutex_);
    localDevice_->pairingConfirmation(address == pairingAddress_);
}

void LocalBluetoothDevice::onPairingDisplayPinCode(const QBluetoothAddress &address, QString pin)
{
    OPENAUTO_LOG(debug) << "[LocalBluetoothDevice] onPairingDisplayPinCode, address: " << address.toString().toStdString()
                           << ", pin: " << pin.toStdString();

    std::lock_guard<decltype(mutex_)> lock(mutex_);
    localDevice_->pairingConfirmation(address == pairingAddress_);
}

void LocalBluetoothDevice::onPairingFinished(const QBluetoothAddress &address, QBluetoothLocalDevice::Pairing pairing)
{
    OPENAUTO_LOG(debug) << "[LocalBluetoothDevice] onPairingDisplayPinCode, address: " << address.toString().toStdString()
                           << ", pin: " << pairing;

    std::lock_guard<decltype(mutex_)> lock(mutex_);

    if(address == pairingAddress_)
    {
        if(pairing != QBluetoothLocalDevice::Unpaired)
        {
            pairingPromise_->resolve();
        }
        else
        {
            pairingPromise_->reject();
        }

        pairingPromise_.reset();
        pairingAddress_ = QBluetoothAddress();
    }
}

void LocalBluetoothDevice::onError(QBluetoothLocalDevice::Error error)
{
    OPENAUTO_LOG(debug) << "[LocalBluetoothDevice] onError, error: " << error;

    std::lock_guard<decltype(mutex_)> lock(mutex_);

    if(pairingPromise_ != nullptr)
    {
        pairingPromise_->reject();
        pairingPromise_.reset();
        pairingAddress_ = QBluetoothAddress();
    }
}

void LocalBluetoothDevice::onHostModeStateChanged(QBluetoothLocalDevice::HostMode state)
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);

    if(state == QBluetoothLocalDevice::HostPoweredOff && pairingPromise_ != nullptr)
    {
        pairingPromise_->reject();
        pairingPromise_.reset();
        pairingAddress_ = QBluetoothAddress();
    }
}

void LocalBluetoothDevice::onClientConnected() {
    auto socket = rfcommServer_->nextPendingConnection();

    if(socket != nullptr)
    {
        OPENAUTO_LOG(info) << "[BluetoothServer] rfcomm client connected, peer name: " << socket->peerName().toStdString();
    }
    else
    {
        OPENAUTO_LOG(error) << "[BluetoothServer] received null socket during client connection.";
    }
}

}
}
}
}
