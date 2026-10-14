#include "stream_decoder.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace protocol {
StreamDecoder::StreamDecoder(std::size_t maxMessageLength,
                             std::size_t maxQueuedMessages)
    : maxMessageLength(maxMessageLength), maxQueuedMessages(maxQueuedMessages) {
    if (maxMessageLength == 0 ||
        maxMessageLength > std::numeric_limits<std::uint32_t>::max() ||
        maxMessageLength > std::numeric_limits<std::size_t>::max() - 4) {
        throw std::invalid_argument("Invalid maximum message length");
    }

    if (maxQueuedMessages == 0) {
        throw std::invalid_argument("Message queue limit must be positive");
    }
}

void StreamDecoder::append(const Bytes& data) {
    if (failed || closed) {
        throw std::logic_error("Cannot append to a failed or closed stream");
    }

    std::size_t offset = 0;
    try {
        while (offset < data.size()) {
            auto count = std::min(targetSize - pending.size(), data.size() - offset);
            pending.insert(pending.end(), data.begin() + offset,
                           data.begin() + offset + count);
            offset += count;

            if (pending.size() != targetSize) {
                continue;
            }

            if (!hasHandshake()) {
                peerId = decodeHandshake(pending);
                pending.clear();
                targetSize = 4;
                continue;
            }

            if (targetSize == 4) {
                std::uint32_t length = 0;
                for (auto byte : pending) {
                    length = (length << 8) | byte;
                }

                if (length == 0) {
                    throw std::invalid_argument("Message length must include a type");
                }

                if (length > maxMessageLength) {
                    throw std::length_error("Message exceeds stream length limit");
                }

                targetSize = 4 + static_cast<std::size_t>(length);
                continue;
            }

            if (messages.size() == maxQueuedMessages) {
                throw std::length_error("Stream message queue is full");
            }

            messages.push_back(decodeMessage(pending));
            pending.clear();
            targetSize = 4;
        }
    } catch (...) {
        failed = true;
        throw;
    }
}

bool StreamDecoder::hasHandshake() const {
    return peerId != 0;
}

std::uint32_t StreamDecoder::getPeerId() const {
    if (!hasHandshake()) {
        throw std::logic_error("Handshake has not been received");
    }

    return peerId;
}

bool StreamDecoder::hasMessage() const {
    return !messages.empty();
}

Message StreamDecoder::popMessage() {
    if (!hasMessage()) {
        throw std::logic_error("No complete message is available");
    }

    auto message = std::move(messages.front());
    messages.pop_front();

    return message;
}

void StreamDecoder::finish() {
    if (failed) {
        throw std::logic_error("Cannot finish a failed stream");
    }

    if (closed) {
        return;
    }

    if (!hasHandshake()) {
        failed = true;
        throw std::invalid_argument("Stream ended before a complete handshake");
    }

    if (!pending.empty()) {
        failed = true;
        throw std::invalid_argument("Stream ended during a message");
    }

    closed = true;
}
}
