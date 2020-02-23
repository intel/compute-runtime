/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <memory>
namespace NEO {
struct HardwareInfo;
class Debugger {
  public:
    static std::unique_ptr<Debugger> create(HardwareInfo *hwInfo);
    virtual ~Debugger() = default;
    virtual bool isDebuggerActive() = 0;
};
} // namespace NEO