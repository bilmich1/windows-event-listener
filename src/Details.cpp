#include "Details.h"

#include <boost/property_tree/xml_parser.hpp>
#include <boost/format.hpp>

#include <stringapiset.h>

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

std::wstring readXmlFile(const std::filesystem::path& file_path)
{
    try
    {
        boost::property_tree::wptree tree;
        boost::property_tree::read_xml(file_path.string(), tree);

        std::wostringstream string_stream;
        boost::property_tree::write_xml(string_stream, tree);

        return string_stream.str();
    }
    catch (const std::exception& ex)
    {
        const auto message = boost::format("File could not be read: %1%, reason: %2%") % file_path % ex.what();
        throw std::runtime_error(message.str());
    }
}

boost::property_tree::ptree renderEvent(EVT_HANDLE event_handle, EVT_RENDER_FLAGS render_flag)
{
    DWORD unused_property_count = 0;
    constexpr EVT_HANDLE context = nullptr;
    constexpr PVOID get_required_buffer_size = nullptr;

    auto throw_error_code_exception = [](DWORD error_code) {
        const auto message = boost::format("EvtRender failed with error code %1%") % error_code;
        throw std::runtime_error(message.str());
    };

    DWORD error_code = ERROR_SUCCESS;
    DWORD buffer_size = 0;
    DWORD buffer_used = 0;

    if (!EvtRender(
        context,
        event_handle,
        render_flag,
        buffer_size,
        get_required_buffer_size,
        &buffer_used,
        &unused_property_count))
    {
        error_code = GetLastError();
        if (ERROR_INSUFFICIENT_BUFFER == error_code)
        {
            buffer_size = buffer_used;
            std::vector<WCHAR> message_content(buffer_size);

            EvtRender(
                context,
                event_handle,
                render_flag,
                buffer_size,
                message_content.data(),
                &buffer_used,
                &unused_property_count);

            if (error_code = GetLastError(); ERROR_SUCCESS != error_code)
            {
                throw_error_code_exception(error_code);
            }

            try
            {
                boost::property_tree::ptree event;
                std::stringstream string_stream;
                string_stream << toString(message_content);
                boost::property_tree::read_xml(string_stream, event);
                return event;
            }
            catch (const std::exception& ex)
            {
                throw std::runtime_error(ex.what());
            }
        }
        else
        {
            throw_error_code_exception(error_code);
        }
    }

    throw std::runtime_error("EvtRender succeeded with get_required_buffer_size, this should never happen.");
}