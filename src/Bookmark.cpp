#include "Bookmark.h"

#include <boost/property_tree/xml_parser.hpp>
#include <boost/format.hpp>

Bookmark::Bookmark(const std::filesystem::path& bookmark_path)
    : bookmark_path_(bookmark_path)
{
    if (std::filesystem::exists(bookmark_path_))
    {
        const auto bookmark = readXmlFile(bookmark_path_);
        bookmark_handle_.reset(EvtCreateBookmark(bookmark.c_str()));
    }
    else
    {
        constexpr LPCWSTR create_new_bookmark = nullptr;
        bookmark_handle_.reset(EvtCreateBookmark(create_new_bookmark));
    }

    if (nullptr == bookmark_handle_)
    {
        const auto message = boost::format("EvtCreateBookmark failed with error code %1%") % GetLastError();
        throw std::runtime_error(message.str());
    }
}

Bookmark::~Bookmark()
{
    bookmark_handle_.reset();
}

EVT_HANDLE Bookmark::getHandle() const
{
    assert(nullptr != bookmark_handle_);

    return bookmark_handle_.get();
}

void Bookmark::update(EVT_HANDLE event_handle)
{
    assert(nullptr != bookmark_handle_);

    if (!EvtUpdateBookmark(bookmark_handle_.get(), event_handle))
    {
        const auto message = boost::format("EvtUpdateBookmark failed with error code %1%") % GetLastError();
        throw std::runtime_error(message.str());
    }
}

void Bookmark::save() const
{
    assert(nullptr != bookmark_handle_);

    constexpr EVT_RENDER_FLAGS render_flag = EvtRenderBookmark;
    const auto event_message = renderEvent(bookmark_handle_.get(), render_flag);
    boost::property_tree::write_xml(bookmark_path_.string(), event_message);
}