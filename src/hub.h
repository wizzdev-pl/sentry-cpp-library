#ifndef SENTRY_HUB_H
#define SENTRY_HUB_H

#include <atomic>
#include <functional>

#include "json.h"
#include "scope.h"
#include "sentry_common.h"
#include "transport.h"

using json = ::nlohmann::json;

namespace Sentry
{

class Hub
{
public:

    Hub();
    ~Hub();

    EErrorCode init(const std::string dsn, int maxBreadcrumbs=-1, bool sourceAvailable=false, int sampleRate=100);

    bool isInitialised();

    std::string captureException(const std::exception& exception, const json& context, const bool handled=false); // cals capture event

    //std::string captureMessage();

    std::string captureEvent(const json& event);

    void setTag(const std::string& key, const std::string& value);
    void setExtra(const std::string& key, const std::string& value);

    void addBreadcrumb(const json& attributes); // hint)? Adds a breadcrumb to the current scope.

    const std::string& lastEventId();

    static void signalsHandler(int sig);
    static void terminationHandler();

    void installHandler();

private:

    Hub(const Hub&) = delete;
    Hub& operator=(const Hub&) = delete;
    Hub(const Hub&&) = delete;
    Hub&& operator=(const Hub&&) = delete;

    void closeHttpConnection();

    std::string generateUuid();
    std::string ISO8601_timestamp();
    void timeFromISO6801String(const std::string&  timestamp, struct tm &timestampStruct);
    bool isTimeDiffGreaterThan(const std::string& timestamp1, const std::string& timestamp2, double diffInSeconds);

    void removeOldEventsFromList();
    bool checkIfEventTooOften(json event, const std::string &timestamp);

    std::atomic_bool m_initialised;

    int m_sampleRate;
    std::terminate_handler default_termination_handler = nullptr;
    static Hub* m_hub_that_installed_termination_handler;

    std::string m_lastEventId;

    mutable std::mutex m_lastEventsListMutex;
    // <message, timestamp>
    std::multimap<std::string, std::string> m_listOfLastUniqueEventsWithTimestamps;

    std::shared_ptr<Transport> m_pHttpClient;
    Scope m_scope;
    mutable std::mutex m_scopeMutex;

    bool m_isSourceAvailable;

    size_t m_maxEventsPerInterval = 3;                      // send max identical 3 events
    size_t m_maxEventsRepetitionIntervalInSeconds = 3600;   // per 1 hour

};


} // namespace Sentry

#endif // SENTRY_HUB_H
