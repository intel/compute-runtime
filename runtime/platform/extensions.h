/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/hw_info.h"

#include <string>

namespace NEO {

extern const char *deviceExtensionsList;

std::string getExtensionsList(const HardwareInfo &hwInfo);
std::string removeLastSpace(std::string &s);
std::string convertEnabledExtensionsToCompilerInternalOptions(const char *deviceExtensions);

} // namespace NEO
