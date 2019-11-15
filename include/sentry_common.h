#ifndef SENTRY_COMMON_H
#define SENTRY_COMMON_H

#include <string>

#define LOG(msg, ...) Sentry::logSentryInternal(__PRETTY_FUNCTION__, msg)


static constexpr char GET[] = "GET";
static constexpr char POST[] = "POST";

constexpr uint64_t NANOSECONDS_IN_SECOND = 1000000000;
constexpr uint64_t MILLISECONDS_IN_SECOND = 1000;

constexpr size_t HTTP_CLIENT_TIMOUT_IN_SECONDS = 1;

namespace Sentry
{

#define ERROR_CODES_LIST \
    EECODE_MAP(NO_ERROR, "No error occured") \
    EECODE_MAP(NO_DSN, "Sentry diabled. No DSN key provided. DSN key must be passed to init function or SENTRY_DSN environment variable must be set.") \
    EECODE_MAP(WRONG_DSN, "Wrong DNS key provided!") \
    EECODE_MAP(CONNECTION_ERROR, "Failed to establish connection. Event will be stored and retried later.")\


enum class EErrorCode
{
#define EECODE_MAP(name, description) name,
    ERROR_CODES_LIST
#undef EECODE_MAP
    SIZE
};

void logSentryInternal(const std::string& methodName, const std::string& msg);

enum class EventLevel
{
    LEVEL_DEBUG = -1,
    LEVEL_INFO,
    LEVEL_WARNING,
    LEVEL_ERROR,
    LEVEL_FATAL,
};

const std::string levelToString(EventLevel level);


} //namespace


#endif // SENTRY_COMMON_H
