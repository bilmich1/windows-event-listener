#pragma once

#include "EventListener.h"
#include "Details.h"
#include "Bookmark.h"

#include <Windows.h>
#include <winevt.h>

#include <filesystem>
#include <optional>

DWORD WINAPI eventListenerCallback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID context, EVT_HANDLE event_handle);

class WindowsEventListener : public IEventListener
{
public:
    WindowsEventListener(const std::filesystem::path& query_path,
        const std::optional<std::filesystem::path>& bookmark_path);
    ~WindowsEventListener() override;

    void start() override;

    void updateBookmark() override;

private:
    friend DWORD WINAPI eventListenerCallback(
        EVT_SUBSCRIBE_NOTIFY_ACTION action,
        PVOID context,
        EVT_HANDLE event_handle);
    DWORD callbackImpl(EVT_SUBSCRIBE_NOTIFY_ACTION action, EventHandleUniquePtr event_handle) noexcept;

    const std::filesystem::path query_path_;
    const std::optional<std::filesystem::path> bookmark_path_;

    std::optional<Bookmark> bookmark_ = std::nullopt;
    EventHandleUniquePtr subscription_handle_ = nullptr;
};
