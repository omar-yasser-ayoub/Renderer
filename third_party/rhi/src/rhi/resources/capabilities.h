#pragma once

#include <string>

namespace rhi {

struct Capabilities {
    std::string deviceName = "N/A";
    bool SupportsMeshShaders = false;
    bool SupportsRayTracing = false;
    bool SupportsAnisotropy = false;
};

}
