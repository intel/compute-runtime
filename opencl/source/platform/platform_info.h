/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>

struct PlatformInfo {
    std::string profile = "FULL_PROFILE";
    std::string version = "";
    std::string name = "Intel(R) OpenCL HD Graphics";
    std::string vendor = "Intel(R) Corporation";
    std::string extensions;
    std::string icdSuffixKhr = "INTEL";
};
