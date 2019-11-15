#include "sentry_common.h"
#include "transport.h"

#include <regex>
#include <sstream>


#define SENTRY_VERSION 7


namespace Sentry
{

EErrorCode SentryDSN::parseDSN(const std::string& dsn, SentryDSN* newDSNStruct)
{
    // '{PROTOCOL}://{PUBLIC_KEY}:{SECRET_KEY}@{HOST_PATH}/{PROJECT_ID}'
    std::regex re("(.*)://(.*)(:.*)*@(.*)/(.*)");

    std::smatch match;
    if (std::regex_search(dsn, match, re))
    {
        newDSNStruct->protocol = match.str(1);
        newDSNStruct->publicKey = match.str(2);
        newDSNStruct->secretKey = match.str(3);
        newDSNStruct->hostPath = match.str(4);
        newDSNStruct->projectID = match.str(5);

        newDSNStruct->baseURI = newDSNStruct->protocol + "://" + newDSNStruct->hostPath;

        newDSNStruct->sentry_endpoint_path = "/api/" + newDSNStruct->projectID + "/store/";
        newDSNStruct->sentry_endpoint = newDSNStruct->baseURI + newDSNStruct->sentry_endpoint_path;

        return EErrorCode::NO_ERROR;
    }

    else
    {
        return EErrorCode::WRONG_DSN;
    }

}

Transport::Transport()
:
m_lastConnectionRequestTime(),
m_thread(),
m_tasks(),
m_tasksQueueMutex(),
m_running(false),
m_shouldStop(false),
m_pHttpClient(nullptr),
m_actionInProgressMutex(),
m_conditionVariable(),
m_state(State::NO_CONNECTION),
m_stateMutex()
{

}

Transport::~Transport()
{

}

void Transport::start()
{
    //start thread
    m_thread = std::thread(&Transport::run, this);

    m_running = true;
}

EErrorCode Transport::setupClient(SentryDSN dsn)
{
    m_dsnStruct = dsn;

#ifdef DEBUG_SENTRYCPP
        std::stringstream ss;
        ss << "Protocol: " << m_dsnStruct.protocol
           << " public key: " << m_dsnStruct.publicKey
           << " host path: " << m_dsnStruct.hostPath;
        if (m_dsnStruct.secretKey != "")
            ss << ", " << "sentry_secret key: " << m_dsnStruct.secretKey;
        ss << std::endl << "Endpoint path: " << m_dsnStruct.sentry_endpoint_path;
        LOG(ss.str());
#endif // DEBUG_SENTRYCPP

    m_pHttpClient = std::make_shared<http::Client>(m_dsnStruct.hostPath);
    m_pHttpClient->config.timeout = HTTP_CLIENT_TIMOUT_IN_SECONDS;  /// ? no set func???

    return EErrorCode::NO_ERROR;
}

void Transport::stop()
{
    // TODO: save events not sent?
    m_shouldStop = true;
    m_thread.join();
}

void Transport::run()
{
    while(!m_shouldStop)
    {
        perform();
    }
    // perform until the list of events to send is empty
    while(!m_tasks.empty())
    {
        perform();
    }
    m_running = false;
}

template<typename F>
void Transport::addActionToQueue(F actionToEnqueue)
{
    if (!m_shouldStop)
    {
        std::lock_guard<std::mutex> lock(m_tasksQueueMutex);
        m_tasks.push(actionToEnqueue);
    }
}

void Transport::perform()
{
    // take request from queue
    switch (m_state)
    {
    case State::NO_CONNECTION:

        performNoConnection();
        break;

    case State::SEND_EVENTS:

        performSendEvents();
        break;

    case State::DROP_EVENTS:
        break;

    default:
        //error not recognised state!
        break;
    }
}

void Transport::performNoConnection()
{
    if (reconnectTimeoutReached())
    {
        changeState(State::SEND_EVENTS);
    }
    else
    {
        // do nothing
    }
}

bool Transport::reconnectTimeoutReached()
{
    auto timeSinceLastRequest = std::chrono::system_clock::now() - m_lastConnectionRequestTime;

    return (std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceLastRequest).count() >= RECONNECTION_TIMEOUT_MILLISECONDS);
}

void Transport::performSendEvents()
{
    if (m_tasks.empty())
        return;

    std::function<void()>  funcToExecute;
    {
        std::lock_guard<std::mutex> lock(m_tasksQueueMutex);
        assert(m_tasks.size() > 0);
        funcToExecute = m_tasks.front();
        m_tasks.pop();
    }

    if (funcToExecute)
    {
        funcToExecute();
    }
}

void Transport::performDropEvents()
{
    if (droppingEventsTimeoutReached())
    {
        changeState(State::SEND_EVENTS);
    }
    else
    {
        // do nothing
    }
}

bool Transport::droppingEventsTimeoutReached()
{
    auto timeSinceLastRequest = std::chrono::system_clock::now() - m_lastRequestBeforeDroppingTime;

    return (std::chrono::duration_cast<std::chrono::milliseconds>(timeSinceLastRequest).count() >= m_retryAfterMilliseconds);
}

void Transport::changeState(State newState)
{
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_state = newState;
}

void Transport::sendEvent(const std::string& contents)
{
    auto actionToEnqueue = [=] () { sendPost(contents); };

    addActionToQueue(actionToEnqueue);
}

void Transport::sendEventRetry(const std::string& contents, const http::Header& header)
{
    auto actionToEnqueue = [=]() { sendPost(contents, header, true); };

    addActionToQueue(actionToEnqueue);
}

void Transport::sendPost(const std::string& content)
{
    if (m_pHttpClient == nullptr)
    {
        //raise - client not set!
        return;
    }
    auto header = createHeader();

#ifdef DEBUG_SENTRYCPP
    std::stringstream ss;
    ss << "Path: " << m_dsnStruct.sentry_endpoint_path << "Request headers: " << std::endl;
    for (auto h : header) {
        ss << h.first << ": " << h.second << std::endl;
    }
    ss << "Request contents: " << content << std::endl;
    ss << "Http client url: " << m_dsnStruct.sentry_endpoint << std::endl;
    LOG(ss.str());
#endif // DEBUG_SENTRYCPP

    sendPost(content, header);
}

void Transport::sendPost(const std::string& content, const http::Header& header, bool isRetrying)
{
    try
    {
        auto response = m_pHttpClient->request(POST, m_dsnStruct.sentry_endpoint, content, header);

        //check response
        bool ok = checkResponse(response);
        if ((!ok) && (!isRetrying))
        {
            sendEventRetry(content, header);
        }

    }
    catch(SimpleWeb::system_error& e)
    {
        LOG("Could not send event! ");
        LOG(e.code().message());
        LOG(e.what());
        changeState(State::NO_CONNECTION); // ? does every error mean that?
        m_lastConnectionRequestTime = std::chrono::system_clock::now();
    }
}

http::Header Transport::createHeader()
{
    /*
     * Authentication header:
     * X-Sentry-Auth: Sentry sentry_version=5,
      sentry_client=<client version, arbitrary>,
      sentry_timestamp=<current timestamp>,
      sentry_key=<public api key>,
      sentry_secret=<secret api key>
    */

    http::Header header;
    std::string client_version = "sentry_cpp/0.1";
    const auto tp = std::chrono::system_clock::now();
    const auto dur = tp.time_since_epoch();
    std::string timestamp = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(dur).count());

    std::string header_info;
    {
    std::stringstream ss;
    ss << "Sentry " << "sentry_version=" << SENTRY_VERSION << ", "
       << "sentry_client=" << client_version << ", "
       << "sentry_timestamp=" << timestamp << ", "
       << "sentry_key=" << m_dsnStruct.publicKey;
    if (m_dsnStruct.secretKey != "")
        ss << ", " << "sentry_secret=" << m_dsnStruct.secretKey;
    header_info = ss.str();
    }

    header.emplace("X-Sentry-Auth", header_info);
    header.emplace("Content-Type", "application/json");
    header.emplace("User-Agent", client_version);

    return header;
}

bool Transport::checkResponse(std::shared_ptr<http::Response> response)
{
    if (SimpleWeb::status_code(response->status_code) == SimpleWeb::StatusCode::success_ok)
    {
        return true;
    }
    else if (SimpleWeb::status_code(response->status_code) == SimpleWeb::StatusCode::client_error_too_many_requests)
    {
        handleTooManyRequests(response);
        return true;
    }
    else
    {
#ifdef DEBUG_SENTRYCPP
            std::stringstream ss;
            ss << "Response "
               << "status code: " << response->status_code << std::endl;
            ss << "Response headers" << std::endl;
            for (auto h : response->header) {
                ss << h.first << ": " << h.second << std::endl;
            }
            ss << "Response content: " << response->content.string() << std::endl;
            LOG(ss.str());
#endif //DEBUG_SENTRYCPP
        return false;
    }
}

void Transport::handleTooManyRequests(std::shared_ptr<http::Response> errorResponse)
{
    m_retryAfterMilliseconds = RECONNECTION_TIMEOUT_MILLISECONDS; // default
    for (auto& h : errorResponse->header) {
        if (h.first == "Retry-After")
        {
            m_retryAfterMilliseconds = std::stoul(h.second.c_str()) * MILLISECONDS_IN_SECOND;
        }
    }
    changeState(State::DROP_EVENTS);
}

} // namespace
