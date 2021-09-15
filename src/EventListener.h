#pragma once

#include <boost/property_tree/ptree.hpp>

#include <vector>
#include <functional>
#include <string>
#include <mutex>
#include <variant>

class IEventListener
{
public:
    using PublishType = std::variant<boost::property_tree::ptree, std::exception_ptr>;
    using event_observer = std::function<void(const PublishType&)>;

    virtual ~IEventListener() = default;

    void registerObserver(event_observer observer);

    virtual void start() = 0;

    virtual void updateBookmark() = 0;

protected:
    void publish(const PublishType& event) const;

private:
    mutable std::mutex observers_mutex_;
    std::vector<event_observer> event_observers_;
};

std::unique_ptr<IEventListener> makeEventListener();