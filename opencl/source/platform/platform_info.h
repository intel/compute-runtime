/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

#include <string>
#include <vector>

struct PlatformInfo {
    std::vector<cl_name_version> extensionsWithVersion;
    std::string profile = "FULL_PROFILE";
    std::string version = "";
    std::string name = "Intel(R) OpenCL HD Graphics";
    std::string vendor = "Intel(R) Corporation";
    std::string extensions;
    std::string icdSuffixKhr = "INTEL";
    cl_version numericVersion = 0;
};
