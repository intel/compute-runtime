/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <memory>

namespace NEO {
class ExecutionEnvironment;

class PdhInterface {
  public:
    virtual ~PdhInterface() = default;

    virtual uint64_t getCurrentMemoryUsage(uint32_t rootDeviceIndex, bool dedicatedCounter) = 0;

    static std::unique_ptr<PdhInterface> create(ExecutionEnvironment &executionEnvironment);

  protected:
    PdhInterface() = default;
};

} // namespace NEO
