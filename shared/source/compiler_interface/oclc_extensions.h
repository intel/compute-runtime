/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/stackvec.h"

#include "CL/cl.h"

#include <string>

using OpenClCFeaturesContainer = StackVec<cl_name_version, 15>;

namespace NEO {
struct HardwareInfo;

namespace Extensions {
constexpr const char *const sharingFormatQuery = "cl_intel_sharing_format_query ";
}
extern const char *deviceExtensionsList;

std::string getExtensionsList(const HardwareInfo &hwInfo);
void getOpenclCFeaturesList(const HardwareInfo &hwInfo, OpenClCFeaturesContainer &openclCFeatures);
std::string convertEnabledExtensionsToCompilerInternalOptions(const char *deviceExtensions,
                                                              OpenClCFeaturesContainer &openclCFeatures);
std::string getOclVersionCompilerInternalOption(unsigned int oclVersion);

} // namespace NEO
