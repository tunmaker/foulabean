#pragma once

#include <QAbstractListModel>
#include <QVector>
#include <QtQml/qqmlregistration.h>

#include "BridgeTypes.h"

class AdcModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by SimulationController")

    Q_PROPERTY(int channelCount READ channelCount NOTIFY channelCountChanged FINAL)

public:
    enum Roles {
        ChannelNumberRole = Qt::UserRole + 1,
        ValueRole,
    };

    explicit AdcModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int channelCount() const;

    void resetChannels(int count, const QVector<AdcChannelData> &channels);
    void updateChannel(int channel, double newValue);

signals:
    void channelCountChanged();

private:
    struct ChannelEntry {
        int channel = 0;
        double value = 0.0;
    };
    QVector<ChannelEntry> m_channels;

    int findChannelRow(int channel) const;
};
