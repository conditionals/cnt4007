#include "file_manager.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstring>
#include <stdexcept>

FileManager::FileManager(uint32_t myPeerId, const CommonConfig& config, bool iHaveFile)
    : myPeerId(myPeerId), config(config) {
    
    this->peerDir = "peer_" + std::to_string(myPeerId) + "/";
    createPeerDirectory();

    // Initialize bitfield to all false (0)
    localBitfield.assign(config.totalPieces, false);

    if (iHaveFile) {
        loadInitialFile();
    }
}

void FileManager::createPeerDirectory() {
    // Create the peer_[ID] directory if it doesn't exist
    mkdir(peerDir.c_str(), 0755);
}

void FileManager::loadInitialFile() {
    // If we are a seeder, we read the file, verify it, and set all bits to 1
    std::ifstream inFile(config.fileName, std::ios::binary);
    if (!inFile) {
        throw std::runtime_error("Seeder cannot find the file: " + config.fileName);
    }

    // We don't necessarily need to keep the whole file in RAM
    // We just mark our bitfield as complete; the getPieceData() method will read from disk on demand
    for (uint32_t i = 0; i < config.totalPieces; ++i) {
        localBitfield[i] = true;
    }
    
    std::cout << "[FileManager] Peer " << myPeerId << " loaded complete file. Total pieces: " 
              << config.totalPieces << std::endl;
}

protocol::Bytes FileManager::getLocalBitfieldBytes() const {
    return protocol::packBitfield(localBitfield);
}

protocol::Bytes FileManager::getPieceData(uint32_t pieceIndex) const {
    if (pieceIndex >= config.totalPieces || !localBitfield[pieceIndex]) {
        throw std::invalid_argument("Requested piece not available locally.");
    }

    std::ifstream inFile(config.fileName, std::ios::binary);
    if (!inFile) throw std::runtime_error("Cannot open file for reading piece.");

    // Calculate offset and size for this specific piece
    uint64_t offset = static_cast<uint64_t>(pieceIndex) * config.pieceSize;
    uint32_t sizeToRead = config.pieceSize;
    
    // Handle the last piece which might be smaller
    if (pieceIndex == config.totalPieces - 1) {
        sizeToRead = config.fileSize - offset;
    }

    protocol::Bytes data(sizeToRead);
    inFile.seekg(offset);
    inFile.read(reinterpret_cast<char*>(data.data()), sizeToRead);

    return data;
}

bool FileManager::savePiece(uint32_t pieceIndex, const protocol::Bytes& pieceData) {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    if (localBitfield[pieceIndex]) {
        return false; // Already have it
    }

    // Open the peer's specific file in append/binary mode or seek to write
    std::string filePath = peerDir + config.fileName;
    std::fstream outFile(filePath, std::ios::in | std::ios::out | std::ios::binary);
    
    // If file doesn't exist yet, create it
    if (!outFile) {
        outFile.open(filePath, std::ios::out | std::ios::binary);
        // Pre-allocate file size if needed, or just let it grow
        outFile.close();
        outFile.open(filePath, std::ios::in | std::ios::out | std::ios::binary);
    }

    uint64_t offset = static_cast<uint64_t>(pieceIndex) * config.pieceSize;
    outFile.seekp(offset);
    outFile.write(reinterpret_cast<const char*>(pieceData.data()), pieceData.size());

    // Update local state
    localBitfield[pieceIndex] = true;
    
    std::cout << "[FileManager] Peer " << myPeerId << " saved piece " << pieceIndex 
              << ". Progress: " << pieceIndex + 1 << "/" << config.totalPieces << std::endl;

    return true;
}

bool FileManager::hasCompleteFile() const {
    for (bool has : localBitfield) {
        if (!has) return false;
    }
    return true;
}