#ifndef PEER_CONNECTION_H
#define PEER_CONNECTION_H

#include "protocol.h"
#include "stream_decoder.h"
#include <functional>
#include <thread>
#include <atomic>
#include <cstdint>

// Forward declaration
class FileManager;

class PeerConnection {
public:
    // Constructor for an outgoing connection (Client)
    PeerConnection(int socketFd, uint32_t remotePeerId, uint32_t myPeerId, FileManager& fileMgr);
    
    // Constructor for an incoming connection (Server)
    // remotePeerId is unknown until handshake is received
    PeerConnection(int socketFd, uint32_t myPeerId, FileManager& fileMgr);

    ~PeerConnection();

    // Starts the background thread to read from the socket
    void startReadLoop();

    // Sends a handshake to the remote peer
    void sendHandshake();

    // Sends a protocol message to the remote peer
    void sendMessage(const protocol::Message& msg);

    void closeConnection();

    uint32_t getRemotePeerId() const { return remotePeerId; }

private:
    int sockfd;
    uint32_t myPeerId;
    uint32_t remotePeerId;
    FileManager& fileManager;
    
    protocol::StreamDecoder decoder;
    std::thread readThread;
    std::atomic<bool> isConnected;

    // The main loop that reads bytes from TCP and feeds them to the StreamDecoder
    void receiveLoop();
    
    // Processes a fully decoded message
    void handleMessage(const protocol::Message& msg);
};

#endif