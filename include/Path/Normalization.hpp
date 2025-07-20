#ifndef hpp_Normalization_hpp
#define hpp_Normalization_hpp

// We need Read only strings
#include "Strings/ROString.hpp"

namespace Path
{

    /** Normalize the request URI.
        This method fix in place the Request URI to normalize path and URL encoded chars.
        It's a complex method so it's only enabled when MaxSupport is defined.
        It modifies the ROString in place (not only the pointers, but also the underlying buffer).
        So make sure it's not called on a Read Only data page or it will segfault */
    ROString normalize(ROString & absolutePath, const bool fixEncoding = false);

    /** Decode URL special percent encoding and rewrite in place with decoded content */
    ROString URLDecode(ROString input);
}

#endif
