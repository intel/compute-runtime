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
    uint32_t argType = 0;
    uint32_t argIndex = 0;
};

struct SignalEventVariableDescriptor {
    Event *event = nullptr;
};

struct WaitEventVariableDescriptor {
    Event *event = nullptr;
    uint32_t waitEventIndex = 0;
    uint32_t waitEventPackets = 0;
};

struct MutableVariableDescriptor {
    Variable *var = nullptr;
    union {
        KernelArgumentVariableDescriptor kernelArguments;
        SignalEventVariableDescriptor signalEvent;
        WaitEventVariableDescriptor waitEvents;
    };
    ze_mutable_command_exp_flag_t varType;
};

using MutationVariables = std::vector<MutableVariableDescriptor>;

} // namespace L0::MCL
