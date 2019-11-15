#include <iostream>

#include "json.h"
#include "sentry.h"

using namespace std;

using json = ::nlohmann::json;

void foo2()
{
    int *foo = (int*)-1; // make a bad pointer
     printf("%d\n", *foo);       // causes segfault
}
void foo()
{
    foo2();
}

void bar()
{
    throw runtime_error("my error2");
    std::cout << "bar\n";
}

void bar2()
{
    bar();
}

void sendEvent()
{
    json event;
    event["message"] = "Custom event";
    event["level"] = "warning";
    Sentry::captureEvent(event);
}

int main()
{
    std::string dsn = "https://example@sentry.io/example";  // paste here the dsn of your project

    Sentry::SentryOptions initSentryParameters = Sentry::SentryOptions();
    initSentryParameters.dsn = dsn;
    initSentryParameters.release = "1.1.1";

    auto sentryErrorCode = Sentry::init(initSentryParameters);
    if (sentryErrorCode != Sentry::EErrorCode::NO_ERROR)
    {
	    std::cout << "Sentry error: " << Sentry::getErrorDescription(sentryErrorCode);
    }

    Sentry::setTag("key", "value");


    for(int i=0; i<5; i++)
    {
        std::string message = "Test breadcrumb ";
        message += std::to_string(i);
        json breadcrumb =
        {
         {"category", "log"},
         {"type", "default"},
         {"level", "info"},
         {"message", message}
        };
        Sentry::addBreadcrumb(breadcrumb);
    }

    Sentry::log(Sentry::EventLevel::LEVEL_ERROR, "This is an error!");

    //foo();
    //bar();
    sendEvent();

    cout << "Hello World!" << endl;
    return 0;
}
