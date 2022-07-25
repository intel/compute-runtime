/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/preamble.h"

namespace NEO {
struct Gen9Family;
template <>
struct L3CNTLREGConfig<IGFX_SKYLAKE> {
    static const uint32_t valueForSLM = 0x60000321u;
    static const uint32_t valueForNoSLM = 0x80000340u;
};

template <>
struct L3CNTLRegisterOffset<Gen9Family> {
    static const uint32_t registerOffset = 0x7034;
};

template <>
struct L3CNTLREGConfig<IGFX_BROXTON> {
    static const uint32_t valueForSLM = 0x60000321u;
    static const uint32_t valueForNoSLM = 0x80000340u;
};

namespace DebugControlReg2 {
constexpr uint32_t address = 0xE404;
constexpr uint32_t getRegData(const int32_t &policy) {
    return policy == ThreadArbitrationPolicy::RoundRobin ? 0x100 : 0x0;
};
static const int32_t supportedArbitrationPolicy[] = {
    ThreadArbitrationPolicy::AgeBased,
    ThreadArbitrationPolicy::RoundRobin};
} // namespace DebugControlReg2

} // namespace NEO
