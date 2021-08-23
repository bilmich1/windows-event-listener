#include "WindowsEventListener.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <fstream>

#include <stringapiset.h>
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
            message_stream << "getEventMessage::EvtRender failed with " << status;
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
    PVOID context = reinterpret_cast<PVOID>(this);

    if (bookmark_path_.has_value())
    {
        if (std::filesystem::exists(*bookmark_path_))
        {
            std::wifstream file(*bookmark_path_);
            std::wstring bookmark_content;

            file.seekg(0, std::ios::end);
            bookmark_content.reserve(file.tellg());
            file.seekg(0, std::ios::beg);

            bookmark_content.assign((std::istreambuf_iterator<wchar_t>(file)), std::istreambuf_iterator<wchar_t>());

            boomark_handle_ = EvtCreateBookmark(bookmark_content.c_str());
        }
        else
        {
            constexpr LPCWSTR create_new_bookmark = nullptr;
            boomark_handle_ = EvtCreateBookmark(create_new_bookmark);
        }
    }

    event_handle_ = EvtSubscribe(local_computer,
        null_signal_event_because_using_callback,
        channel_.c_str(),
        xpath_query_.c_str(),
        boomark_handle_,
        context,
        eventListenerCallback,
        nullptr == boomark_handle_ ? EvtSubscribeStartAtOldestRecord : EvtSubscribeStartAfterBookmark);

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
        if (nullptr != boomark_handle_)
        {
            EvtUpdateBookmark(boomark_handle_, event_handle);
        }

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
    saveBookmark();

    if (boomark_handle_ != nullptr)
    {
        EvtClose(boomark_handle_);
        boomark_handle_ = nullptr;
    }

    if (event_handle_ != nullptr)
    {
        EvtClose(event_handle_);
        event_handle_ = nullptr;
    }
}

void WindowsEventListener::saveBookmark()
{
    if (bookmark_path_.has_value() && nullptr != boomark_handle_)
    {
        constexpr EVT_HANDLE context = nullptr;
        constexpr PVOID get_required_buffer_size = nullptr;
        DWORD unused_property_count = 0;

        DWORD status = ERROR_SUCCESS;
        DWORD buffer_size = 0;
        DWORD buffer_used = 0;


        if (!EvtRender(context, boomark_handle_, EvtRenderBookmark, buffer_size, get_required_buffer_size, &buffer_used, &unused_property_count))
        {
            status = GetLastError();
            if (ERROR_INSUFFICIENT_BUFFER == status)
            {
                buffer_size = buffer_used;
                std::vector<WCHAR> message_content(buffer_size);
                
                EvtRender(context, boomark_handle_, EvtRenderBookmark, buffer_size, message_content.data(), &buffer_used, &unused_property_count);
                status = GetLastError();
                if (ERROR_SUCCESS != status)
                {
                    std::stringstream message_stream;
                    message_stream << "saveBookmark::EvtRender failed with " << status;
                    publish(message_stream.str());
                }

                std::wofstream file(*bookmark_path_, std::ios_base::out | std::ios_base::trunc);
                auto state = file.rdstate();
                for (auto i = message_content.begin(); i != message_content.end() && *i != 0; ++i)
                {
                    file << *i;
                }
            }
        }
    }
}
