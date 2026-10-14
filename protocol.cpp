#include "protocol.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>

namespace protocol {
namespace {
const std::string handshakeHeader = "P2PFILESHARINGPROJ";

// Use network byte order for every four-byte integer.
void appendInt(Bytes& data, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) {
        data.push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

std::uint32_t readInt(const Bytes& data, std::size_t offset) {
    if (offset > data.size() || data.size() - offset < 4) {
        throw std::invalid_argument("Missing four-byte integer");
    }

    std::uint32_t value = 0;
    for (std::size_t i = offset; i < offset + 4; ++i) {
        value = (value << 8) | data[i];
    }

    return value;
}

void validateMessage(const Message& message) {
    switch (message.type) {
        case MessageType::Choke:
        case MessageType::Unchoke:
        case MessageType::Interested:
        case MessageType::NotInterested:
            if (!message.payload.empty()) {
                throw std::invalid_argument("Control message has a payload");
            }

            break;

        case MessageType::Have:
        case MessageType::Request:
            if (message.payload.size() != 4) {
                throw std::invalid_argument("Index message needs four bytes");
            }

            break;

        case MessageType::Piece:
            if (message.payload.size() <= 4) {
                throw std::invalid_argument("Piece message needs an index and content");
            }

            break;

        case MessageType::Bitfield:
            break;

        default:
            throw std::invalid_argument("Unknown message type");
    }
}
} // namespace

Bytes encodeHandshake(std::uint32_t peerId) {
    if (peerId == 0) {
        throw std::invalid_argument("Peer ID must be positive");
    }

    Bytes data(handshakeHeader.begin(), handshakeHeader.end());
    data.resize(28, 0);
    appendInt(data, peerId);

    return data;
}

std::uint32_t decodeHandshake(const Bytes& data) {
    if (data.size() != 32 ||
        !std::equal(handshakeHeader.begin(), handshakeHeader.end(), data.begin())) {
        throw std::invalid_argument("Invalid handshake header or size");
    }

    for (std::size_t i = 18; i < 28; ++i) {
        if (data[i] != 0) {
            throw std::invalid_argument("Handshake reserved bytes must be zero");
        }
    }

    auto peerId = readInt(data, 28);
    if (peerId == 0) {
        throw std::invalid_argument("Peer ID must be positive");
    }

    return peerId;
}

Bytes encodeMessage(const Message& message) {
    validateMessage(message);
    if (message.payload.size() > std::numeric_limits<std::uint32_t>::max() - 1) {
        throw std::invalid_argument("Message exceeds four-byte length limit");
    }

    Bytes data;
    data.reserve(5 + message.payload.size());
    appendInt(data, static_cast<std::uint32_t>(message.payload.size() + 1));
    data.push_back(static_cast<std::uint8_t>(message.type));
    data.insert(data.end(), message.payload.begin(), message.payload.end());

    return data;
}

// The caller must assemble exactly one complete frame from the TCP stream.
Message decodeMessage(const Bytes& data) {
    if (data.size() < 5 || readInt(data, 0) != data.size() - 4) {
        throw std::invalid_argument("Message length does not match frame size");
    }

    Message message{static_cast<MessageType>(data[4]), Bytes(data.begin() + 5, data.end())};
    validateMessage(message);

    return message;
}

Message makeIndexMessage(MessageType type, std::uint32_t pieceIndex) {
    if (type != MessageType::Have && type != MessageType::Request) {
        throw std::invalid_argument("Expected have or request message type");
    }

    Message message{type, {}};
    appendInt(message.payload, pieceIndex);

    return message;
}

Message makePieceMessage(std::uint32_t pieceIndex, const Bytes& content) {
    Message message{MessageType::Piece, {}};
    appendInt(message.payload, pieceIndex);
    message.payload.insert(message.payload.end(), content.begin(), content.end());
    validateMessage(message);

    return message;
}

std::uint32_t getPieceIndex(const Message& message) {
    if (message.type != MessageType::Have && message.type != MessageType::Request &&
        message.type != MessageType::Piece) {
        throw std::invalid_argument("Message does not contain a piece index");
    }

    validateMessage(message);

    return readInt(message.payload, 0);
}

Bytes packBitfield(const std::vector<bool>& pieces) {
    Bytes data(pieces.size() / 8 + (pieces.size() % 8 != 0), 0);
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        if (pieces[i]) {
            data[i / 8] |= static_cast<std::uint8_t>(1u << (7 - i % 8));
        }
    }

    return data;
}

std::vector<bool> unpackBitfield(const Bytes& data, std::size_t numPieces) {
    if (data.size() != numPieces / 8 + (numPieces % 8 != 0)) {
        throw std::invalid_argument("Bitfield size does not match piece count");
    }

    auto usedBits = numPieces % 8;
    if (usedBits != 0 && (data.back() & ((1u << (8 - usedBits)) - 1)) != 0) {
        throw std::invalid_argument("Bitfield spare bits must be zero");
    }

    std::vector<bool> pieces(numPieces, false);
    for (std::size_t i = 0; i < numPieces; ++i) {
        pieces[i] = (data[i / 8] & (1u << (7 - i % 8))) != 0;
    }

    return pieces;
}
} // namespace protocol
