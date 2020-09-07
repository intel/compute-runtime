/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_info.h"
#include "shared/source/utilities/stackvec.h"

#include "CL/cl.h"

#include <string>

namespace NEO {
namespace Extensions {
constexpr const char *const sharingFormatQuery = "cl_intel_sharing_format_query ";
}
extern const char *deviceExtensionsList;

std::string getExtensionsList(const HardwareInfo &hwInfo);
void getOpenclCFeaturesList(const HardwareInfo &hwInfo, StackVec<cl_name_version, 15> &openclCFeatures);
std::string removeLastSpace(std::string &s);
std::string convertEnabledExtensionsToCompilerInternalOptions(const char *deviceExtensions);
std::string convertEnabledOclCFeaturesToCompilerInternalOptions(StackVec<cl_name_version, 15> &openclCFeatures);

} // namespace NEO
