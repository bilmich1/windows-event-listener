#pragma once

#include <vector>
#include <functional>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>

class EventListener
{
public:
    using event_observer = std::function<void(const std::string&)>;

    EventListener();
    ~EventListener();

    void registerObserver(event_observer observer);

private:
    void publish(const std::string& event) const;

    mutable std::mutex observers_mutex_;
    std::vector<event_observer> event_observers_;

    std::thread thread_;
    std::atomic_bool stop_;
    void work() const;
};