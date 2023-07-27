/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/stackvec.h"

#include "CL/cl.h"

#include <string>

using OpenClCFeaturesContainer = StackVec<cl_name_version, 28>;

namespace NEO {
struct HardwareInfo;
class CompilerProductHelper;

namespace Extensions {
inline constexpr const char *const sharingFormatQuery = "cl_intel_sharing_format_query ";
}

void getOpenclCFeaturesList(const HardwareInfo &hwInfo, OpenClCFeaturesContainer &openclCFeatures, const CompilerProductHelper &compilerProductHelper);
std::string convertEnabledExtensionsToCompilerInternalOptions(const char *deviceExtensions,
                                                              OpenClCFeaturesContainer &openclCFeatures);
std::string getOclVersionCompilerInternalOption(unsigned int oclVersion);

} // namespace NEO
