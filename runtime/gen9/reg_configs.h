/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/preamble.h"
#include "runtime/command_stream/thread_arbitration_policy.h"

namespace NEO {
struct SKLFamily;
template <>
struct L3CNTLREGConfig<IGFX_SKYLAKE> {
    static const uint32_t valueForSLM = 0x60000321u;
    static const uint32_t valueForNoSLM = 0x80000340u;
};

template <>
struct L3CNTLRegisterOffset<SKLFamily> {
    static const uint32_t registerOffset = 0x7034;
};

template <>
struct L3CNTLREGConfig<IGFX_BROXTON> {
    static const uint32_t valueForSLM = 0x60000321u;
    static const uint32_t valueForNoSLM = 0x80000340u;
};

namespace DebugControlReg2 {
constexpr uint32_t address = 0xE404;
constexpr uint32_t getRegData(const uint32_t &policy) {
    return policy == ThreadArbitrationPolicy::RoundRobin ? 0x100 : 0x0;
};
} // namespace DebugControlReg2

} // namespace NEO
