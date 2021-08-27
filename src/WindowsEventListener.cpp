#include "WindowsEventListener.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <fstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include <stringapiset.h>
// Link with the event lib
#pragma comment(lib, "wevtapi.lib")

DWORD WINAPI eventListenerCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context, EVT_HANDLE event_handle)
{
    auto* event_listener = reinterpret_cast<WindowsEventListener*>(context);
    return event_listener->callbackImpl(action, event_handle);
}

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

IEventListener::PublishType getEventMessage(EVT_HANDLE event_handle)
{
    DWORD unused_property_count = 0;
    constexpr EVT_HANDLE context = nullptr;
    constexpr PVOID get_required_buffer_size = nullptr;

    DWORD buffer_size = 0;
    DWORD buffer_used = 0;

    if (!EvtRender(context, event_handle, EvtRenderEventXml, buffer_size, get_required_buffer_size, &buffer_used, &unused_property_count))
    {
        if (auto status = GetLastError(); ERROR_INSUFFICIENT_BUFFER == status)
        {
            buffer_size = buffer_used;
            std::vector<WCHAR> message_content(buffer_size);
            EvtRender(context, event_handle, EvtRenderEventXml, buffer_size, message_content.data(), &buffer_used, &unused_property_count);
            if (auto status = GetLastError(); ERROR_SUCCESS != status)
            {
                std::stringstream message_stream;
                message_stream << "saveBookmark::EvtRender failed with " << status;
                return std::make_exception_ptr(std::runtime_error((message_stream.str())));
            }

            namespace pt = boost::property_tree;

            pt::ptree tree;
            std::stringstream string_stream;
            string_stream << toString(message_content);

            pt::read_xml(string_stream, tree);
            return tree;
        }
    }

    auto status = GetLastError();
    std::stringstream message_stream;
    message_stream << "saveBookmark::EvtRender failed with " << status;
    return std::make_exception_ptr(std::runtime_error((message_stream.str())));

}

std::wstring readFileContent(const std::filesystem::path& file_path)
{
    namespace pt = boost::property_tree;

    pt::wptree tree;
    pt::read_xml(file_path.string(), tree);

    std::wostringstream string_stream;
    pt::write_xml(string_stream, tree);

    return string_stream.str();
}

WindowsEventListener::WindowsEventListener(
    const std::filesystem::path& query_path,
    const std::optional<std::filesystem::path>& bookmark_path)
    : query_path_(query_path)
    , bookmark_path_(bookmark_path)
{
    if (!std::filesystem::exists(query_path_))
    {
        throw std::runtime_error("query_path doesn't exists");
    }
}

WindowsEventListener::~WindowsEventListener()
{
    cleanup();
}

void WindowsEventListener::start()
{
    constexpr EVT_HANDLE local_computer = nullptr;
    constexpr HANDLE null_signal_event_because_using_callback = nullptr;
    constexpr LPCWSTR unused_channel_name = nullptr;
    PVOID context = reinterpret_cast<PVOID>(this);

    if (bookmark_path_.has_value())
    {
        if (std::filesystem::exists(*bookmark_path_))
        {
            const auto bookmark_content = readFileContent(*bookmark_path_);
            boomark_handle_ = EvtCreateBookmark(bookmark_content.c_str());
        }
        else
        {
            constexpr LPCWSTR create_new_bookmark = nullptr;
            boomark_handle_ = EvtCreateBookmark(create_new_bookmark);
        }
    }

    auto query = readFileContent(query_path_);

    event_handle_ = EvtSubscribe(local_computer,
        null_signal_event_because_using_callback,
        unused_channel_name,
        query.c_str(),
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

        throw std::runtime_error(message_stream.str());
        cleanup();
    }
}

DWORD WindowsEventListener::callbackImpl(EVT_SUBSCRIBE_NOTIFY_ACTION action, EVT_HANDLE event_handle)
{
    DWORD status = ERROR_SUCCESS;

    std::stringstream message_stream;

    switch (action)
    {
    case EvtSubscribeActionDeliver:
    {
        if (nullptr != boomark_handle_)
        {
            EvtUpdateBookmark(boomark_handle_, event_handle);
        }

        publish(getEventMessage(event_handle));
        constexpr auto ignored = ERROR_SUCCESS;
        return ignored;
    }
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
    default:
    {
        message_stream << "SubscriptionCallback: Unknown action.\n";
    }
    }

    publish(std::make_exception_ptr(std::runtime_error((message_stream.str()))));
    cleanup();

    constexpr auto ignored = ERROR_SUCCESS;
    return ignored;
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
            if (status = GetLastError(); ERROR_INSUFFICIENT_BUFFER == status)
            {
                buffer_size = buffer_used;
                std::vector<WCHAR> message_content(buffer_size);

                EvtRender(context, boomark_handle_, EvtRenderBookmark, buffer_size, message_content.data(), &buffer_used, &unused_property_count);
                if (status = GetLastError(); ERROR_SUCCESS != status)
                {
                    std::stringstream message_stream;
                    message_stream << "saveBookmark::EvtRender failed with " << status;
                    publish(std::make_exception_ptr(std::runtime_error((message_stream.str()))));
                }

                try
                {
                    namespace pt = boost::property_tree;

                    pt::ptree tree;
                    std::stringstream string_stream;
                    string_stream << toString(message_content);

                    pt::read_xml(string_stream, tree);
                    pt::write_xml(bookmark_path_->string(), tree);
                }
                catch (const std::exception& e)
                {
                    std::stringstream message_stream;
                    message_stream << "saveBookmark save failed: " << e.what();
                    publish(std::make_exception_ptr(std::runtime_error((message_stream.str()))));
                }
            }
        }
    }
}
