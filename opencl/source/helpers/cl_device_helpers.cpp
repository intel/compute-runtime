/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_device_helpers.h"

namespace NEO {
void ClDeviceHelper::getExtraDeviceInfo(const ClDevice &clDevice, cl_device_info paramName, ClDeviceInfoParam &param, const void *&src, size_t &size, size_t &retSize) {}
cl_device_feature_capabilities_intel ClDeviceHelper::getExtraCapabilities(const HardwareInfo &hwInfo) { return 0; }
} // namespace NEO
