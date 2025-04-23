#pragma once
#include "bit_cast.hh"
#include "logger.hh"

#include <exception>
#include <filesystem>
#include <string>


#define THROW_EXCEPTION(C_FMT, ...) ::utils::throw_exception(__FILE__, __LINE__, C_FMT, __VA_ARGS__)

namespace utils
{    
    template <typename... _Args>
    [[noreturn]]
    static inline void throw_exception(
        const std::filesystem::path& file, 
        const uint32_t line,
        const std::string_view format, 
        _Args&&... args)
    {
        std::runtime_error e{ "" };

#pragma warning(push)
#pragma warning(disable : 4996)
        int len = ::_snprintf(nullptr, 0, format.data(), args...);
        if (len > 0) {
            int buf_len = len + 1;
            char* buf = utils::bit_cast<char*>(::calloc(sizeof(char), buf_len));
            ::_snprintf(buf, buf_len, format.data(), std::forward<_Args>(args)...);
            e = std::runtime_error{ buf };
            ::free(buf);
        }
#pragma warning(pop)

        //::OutputDebugStringA(
        //    std::string{
        //        "Exception at <" + file.filename().string() + ":" + std::to_string(line) + "> : " + e.what()
        //    }.c_str()
        //);

        LOG_CRITICAL(L"Exception thrown at <%S:%u> -> %S", file.filename().string().c_str(), line, e.what());

        throw e;
    }

} // namespace