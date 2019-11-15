#include "scope.h"

#include "sentry_common.h"

#include <unistd.h> //gethostname


using json = nlohmann::json;

namespace Sentry
{

Scope::Scope()
:
m_userName(),
m_transactionName(),
m_release(),
m_level(EventLevel::LEVEL_INFO),
m_maxBreadcrumbs(100),
m_breadcrumbs(),
m_extras(),
m_tags()
{
    clear();
    setDefaultTags();
}

void Scope::setDefaultTags()
{
    // TODO add more general solution!
    // Linux only for now

    char* userLogin = getlogin();
    if (userLogin != nullptr)
    {
        setTag("user_login", std::string(userLogin));
    }
    const int hostnameLen = 50;
    char hostName[hostnameLen];
    int success = gethostname(hostName, hostnameLen);
    if (success == 0)
    {
       setTag("server_name", std::string(hostName));
    }
}

void Scope::setUser(const std::string& user)
{
    m_userName = user;
}

void Scope::setTransaction(const std::string& transaction_name)
{
    m_transactionName = transaction_name;
}

void Scope::setExtra(const std::string& key, const std::string& value="")
{
    setValueMap(key, value, &m_extras);
}

void Scope::setExtras(json extras)
{
    m_extras.insert(extras.begin(), extras.end());
}

void Scope::setTag(const std::string& key, const std::string& value)
{
    setValueMap(key, value, &m_tags);
}

void Scope::setTags(json tags)
{
    m_tags.insert(tags.begin(), tags.end());
}

void Scope::setLevel(EventLevel level)
{
    m_level = level;
}

void Scope::setRelease(const std::string& release)
{
    m_release = release;
}

void Scope::addBreadcrumb(json crumb)
{
    m_breadcrumbs.push_back(crumb);
    if (m_breadcrumbs.size() > m_maxBreadcrumbs)
    {
        m_breadcrumbs.pop_front();
    }
}

void Scope::clearBreadcrumbs()
{
    m_breadcrumbs.clear();
}

void Scope::clear()
{
    //TODO set default values!!!
    clearBreadcrumbs();
}

void Scope::setMaxBreadcrumbs(uint16_t maxBreadcrumbs)
{
    m_maxBreadcrumbs = maxBreadcrumbs;
}

void Scope::applyToEvent(json &event)
{
    if (!m_breadcrumbs.empty())
        event.push_back({"breadcrumbs", getBreadcrumbs()});
    if (!m_tags.empty())
        event.push_back({"tags", getTags()});
    if (!m_extras.empty())
        event.push_back({"extra", getExtras()});
    if (m_transactionName != "")
        event.push_back({"transaction", getTransaction()});
    if (m_release != "")
        event.push_back({"release", getRelease()});

    event.push_back({"level", getLevelStr()});

    //event.push_back({"fingerprint", }) //TODO
}

void Scope::setValueMap(std::string key, std::string value, json* map_object)
{
    if (value == "")
    {
        if (map_object->find(key) != map_object->end())
        {
            map_object->erase(key);
        }
    }
    else
    {
        map_object->emplace(key, value);
    }
}

const std::string& Scope::getUser()
{
    return m_userName;
}

const std::string& Scope::getTransaction()
{
    return m_transactionName;
}

json Scope::getExtras()
{
    return m_extras;
}

json Scope::getTags()
{
    return m_tags;
}

json Scope::getContexts()
{
    return m_contexts;
}

const EventLevel& Scope::getLevel()
{
    return m_level;
}

const std::string& Scope::getRelease()
{
    return m_release;
}

std::string Scope::getLevelStr()
{ 
    return levelToString(m_level);
}

json Scope::getBreadcrumbs()
{
    json breadcrumbs;
    breadcrumbs["values"] = m_breadcrumbs;
    return breadcrumbs;
}

} // namespace Sentry'





