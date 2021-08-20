#pragma once

#include <vector>
#include <functional>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

class IEventListener
{
public:
    using event_observer = std::function<void(const std::string&)>;

    void registerObserver(event_observer observer);

    virtual void start() = 0;

protected:
    void publish(const std::string& event) const;

private:
    mutable std::mutex observers_mutex_;
    std::vector<event_observer> event_observers_;
};

std::unique_ptr<IEventListener> makeEventListener();