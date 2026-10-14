#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace protocol {
    using Bytes = std::vector<std::uint8_t>;

    enum class MessageType : std::uint8_t {
        Choke = 0,
        Unchoke = 1,
        Interested = 2,
        NotInterested = 3,
        Have = 4,
        Bitfield = 5,
        Request = 6,
        Piece = 7
    };

    struct Message {
        MessageType type;
        Bytes payload;
    };

    Bytes encodeHandshake(std::uint32_t peerId);
    std::uint32_t decodeHandshake(const Bytes& data);
    Bytes encodeMessage(const Message& message);

    Message decodeMessage(const Bytes& data);
    Message makeIndexMessage(MessageType type, std::uint32_t pieceIndex);
    Message makePieceMessage(std::uint32_t pieceIndex, const Bytes& content);

    std::uint32_t getPieceIndex(const Message& message);
    Bytes packBitfield(const std::vector<bool>& pieces);
    std::vector<bool> unpackBitfield(const Bytes& data, std::size_t numPieces);

}

#endif
