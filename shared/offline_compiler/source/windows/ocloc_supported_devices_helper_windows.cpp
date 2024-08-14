/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"
#include "shared/source/helpers/product_config_helper.h"

#include <string>

namespace Ocloc {
using namespace NEO;

std::string SupportedDevicesHelper::getOutputFilenameSuffix(SupportedDevicesMode mode) const {
    return std::string("_supported_devices") + fileExtension.data();
}

std::string SupportedDevicesHelper::getCurrentOclocOutputFilename() const {
    return getCurrentOclocName() + getOutputFilenameSuffix(SupportedDevicesMode::unknown);
}

std::string SupportedDevicesHelper::getCurrentOclocName() const {
    return "ocloc";
}

std::string SupportedDevicesHelper::extractOclocName(std::string_view oclocLibName) const {
    return "";
}

std::string SupportedDevicesHelper::getDataFromFormerOcloc() const {
    return "";
}
} // namespace Ocloc
