/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include <vector>

namespace L0 {
struct Event;
} // namespace L0

namespace L0::MCL {
struct Variable;

struct KernelArgumentVariableDescriptor {
    Variable *kernelArgumentVariable = nullptr;
    uint32_t argIndex = 0;
};

struct SignalEventVariableDescriptor {
    Variable *eventVariable = nullptr;
    Event *event = nullptr;
};

struct WaitEventVariableDescriptor {
    Variable *eventVariable = nullptr;
    Event *event = nullptr;
    uint32_t waitEventIndex = 0;
    uint32_t waitEventPackets = 0;
};

struct KernelVariableDescriptor {
    std::vector<KernelArgumentVariableDescriptor> kernelArguments;
    Variable *groupCount = nullptr;
    Variable *groupSize = nullptr;
    Variable *globalOffset = nullptr;
};

struct MutationVariables {
    KernelVariableDescriptor kernelVariables;
    SignalEventVariableDescriptor signalEvent;
    std::vector<WaitEventVariableDescriptor> waitEvents;
};

} // namespace L0::MCL
