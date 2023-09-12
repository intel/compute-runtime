/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "CL/cl.h"

#include <string>
#include <vector>

namespace NEO {

struct PlatformInfo {
    std::vector<cl_name_version> extensionsWithVersion;
    std::string profile = "FULL_PROFILE";
    std::string version = "";
    std::string name = "Intel(R) OpenCL Graphics";
    std::string vendor = "Intel(R) Corporation";
    std::string extensions;
    std::string icdSuffixKhr = "INTEL";
    cl_version numericVersion = 0;
};

} // namespace NEO
