/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace OCLRT {

extern const char *deviceExtensionsList;

std::string getExtensionsList(const HardwareInfo &hwInfo);
std::string removeLastSpace(std::string &s);
std::string convertEnabledExtensionsToCompilerInternalOptions(const char *deviceExtensions);

} // namespace OCLRT