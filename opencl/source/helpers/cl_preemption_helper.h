/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption.h"

namespace NEO {
class Kernel;
class Device;
struct MultiDispatchInfo;

class ClPreemptionHelper {
  public:
    static PreemptionMode taskPreemptionMode(Device &device, const MultiDispatchInfo &multiDispatchInfo);
};

} // namespace NEO
