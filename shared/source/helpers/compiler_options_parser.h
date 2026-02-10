/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/zebin/zeinfo.h"

#include <string>

namespace NEO {

extern const std::string clStdOptionName;
struct HardwareInfo;

bool requiresOpenClCFeatures(const std::string &compileOptions);
bool requiresAdditionalExtensions(const std::string &compileOptions);

bool requiresL1PolicyMissmatchCheck();
bool checkAndReplaceL1CachePolicy(std::string &buildOptions, NEO::Zebin::ZeInfo::Types::Version version, const char *currentCachePolicy);

void appendAdditionalExtensions(std::string &extensions, const std::string &compileOptions, const std::string &internalOptions);
void appendExtensionsToInternalOptions(const HardwareInfo &hwInfo, const std::string &options, std::string &internalOptions);
void removeNotSupportedExtensions(std::string &extensions, const std::string &compileOptions, const std::string &internalOptions);

} // namespace NEO
