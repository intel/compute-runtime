/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/kernel/kernel_arg_descriptor.h"

#include <cstddef>
#include <cstdint>

namespace NEO {

class CommandContainer;
struct DispatchKernelEncoderI;
struct IndirectParamsInInlineDataArgs;

template <typename GfxFamily>
struct EncodeIndirectParams {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using MI_LOAD_REGISTER_MEM = typename GfxFamily::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_MATH = typename GfxFamily::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;

    static void encode(CommandContainer &container, uint64_t crossThreadDataGpuVa, DispatchKernelEncoderI *dispatchInterface, uint64_t implicitArgsGpuPtr, IndirectParamsInInlineDataArgs *outArgs);
    static void setGroupCountIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset offsets[3], uint64_t crossThreadAddress, IndirectParamsInInlineDataArgs *outArgs);
    static void setWorkDimIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset offset, uint64_t crossThreadAddress, const uint32_t *groupSize, IndirectParamsInInlineDataArgs *outArgs);
    static void setGlobalWorkSizeIndirect(CommandContainer &container, const NEO::CrossThreadDataOffset offsets[3], uint64_t crossThreadAddress, const uint32_t *lws, IndirectParamsInInlineDataArgs *outArgs);
    static void applyInlineDataGpuVA(IndirectParamsInInlineDataArgs &args, uint64_t inlineDataGpuVa);

    static size_t getCmdsSizeForSetWorkDimIndirect(const uint32_t *groupSize, bool misalignedPtr);
};

} // namespace NEO
