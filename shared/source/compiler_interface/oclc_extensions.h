/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/stackvec.h"

#include "CL/cl.h"

#include <string>

using OpenClCFeaturesContainer = StackVec<cl_name_version, 35>;

namespace NEO {
struct HardwareInfo;
class CompilerProductHelper;
class ReleaseHelper;

namespace Extensions {
inline constexpr const char *const sharingFormatQuery = "cl_intel_sharing_format_query ";
}

void getOpenclCFeaturesList(const HardwareInfo &hwInfo, OpenClCFeaturesContainer &openclCFeatures, const CompilerProductHelper &compilerProductHelper, const ReleaseHelper *releaseHelper);
std::string convertEnabledExtensionsToCompilerInternalOptions(const char *deviceExtensions,
                                                              OpenClCFeaturesContainer &openclCFeatures);
std::string getOclVersionCompilerInternalOption(unsigned int oclVersion);

cl_version getOclCExtensionVersion(std::string name, cl_version defaultVer);

} // namespace NEO
