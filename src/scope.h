#ifndef SENTRY_SCOPE_H
#define SENTRY_SCOPE_H

#include "sentry_common.h"
#include "json.h"

#include <iostream>
#include <list>
#include <map>


/*
 * A scope holds data that should implicitly be sent with Sentry events.
 * It can hold context data, extra parameters, level overrides, fingerprints etc.
 */

using json = nlohmann::json;

namespace Sentry
{

namespace sentry_types
{
using Extras = std::map<std::string, std::string>;
using Tags = std::map<std::string, std::string>;
}

class Scope
{

public:

    Scope();

    void setMaxBreadcrumbs(uint16_t maxBreadcrumbs);
    void setUser(const std::string& user);
    void setExtra(const std::string& key, const std::string& value);
    void setExtras(json extras);
    void setTag(const std::string& key, const std::string& value);
    void setTags(json tags);
    void setContext(const std::string& key, const std::string& value);
    void setLevel(EventLevel level);
    void setTransaction(const std::string& transaction_name);
    void setRelease(const std::string& release);
    //void set_fingerprint(xxx);
    //void add_event_processor(processor);
    //void add_error_processor(processor);
    void clear();
    void addBreadcrumb(json crumb);
    void clearBreadcrumbs();
    void applyToEvent(json &event);

    // change to json all !
    const std::string& getUser();
    const std::string& getTransaction();
    const EventLevel& getLevel();
    std::string getLevelStr();
    const std::string& getRelease();

    json getExtras();
    json getTags();
    json getContexts();
    json getBreadcrumbs();

private:

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    Scope(const Scope&&) = delete;
    Scope&& operator=(const Scope&&) = delete;

    void setDefaultTags();
    void setValueMap(std::string key, std::string value, json* map_object);

    std::string m_userName;
    std::string m_transactionName;
    std::string m_release;
    EventLevel m_level;
    std::vector<std::string> m_fingerprint;

    uint16_t m_maxBreadcrumbs;
    std::list<json> m_breadcrumbs;

    json m_extras;
    json m_tags;
    json m_contexts;

};

} // namespace Sentry

#endif // SENTRY_SCOPE_H
