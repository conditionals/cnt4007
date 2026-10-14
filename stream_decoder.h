#ifndef STREAM_DECODER_H
#define STREAM_DECODER_H

#include "protocol.h"

#include <deque>

namespace protocol {
    class StreamDecoder {
    public:
        explicit StreamDecoder(std::size_t maxMessageLength,
                               std::size_t maxQueuedMessages = 64);

        void append(const Bytes& data);
        bool hasHandshake() const;
        std::uint32_t getPeerId() const;
        bool hasMessage() const;
        Message popMessage();
        void finish();

    private:
        std::size_t maxMessageLength;
        std::size_t maxQueuedMessages;
        Bytes pending;
        std::deque<Message> messages;
        std::size_t targetSize = 32;
        std::uint32_t peerId = 0;
        bool failed = false;
        bool closed = false;
    };
}

#endif
