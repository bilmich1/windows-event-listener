#include "WindowsEventListener.h"

#include <stringapiset.h>
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

std::string toString(const std::vector<WCHAR>& wide_string_data)
{
    constexpr UINT default_code_page = CP_ACP;
    constexpr DWORD flags = 0;
    constexpr int null_terminated_string = -1;
    constexpr LPSTR unused_buffer = nullptr;
    constexpr int get_buffer_size = 0;
    constexpr LPCCH use_default_char = nullptr;
    BOOL has_used_default_char = 0;

    const auto output_size = WideCharToMultiByte(default_code_page,
        flags,
        wide_string_data.data(),
        null_terminated_string,
        unused_buffer,
        get_buffer_size,
        use_default_char,
        &has_used_default_char);

    if (output_size == 0)
    {
        throw std::runtime_error("WideCharToMultiByte returned 0");
    }

    std::vector<CHAR> output_buffer(output_size);
    WideCharToMultiByte(default_code_page,
        flags,
        wide_string_data.data(),
        null_terminated_string,
        output_buffer.data(),
        output_size,
        use_default_char,
        &has_used_default_char);

    return std::string(output_buffer.begin(), output_buffer.end());
}

EventMessage getEventMessage(EVT_HANDLE event_handle)
{
    DWORD unused_property_count = 0;
    constexpr EVT_HANDLE context = nullptr;
    constexpr PVOID get_required_buffer_size = nullptr;

    DWORD status = ERROR_SUCCESS;
    DWORD buffer_size = 0;
    DWORD buffer_used = 0;

    std::stringstream message_stream;

    if (!EvtRender(context, event_handle, EvtRenderEventXml, buffer_size, get_required_buffer_size, &buffer_used, &unused_property_count))
    {
        status = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER == status)
        {
            buffer_size = buffer_used;
            std::vector<WCHAR> message_content(buffer_size);
            EvtRender(context, event_handle, EvtRenderEventXml, buffer_size, message_content.data(), &buffer_used, &unused_property_count);
            message_stream << toString(message_content);
        }

        status = GetLastError();
        if (ERROR_SUCCESS != status)
        {
            message_stream << "EvtRender failed with " << status;
        }
    }

    return { status, message_stream.str() };
}

WindowsEventListener::WindowsEventListener(const std::wstring& channel,
    const std::wstring& xpath_query,
    const std::optional<std::filesystem::path>& bookmark_path)
    : channel_(channel)
    , xpath_query_(xpath_query)
    , bookmark_path_(bookmark_path)
{
}

WindowsEventListener::~WindowsEventListener()
{
    cleanup();
}

void WindowsEventListener::start()
{
    constexpr EVT_HANDLE local_computer = nullptr;
    constexpr HANDLE null_signal_event_because_using_callback = nullptr;
    constexpr EVT_HANDLE bookmark = nullptr;
    PVOID context = reinterpret_cast<PVOID>(this);

    event_handle_ = EvtSubscribe(local_computer,
        null_signal_event_because_using_callback,
        channel_.c_str(),
        xpath_query_.c_str(),
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
            message_stream << "Channel was not found\n";
        }
        else if (ERROR_EVT_INVALID_QUERY == status)
        {
            message_stream << "Query is not valid\n";

            DWORD buffer_size = 0;
            EvtGetExtendedStatus(0, nullptr, &buffer_size);
            std::vector<WCHAR> buffer(buffer_size);
            EvtGetExtendedStatus(buffer_size, buffer.data(), &buffer_size);

            message_stream << "details: " << toString(buffer) << "\n";
        }
        else
        {
            message_stream << "EvtSubscribe failed with code: " << status << "\n";
        }

        publish(message_stream.str());
        cleanup();
    }
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
        const auto event_code = reinterpret_cast<DWORD_PTR>(event_handle);
        if (ERROR_EVT_QUERY_RESULT_STALE == event_code)
        {
            message_stream << "The subscription callback was notified that event records are missing.\n";
            // Handle if this is an issue for your application.
        }
        else
        {
            message_stream << "The subscription callback received the following Win32 error: " << event_code << "\n";
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
