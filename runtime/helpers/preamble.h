/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/pipeline_select_helper.h"

#include "igfxfmid.h"

#include <cstddef>
#include <cstdint>

namespace NEO {

struct HardwareInfo;
class Device;
struct DispatchFlags;
class GraphicsAllocation;
class LinearStream;

template <typename GfxFamily>
struct PreambleHelper {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    static void programL3(LinearStream *pCommandStream, uint32_t l3Config);
    static void programPipelineSelect(LinearStream *pCommandStream, const DispatchFlags &dispatchFlags);
    static uint32_t getDefaultThreadArbitrationPolicy();
    static void programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy);
    static void programPreemption(LinearStream *pCommandStream, Device &device, GraphicsAllocation *preemptionCsr);
    static void addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo);
    static void programVFEState(LinearStream *pCommandStream, const HardwareInfo &hwInfo, int scratchSize, uint64_t scratchAddress);
    static void programPreamble(LinearStream *pCommandStream, Device &device, uint32_t l3Config,
                                uint32_t requiredThreadArbitrationPolicy, GraphicsAllocation *preemptionCsr);
    static void programKernelDebugging(LinearStream *pCommandStream);
    static uint32_t getL3Config(const HardwareInfo &hwInfo, bool useSLM);
    static size_t getAdditionalCommandsSize(const Device &device);
    static size_t getThreadArbitrationCommandsSize();
    static size_t getVFECommandsSize();
    static size_t getKernelDebuggingCommandsSize(bool debuggingActive);
    static void programGenSpecificPreambleWorkArounds(LinearStream *pCommandStream, const HardwareInfo &hwInfo);
    static uint32_t getUrbEntryAllocationSize();
};

template <PRODUCT_FAMILY ProductFamily>
static uint32_t getL3ConfigHelper(bool useSLM);

template <PRODUCT_FAMILY ProductFamily>
struct L3CNTLREGConfig {
    static const uint32_t valueForSLM;
    static const uint32_t valueForNoSLM;
};

template <PRODUCT_FAMILY ProductFamily>
uint32_t getL3ConfigHelper(bool useSLM) {
    if (!useSLM) {
        return L3CNTLREGConfig<ProductFamily>::valueForNoSLM;
    }
    return L3CNTLREGConfig<ProductFamily>::valueForSLM;
}

template <typename GfxFamily>
struct L3CNTLRegisterOffset {
    static const uint32_t registerOffset;
};

namespace DebugModeRegisterOffset {
static constexpr uint32_t registerOffset = 0x20ec;
static constexpr uint32_t debugEnabledValue = (1 << 6) | (1 << 22);
}; // namespace DebugModeRegisterOffset

namespace TdDebugControlRegisterOffset {
static constexpr uint32_t registerOffset = 0xe400;
static constexpr uint32_t debugEnabledValue = (1 << 4) | (1 << 7);
}; // namespace TdDebugControlRegisterOffset

} // namespace NEO
