/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"

namespace NEO {
namespace Queries {
enum class QueryType {
    invalid,
    help,
    neoRevision,
    igcRevision,
    oclDriverVersion,
    oclDeviceExtensions,
    oclDeviceExtensionsWithVersion,
    oclDeviceProfile,
    oclDeviceOpenCLCAllVersions,
    oclDeviceOpenCLCFeatures,
    supportedDevices
};

inline constexpr ConstStringRef queryNeoRevision = "NEO_REVISION";
inline constexpr ConstStringRef queryIgcRevision = "IGC_REVISION";
inline constexpr ConstStringRef queryOCLDriverVersion = "OCL_DRIVER_VERSION";
inline constexpr ConstStringRef queryOCLDeviceExtensions = "CL_DEVICE_EXTENSIONS";
inline constexpr ConstStringRef queryOCLDeviceExtensionsWithVersion = "CL_DEVICE_EXTENSIONS_WITH_VERSION";
inline constexpr ConstStringRef queryOCLDeviceProfile = "CL_DEVICE_PROFILE";
inline constexpr ConstStringRef queryOCLDeviceOpenCLCAllVersions = "CL_DEVICE_OPENCL_C_ALL_VERSIONS";
inline constexpr ConstStringRef queryOCLDeviceOpenCLCFeatures = "CL_DEVICE_OPENCL_C_FEATURES";
inline constexpr ConstStringRef querySupportedDevices = "SUPPORTED_DEVICES";
}; // namespace Queries
} // namespace NEO
