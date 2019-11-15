#include "sentry_common.h"

#include <iostream>


namespace Sentry
{

void logSentryInternal(const std::string& methodName, const std::string& msg)
{
    std::cout << "[SentryCpp]" << "[" << methodName << "] " << msg << std::endl;
}

const std::string levelToString(EventLevel level)
{
    switch (level)
    {
    case EventLevel::LEVEL_FATAL:
        return "fatal";
    case EventLevel::LEVEL_ERROR:
        return "error";
    case EventLevel::LEVEL_WARNING:
        return "warning";
    case EventLevel::LEVEL_DEBUG:
        return "debug";
    case EventLevel::LEVEL_INFO:
        return "info";
    default:
        return "unknown";
    }
}

} // namespace
