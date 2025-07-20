#include "../include/Path/Normalization.hpp"

namespace Path {

    ROString normalize(ROString & absolutePath, const bool fixEncoding)
    {
        ROString pathToNormalize = absolutePath;
        // Use stack here to store path for normalizing
        struct Segment {
            ROString segment;
            enum Type { Empty = 0, Self = 1, Parent = 2, Child = 3 } type = Empty;
            bool keep = true;
            void classify()
            {
                keep = true;
                if (!segment)               type = Segment::Empty;
                else if (segment == ".")    type = Segment::Self;
                else if (segment == "..")   type = Segment::Parent;
                else                        type = Segment::Child;
            }
        };
        Segment segments[128]; // Start and length for each segment ('/' + something else) found in the given path
        size_t ip = 0;
        // First pass, slip through the initial path and compute segments
        while (pathToNormalize) {
            const char * s = pathToNormalize.getData();
            size_t i = 0;
            while (i != pathToNormalize.getLength())
            {
                if (s[i] == '/') {
                    segments[ip].segment = pathToNormalize.midString(0, i);
                    segments[ip].classify();
                    // Optimization: don't store empty segment or self segment too
                    if (segments[ip].type != Segment::Empty && segments[ip].type != Segment::Self)
                    {
                        if (ip >= sizeof(segments) / sizeof(*segments)) return ""; // Path too deep, let's refuse this
                        ip++;
                    }
                    pathToNormalize.splitAt(i+1);
                    break;
                }
                i++;
            }
            if (i == pathToNormalize.getLength() && i)
            {
                // Store last segment
                segments[ip].segment = pathToNormalize;
                segments[ip].classify();
                ip++;
                break;
            }
        }

        if (!ip) return "/";
        // Trim all parent segments from the left, since we need to start from a child anyway
        size_t first = 0, end = ip, ptr = 0;
        while (segments[first].type == Segment::Parent && first < end) first++;

        ptr = first; ip = first;
        // Traverse the segment now, advancing or backtracking the pointer depending on what we've found
        while (ip < end)
        {
            if (segments[ip].type == Segment::Child) { ptr = ip; }
            else if (segments[ip].type == Segment::Parent) {
                segments[ptr].keep = false;
                while (ptr > 0)
                {
                    ptr--;
                    if (segments[ptr].type == Segment::Child && segments[ptr].keep == true) break;
                }
            }
            ip++;
        }

        // Finally compute the final path (using memmove here, will likely crash on Read Only section)
        char * dest = const_cast<char*>(absolutePath.getData());
        size_t len = 0;
        for (ip = first; ip < end; ip++)
        {
            if (segments[ip].keep && segments[ip].type == Segment::Child) {
                if (ip || segments[ip].segment.getData() != absolutePath.getData()) dest[len++] = '/';
                memmove(&dest[len], segments[ip].segment.getData(), segments[ip].segment.getLength());
                len += segments[ip].segment.getLength();
            }
        }

        if (fixEncoding) return URLDecode(ROString(dest, len));
        return ROString(dest, len);
    }

    #define IsHex(X)    ((X >= '0' && X <= '9') || (X >= 'a' && X <= 'f') || (X >= 'A' && X <= 'F'))
    #define ToHex(X)    (X >= '0' && X <= '9' ? X - '0' : (X >= 'a' ? X - 'a' + 10 : X - 'A' + 10))

    ROString URLDecode(ROString input)
    {
        const char * s = input.getData();
        char * dest = const_cast<char*>(s);
        size_t i = 0, o = 0;
        while (i < input.getLength())
        {
            if (s[i] == '+') dest[o++] = ' ';
            else if (s[i] != '%' || i == (input.getLength() - 2)) dest[o++] = s[i]; // A valid percent encoding requires at least % + 1 hexadecimal character
            else {
                // Check no encoding
                char first = s[i+1];
                if (!IsHex(first)) // Bad encoding, let's output '%' directly
                    dest[o++] = s[i];
                else {
                    unsigned char c = ToHex(first); // >= '0' && first <= '9' ? first - '0' : (first >= 'a' ? first - 'a' + 10 : first - 'A' + 10);
                    if (i < input.getLength() - 2 && IsHex(s[i+2])) {
                        c = (c << 4) | ToHex(s[i+2]);
                        i++;
                    }
                    dest[o++] = c;
                    i++;
                }
            }
            i++;
        }
        return ROString(dest, o);
    }
}
