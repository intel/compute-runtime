/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>
#include <vector>

namespace NEO {
std::vector<std::string> getBuiltInFileNames(bool imagesSupport);
std::string getBuiltInHashFileName(uint64_t hash, bool imagesSupport);
} // namespace NEO
