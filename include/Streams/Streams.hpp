#ifndef hpp_Streams_hpp
#define hpp_Streams_hpp

// We need types
#include "Types.hpp"
// We need FILE declaration and its functions
#include <bits/c++config.h>
#include <cstdio>
// We need ROString for memory view buffer
#include "Strings/ROString.hpp"
// We need int2Str function too
#include "Strings/RWString.hpp"
// We need socket code too
#include "Network/Socket.hpp"

/** This is where streams are declared */
namespace Streams
{
    /** Common interface */
    template <typename Child>
    struct Base
    {
        /** Get the current expected size for this stream or 0 if unknown */
        std::size_t getSize() const { return c()->getSize(); }
        /** Check if this stream has content (likely) */
   //     bool hasContent() const { return c()->hasContent(); }
        /** Get the current position */
        std::size_t getPos() const { return c()->getPos(); }
        /** Set the current position
            @return false on failure to change the current position */
        bool setPos(const std::size_t pos) { return c()->setPos(pos); }
        /** Map the given stream to a pointer (if possible). Might be faster than reading it
            @param  size    The desired mapped size. If 0, the whole stream is mapped.
            @return A pointer to the stream data or 0 upon failure to map. */
        void * map(const std::size_t size = 0) { return c()->map(size); }
        /** Unmap the given stream mapping */
        void unmap(void * buffer) { return c()->unmap(buffer); }

    private:
        /** Select the most derived implementation each time */
        auto * c() { return static_cast<Child*>(this)->c(); }
        const auto * c() const { return static_cast<const Child*>(this)->c(); }
    };

    /** Input stream base interface */
    template <typename Child>
    struct Input : public Base< Input<Child> >
    {
        /** Read data into the given buffer for this stream
            @param buffer   The buffer to write data into
            @param size     The buffer size in bytes
            @return the number of bytes read or 0 if none or error or end of stream */
        std::size_t read(void * buffer, const std::size_t size) { return c()->read(buffer, size); }

        // Base interface
    // public:
    //     std::size_t getSize() const { return c()->getSize(); }
    //     bool hasContent() const  { return c()->getSize() > 0 ? true : c()->_hasContent(); }
    //     std::size_t getPos() const { return c()->getPos(); }
    //     bool setPos(const std::size_t pos) { return c()->setPos(pos); }
    //     void * map(const std::size_t size = 0) { return c()->map(size); }
    //     void unmap(void * buffer) { return c()->unmap(buffer); }
    private:
        Child * c() { return static_cast<Child*>(this); }
        const Child * c() const { return static_cast<const Child*>(this); }
        friend struct Input::Base;
    };

    /** Output stream base interface */
    template <typename Child>
    struct Output : public Base< Output<Child> >
    {
        /** Write data from the given buffer for this stream
            @param buffer   The buffer to read data from
            @param size     The buffer size in bytes
            @return the number of bytes written or 0 if none or error or end of stream */
        std::size_t write(const void * buffer, const std::size_t size) { return c()->write(buffer, size); }

        // Base interface
    // public:
    //     std::size_t getSize() const { return c()->getSize(); }
    //     bool hasContent() const  { return c()->getSize() > 0 ? true : c()->hasContent(); }
    //     std::size_t getPos() const { return c()->getPos(); }
    //     bool setPos(const std::size_t pos) { return c()->setPos(pos); }
    //     void * map(const std::size_t size = 0) { return c()->map(size); }
    //     void unmap(void * buffer) { return c()->unmap(buffer); }
    private:
        Child * c() { return static_cast<Child*>(this); }
        const Child * c() const { return static_cast<const Child*>(this); }
        friend struct Output::Base;
    };

    namespace Private
    {
        /** A non mappable stream */
        struct NonMappeable
        {
            void * map(const std::size_t size = 0)  { return nullptr; }
            void unmap(void * buffer)               { }
        };

        /** A no content stream */
        struct NoContent
        {
            std::size_t getSize() const             { return 0; }
            bool hasContent() const                 { return false; }
        };

        /** A with content stream */
        struct WithContent
        {
            bool hasContent() const                { return true; }
        };

        /** Base class to avoid useless binary size, not instantiable */
        struct FileBase : public NonMappeable
        {
            std::size_t getSize() const             { return size; }
            bool hasContent() const                 { return f ? true : false; }
            std::size_t getPos() const              { return f ? (std::size_t)ftello(f) : 0; }
            bool setPos(const std::size_t pos)      { return f ? fseeko(f, pos, SEEK_SET) == 0 : false; }

        public:
            FileBase(const char * path, bool write) : f(fopen(path, write ? "wb" : "rb")), size(0)
            {
                if (f) {
                    fseeko(f, 0, SEEK_END);
                    size = (std::size_t)ftello(f);
                    fseeko(f, 0, SEEK_SET);
                }
            }
            FileBase(FileBase && move) : f(move.f), size(move.size) { move.close(false); }
            // virtual ~FileBase() { if (f) fclose(f); } // Would be convenient, but this imply a virtual table cost we don't want to pay

        protected:
            void close(bool close = true) { if (close && f) fclose(f); f = nullptr; size = 0; }
            // Prevent instantiating this class directly, except for derived class
            ~FileBase() {}
            FILE * f;
            std::size_t size;
        };
        /** A non seekable stream */
        struct NonSeekable
        {
            std::size_t getPos() const              { return 0; }
            bool setPos(const std::size_t pos)      { return false; }
        };
    }

    /** A null stream that does nothing */
    struct Null : public Input<Null>, public Output<Null>, public Private::NonSeekable, public Private::NonMappeable, public Private::NoContent
    {
        std::size_t read(void * buf, const std::size_t size) { return 0; }
        std::size_t write(const void * buf, const std::size_t size) { return 0; }
        std::size_t getSize() const { return 0; }
        Null() {}
        template <typename ... T>  Null(T&&...) {}
    };

    /** An empty stream that does nothing (but declare a content) */
    struct Empty : public Input<Empty>, public Private::NonSeekable, public Private::NonMappeable, public Private::WithContent
    {
        std::size_t read(void * buf, const std::size_t size) { return 0; }
        std::size_t write(const void * buf, const std::size_t size) { return 0; }
        std::size_t getSize() const { return 0; }
        Empty() {}
        template <typename ... T>  Empty(T&&...) {}
    };

    /** A memory backed buffer stream. This isn't allocating any buffer */
    struct MemoryView : public Input<MemoryView>
    {
        std::size_t getSize() const             { return (std::size_t)buffer.getLength(); }
        bool hasContent() const                 { return getSize() > 0; }
        std::size_t getPos() const              { return pos; }
        bool setPos(const std::size_t pos)      { return pos <= getSize() ? this->pos = pos, true : false; }

        void * map(const std::size_t size = 0)  { return size <= buffer.getLength() ? (void*)buffer.getData() : nullptr; }
        void unmap(void * buffer)               { }
        std::size_t read(void * buf, const std::size_t size)
        {
            std::size_t q = min((getSize() - pos), size);
            if (q) memcpy(buf, buffer.getData() + pos, q);
            pos += q;
            return q;
        }
        // std::size_t write(const void * buf, const std::size_t size)
        // {
        //     std::size_t q = min((getSize() - pos), size);
        //     if (q) memcpy((const_cast<char*>(buffer.getData()) + pos), buf, q);
        //     pos += q;
        //     return q;
        // }

    public:
        MemoryView(const uint8 * buffer, std::size_t size) : buffer((const char*)buffer, (int)size), pos(0) {}
        MemoryView(const ROString & buffer) : buffer(buffer), pos(0) {}

    private:
        ROString buffer;
        std::size_t pos;
    };

    /** A file based input stream */
    struct FileInput final : public Input<FileInput>, public Private::FileBase
    {
        using Private::FileBase::getSize;
        std::size_t read(void * buf, const std::size_t size) { return f ? fread(buf, 1, size, f) : 0; }
        FileInput(const char * path) : FileBase(path, false) {}
        ~FileInput() { close(); }

        FileInput(const FileInput &) = delete;
        FileInput(FileInput && input) : FileBase(std::move(input)) {}
    };

    /** A file based output stream */
    struct FileOutput final : public Output<FileOutput>, public Private::FileBase
    {
        using Private::FileBase::getSize;
        std::size_t write(const void * buf, const std::size_t size) { return f ? fwrite(buf, 1, size, f) : 0; }
        FileOutput(const char * path) : FileBase(path, true) {}
        ~FileOutput() { close(); }

        FileOutput(const FileOutput &) = delete;
        FileOutput(FileOutput && output) : FileBase(std::move(output)) {}
    };

    /** A socket stream that doesn't own the socket */
    struct Socket final : public Input<Socket>, public Output<Socket>, public Private::NonSeekable, public Private::NonMappeable, public Private::WithContent
    {
        std::size_t getSize() const             { return 0; }
        std::size_t read(void * buf, const std::size_t size) { return socket->recv((char*)buf, (uint32)size).getCount(); }
        std::size_t write(const void * buf, const std::size_t size) { return socket->send((const char*)buf, (uint32)size).getCount(); }
        Socket(Network::BaseSocket & socket) : socket(&socket) {}
    protected:
        Network::BaseSocket * socket;
    };

    /** A chunk based output stream, following HTTP/1.1 RFC standard */
    struct ChunkedOutput final : public Output<ChunkedOutput>, public Private::NonSeekable, public Private::NonMappeable, public Private::WithContent
    {
        std::size_t getSize() const { return 0; }
        std::size_t write(const void * buf, const std::size_t size) {
            // Write chunk header to the given socket
            char buffer[sizeof("FFFFFFFF\r\n")] = {};
            intToStr((int)size, buffer, 16);
            std::size_t l = strlen(buffer);
            memcpy(&buffer[l], "\r\n", 2);
            if (socketStream.write(buffer, l+2) != l+2) return 0;
            // Write chunk data
            if (socketStream.write(buf, size) != size) return 0;
            // Write end of this chunk
            if (socketStream.write("\r\n", 2) != 2) return 0;
            return size;
        }

        ChunkedOutput(Network::BaseSocket & socket) : socketStream(socket) {}
    protected:
        Socket socketStream;
    };


    /** A chunk based input stream, following HTTP/1.1 RFC standard */
    struct ChunkedInput final : public Input<ChunkedInput>, public Private::NonSeekable, public Private::NonMappeable, public Private::WithContent
    {
        std::size_t getSize() const { return 0; }
        std::size_t read(void * _buf, const std::size_t size) {
            char buffer[sizeof("FFFFFFFF\r\n")] = {};
            char * buf = (char*)_buf;
            if (size < sizeof(buffer)) return 0; // If we can't store the buffer length in the expected buffer, don't do nittypicky code below, just bail out

            std::size_t p = 0, s = 0, toRead = 0;

            if (remChunkSize == 0)
            {   // Need to read chunked length first
                s = socketStream.read(buffer, sizeof(buffer));
                if (s == 0) return 0;
                ROString hdr(buffer, s);
                remChunkSize = hdr.parseInt(16);
                hdr.splitFrom("\r\n");
                if (remChunkSize == 0) return 0; // End of stream
                toRead = min(hdr.getLength(), remChunkSize);
                memcpy(&buf[p], hdr.getData(), toRead);
                p += toRead;
                remChunkSize -= toRead;
            }

            // Try to read as much as possible from the buffer here
            toRead = min(size - p, remChunkSize);
            s = socketStream.read(&buf[p], min(size - p, remChunkSize));
            if (s == 0) return 0; // Error here

            if (s == remChunkSize)
            {   // Need to read the end of chunk marker here
                if (socketStream.read(buffer, 2) != 2) return 0; // Error here too
            }

            remChunkSize -= s;
            return s+p;
        }

        ChunkedInput(Network::BaseSocket & socket) : socketStream(socket), remChunkSize(0) {}
    protected:
        Socket socketStream;
        std::size_t remChunkSize;
    };

    /** The get data callback function that should follow this signature:
        @code
            std::size_t callback(char * buffer, const std::size_t size)
        @endcode
        */
    template <typename Func>
    concept GetDataCallback = requires (Func f, char * buffer, const std::size_t size) {
        // Make sure the signature matches
        (std::size_t)f(buffer, size);
    };

    template<GetDataCallback auto GDCB>
    struct CallbackInput : public Input<CallbackInput<GDCB>>, public Private::NonSeekable, public Private::NonMappeable, public Private::WithContent
    {
        std::size_t getSize() const { return 0; }
        std::size_t read(void * _buf, const std::size_t size) {
            std::size_t s = GDCB((char*)_buf, size);
            return s;
        }
    };


    /** Copy a stream to another one until either one fails or the total amount specified is reached
        @return the number of bytes copied */
    template <typename In, typename Out>
    std::size_t copy(In & in, Out & out, const std::size_t size = (std::size_t)-1)
    {
        uint8 buffer[256];
        std::size_t total = 0, step = 0;
        while (true)
        {
            step = in.read(buffer, min(size - total, sizeof(buffer)));
            if (step == 0) return total;
            std::size_t c = out.write(buffer, step);
            if (c != step) { total += c; return total; }
            total += c;
            if (size == total) return total;
        }
    }

    /** Copy a stream to another one until either one fails or the total amount specified is reached, using the given buffer
        @return the number of bytes copied */
    template <typename In, typename Out>
    std::size_t copy(In & in, Out & out, uint8 * buffer, std::size_t bufSize, const std::size_t size = (std::size_t)-1)
    {
        std::size_t total = 0, step = 0;
        while (true)
        {
            step = in.read(buffer,  min(size - total, bufSize));
            if (step == 0) return total;
            std::size_t c = out.write(buffer, step);
            if (c != step) { total += c; return total; }
            total += c;
            if (size == total) return total;
        }
    }

}

#endif
