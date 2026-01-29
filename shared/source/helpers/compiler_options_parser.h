/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {

extern const std::string clStdOptionName;
struct HardwareInfo;

bool requiresOpenClCFeatures(const std::string &compileOptions);
bool requiresAdditionalExtensions(const std::string &compileOptions);
bool checkAndReplaceL1CachePolicy(std::string &buildOptions, const char *currentCachePolicy);

void appendAdditionalExtensions(std::string &extensions, const std::string &compileOptions, const std::string &internalOptions);
void appendExtensionsToInternalOptions(const HardwareInfo &hwInfo, const std::string &options, std::string &internalOptions);
void removeNotSupportedExtensions(std::string &extensions, const std::string &compileOptions, const std::string &internalOptions);

} // namespace NEO
