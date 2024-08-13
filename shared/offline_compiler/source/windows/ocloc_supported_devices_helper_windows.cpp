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

std::string SupportedDevicesHelper::getOclocCurrentVersionOutputFilename() const {
    return getOclocCurrentVersion() + getOutputFilenameSuffix(SupportedDevicesMode::unknown);
}

std::string SupportedDevicesHelper::getOclocCurrentVersion() const {
    return "ocloc";
}

std::string SupportedDevicesHelper::getOclocFormerVersion() const {
    return "";
}

std::string SupportedDevicesHelper::extractOclocVersion(std::string_view oclocLibNameWithVersion) const {
    return "";
}

std::string SupportedDevicesHelper::getDataFromFormerOclocVersion() const {
    return "";
}
} // namespace Ocloc
