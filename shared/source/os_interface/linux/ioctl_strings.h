/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_wrappers.h"

#include <string>

namespace NEO {
namespace IoctlToStringHelper {
std::string getIoctlParamString(int param);
std::string getIoctlParamStringRemaining(int param);
std::string getIoctlString(DrmIoctl ioctlRequest);
} // namespace IoctlToStringHelper
} // namespace NEO
