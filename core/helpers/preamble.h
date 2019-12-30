/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/pipeline_select_helper.h"

#include "igfxfmid.h"

#include <cstddef>
#include <cstdint>

namespace NEO {

struct HardwareInfo;
class Device;
struct DispatchFlags;
class GraphicsAllocation;
class LinearStream;
struct PipelineSelectArgs;

template <typename GfxFamily>
struct PreambleHelper {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using VFE_STATE_TYPE = typename GfxFamily::VFE_STATE_TYPE;

    static void programL3(LinearStream *pCommandStream, uint32_t l3Config);
    static void programPipelineSelect(LinearStream *pCommandStream,
                                      const PipelineSelectArgs &pipelineSelectArgs,
                                      const HardwareInfo &hwInfo);
    static uint32_t getDefaultThreadArbitrationPolicy();
    static void programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy);
    static void programPreemption(LinearStream *pCommandStream, Device &device, GraphicsAllocation *preemptionCsr);
    static void addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo);
    static uint64_t programVFEState(LinearStream *pCommandStream,
                                    const HardwareInfo &hwInfo,
                                    int scratchSize,
                                    uint64_t scratchAddress,
                                    uint32_t maxFrontEndThreads);
    static void programAdditionalFieldsInVfeState(VFE_STATE_TYPE *mediaVfeState, const HardwareInfo &hwInfo);
    static void programPreamble(LinearStream *pCommandStream, Device &device, uint32_t l3Config,
                                uint32_t requiredThreadArbitrationPolicy, GraphicsAllocation *preemptionCsr, GraphicsAllocation *perDssBackedBuffer);
    static void programKernelDebugging(LinearStream *pCommandStream);
    static void programPerDssBackedBuffer(LinearStream *pCommandStream, const HardwareInfo &hwInfo, GraphicsAllocation *perDssBackBufferOffset);
    static uint32_t getL3Config(const HardwareInfo &hwInfo, bool useSLM);
    static bool isL3Configurable(const HardwareInfo &hwInfo);
    static size_t getAdditionalCommandsSize(const Device &device);
    static size_t getThreadArbitrationCommandsSize();
    static size_t getVFECommandsSize();
    static size_t getKernelDebuggingCommandsSize(bool debuggingActive);
    static void programGenSpecificPreambleWorkArounds(LinearStream *pCommandStream, const HardwareInfo &hwInfo);
    static uint32_t getUrbEntryAllocationSize();
    static size_t getPerDssBackedBufferCommandsSize(const HardwareInfo &hwInfo);
    static size_t getCmdSizeForPipelineSelect(const HardwareInfo &hwInfo);
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

template <typename GfxFamily>
struct DebugModeRegisterOffset {
    enum {
        registerOffset = 0x20ec,
        debugEnabledValue = (1 << 6) | (1 << 22)
    };
};

namespace TdDebugControlRegisterOffset {
static constexpr uint32_t registerOffset = 0xe400;
static constexpr uint32_t debugEnabledValue = (1 << 4) | (1 << 7);
}; // namespace TdDebugControlRegisterOffset

} // namespace NEO
