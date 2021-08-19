#include "EventListener.h"
#include "WindowsEventListener.h"

#include <functional>

void IEventListener::registerObserver(IEventListener::event_observer observer)
{
    std::unique_lock<std::mutex> lock(observers_mutex_);
    event_observers_.push_back(observer);
}

void IEventListener::publish(const std::string& event) const
{
    std::unique_lock<std::mutex> lock(observers_mutex_);
    for (const auto& observer : event_observers_)
    {
        observer(event);
    }
}

std::unique_ptr<IEventListener> makeEventListener()
{
    return std::make_unique<WindowsEventListener>();
}