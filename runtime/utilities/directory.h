/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>
#include <vector>

namespace OCLRT {

class Directory {
  public:
    static std::vector<std::string> getFiles(std::string &path);
};
}; // namespace OCLRT
