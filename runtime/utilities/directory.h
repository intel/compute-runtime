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
    static std::vector<std::string> getFiles(const std::string &path);
};
}; // namespace NEO
