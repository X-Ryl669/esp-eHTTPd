#ifndef hpp_RingBuffer_hpp
#define hpp_RingBuffer_hpp

// We need types
#include "Types.hpp"

namespace Container
{
    /** A Power-Of-2 ring buffer (aka Circular Buffer). */
    template <std::size_t sizePowerOf2>
    struct RingBuffer
    {
        /** Read and write pointer in the ring buffer */
        uint32                          r, w;
        uint32                          lastLogPos = sizePowerOf2;
        /** Buffer size minus 1 in bytes */
        static constexpr const uint32   sm1 = sizePowerOf2 - 1;
        /** The buffer to write packets into */
        uint8                           buffer[sizePowerOf2];

        /** Get the consumed size in the buffer */
        inline uint32 getSize() const { return w >= r ? w - r : (sm1 - r + w + 1); }
        /** Get the available size in the buffer */
        inline uint32 freeSize() const { return sm1 - getSize(); }
        /** Fetch the current read position (used to restore the read pointer later on on rollback) */
        inline uint32 fetchReadPos() const { return r; }
        /** Fetch the current read position (used to restore the read pointer later on on rollback) */
        inline uint32 fetchWritePos() const { return w; }
        /** Rollback with the saved read position */
        inline void rollback(const uint32 readPos) { if (readPos >= sm1) return; r = readPos; }
        /** Rollback the saved write position */
        inline void rollbackWrite(const uint32 writePos) { if (writePos >= sm1) return; w = writePos; }
        /** Check if the buffer is full (and, if configured to do so, clean the buffer until there's enough free space) */
        bool canFit(const uint32 size)
        {
            if (size > sm1) return false; // Can't store the data anyway
            if (freeSize() >= size) return true;
            // Clean the logs recursively until we have enough space
            while (freeSize() < size)
                // The idea, here is to clean item one by one until there's enough space for the new item.
                if (!extract()) return false;

            return true;
        }

        /** Add data to this buffer (no allocation is done at this time) */
        bool save(const uint8 * packet, uint32 size)
        {
            // Check we can fit the packet
            if (!canFit(size)) return false;

            const uint32 part1 = std::min(size, sm1 - w + 1);
            const uint32 part2 = size - part1;

            memcpy((buffer + w), packet, part1);
            memcpy((buffer), packet + part1, part2);

            w = (w + size) & sm1;
            return true;
        }
        /** Extract data from the buffer.
            We can skip one copy to rebuilt a contiguous packet by simply returning
            the two part in the ring buffer and let the application send them successively.
            From the receiving side, they'll be received as one contiguous packet. */
        bool load(uint32 size, const uint8 *& packetHead, uint32 & sizeHead, const uint8 *& packetTail,  uint32 & sizeTail)
        {
            if (getSize() < size) return false;
            packetHead = buffer + r;
            sizeHead = std::min(size, (sm1 - r + 1));
            sizeTail = size - sizeHead;
            packetTail = buffer;
            r = (r + size) & sm1;
            return true;
        }

        /** Peek a byte from the buffer. Doesn't advance the read pointer */
        bool peek(uint8 & x)
        {
            if (!getSize()) return false;
            x = buffer[r];
            return true;
        }

        /** The generic save function for the logs */
        template <typename T>
        bool saveType(const T val)
        {
            return save((const uint8*)&val, sizeof(val));
        }
        /** Save a string to the buffer
            @param str  A pointer on the C string to save
            @param len  If non zero, contains the actual number of bytes to save (don't include the final NUL)
                        Else, compute the string length from the actual string size. */
        bool saveString(const char * str, std::size_t len = 0)
        {
            if (!len) len = strlen(str);
            uint8 c = 0;
            return save((const uint8*)str, len) && save(&c, 1);
        }
        /** Save a pointer without VLC */
        bool save(const void * ptr)
        {
            uintptr_t p = (uintptr_t)(ptr);
            return save((const uint8*)&p, sizeof(p));
        }
        bool save(const char * ptr)
        {
            static_assert(sizeof(ptr) == sizeof(void*), "You shouldn't use this to store a string, instead use saveString");
            return false;
        }
        /** Save a float or a double */
        bool save(const double i)       { return saveType(i); }
        /** Save a long double */
        bool save(const long double i)  { return saveType(i); }

        /** Generic load a value from the buffer */
        template<typename T>
        bool loadType(T & val)
        {
            const uint8 * head = 0, * tail = 0;
            uint32 sh = 0, st = 0;
            if (!load(sizeof(val), head, sh, tail, st)) return false;
            memcpy((uint8*)&val, head, sh);
            memcpy((uint8*)&val + sh, tail, st);
            return true;
        }
        /** Load a string from the buffer
            @param str  If not null, will copy the C string into (including the terminating NUL byte)
            @param len  On output, will be filled with the number of bytes required to load this string, including the terminating NUL byte */
        bool loadString(char * str, std::size_t & len)
        {
            // Find length first
            for (len = 0; ((r + len) & sm1) != w; len++)
                if (buffer[(r+len) & sm1] == 0) break;

            if (((len + r) & sm1) == w) return false;
            len++; // Account for NUL byte
            if (!str) return true;

            for (std::size_t l = 0; l < len; l++)
                str[l] = buffer[(r + l) & sm1];

            r = (r + len) & sm1;
            return true;
        }
        /** Load a pointer without VLC decoding */
        bool load(const void *& ptr)
        {
            uintptr_t p = {};
            if (!loadType(p)) return false;
            ptr = (const void*)p;
            return true;
        }
        bool load(const char * ptr)
        {
            static_assert(sizeof(ptr) == sizeof(void*), "You shouldn't use this to load a string, instead use loadString");
            return false;
        }

        bool load(double & i) { return loadType(i); }
        bool load(long double & i) { return loadType(i); }

        /** Check if the data at position readPos match exactly the given value.
            @return true if it does and increase readPos by the required size in that case for the next value type.
                            else it returns false and doesn't modify readPos */
        bool matchValue(uint32 & readPos, const auto & val)
        {
            uint32 rSave = r;
            r = readPos;
            auto tmp = val;
            if (!load(tmp) || tmp != val) { r = rSave; return false; }
            readPos = r;
            r = rSave;
            return true;
        }
        /** Check if the data at position readPos match exactly the given value.
            @return true if it does and increase readPos by the required size in that case for the next value type.
                            else it returns false and doesn't modify readPos */
        bool matchValue(uint32 & readPos, void * val)
        {
            uint32 rSave = r;
            r = readPos;
            const void * tmp = 0;
            if (!load(tmp) || tmp != val) { r = rSave; return false; }
            readPos = r;
            r = rSave;
            return true;
        }
        /** Check if the data at position readPos match exactly the given value.
            @return true if it does and increase readPos by the required size in that case for the next value type.
                            else it returns false and doesn't modify readPos */
        bool matchValue(uint32 & readPos, const char * val, const std::size_t & len)
        {
            uint32 rSave = r;
            r = readPos;
            std::size_t expLen = 0;
            if (!loadString(nullptr, expLen) || expLen != (len+1)) { r = rSave; return false; }
            // Then compare the string itself to match, ignoring the last 0 sentinel that might not be present in the string itself
            if (memcmp(val, &buffer[r], std::min((uint32)len - 1, sm1 - r + 1))) { r = rSave; return false; }
            if (len - 1 > (sm1 - r + 1) && memcmp(&val[sm1 - r + 1], buffer, len - 1 - (sm1 - r + 1))) { r = rSave; return false; }
            readPos = (r + len + 1) & sm1;
            r = rSave;
            return true;
        }

        /** Generic load a value from the buffer */
        template<typename T>
        bool loadTypeAt(const uint32 pos, T & val)
        {
            const uint8 * head = 0, * tail = 0;
            uint32 sh = 0, st = 0;
            uint32 rSave = r; r = pos & sm1;
            if (!load(sizeof(val), head, sh, tail, st)) { r = rSave; return false; }
            r = rSave;
            memcpy((uint8*)&val, head, sh);
            memcpy((uint8*)&val + sh, tail, st);
            return true;
        }

        /** The generic save function for the logs */
        template <typename T>
        bool saveTypeAt(const uint32 pos, const T val)
        {
            uint32 wSave = w; w = pos & sm1;
            bool ret = save((const uint8*)&val, sizeof(val));
            w = wSave;
            return ret;
        }

        bool duplicateData(const uint32 from, const uint32 to)
        {
            uint32 wSave = w;
            if (!save(&buffer[from], std::min((uint32)to - from, sm1 - from))) { w = wSave; return false; }
            if (to < from && !save(buffer, to)) { w = wSave; return false; }
            return true;
        }

        inline bool consume(const uint32 s) { if (getSize() <= s) return false; r = (r + s) & sm1; return true; }

        // Not implemented for non typed items. This need more work
        inline bool extract() { return false; }

        /** Build the ring buffer */
        RingBuffer() : r(0), w(0)
        {
            static_assert(!(sizePowerOf2 & (sizePowerOf2 - 1)), "Size must be a power of two");
            static_assert(sizePowerOf2 > 32, "A minimum size is required");
        }
    };

    /** A Power-Of-2 stack buffer.
        Unlike the ring buffer, this doesn't wrap around if full.
        Thus, data stored in the buffer is always contiguous. */
    template <std::size_t sizePowerOf2>
    struct FixedSize
    {
        /** Write pointer in the buffer */
        uint32                          w;
        /** The buffer to write packets into */
        uint8                           buffer[sizePowerOf2];

        /** Get the consumed size in the buffer */
        inline uint32 getSize() const { return w; }
        /** Get the available size in the buffer */
        inline uint32 freeSize() const { return sizePowerOf2 - getSize(); }
        /** Fetch the current read position (used to restore the read pointer later on on rollback) */
        inline uint32 fetchWritePos() const { return w; }
        /** Get the current head in the buffer */
        inline const uint8 * getHead() const { return &buffer[w]; }
        /** Rollback the saved write position */
        inline void rollbackWrite(const uint32 writePos) { if (writePos >= sizePowerOf2) return; w = writePos; }
        /** Check if the buffer is full (and, if configured to do so, clean the buffer until there's enough free space) */
        bool canFit(const uint32 size)
        {
            if (size > sizePowerOf2) return false; // Can't store the data anyway
            if (freeSize() >= size) return true;
            return false;
        }
        /** Add data to this buffer (no allocation is done at this time) */
        bool save(const uint8 * packet, uint32 size)
        {
            if (!canFit(size)) return false;
            memcpy((buffer + w), packet, size);
            w += size;
            return true;
        }
        /** Save a string to the buffer
            @param str  A pointer on the C string to save
            @param len  If non zero, contains the actual number of bytes to save (don't include the final NUL)
                        Else, compute the string length from the actual string size.
            @return A pointer on the saved string (a copy) that will persist as long as this instance */
        const char * saveString(const char * str, std::size_t len = 0)
        {
            if (!len) len = strlen(str);
            uint8 c = 0;
            if (save((const uint8*)str, len))
                return (const char*)&buffer[w - len];
            return 0;
        }
        /** Reset the buffer */
        void reset() { w = 0;
#ifdef ParanoidServer
            Zero(buffer);
#endif
        }

        /** Build the ring buffer */
        FixedSize() : w(0)
        {
            static_assert(sizePowerOf2 > 32, "A minimum size is required");
        }
    };


    /** A Power-Of-2 stack buffer with 2 write heads.
        This container is a bit strange, because it's dealing with area.
        The "transcient" area start from the beginning of the buffer to the second head (initially set to the end of the buffer).
        Whenever some data need to be persisted, the second head is moved down toward the beggining of the buffer and the
        data to keep is copied in the "vault" space created.
        The "transcient" area is thus shrinked by the space consumed by the used "vault" space.

        Examples:
        @code
        [                                                      ]
        ^                                                      ^
        |__ w                                              v __|      w is the write head for the transcient, v for the vault

        Write some bytes in transcient buffer:
        [GET / HTTP/1.1\r\n                                    ]
                           ^                                   ^
                           |__ w                           v __|

        Persist important data in the vault:
        [GET / HTTP/1.1\r\n                                   /]
                           ^                                  ^
                           |__ w                          v __|


        Reset transcient buffer for next part of the message (and receive new part):
        [Host: example.com\r\n                                /]
                              ^                               ^
                              |__ w                       v __|

        Persist important data in the vault:
        [Host: example.com\r\n                     example.com/]
                              ^                    ^
                              |__ w            v __|

        Reset transcient buffer for next part of the message (and receive new part):
        [Connection: close\r\n                     example.com/]
                              ^                    ^
                              |__ w            v __|
        And so on...
        @endcode

        This kind of buffer is perfectly suited for parsing code that's reducing the amount of data while parsed.
        The parsed data is likely to consume less space than the textual version in the "transcient" area, thus the same buffer
        can be gradually reused to parse a huge input message down to a small abstract tree.

        Unlike the ring buffer, this doesn't wrap around if full.
        Thus, data stored in the buffer is always contiguous. */
    template <std::size_t sizePowerOf2>
    struct TranscientVault
    {
        static constexpr std::size_t BufferSize = sizePowerOf2;
        /** Write pointer in the buffer (for the transcient buffer) */
        uint32                          w;
        /** Vault pointer in the buffer */
        uint32                          v;
        /** The buffer to write packets into */
        uint8                           buffer[sizePowerOf2];

        /** Get the consumed size in the transcient buffer */
        inline uint32 getSize() const { return w; }
        /** Get the size of the vault */
        inline uint32 vaultSize() const { return sizePowerOf2 - v; }
        /** Get the available size in the buffer */
        inline uint32 freeSize() const { return v - w; }
        /** Get the maximum size that can be stored in this buffer (without overwriting the vault) */
        inline uint32 maxSize() const { return v; }

        /** Stored the given amount in the transcient buffer (to be used with getTail later on)
            @warning Using the buffer directly is dangerous, make sure you're not overwriting the vault here */
        inline void stored(const uint32 s) { w += s; }
        /** Get the current head in the transcient buffer
            @warning Using the buffer directly is dangerous, make sure you're not overwriting the vault here */
        inline uint8 * getTail() { return &buffer[w]; }
        /** Get the head of the transcient buffer */
        inline uint8 * getHead() { return buffer; }
        /** Get the head of the vault buffer */
        inline uint8 * getVaultHead() { return &buffer[v]; }
        /** Check if the given pointer is inside our buffer */
        inline bool contains(const void * ptr) const { return ((const uint8*)ptr) >= buffer && ((const uint8*)ptr) < &buffer[sizePowerOf2]; }

        /** Get the transcient buffer as a view */
        template <typename T> inline T getView() {
            if constexpr(requires {T(buffer, w); }) {
                return T(buffer, w);
            } else return T((const char*)buffer, std::size_t(w));
        }
        /** Get the vault buffer as a view */
        template <typename T> inline T getVaultView() {
            if constexpr(requires {T(buffer, w); }) {
                return T(getVaultHead(), vaultSize());
            } else {
                return T((const char*)getVaultHead(), std::size_t(vaultSize()));
            }
        }
        /** Reset the write head for the transcient buffer */
        inline void resetTranscient(const uint32 size = 0) { if (size >= v) return; w = size; }
        /** Reset the write head for the transcient buffer */
        inline void resetVault(const uint32 size = 0) { if (size >= vaultSize()) return; v = sizePowerOf2 - size; }
        /** Drop count bytes from the beginning of the transcient buffer */
        inline void drop(const uint32 size)
        {
            if (size >= w) resetTranscient();
            else
            {
                memmove(buffer, &buffer[size], w - size);
                memset(&buffer[w - size], 0, size);
                w -= size;
            }
        }
        inline void drop(const void * ptr) { drop((uint32)((const uint8*)ptr - buffer)); }
        /** Check if the buffer is full (and, if configured to do so, clean the buffer until there's enough free space) */
        bool canFit(const uint32 size)
        {
            if (freeSize() >= size) return true;
            return false;
        }
        /** Add data to this buffer (no allocation is done at this time) */
        bool save(const uint8 * packet, uint32 size)
        {
            if (!canFit(size)) return false;
            memcpy((buffer + w), packet, size);
            w += size;
            return true;
        }
        /** Save a string to the buffer
            @param str  A pointer on the C string to save
            @param len  If non zero, contains the actual number of bytes to save (don't include the final NUL)
                        Else, compute the string length from the actual string size.
            @return A pointer on the saved string (a copy) that will persist as long as this instance */
        const char * saveString(const char * str, std::size_t len = 0)
        {
            if (!len) len = strlen(str);
            if (save((const uint8*)str, len))
                return (const char*)&buffer[w - len];
            return 0;
        }
        /** Reset the buffer */
        void reset() { w = 0; v = sizePowerOf2;
#ifdef ParanoidServer
            Zero(buffer);
#endif
        }
        /** Persist data to the vault */
        bool saveInVault(const uint8 * packet, uint32 size)
        {
            if (!canFit(size)) return false;
            memcpy((buffer + v - size), packet, size);
            v -= size;
            return true;
        }
        /** Reserve some space for the vault */
        uint8 * reserveInVault(uint32 size)
        {
            if (!canFit(size)) return nullptr;
            v -= size;
            return &buffer[v];
        }
        /** Save a string in the vault
            @param str  A pointer on the C string to save
            @param len  If non zero, contains the actual number of bytes to save (don't include the final NUL)
                        Else, compute the string length from the actual string size.
            @return A pointer on the saved string (a copy) that will persist as long as this instance */
        const char * saveStringInVault(const char * str, std::size_t len = 0)
        {
            if (!len) len = strlen(str);
            if (saveInVault((const uint8*)str, len))
                return (const char*)getVaultHead();
            return 0;
        }
        /** Save a string in the vault by dropping the given amount from the transcient buffer.
            Because the string is likely coming from the transcient buffer, it's copied to a stack allocated buffer first,
            then dropped from the transcient buffer to make space for it in the vault. This invalidate all pointers held to the transcient buffer */
        const char * transferStringToVault(const char * str, std::size_t len = 0, std::size_t futureDrop = 0)
        {
            if (!len) len = strlen(str);
            // Save the string to the stack before being erased
            char * tmp = (char*)alloca(len);
            memcpy(tmp, str, len);

            if (futureDrop >= w) w = 0;
            else drop(futureDrop);
            if (saveInVault((const uint8*)tmp, len))
                return (const char*)getVaultHead();
            return 0;
        }

        /** Build the ring buffer */
        TranscientVault() : w(0), v(sizePowerOf2)
        {
            static_assert(sizePowerOf2 > 32, "A minimum size is required");
        }
    };



}

#endif
