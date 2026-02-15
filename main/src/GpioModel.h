#pragma once

#include <QAbstractListModel>
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
    };

    explicit GpioModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int pinCount() const;

    void resetPins(const QVector<GpioPinData> &pins);
    void updatePin(int pin, int newState);

signals:
    void pinCountChanged();

private:
    struct PinEntry {
        int pin = 0;
        int state = 0;
    };
    QVector<PinEntry> m_pins;

    static QString stateToString(int state);
    int findPinRow(int pin) const;
};
