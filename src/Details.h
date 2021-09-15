#pragma once

#include <Windows.h>
#include <winevt.h>

#include <memory>
#include <filesystem>
#include <string>

#include <boost/property_tree/ptree.hpp>

struct EventHandleDeleter
{
    void operator()(void* handle)
    {
        if (nullptr != handle)
        {
            EvtClose(handle);
        }
    }
};

using EventHandleUniquePtr = std::unique_ptr<void, EventHandleDeleter>;

std::string toString(const std::vector<WCHAR>& wide_string_data);

std::wstring readXmlFile(const std::filesystem::path& file_path);

boost::property_tree::ptree renderEvent(EVT_HANDLE event_handle, EVT_RENDER_FLAGS render_flag);
