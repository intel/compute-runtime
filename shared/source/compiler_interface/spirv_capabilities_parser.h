/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/yaml/yaml_parser.h"

#include "cif/common/cif_main.h"
#include "igc_interface.h"

#include <cstdint>
#include <string>
#include <vector>

namespace NEO {

struct SpirvCapabilityInfo {
    std::string name;
    uint32_t id = 0;
};

struct SpirvExtensionInfo {
    std::string name;
    std::string url;
    std::vector<SpirvCapabilityInfo> supportedCapabilities;
};

class SpirvCapabilitiesParser {
  public:
    static bool parseSpirvExtensionsYAML(const std::string &yamlText,
                                         std::vector<SpirvExtensionInfo> &outExtensions,
                                         std::string &outErrReason,
                                         std::string &outWarning);

    // Fetches the SPIR-V extensions/capabilities YAML from an already-initialized
    // IGC device context. The platform embedded in deviceCtx by
    // initializeIgcDeviceContext() is what IGC filters the reported set against,
    // so the result is per-device.
    static std::string getSpirvExtensionsYAMLFromDeviceCtx(NEO::IgcOclDeviceCtxTag *deviceCtx, CIF::CIFMain *igcMain);
};

} // namespace NEO
