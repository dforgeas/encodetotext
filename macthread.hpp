#pragma once
#include "queue.hpp"
#include "crypto.hpp"

#include <thread>
#include <array>
#include <memory>
#include <cstdint>

// First element of the block is its length
typedef std::array<std::uint32_t, 1 + (CbcMac::stateSize << 10)> Block;
typedef std::shared_ptr<Block> BlockPtr;

void mac_process_buffer(const Block &block, CbcMac &mac);

class MacThread
{
public:
    MacThread(const CbcMac &cbcMac);
    
    Queue<BlockPtr> &queue()
    {
        return queue_;
    }

    // join the thread and return the mac
    const CbcMac &finish();

private:
    Queue<BlockPtr> queue_;
    CbcMac cbcMac;
    std::thread thread;

    void process();
};