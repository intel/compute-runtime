/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <vector>
#include <string>

namespace OCLRT {

class Directory {
  public:
    static std::vector<std::string> getFiles(std::string &path);
};
}; // namespace OCLRT
