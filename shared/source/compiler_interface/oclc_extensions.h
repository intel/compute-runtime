/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/stackvec.h"

#include "CL/cl.h"

#include <string>
#include <string_view>

using OpenClCFeaturesContainer = StackVec<cl_name_version, 35>;

namespace NEO {
struct HardwareInfo;
class CompilerProductHelper;
class ReleaseHelper;

constexpr inline std::string_view oclVersionCompilerInternalOption = "-ocl-version=300 ";

namespace Extensions {
inline constexpr const char *const sharingFormatQuery = "cl_intel_sharing_format_query ";
}

void getOpenclCFeaturesList(const HardwareInfo &hwInfo, OpenClCFeaturesContainer &openclCFeatures, const CompilerProductHelper &compilerProductHelper, const ReleaseHelper *releaseHelper);
std::string convertEnabledExtensionsToCompilerInternalOptions(const char *deviceExtensions,
                                                              OpenClCFeaturesContainer &openclCFeatures);

cl_version getOclCExtensionVersion(std::string name, cl_version defaultVer);

} // namespace NEO
