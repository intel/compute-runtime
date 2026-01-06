/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "neo_igfxfmid.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NEO {
enum class EngineGroupType : uint32_t;
struct HardwareInfo;
class Device;
struct DispatchFlags;
class GraphicsAllocation;
class LinearStream;
struct PipelineSelectArgs;
struct StreamProperties;
struct RootDeviceEnvironment;

template <typename GfxFamily>
struct PreambleHelper {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    static void programL3(LinearStream *pCommandStream, uint32_t l3Config, bool isBcs);
    static void programPipelineSelect(LinearStream *pCommandStream,
                                      const PipelineSelectArgs &pipelineSelectArgs,
                                      const RootDeviceEnvironment &rootDeviceEnvironment);
    static void programPreemption(LinearStream *pCommandStream, Device &device, GraphicsAllocation *preemptionCsr);
    static void addPipeControlBeforeVfeCmd(LinearStream *pCommandStream, const HardwareInfo *hwInfo, EngineGroupType engineGroupType);
    static void appendProgramVFEState(const RootDeviceEnvironment &rootDeviceEnvironment, const StreamProperties &streamProperties, void *cmd);
    static void *getSpaceForVfeState(LinearStream *pCommandStream,
                                     const HardwareInfo &hwInfo,
                                     EngineGroupType engineGroupType,
                                     uint64_t *cmdBufferGpuAddress);
    static void programVfeState(void *pVfeState,
                                const RootDeviceEnvironment &rootDeviceEnvironment,
                                uint32_t scratchSize,
                                uint64_t scratchAddress,
                                uint32_t maxFrontEndThreads,
                                const StreamProperties &streamProperties);
    static uint64_t getScratchSpaceAddressOffsetForVfeState(LinearStream *pCommandStream, void *pVfeState);
    static void programPreamble(LinearStream *pCommandStream, Device &device, uint32_t l3Config,
                                GraphicsAllocation *preemptionCsr, bool isBcs);
    static void programSemaphoreDelay(LinearStream *pCommandStream, bool isBcs);
    static uint32_t getL3Config(const HardwareInfo &hwInfo, bool useSLM);
    static bool isSystolicModeConfigurable(const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getAdditionalCommandsSize(const Device &device);
    static std::vector<int32_t> getSupportedThreadArbitrationPolicies();
    static size_t getVFECommandsSize();
    static size_t getKernelDebuggingCommandsSize(bool debuggingActive);
    static void programGenSpecificPreambleWorkArounds(LinearStream *pCommandStream, const HardwareInfo &hwInfo);
    static uint32_t getUrbEntryAllocationSize();
    static size_t getCmdSizeForPipelineSelect(const RootDeviceEnvironment &rootDeviceEnvironment);
    static size_t getSemaphoreDelayCommandSize();
    static uint32_t getScratchSizeValueToProgramMediaVfeState(uint32_t scratchSize);
    static void setSingleSliceDispatchMode(void *cmd, bool enable);
};

template <PRODUCT_FAMILY productFamily>
static uint32_t getL3ConfigHelper(bool useSLM);

template <PRODUCT_FAMILY productFamily>
struct L3CNTLREGConfig {
    static const uint32_t valueForSLM;
    static const uint32_t valueForNoSLM;
};

template <PRODUCT_FAMILY productFamily>
uint32_t getL3ConfigHelper(bool useSLM) {
    if (!useSLM) {
        return L3CNTLREGConfig<productFamily>::valueForNoSLM;
    }
    return L3CNTLREGConfig<productFamily>::valueForSLM;
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
