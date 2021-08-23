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
    const std::wstring channel = L"Application";
    const std::wstring xpath_query = L"*[System[(Level <= 3) and TimeCreated[timediff(@SystemTime) <= 86400000]]]";
    const std::filesystem::path bookmark_path = "./bookmark.xml";

    return std::make_unique<WindowsEventListener>(channel, xpath_query, bookmark_path);
}