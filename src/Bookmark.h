#pragma once

#include "Details.h"

#include <Windows.h>
#include <winevt.h>
#include <filesystem>

class Bookmark
{
public:
    Bookmark(const std::filesystem::path& bookmark_path);
    ~Bookmark();

    EVT_HANDLE getHandle() const;
    void update(EVT_HANDLE event_handle);
    void save() const;

private:
    const std::filesystem::path bookmark_path_;
    EventHandleUniquePtr bookmark_handle_ = nullptr;
};
