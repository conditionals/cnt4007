#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "protocol.h"
#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

// Holds the configuration parsed from Common.cfg
struct CommonConfig {
    int numPreferredNeighbors;
    int unchokingInterval;
    int optimisticUnchokingInterval;
    std::string fileName;
    uint64_t fileSize;
    uint32_t pieceSize;
    uint32_t totalPieces;
};

// Holds information about a peer parsed from PeerInfo.cfg
struct PeerInfo {
    uint32_t peerId;
    std::string hostname;
    int port;
    bool hasFile;
};

class FileManager {
public:
    // Initialize the manager, create directories, and load the file if we are a seeder
    FileManager(uint32_t myPeerId, const CommonConfig& config, bool iHaveFile);

    // Get the local bitfield to send in the initial Bitfield message
    protocol::Bytes getLocalBitfieldBytes() const;
    
    // Get a specific piece from disk to send to a neighbor
    protocol::Bytes getPieceData(uint32_t pieceIndex) const;

    // Save a received piece to disk and update the local bitfield
    bool savePiece(uint32_t pieceIndex, const protocol::Bytes& pieceData);

    // Check if we have completed the download
    bool hasCompleteFile() const;

    // Getters for config
    uint32_t getTotalPieces() const { return config.totalPieces; }
    uint32_t getPieceSize() const { return config.pieceSize; }

private:
    uint32_t myPeerId;
    CommonConfig config;
    std::string peerDir; // eg "peer_1001/"
    
    // Tracks which pieces we currently have
    std::vector<bool> localBitfield; 
    
    // Mutex to protect file writes and bitfield updates from multiple peer threads
    std::mutex stateMutex; 

    void loadInitialFile();
    void createPeerDirectory();
};

#endif