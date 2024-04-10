/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace L0 {
namespace Sysman {

// Each entry of this map corresponds to one particular graphics card type. GUID string will help in identify the card type.
const std::map<unsigned long, std::map<std::string, uint32_t>> guidToKeyOffsetMap;
} // namespace Sysman
} // namespace L0
