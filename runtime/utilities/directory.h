/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <string>
#include <vector>

namespace NEO {

class Directory {
  public:
    static std::vector<std::string> getFiles(std::string &path);
};
}; // namespace NEO
