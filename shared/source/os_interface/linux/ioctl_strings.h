/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <string>

namespace NEO {
namespace IoctlToStringHelper {
std::string getIoctlParamString(int param);
std::string getIoctlParamStringRemaining(int param);
std::string getIoctlString(unsigned long request);
std::string getIoctlStringRemaining(unsigned long request);
} // namespace IoctlToStringHelper
} // namespace NEO
