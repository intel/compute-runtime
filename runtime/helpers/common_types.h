/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <bitset>
#include <memory>
#include <vector>
namespace NEO {
class CommandStreamReceiver;
struct EngineControl;
using CsrContainer = std::vector<std::vector<std::unique_ptr<CommandStreamReceiver>>>;
using EngineControlContainer = std::vector<EngineControl>;
using DeviceBitfield = std::bitset<32>;
} // namespace NEO