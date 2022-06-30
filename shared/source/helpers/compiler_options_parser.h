/*
 * Copyright (C) 2020-2021 Intel Corporation
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

void appendExtensionsToInternalOptions(const HardwareInfo &hwInfo, const std::string &options, std::string &internalOptions);

} // namespace NEO
