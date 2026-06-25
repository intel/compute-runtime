/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/zebin/zeinfo.h"

#include <cstdint>
#include <optional>
#include <string>

namespace NEO {

extern const std::string clStdOptionName;
struct HardwareInfo;

bool requiresOpenClCFeatures(const std::string &compileOptions);
bool requiresAdditionalExtensions(const std::string &compileOptions);

bool requiresL1PolicyMissmatchCheck();
bool replaceL1CachePolicyInBuildOptions(std::string &buildOptions, const char *currentCachePolicy);
bool checkAndReplaceL1CachePolicy(std::string &buildOptions, NEO::Zebin::ZeInfo::Types::Version version, const char *currentCachePolicy);
std::optional<uint32_t> getL1CacheControlForZebinPolicy(NEO::Zebin::ZeInfo::Types::L1CachePolicy::L1CachePolicy binaryPolicy);
bool checkL1CachePolicyMismatch(NEO::Zebin::ZeInfo::Types::L1CachePolicy::L1CachePolicy binaryPolicy, uint32_t driverDefaultL1CacheControl);

void appendAdditionalExtensions(std::string &extensions, const std::string &compileOptions, const std::string &internalOptions);
void appendExtensionsToInternalOptions(const HardwareInfo &hwInfo, const std::string &options, std::string &internalOptions);
void removeNotSupportedExtensions(std::string &extensions, const std::string &compileOptions, const std::string &internalOptions);

} // namespace NEO
