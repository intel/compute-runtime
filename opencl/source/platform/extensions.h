/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/utilities/stackvec.h"

#include "opencl/source/cl_device/cl_device_info.h"

#include <string>

namespace NEO {
namespace Extensions {
constexpr const char *const sharingFormatQuery = "cl_intel_sharing_format_query ";
}
extern const char *deviceExtensionsList;

std::string getExtensionsList(const HardwareInfo &hwInfo);
void getOpenclCFeaturesList(const HardwareInfo &hwInfo, OpenClCFeaturesContainer &openclCFeatures);
std::string convertEnabledExtensionsToCompilerInternalOptions(const char *deviceExtensions,
                                                              OpenClCFeaturesContainer &openclCFeatures);
std::string convertEnabledOclCFeaturesToCompilerInternalOptions(OpenClCFeaturesContainer &openclCFeatures);

} // namespace NEO
