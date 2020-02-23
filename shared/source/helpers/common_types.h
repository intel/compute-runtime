/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <bitset>
#include <memory>
#include <vector>
namespace NEO {
struct EngineControl;
using EngineControlContainer = std::vector<EngineControl>;
using DeviceBitfield = std::bitset<32>;
} // namespace NEO