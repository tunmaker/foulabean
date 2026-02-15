#include "AdcModel.h"

#include <cmath>

AdcModel::AdcModel(QObject *parent)
    : QAbstractListModel(parent) {}

int AdcModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_channels.size();
}

QVariant AdcModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_channels.size())
        return {};

    const auto &entry = m_channels[index.row()];
    switch (role) {
    case ChannelNumberRole: return entry.channel;
    case ValueRole:         return entry.value;
    default:                return {};
    }
}

QHash<int, QByteArray> AdcModel::roleNames() const {
    return {
        {ChannelNumberRole, "channelNumber"},
        {ValueRole, "value"},
    };
}

int AdcModel::channelCount() const {
    return m_channels.size();
}

void AdcModel::resetChannels(int count, const QVector<AdcChannelData> &channels) {
    Q_UNUSED(count)
    beginResetModel();
    m_channels.clear();
    m_channels.reserve(channels.size());
    for (const auto &ch : channels) {
        m_channels.append({ch.channel, ch.value});
    }
    endResetModel();
    emit channelCountChanged();
}

void AdcModel::updateChannel(int channel, double newValue) {
    int row = findChannelRow(channel);
    if (row < 0) return;
    if (std::abs(m_channels[row].value - newValue) < 1e-9) return;
    m_channels[row].value = newValue;
    QModelIndex idx = index(row);
    emit dataChanged(idx, idx, {ValueRole});
}

int AdcModel::findChannelRow(int channel) const {
    for (int i = 0; i < m_channels.size(); ++i) {
        if (m_channels[i].channel == channel) return i;
    }
    return -1;
}
