#ifndef SENTRY_H
#define SENTRY_H

#include "sentry_common.h"

#include <iostream>

#include "json.h"

using json = ::nlohmann::json;


namespace Sentry
{
class SentryOptions
{
public:
    std::string dsn = "";
    std::string release = "";
    std::string environment = "";
    int sampleRate = 100;
    int maxBreadcrumbs = 100;
    bool debug = false;
    bool attachStackTrace = false;
};

EErrorCode init(const SentryOptions& initParameters);

void log(EventLevel level, const std::string& message);

std::string captureEvent(const json& event);

std::string lastEventId();

void addBreadcrumb(const json& crumb);
void setTag(const std::string& key, const std::string& value);
void setExtra(const std::string& key, const std::string& value);

const char* getErrorDescription(EErrorCode errorCode);

} // namespace Sentry


#endif // SENTRY_H
