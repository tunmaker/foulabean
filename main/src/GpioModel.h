#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <QtQml/qqmlregistration.h>

#include "BridgeTypes.h"

class GpioModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by SimulationController")

    Q_PROPERTY(int pinCount READ pinCount NOTIFY pinCountChanged FINAL)

public:
    enum Roles {
        PinNumberRole = Qt::UserRole + 1,
        StateRole,
        StateNameRole,
        PortNameRole,
    };

    explicit GpioModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int pinCount() const;

    // Clear all pins (used on disconnect)
    void resetPins(const QVector<GpioPinData> &pins);

    // Replace all pins for a given port without disturbing other ports
    void setPortPins(int portIndex, const QString &portName, const QVector<GpioPinData> &pins);

    // Update a single pin's state within a port (no model reset, O(n) search)
    void updatePortPin(int portIndex, int pin, int newState);

signals:
    void pinCountChanged();

private:
    struct PinEntry {
        int portIndex = 0;
        QString portName;
        int pin = 0;
        int state = 0;
    };
    QVector<PinEntry> m_pins;

    static QString stateToString(int state);
    int findPortPinRow(int portIndex, int pin) const;
};
