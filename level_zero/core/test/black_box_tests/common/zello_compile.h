/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

std::vector<uint8_t> compileToSpirV(const std::string &src, const std::string &options, std::string &outCompilerLog);
std::vector<uint8_t> compileToNative(const std::string &src, const std::string &deviceName, const std::string &revisionId, const std::string &options, const std::string &internalOptions, std::string &outCompilerLog);

extern const char *memcpyBytesTestKernelSrc;

extern const char *memcpyBytesWithPrintfTestKernelSrc;
