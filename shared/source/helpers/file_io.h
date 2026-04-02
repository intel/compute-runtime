/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace NEO {

bool fileExists(const std::string &fileName);
bool fileExistsHasSize(const std::string &fileName);
size_t writeDataToFile(const char *filename, std::string_view data, bool append);
std::unique_ptr<char[]> loadDataFromFile(const char *filename, size_t &retSize);
void dumpFileIncrement(const char *data, size_t dataSize, const std::string &filename, const std::string &extension);

} // namespace NEO
