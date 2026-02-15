#include "GpioModel.h"

GpioModel::GpioModel(QObject *parent)
    : QAbstractListModel(parent) {}

int GpioModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_pins.size();
}

QVariant GpioModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_pins.size())
        return {};

    const auto &entry = m_pins[index.row()];
    switch (role) {
    case PinNumberRole:  return entry.pin;
    case StateRole:      return entry.state;
    case StateNameRole:  return stateToString(entry.state);
    default:             return {};
    }
}

QHash<int, QByteArray> GpioModel::roleNames() const {
    return {
        {PinNumberRole, "pinNumber"},
        {StateRole, "state"},
        {StateNameRole, "stateName"},
    };
}

int GpioModel::pinCount() const {
    return m_pins.size();
}

void GpioModel::resetPins(const QVector<GpioPinData> &pins) {
    beginResetModel();
    m_pins.clear();
    m_pins.reserve(pins.size());
    for (const auto &p : pins) {
        m_pins.append({p.pin, p.state});
    }
    endResetModel();
    emit pinCountChanged();
}

void GpioModel::updatePin(int pin, int newState) {
    int row = findPinRow(pin);
    if (row < 0) return;
    if (m_pins[row].state == newState) return;
    m_pins[row].state = newState;
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {StateRole, StateNameRole});
}

QString GpioModel::stateToString(int state) {
    switch (state) {
    case 0:  return QStringLiteral("Low");
    case 1:  return QStringLiteral("High");
    case 2:  return QStringLiteral("HighZ");
    default: return QStringLiteral("Unknown");
    }
}

int GpioModel::findPinRow(int pin) const {
    for (int i = 0; i < m_pins.size(); ++i) {
        if (m_pins[i].pin == pin) return i;
    }
    return -1;
}
