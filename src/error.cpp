#include "error.h"

#include <string>
#include <string_view>

Error makeError(ErrorCode code, std::string_view message)
{
    return Error{code, std::string{message}};
}

std::string_view toString(ErrorCode code) noexcept
{
    switch (code)
    {
        case ErrorCode::none: return "none";
        case ErrorCode::cancelled: return "cancelled";
        case ErrorCode::notFound: return "notFound";
        case ErrorCode::accessDenied: return "accessDenied";
        case ErrorCode::ioFailure: return "ioFailure";
        case ErrorCode::invalidArgument: return "invalidArgument";
        case ErrorCode::invalidData: return "invalidData";
        case ErrorCode::parseFailure: return "parseFailure";
        case ErrorCode::unsupported: return "unsupported";
        case ErrorCode::outOfMemory: return "outOfMemory";
        case ErrorCode::glFailure: return "glFailure";
        case ErrorCode::deviceLost: return "deviceLost";
        case ErrorCode::timeout: return "timeout";
    }

    return "unknown";
}
