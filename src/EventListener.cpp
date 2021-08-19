#include "EventListener.h"

#include <chrono>

EventListener::EventListener()
{
    stop_ = false;
    thread_ = std::thread([this]() { work(); });
}

EventListener::~EventListener()
{
    stop_ = true;
    if (thread_.joinable())
    {
        thread_.join();
    }
}

void EventListener::registerObserver(EventListener::event_observer observer)
{
    std::unique_lock<std::mutex> lock(observers_mutex_);
    event_observers_.push_back(observer);
}

void EventListener::publish(const std::string& event) const
{
    std::unique_lock<std::mutex> lock(observers_mutex_);
    for (const auto& observer : event_observers_)
    {
        observer(event);
    }
}

void EventListener::work() const
{
    while (!stop_)
    {
        publish("This is a message");
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
