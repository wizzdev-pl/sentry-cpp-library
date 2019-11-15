#ifndef SENTRY_TRANSPORT_H
#define SENTRY_TRANSPORT_H

#include "sentry_common.h"

#include "client_http.hpp"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>


/*
 * From: https://docs.sentry.io/development/sdk-dev/unified-api/ :
     * The transport is an internal construct of the client that abstracts away the event sending.
     * Typically the transport runs in a separate thread and gets events to send via a queue.
     * The transport is responsible for sending, retrying and handling rate limits.
     * The transport might also persist unsent events across restarts if needed.
*/


namespace http
{
using Client = ::SimpleWeb::Client<::SimpleWeb::HTTP>;
using Header = ::SimpleWeb::CaseInsensitiveMultimap;
using Response = ::SimpleWeb::ClientBase<asio::basic_stream_socket<asio::ip::tcp>>::Response;
}

namespace
{
constexpr unsigned int RECONNECTION_TIMEOUT_MILLISECONDS = 10000;
}

namespace Sentry
{

class SentryDSN
{
public:

    std::string protocol;
    std::string publicKey;
    std::string secretKey;
    std::string hostPath;
    std::string projectID;

    // {BASE_URI} = '{PROTOCOL}://{HOST}{PATH}'
    std::string baseURI;

    // endpoint = '{BASE_URI}/api/{PROJECT_ID}/store/'
    std::string sentry_endpoint;
    std::string sentry_endpoint_path; // without host

    static EErrorCode parseDSN(const std::string &dsn, SentryDSN* newDSNStruct);
};

class Transport
{
public:

    Transport();
    ~Transport();

    void start();
    void stop();
    EErrorCode setupClient(SentryDSN dsn);
    void sendEvent(const std::string& contents);
    void sendEventRetry(const std::string& contents, const http::Header& header);

private:

    enum class State
    {
        NO_CONNECTION,
        SEND_EVENTS,
        DROP_EVENTS
    };

    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;
    Transport(const Transport&&) = delete;
    Transport&& operator=(const Transport&&) = delete;

    void run();

    template<typename F>
    void addActionToQueue(F actionToEnqueue);

    void perform();

    void performNoConnection();
    void performSendEvents();
    void performDropEvents();

    void changeState(State newState);

    std::chrono::time_point<std::chrono::system_clock> m_lastConnectionRequestTime;
    bool reconnectTimeoutReached();

    uint64_t m_retryAfterMilliseconds;
    std::chrono::time_point<std::chrono::system_clock> m_lastRequestBeforeDroppingTime;
    bool droppingEventsTimeoutReached();

    void sendPost(const std::string& contents);
    void sendPost(const std::string& contents, const http::Header& header, bool isRetrying = false);
    bool checkResponse(std::shared_ptr<http::Response> response);

    void handleTooManyRequests(std::shared_ptr<http::Response> errorResponse);

    //const std::string create_autentication_string();
    http::Header createHeader();

    // its own thread / worker
    std::thread m_thread;
    // tasks queue
    std::queue<std::function<void()>> m_tasks;
    mutable std::mutex m_tasksQueueMutex;

    std::atomic_bool m_running;
    std::atomic_bool m_shouldStop;

    SentryDSN m_dsnStruct;

    std::shared_ptr<http::Client> m_pHttpClient;

    mutable std::mutex m_actionInProgressMutex;
    std::condition_variable m_conditionVariable;

    State m_state;
    mutable std::mutex m_stateMutex;
};

}
#endif // SENTRY_TRANSPORT_H
