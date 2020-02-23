/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string>
#include <vector>

#pragma once

namespace NEO {
std::vector<std::string> getBuiltInFileNames();
std::string getBuiltInHashFileName(uint64_t hash);
} // namespace NEO
