/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {

extern const std::string clStdOptionName;

bool requiresOpenClCFeatures(const std::string &compileOptions);
bool requiresAdditionalExtensions(const std::string &compileOptions);

} // namespace NEO
