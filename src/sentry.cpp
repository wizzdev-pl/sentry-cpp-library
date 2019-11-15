#include "sentry.h"

#include "transport.h"
#include "hub.h"

#include "json.h"

using json = ::nlohmann::json;

namespace Sentry
{

static Hub mainHub;

EErrorCode init(const SentryOptions& initParameters)
{
    // take options, confugure client
    // return some handle for e.g. disposing
    // setup default integrations
    std::string finalDsn = initParameters.dsn;

    if (initParameters.dsn == "")
    {
        if (const char* dsnEnv = std::getenv("SENTRY_DSN"))
        {
            finalDsn = dsnEnv;
        }
    }

    if (finalDsn == "")
    {
        return EErrorCode::NO_DSN;
    }

   auto errorCode = mainHub.init(finalDsn,
                                 initParameters.maxBreadcrumbs,
                                 initParameters.attachStackTrace,
                                 initParameters.sampleRate);
   if (errorCode != EErrorCode::NO_ERROR)
   {
       return errorCode;
   }

   if (initParameters.release != "")
   {
       mainHub.setTag("release", initParameters.release);
   }
   if (initParameters.environment != "")
   {
       mainHub.setTag("environment", initParameters.environment);
   }

   return errorCode;
}

std::string captureEvent(const json& event)
{
    /*
    Takes an already assembled event and dispatches it to the currently active hub.
    The event object can be a plain dictionary or a typed object whatever makes more sense in the SDK.
    It should follow the native protocol as close as possible ignoring platform specific renames (case styles etc.).
    */
    if (!mainHub.isInitialised())
        return "";

    return mainHub.captureEvent(event);
}

//std::string captureException(error)
//{
//    /*
//    Report an error or exception object. Depending on the platform different parameters are possible.
//    The most obvious version accepts just an error object but also variations are possible where no error is passed
//    and the current exception is used.
//    */
//    return mainHub.capture_exception(error)
//}

//std::string captureMessage(message, level)
//{
//    /*
//     * Reports a message.
//     * The level can be optional in language with default parameters in which case it should default to info.
//    */
//    return mainHub.capture_message(message. level);
//}

void addBreadcrumb(const json& crumb)
{
    /*
    Adds a new breadcrumb to the scope.
    If the total number of breadcrumbs exceeds the max_breadcrumbs setting,
    the oldest breadcrumb should be removed in turn. This works like the Hub api with regards to what crumb can be.
    */
    if (!mainHub.isInitialised())
        return;

    mainHub.addBreadcrumb(crumb);
}

void setTag(const std::string& key, const std::string& value)
{
    if (!mainHub.isInitialised())
        return;

    mainHub.setTag(key, value);
}

void setExtra(const std::string& key, const std::string& value)
{
    if (!mainHub.isInitialised())
        return;

    mainHub.setExtra(key, value);
}

std::string lastEventId()
{
 /*
  * Should return the last event ID emitted by the current scope.
  * This is for instance used to implement user feedback dialogs.
  */
    if (!mainHub.isInitialised())
        return "";

    return mainHub.lastEventId();
}

/*
 *This is a convenient function that can be used inside the end application logging interface,
 *It will either add a log to breadcrumbs or send it as an event, depending on int level, and current Sentry settings.
 */
void log(EventLevel level, const std::string& message)
{
    if (!mainHub.isInitialised())
        return;

    EventLevel someLevel = EventLevel::LEVEL_ERROR;
    if (level >= someLevel)
    {
        // prepare event json
        const json event
        {
            {"message", message},
            {"level", levelToString(level)},
        };
        captureEvent(event);
    }
    else
    {
        const json breadcrumb =
        {
         {"category", "log"},
         {"level", levelToString(level)},
         {"message", message}
        };
        addBreadcrumb(breadcrumb);
    }
}

const char* getErrorDescription(EErrorCode errorCode)
{
    switch (errorCode)
    {
        #define EECODE_MAP(name, description) case EErrorCode::name: return description;
                ERROR_CODES_LIST
        #undef EECODE_MAP

        default:
        {
            return "Unknown error code";
        }
    }
}

} //namespace Sentry
