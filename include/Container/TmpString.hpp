#ifndef hpp_TmpString_hpp
#define hpp_TmpString_hpp

// We need read-only strings
#include "Strings/ROString.hpp"
// We need the ring buffer declaration too
#include "RingBuffer.hpp"


namespace Container
{
    // The maximum array of strings to persisting in the vault
    template <std::size_t N>  using MaxPersistStringArrayT = std::array<ROString *, N>;
    typedef MaxPersistStringArrayT<16> MaxPersistStringArray;

    /** A TmpString is a read-only string whose storage is dynamically allocated from a RingBuffer.
        Deallocation isn't managed by the class itself, but by the ring buffer instance.
        It's typically used to avoid heap allocation and memory fragmentation.
        Typical example is for a network "client" that need to persist its current state
        while it's receiving a request (and flushing its network's buffers).
        The client owns the ring buffer and release all of the temporary allocations at once in a single move */
    template <std::size_t N>
    static bool persistString(ROString & stringToPersist, Container::TranscientVault<N> & buffer, std::size_t futureDrop = 0)
    {
        const char * t = buffer.transferStringToVault(stringToPersist.getData(), stringToPersist.getLength(), futureDrop);
        if (!t) return false;
        ROString tmp(t, stringToPersist.getLength());
        stringToPersist.swapWith(tmp);
        return true;
    }
    template <std::size_t N>
    static bool persistStrings(MaxPersistStringArray & stringsToPersist, Container::TranscientVault<N> & buffer, std::size_t futureDrop = 0)
    {
        // Need to compute the total size required for the stack buffer
        std::size_t accLen = 0; std::size_t i = 0;
        for (; i < stringsToPersist.size(); i++)
        {
            if (stringsToPersist[i]) accLen += stringsToPersist[i]->getLength();
            else break; // Stop iterating anyway
        }
        // Save the string to the stack before being erased (in reverse order)
        char * tmp = (char*)alloca(accLen);
        accLen = 0;
        for (std::size_t j = i - 1; j; j--)
        {
            memcpy(&tmp[accLen], stringsToPersist[j]->getData(), stringsToPersist[j]->getLength());
            accLen += stringsToPersist[j]->getLength();
        }

        buffer.drop(futureDrop);
        if (!buffer.saveInVault((const uint8*)tmp, accLen)) return false;
        const char * t = (const char*)buffer.getVaultHead();

        // Swap now the string to the vault area
        accLen = 0;
        for (std::size_t j = i - 1; j; j--)
        {
            ROString x(&t[accLen], stringsToPersist[j]->getLength());
            accLen += stringsToPersist[j]->getLength();
            stringsToPersist[j]->swapWith(x);
        }
        return true;
    }


    /** A basic size limited buffer with content tracking */
    struct TrackedBuffer
    {
        TrackedBuffer(uint8 * buffer, std::size_t N) : buffer((char*)buffer), used(0), N(N) {}
        bool save(const char * data, const std::size_t length) { if (used + length > N) return false; memcpy(&buffer[used], data, length); used += length; return true; }
        bool canFit(const std::size_t len) { return used + len <= N; }
        char * buffer;
        std::size_t used, N;
    };
}

#endif
