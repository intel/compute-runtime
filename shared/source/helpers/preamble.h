/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/definitions/engine_group_types.h"

#include "igfxfmid.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NEO {

struct HardwareInfo;
class Device;
struct DispatchFlags;
class GraphicsAllocation;
class LinearStream;
class LogicalStateHelper;
struct PipelineSelectArgs;
struct StreamProperties;

template <typename GfxFamily>
struct PreambleHelper {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using VFE_STATE_TYPE = typename GfxFamily::VFE_STATE_TYPE;

    static void programL3(LinearStream *pCommandStream, uint32_t l3Config);
    static void programPipelineSelect(LinearStream *pCommandStream,
                                      const PipelineSelectArgs &pipelineSelectArgs,
                                      const HardwareInfo &hwInfo);
    static void appendProgramPipelineSelect(void *cmd, bool isSpecialModeSelected, const HardwareInfo &hwInfo);
    static void programPreemption(LinearStream *pCommandStream, Device &device, GraphicsAllocation *preemptionCsr, LogicalStateHelper *logicalStateHelper);
    static void addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, EngineGroupType engineGroupType);
    static void appendProgramVFEState(const HardwareInfo &hwInfo, const StreamProperties &streamProperties, void *cmd);
    static void *getSpaceForVfeState(LinearStream *pCommandStream,
                                     const HardwareInfo &hwInfo,
                                     EngineGroupType engineGroupType);
    static void programVfeState(void *pVfeState,
                                const HardwareInfo &hwInfo,
                                uint32_t scratchSize,
                                uint64_t scratchAddress,
                                uint32_t maxFrontEndThreads,
                                const StreamProperties &streamProperties,
                                LogicalStateHelper *logicalStateHelper);
    static uint64_t getScratchSpaceAddressOffsetForVfeState(LinearStream *pCommandStream, void *pVfeState);
    static void programPreamble(LinearStream *pCommandStream, Device &device, uint32_t l3Config,
                                GraphicsAllocation *preemptionCsr, LogicalStateHelper *logicalStateHelper);
    static void programKernelDebugging(LinearStream *pCommandStream);
    static void programSemaphoreDelay(LinearStream *pCommandStream);
    static uint32_t getL3Config(const HardwareInfo &hwInfo, bool useSLM);
    static bool isL3Configurable(const HardwareInfo &hwInfo);
    static bool isSystolicModeConfigurable(const HardwareInfo &hwInfo);
    static bool isSpecialPipelineSelectModeChanged(bool lastSpecialPipelineSelectMode, bool newSpecialPipelineSelectMode,
                                                   const HardwareInfo &hwInfo);
    static size_t getAdditionalCommandsSize(const Device &device);
    static std::vector<int32_t> getSupportedThreadArbitrationPolicies();
    static size_t getVFECommandsSize();
    static size_t getKernelDebuggingCommandsSize(bool debuggingActive);
    static void programGenSpecificPreambleWorkArounds(LinearStream *pCommandStream, const HardwareInfo &hwInfo);
    static uint32_t getUrbEntryAllocationSize();
    static size_t getCmdSizeForPipelineSelect(const HardwareInfo &hwInfo);
    static size_t getSemaphoreDelayCommandSize();
    static uint32_t getScratchSizeValueToProgramMediaVfeState(uint32_t scratchSize);
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

template <typename GfxFamily>
struct TdDebugControlRegisterOffset {
    enum {
        registerOffset = 0xe400,
        debugEnabledValue = (1 << 4) | (1 << 7)
    };
};

template <typename GfxFamily>
struct GlobalSipRegister {
    enum {
        registerOffset = 0xE42C,
    };
};

} // namespace NEO
