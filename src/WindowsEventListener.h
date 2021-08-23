#pragma once

#include "EventListener.h"

#include <Windows.h>
#include <winevt.h>

#include <filesystem>
#include <optional>

class WindowsEventListener : public IEventListener
{
public:
    WindowsEventListener(const std::filesystem::path& query_path,
        const std::optional<std::filesystem::path>& bookmark_path);
    ~WindowsEventListener() override;

    void start() override;

    DWORD callbackImpl(EVT_SUBSCRIBE_NOTIFY_ACTION action, EVT_HANDLE event_handle);

private:
    void cleanup();
    void saveBookmark();

    const std::filesystem::path query_path_;
    const std::optional<std::filesystem::path> bookmark_path_;

    EVT_HANDLE event_handle_ = nullptr;
    EVT_HANDLE boomark_handle_ = nullptr;
};
