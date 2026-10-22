#include "peer_connection.h"
#include "file_manager.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <cstring>
#include <stdexcept>

// Helper to send all bytes (handles partial sends)
void sendAll(int sockfd, const protocol::Bytes& data) {
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        ssize_t sent = send(sockfd, data.data() + totalSent, data.size() - totalSent, 0);
        if (sent <= 0) {
            throw std::runtime_error("Failed to send data to socket.");
        }
        totalSent += sent;
    }
}

PeerConnection::PeerConnection(int socketFd, uint32_t remotePeerId, uint32_t myPeerId, FileManager& fileMgr)
    : sockfd(socketFd), myPeerId(myPeerId), remotePeerId(remotePeerId), 
      fileManager(fileMgr), decoder(1024 * 1024), isConnected(true) {} // 1MB max msg size

PeerConnection::PeerConnection(int socketFd, uint32_t myPeerId, FileManager& fileMgr)
    : sockfd(socketFd), myPeerId(myPeerId), remotePeerId(0), 
      fileManager(fileMgr), decoder(1024 * 1024), isConnected(true) {}

PeerConnection::~PeerConnection() {
    closeConnection();
}

void PeerConnection::sendHandshake() {
    protocol::Bytes handshake = protocol::encodeHandshake(myPeerId);
    sendAll(sockfd, handshake);
    std::cout << "[Network] Peer " << myPeerId << " sent handshake to Peer " << remotePeerId << std::endl;
}

void PeerConnection::sendMessage(const protocol::Message& msg) {
    protocol::Bytes encoded = protocol::encodeMessage(msg);
    sendAll(sockfd, encoded);
}

void PeerConnection::startReadLoop() {
    // Launch a dedicated thread for this peer to handle incoming TCP data
    readThread = std::thread(&PeerConnection::receiveLoop, this);
}

void PeerConnection::receiveLoop() {
    protocol::Bytes buffer(4096); // Read in 4KB chunks
    
    while (isConnected) {
        // Read raw bytes from the TCP socket
        ssize_t bytesRead = recv(sockfd, buffer.data(), buffer.size(), 0);
        
        if (bytesRead <= 0) {
            if (bytesRead == 0) {
                std::cout << "[Network] Peer " << remotePeerId << " closed connection." << std::endl;
            } else {
                std::cerr << "[Network] Error reading from socket for Peer " << remotePeerId << std::endl;
            }
            isConnected = false;
            break;
        }

        // Feed the raw bytes into the StreamDecoder
        // We only pass the exact number of bytes read
        protocol::Bytes chunk(buffer.begin(), buffer.begin() + bytesRead);
        
        try {
            decoder.append(chunk);
        } catch (const std::exception& e) {
            std::cerr << "[Network] StreamDecoder error: " << e.what() << std::endl;
            isConnected = false;
            break;
        }

        // Process any fully assembled messages
        while (decoder.hasMessage()) {
            protocol::Message msg = decoder.popMessage();
            handleMessage(msg);
        }
    }
}

void PeerConnection::handleMessage(const protocol::Message& msg) {
    // TODO: This is where the State Machine logic will eventually go
    // For now, we just log that we received it to prove the pipeline works
    std::cout << "[Message] Peer " << myPeerId << " received message type: " 
              << static_cast<int>(msg.type) << " from Peer " << remotePeerId << std::endl;
    
    // Ex: If we receive a Bitfield message, we could update our neighbor tracking here
    if (msg.type == protocol::MessageType::Bitfield) {
        // protocol::unpackBitfield(msg.payload, fileManager.getTotalPieces());
    }
}

void PeerConnection::closeConnection() {
    if (isConnected) {
        isConnected = false;
        if (readThread.joinable()) {
            readThread.join();
        }
        close(sockfd);
    }
}