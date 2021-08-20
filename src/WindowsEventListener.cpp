#include "WindowsEventListener.h"

#include <sstream>

// Link with the event lib
#pragma comment(lib, "wevtapi.lib")

DWORD WINAPI eventListenerCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context, EVT_HANDLE event_handle)
{
    auto* event_listener = reinterpret_cast<WindowsEventListener*>(context);
    return event_listener->callbackImpl(action, event_handle);
}

struct EventMessage
{
    DWORD status_ = ERROR_SUCCESS;
    std::string message_;
};

EventMessage getEventMessage(EVT_HANDLE event_handle)
{
    return { ERROR_SUCCESS, "EventMessage" };
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
        eventListenerCallback,
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

DWORD WindowsEventListener::callbackImpl(EVT_SUBSCRIBE_NOTIFY_ACTION action, EVT_HANDLE event_handle)
{
    DWORD status = ERROR_SUCCESS;

    std::stringstream message_stream;

    switch (action)
    {
        // You should only get the EvtSubscribeActionError action if your subscription flags 
        // includes EvtSubscribeStrict and the channel contains missing event records.
    case EvtSubscribeActionError:
    {
        if (ERROR_EVT_QUERY_RESULT_STALE == (DWORD)event_handle)
        {
            message_stream << "The subscription callback was notified that event records are missing.\n";
            // Handle if this is an issue for your application.
        }
        else
        {
            message_stream << "The subscription callback received the following Win32 error: " << (DWORD)event_handle << "\n";
        }
        break;
    }
    case EvtSubscribeActionDeliver:
    {
        const auto event_message = getEventMessage(event_handle);
        status = event_message.status_;
        message_stream << event_message.message_;
        break;
    }
    default:
    {
        message_stream << "SubscriptionCallback: Unknown action.\n";
    }
    }

    publish(message_stream.str());

    if (ERROR_SUCCESS != status)
    {

        cleanup();
    }

    return status;
}

void WindowsEventListener::cleanup()
{
    if (event_handle_ != nullptr)
    {
        EvtClose(event_handle_);
        event_handle_ = nullptr;
    }
}
