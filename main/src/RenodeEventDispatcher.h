#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <functional>
#include <cstdint>
#include <vector>

// Watches the Renode external-control socket fd for incoming ASYNC_EVENT frames
// and dispatches them to the registered EventCallbackRegistry while the worker
// is idle (between command slots). Uses MSG_PEEK so incomplete frames are left
// intact in the socket buffer for recv_response() to handle during commands.
class RenodeEventDispatcher : public QObject {
    Q_OBJECT

public:
    using EventHandler = std::function<void(uint32_t ed, const uint8_t *data, size_t size)>;

    explicit RenodeEventDispatcher(int fd, EventHandler handler, QObject *parent = nullptr);
    ~RenodeEventDispatcher() override = default;

private slots:
    void onSocketReadable();

private:
    int m_fd;
    QSocketNotifier *m_notifier = nullptr;
    EventHandler m_handler;
};
