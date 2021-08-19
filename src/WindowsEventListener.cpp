#include "WindowsEventListener.h"

#include <sstream>

DWORD WINAPI WindowsEventListenerCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent)
{
    auto* event_listener = reinterpret_cast<WindowsEventListener*>(pContext);
    return event_listener->callbackImpl(action, hEvent);
}

WindowsEventListener::WindowsEventListener()
{
    LPWSTR channel_name = L"<channel name goes here>";
    LPWSTR xpath_query = L"<xpath query goes here>";

    constexpr EVT_HANDLE local_computer = nullptr;
    constexpr HANDLE null_signal_event_because_using_callback = nullptr;
    constexpr EVT_HANDLE bookmark = nullptr;
    PVOID context = reinterpret_cast<PVOID>(this);

    using namespace std::placeholders;

    event_handle_ = EvtSubscribe(local_computer,
        null_signal_event_because_using_callback,
        channel_name,
        xpath_query,
        bookmark,
        context,
        WindowsEventListenerCallback,
        EvtSubscribeStartAtOldestRecord);

    if (event_handle_ == nullptr)
    {
        auto status = GetLastError();

        std::stringstream message_stream;

        if (ERROR_EVT_CHANNEL_NOT_FOUND == status)
        {
            message_stream << "Channel was not found: " << channel_name;
        }
        else if (ERROR_EVT_INVALID_QUERY == status)
        {
            message_stream << "Query is not valid: " << xpath_query << "\n";

            DWORD buffer_size = 0;
            EvtGetExtendedStatus(0, nullptr, &buffer_size);
            LPWSTR buffer = new WCHAR[buffer_size];
            EvtGetExtendedStatus(buffer_size, buffer, &buffer_size);

            message_stream << "details: " << buffer << "\n";
        }
        else
        {
            message_stream << "EvtSubscribe failed with code: " << status << "\n";
        }

        publish(message_stream.str());
        cleanup();
    }
}

WindowsEventListener::~WindowsEventListener()
{
    cleanup();
}

DWORD WindowsEventListener::callbackImpl(EVT_SUBSCRIBE_NOTIFY_ACTION action, EVT_HANDLE hEvent)
{
    return 0;
}

void WindowsEventListener::cleanup()
{
    if (event_handle_ != nullptr)
    {
        EvtClose(event_handle_);
        event_handle_ = nullptr;
    }
}
