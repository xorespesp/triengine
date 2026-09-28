#pragma once
#include <string>

namespace triengine::core
{
    struct graphics_device_info
    {
        std::string api_name;
        std::string api_version;
        std::string driver_version;
        std::string device;
        std::string vendor;
    };

} // namespace triengine::core
