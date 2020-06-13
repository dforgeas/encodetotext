#include "macthread.hpp"
#include "crypto.hpp"

void mac_process_buffer(const Block &block, CbcMac &mac)
{
   const int size = block.front();
   const uint32_t *const native_buffer = block.data() + 1;
   int i; // need signed for the comparison below
   for (i = 0; i < size - CbcMac::stateSize; i += CbcMac::stateSize)
   {
      mac.update(reinterpret_cast<uint32_t const (&)[CbcMac::stateSize]>(native_buffer[i]));
   }

   // update last (potentially partial) block
   uint32_t mac_buffer[CbcMac::stateSize] = {};
   for (auto p = mac_buffer; i < size; ++i)
   {
      *p++ = native_buffer[i];
   }
   mac.update(mac_buffer);
}

MacThread::MacThread(const CbcMac &cbcMac)
    : queue_(50) // each block is 20KB so that's about 1MB
    , cbcMac(cbcMac)
    , thread([this](){ process(); })
{
}

void MacThread::process()
{
    for ( ; ; )
    {
        const BlockPtr blockPtr(queue_.pop());
        if (blockPtr)
            mac_process_buffer(*blockPtr, cbcMac);
        else
            break; // null pointer means end of queue
    }
}

const CbcMac &MacThread::finish()
{
    thread.join();
    return cbcMac;
}