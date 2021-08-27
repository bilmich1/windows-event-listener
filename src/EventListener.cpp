#include "EventListener.h"
#include "WindowsEventListener.h"

#include <functional>

void IEventListener::registerObserver(IEventListener::event_observer observer)
{
    std::unique_lock<std::mutex> lock(observers_mutex_);
    event_observers_.push_back(observer);
}

void IEventListener::publish(const PublishType& event) const
{
    std::unique_lock<std::mutex> lock(observers_mutex_);
    for (const auto& observer : event_observers_)
    {
        observer(event);
    }
}

std::unique_ptr<IEventListener> makeEventListener()
{
    const std::filesystem::path query_path = "C:\\_Projet\\windows-event-listener\\out\\Query.xml";
    const std::filesystem::path bookmark_path = "C:\\_Projet\\windows-event-listener\\out\\Bookmark.xml";

    return std::make_unique<WindowsEventListener>(query_path, bookmark_path);
}