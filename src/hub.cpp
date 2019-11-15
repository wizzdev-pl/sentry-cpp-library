#include "hub.h"

#include "backtracehandler.h"


#include <assert.h>
#include <cxxabi.h>
//#include <dlfcn.h> // for dladdr
#include <iomanip>
#include <exception> // current_exception, exception, get_terminate, rethrow_exception, set_terminate
#include <execinfo.h>
#include <signal.h>
#include <sstream>
#include <iostream>
#include <string>
#include <time.h>
#include <uuid/uuid.h>

#define WAIT_FOR_REQUEST_COMPLETED_TIMEOUT_MS 1000

namespace Sentry
{

Hub* Hub::m_hub_that_installed_termination_handler = nullptr;


Hub::Hub()
:
m_initialised(false),
m_sampleRate(100),
m_lastEventId(),
m_listOfLastUniqueEventsWithTimestamps(),
m_pHttpClient(nullptr),
m_scope(),
m_isSourceAvailable(false)
{

}

Hub::~Hub()
{
    closeHttpConnection();
}

EErrorCode Hub::init(std::string dsn, int maxBreadcrumbs, bool sourceAvailable,  int sampleRate)
{
    // config
    if (maxBreadcrumbs != -1)
        m_scope.setMaxBreadcrumbs(static_cast<uint16_t>(maxBreadcrumbs));
    m_isSourceAvailable = sourceAvailable;

    m_sampleRate = sampleRate;

    EErrorCode errorCode;
    SentryDSN newDSNStruct;

    errorCode = SentryDSN::parseDSN(dsn, &newDSNStruct);

    if (errorCode != EErrorCode::NO_ERROR)
    {
        return errorCode;
    }

    m_pHttpClient = std::make_shared<Transport>();
    m_pHttpClient->start();

    errorCode = m_pHttpClient->setupClient(newDSNStruct);
    if (errorCode == EErrorCode::NO_ERROR)
    {
        m_initialised = true;
        installHandler();
    }

    return errorCode;
}

bool Hub::isInitialised()
{
    return m_initialised;
}

void Hub::closeHttpConnection()
{
    if (m_pHttpClient != nullptr)
    {
        m_pHttpClient->stop();
    }
}

void Hub::setTag(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(m_scopeMutex);
    if (key == "release")
    {
        m_scope.setRelease(value);
    }
    else
    {
        m_scope.setTag(key, value);
    }
}

void Hub::setExtra(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(m_scopeMutex);
    m_scope.setExtra(key, value);
}

void Hub::installHandler()
{
    if (default_termination_handler == nullptr)
    {
        default_termination_handler = std::get_terminate();
        std::set_terminate(&terminationHandler);

        // we remember this client, because we will use it to report
        // uncaught exceptions with it
        m_hub_that_installed_termination_handler = this;
    }

    // TODO - make it controllable in settings which signals to catch?
    signal(SIGINT, signalsHandler);
    signal(SIGTERM, signalsHandler);
    signal(SIGSEGV, signalsHandler);
    signal(SIGABRT, signalsHandler);
    signal(SIGBUS, signalsHandler);
    signal(SIGFPE, signalsHandler);
    signal(SIGILL, signalsHandler);

}

// based on https://github.com/nlohmann/crow
void Hub::terminationHandler()
{
    assert(m_hub_that_installed_termination_handler->m_pHttpClient != nullptr);

#ifdef DEBUG_SENTRYCPP
    LOG("TerminationHandler called!");
#endif // DEBUG_SENTRYCPP

    assert(m_hub_that_installed_termination_handler != nullptr);

    auto current_ex = std::current_exception();
    if (current_ex)
    {
        //m_hub_that_installed_termination_handler->addBreadcrumb({{"level", "critical"}});
        try
        {
            std::rethrow_exception(current_ex);
        }
        catch (const std::exception& e)
        {
            json context;
            context["logger"] = "termination_handler";
            m_hub_that_installed_termination_handler->captureException(e, context, false);
        }
    }

    //wait for transport to send or sefor some timeout
    m_hub_that_installed_termination_handler->closeHttpConnection();

    if (m_hub_that_installed_termination_handler->default_termination_handler != nullptr)
        m_hub_that_installed_termination_handler->default_termination_handler();
}

void Hub::signalsHandler(int sig)
{
    assert(m_hub_that_installed_termination_handler->m_pHttpClient != nullptr);
#ifdef DEBUG_SENTRYCPP
    LOG("Returning to default handler");
#endif
    signal(sig, SIG_DFL); // default signal handler;

    std::cerr << sig << std::endl;
    std::string sig_name = sys_siglist[sig];

#ifdef DEBUG_SENTRYCPP
    std::stringstream ss;
    ss << "Received signal: " << sig_name << std::endl;
    LOG(ss.str());
#endif // DEBUG_SENTRYCPP

    json currentAttributes;
    currentAttributes["type"] = sig_name;

    size_t functionsToSkip = 2+1;
    backtraceHandler bckHandler(m_hub_that_installed_termination_handler->m_isSourceAvailable);
    currentAttributes["stacktrace"]["frames"] = bckHandler.getStacktraceJSON(functionsToSkip, 2);

    json exceptionInterface;
    exceptionInterface["exception"] = currentAttributes;
    exceptionInterface["logger"] = "signals_handler";
    m_hub_that_installed_termination_handler->captureEvent(exceptionInterface);
    m_hub_that_installed_termination_handler->closeHttpConnection();

    raise(sig);
}

std::string Hub::captureException(const std::exception& exception, const json& context, const bool handled)
{

    json currentAttributes;
    currentAttributes["type"] = typeid(exception).name();
    currentAttributes["value"] = exception.what();
    currentAttributes["handled"] = handled;

#ifdef DEBUG_SENTRYCPP
    std::stringstream ss;
    ss << "Capturing exception: " << currentAttributes["type"] << ", what: " << currentAttributes["value"] << '\n';
    LOG(ss.str());
#endif

    // TODO optional parameters:
    // module thread_id mechanism stacktrace

    size_t functionsToSkip = 6;
    backtraceHandler bckHandler(m_isSourceAvailable);
    currentAttributes["stacktrace"]["frames"] = bckHandler.getStacktraceJSON(functionsToSkip, 2);

    json exceptionInterface;
    if (context != nullptr)
    {
        exceptionInterface = context;
    }
    exceptionInterface["exception"] = currentAttributes;
    return captureEvent(exceptionInterface);
}

//std::string Hub::captureMessage()
//{
//    // TODO
//    //capture_event()
//}

std::string Hub::captureEvent(const json& event)
{
    std::string eventId = generateUuid();
    m_lastEventId = eventId;
    std::string timestamp = ISO8601_timestamp();

    if (event.find("message") != event.end())
    {
        if (checkIfEventTooOften(event, timestamp))
        {
    #ifdef DEBUG_SENTRYCPP
        LOG("Dropping the event because happens too often.");
    #endif // DEBUG_SENTRYCPP

            return "";
        }
    }

    json payload = event;
    payload["event_id"] = eventId;
    payload["timestamp"] = timestamp;
    payload["platform"] = "other";   // or undefined?

    m_scope.applyToEvent(payload);  // includes breadcrumbs etc.

    const std::string contentsToSend = payload.dump();

#ifdef DEBUG_SENTRYCPP
    LOG(contentsToSend);
#endif // DEBUG_SENTRYCPP

    // apply event sampling
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    if (std::rand()%100 < m_sampleRate)
    {
        m_pHttpClient->sendEvent(contentsToSend);
    }

    addBreadcrumb(event);

    return eventId;
}

void Hub::addBreadcrumb(const json& attributes)
{
    std::string timestamp = ISO8601_timestamp();
    // default bredcrumb:
    json breadcrumb =
    {{"timestamp", timestamp},
     {"type", "default"},
     {"level", "info"}
    };

    if (attributes.is_object())
    {
        // type
        auto type = attributes.find("type");
        if (type != attributes.end())
        {
            breadcrumb["type"] = *type;
        }
        // level
        auto level = attributes.find("level");
        if (level != attributes.end())
        {
            breadcrumb["level"] = *level;
        }
        // category
        auto category = attributes.find("category");
        if (category != attributes.end())
        {
            breadcrumb["category"] = *category;
        }
        // data
        auto data = attributes.find("data");
        if (data != attributes.end())
        {
            breadcrumb["data"] = *data;
        }
        auto message = attributes.find("message");
        if (message != attributes.end())
        {
            breadcrumb["message"] = *message;
        }
    }// else nothing (adds default)

    // always the same as level
    breadcrumb["category"] = breadcrumb["level"];

    std::lock_guard<std::mutex> lock(m_scopeMutex);
    m_scope.addBreadcrumb(breadcrumb);
}

const std::string& Hub::lastEventId()
{
    return m_lastEventId;
}


std::string Hub::generateUuid()
{
    /*
     * Hexadecimal string representing a uuid4 value.
     * The length is exactly 32 characters. Dashes are not allowed.
    */
    uuid_t new_uuid;
    uuid_generate_random(new_uuid);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for(size_t i=0; i<sizeof(new_uuid)/sizeof(new_uuid[0]); ++i)
        ss << std::setw(2) << static_cast<unsigned>(new_uuid[i]);
    std::string output = ss.str();
    return output;
}

std::string Hub::ISO8601_timestamp()
{
    time_t now;
    time (&now);
    char buf[sizeof("2011-10-08T07:07:09Z")];
    strftime(buf, sizeof(buf), "%FT%TZ", gmtime(&now));
    return std::string(buf);
}

void Hub::timeFromISO6801String(const std::string& timestamp, struct tm &timestampStruct)
{
    strptime(timestamp.c_str(), "%FT%TZ", &timestampStruct);
}

bool Hub::isTimeDiffGreaterThan(const std::string& timestamp1, const std::string& timestamp2, double diffInSeconds)
{
    struct tm timeS1;
    struct tm timeS2;
    timeFromISO6801String(timestamp1, timeS1);
    timeFromISO6801String(timestamp2, timeS2);
    time_t time1 = mktime(&timeS1);
    time_t time2 = mktime(&timeS2);

    double diff = difftime(time2, time1);

    return (diff > diffInSeconds);
}

bool Hub::checkIfEventTooOften(json event, const std::string &timestamp)
{
    // e.g. max 3 events of the same type per hour
    removeOldEventsFromList();

    std::lock_guard<std::mutex> lock(m_lastEventsListMutex);
    if (m_listOfLastUniqueEventsWithTimestamps.count(event["message"]) >= m_maxEventsPerInterval)
    {
        return true;
    }
    // else nothing

    m_listOfLastUniqueEventsWithTimestamps.emplace(event["message"], timestamp);
    return false;
}

void Hub::removeOldEventsFromList()
{
    std::string currTime = ISO8601_timestamp();
    std::lock_guard<std::mutex> lock(m_lastEventsListMutex);
    for (auto const& entry : m_listOfLastUniqueEventsWithTimestamps)
    {
        bool oldEnoughToRemove = isTimeDiffGreaterThan(entry.second,
                                                       currTime,
                                                       m_maxEventsRepetitionIntervalInSeconds);
        if (oldEnoughToRemove)
        {
            m_listOfLastUniqueEventsWithTimestamps.erase(entry.first);
        }
        // else nothing
    }
}

} //namespace Sentry
