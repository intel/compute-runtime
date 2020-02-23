/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <algorithm>
#include <string>

class KernelFilenameHelper {
  public:
    static void getKernelFilenameFromInternalOption(std::string &option, std::string &filename) {
        // remove leading spaces
        size_t position = option.find_first_not_of(" ");
        filename = option.substr(position);
        // replace space with underscore
        std::replace(filename.begin(), filename.end(), ' ', '_');
    }
};
