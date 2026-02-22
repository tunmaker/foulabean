#include "RenodeEventDispatcher.h"

#include "defs.h"   // renode::ASYNC_EVENT, renode::read_u32_le

#include <sys/socket.h>
#include <QDebug>

// ASYNC_EVENT frame layout (bytes from the start of the frame):
//   byte  0    : return_code  = ASYNC_EVENT (6)
//   byte  1    : event_command (e.g. GPIO = 4)
//   bytes 2–5  : event_ed     (uint32_t LE) — event descriptor
//   bytes 6–9  : event_size   (uint32_t LE) — payload size in bytes
//   bytes 10+  : event_data   (event_size bytes)
static constexpr int kMinEventHeader = 10; // bytes before payload

RenodeEventDispatcher::RenodeEventDispatcher(int fd, EventHandler handler, QObject *parent)
    : QObject(parent)
    , m_fd(fd)
    , m_handler(std::move(handler))
{
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, &RenodeEventDispatcher::onSocketReadable);
}

void RenodeEventDispatcher::onSocketReadable()
{
    while (true) {
        // Peek at the minimum header without consuming bytes
        uint8_t header[kMinEventHeader];
        ssize_t peeked = recv(m_fd, header, sizeof(header), MSG_PEEK | MSG_DONTWAIT);
        if (peeked <= 0)
            break;

        // First byte must be ASYNC_EVENT; if not, leave for recv_response()
        if (header[0] != static_cast<uint8_t>(renode::ASYNC_EVENT))
            break;

        // Need the full 10-byte header to know the payload size
        if (peeked < kMinEventHeader)
            break;

        uint32_t event_size = renode::read_u32_le(header + 6);
        size_t   total_size = static_cast<size_t>(kMinEventHeader) + event_size;

        // Peek the complete frame to confirm it has arrived fully
        std::vector<uint8_t> frame(total_size);
        ssize_t available = recv(m_fd, frame.data(), total_size, MSG_PEEK | MSG_DONTWAIT);
        if (available < static_cast<ssize_t>(total_size))
            break; // partial frame — leave in socket buffer, wait for next activation

        // Consume the complete frame
        ssize_t consumed = recv(m_fd, frame.data(), total_size, MSG_DONTWAIT);
        if (consumed < static_cast<ssize_t>(total_size))
            break; // shouldn't happen, but bail safely

        uint32_t event_ed        = renode::read_u32_le(frame.data() + 2);
        const uint8_t *event_data = frame.data() + kMinEventHeader;

        qDebug("[EventDispatcher] ASYNC_EVENT ed=%u size=%u", event_ed, event_size);

        if (m_handler)
            m_handler(event_ed, event_data, event_size);
    }
}
