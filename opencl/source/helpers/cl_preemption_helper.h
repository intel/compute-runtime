/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption.h"

namespace NEO {
class Kernel;
class Device;
class Context;
struct MultiDispatchInfo;

class ClPreemptionHelper {
  public:
    static PreemptionMode taskPreemptionMode(Device &device, const MultiDispatchInfo &multiDispatchInfo);
    static void overrideMidThreadPreemptionSupport(Context &context, bool value);
};

} // namespace NEO
