#include "WindowsEventListener.h"

#include <algorithm>
#include <iterator>
#include <sstream>
#include <fstream>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/format.hpp>

// Link with the event lib
#pragma comment(lib, "wevtapi.lib")

DWORD WINAPI eventListenerCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context, EVT_HANDLE event_handle)
{
    auto* event_listener = reinterpret_cast<WindowsEventListener*>(context);
    return event_listener->callbackImpl(action, EventHandleUniquePtr(event_handle));
}


WindowsEventListener::WindowsEventListener(
    const std::filesystem::path& query_path,
    const std::optional<std::filesystem::path>& bookmark_path)
    : query_path_(query_path)
    , bookmark_path_(bookmark_path)
{
    if (!std::filesystem::exists(query_path_))
    {
        const auto message = boost::format("Query file doesn't exist: %1%") % query_path;
        throw std::runtime_error(message.str());
    }
}

WindowsEventListener::~WindowsEventListener()
{
    EvtClose(subscription_handle_.release());
    bookmark_.reset();
}

void WindowsEventListener::start()
{
    constexpr EVT_HANDLE local_computer = nullptr;
    constexpr HANDLE null_signal_event_because_using_callback = nullptr;
    constexpr LPCWSTR unused_channel_name = nullptr;
    PVOID context = reinterpret_cast<PVOID>(this);

    if (bookmark_path_.has_value())
    {
        bookmark_.emplace(*bookmark_path_);
    }

    auto query = readXmlFile(query_path_);

    subscription_handle_.reset(EvtSubscribe(local_computer,
        null_signal_event_because_using_callback,
        unused_channel_name,
        query.c_str(),
        bookmark_.has_value() ? bookmark_->getHandle() : nullptr,
        context,
        eventListenerCallback,
        bookmark_.has_value() ? EvtSubscribeStartAfterBookmark : EvtSubscribeStartAtOldestRecord));

    if (subscription_handle_ == nullptr)
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
    }
}

void WindowsEventListener::updateBookmark()
{
    if (bookmark_.has_value())
    {
        bookmark_->save();
    }
}

DWORD WindowsEventListener::callbackImpl(EVT_SUBSCRIBE_NOTIFY_ACTION action, EventHandleUniquePtr event_handle) noexcept
{
    try
    {
        switch (action)
        {
        case EvtSubscribeActionDeliver:
        {
            if (bookmark_.has_value())
            {
                bookmark_->update(event_handle.get());
            }

            constexpr EVT_RENDER_FLAGS render_flag = EvtRenderEventXml;
            const auto event_message = renderEvent(event_handle.get(), render_flag);

            publish(event_message);

            break;
        }
        default:
        {
            throw std::runtime_error("WindowsEventListener::callbackImpl failed with: Unkown action");
        }
        }
    }
    catch (const std::exception&)
    {
        publish(std::current_exception());
    }

    constexpr auto ignored = ERROR_SUCCESS;
    return ignored;
}
