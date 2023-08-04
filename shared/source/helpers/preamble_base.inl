/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/register_offsets.h"

#include "reg_configs_common.h"

#include <cstddef>

namespace NEO {

template <typename GfxFamily>
std::vector<int32_t> PreambleHelper<GfxFamily>::getSupportedThreadArbitrationPolicies() {
    return {};
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programGenSpecificPreambleWorkArounds(LinearStream *pCommandStream, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programSemaphoreDelay(LinearStream *pCommandStream) {
    if (DebugManager.flags.ForceSemaphoreDelayBetweenWaits.get() > -1) {
        uint32_t valueOfNewSemaphoreDelay = DebugManager.flags.ForceSemaphoreDelayBetweenWaits.get();
        LriHelper<GfxFamily>::program(pCommandStream,
                                      SEMA_WAIT_POLL,
                                      valueOfNewSemaphoreDelay,
                                      true);
    };
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getSemaphoreDelayCommandSize() {
    return sizeof(MI_LOAD_REGISTER_IMM);
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getAdditionalCommandsSize(const Device &device) {
    size_t totalSize = PreemptionHelper::getRequiredPreambleSize<GfxFamily>(device);
    bool debuggingEnabled = device.getDebugger() != nullptr;
    totalSize += getKernelDebuggingCommandsSize(debuggingEnabled);
    return totalSize;
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t size = 0;
    using PIPELINE_SELECT = typename GfxFamily::PIPELINE_SELECT;
    size += sizeof(PIPELINE_SELECT);
    if (MemorySynchronizationCommands<GfxFamily>::isBarrierPriorToPipelineSelectWaRequired(rootDeviceEnvironment)) {
        size += sizeof(PIPE_CONTROL);
    }
    return size;
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreamble(LinearStream *pCommandStream, Device &device, uint32_t l3Config,
                                                GraphicsAllocation *preemptionCsr, LogicalStateHelper *logicalStateHelper) {
    programL3(pCommandStream, l3Config);
    programPreemption(pCommandStream, device, preemptionCsr, logicalStateHelper);
    programGenSpecificPreambleWorkArounds(pCommandStream, device.getHardwareInfo());
    programSemaphoreDelay(pCommandStream);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreemption(LinearStream *pCommandStream, Device &device, GraphicsAllocation *preemptionCsr, LogicalStateHelper *logicalStateHelper) {
    PreemptionHelper::programCsrBaseAddress<GfxFamily>(*pCommandStream, device, preemptionCsr, logicalStateHelper);
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getKernelDebuggingCommandsSize(bool debuggingActive) {
    if (debuggingActive) {
        return 2 * sizeof(MI_LOAD_REGISTER_IMM);
    }
    return 0;
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::appendProgramVFEState(const RootDeviceEnvironment &rootDeviceEnvironment, const StreamProperties &streamProperties, void *cmd) {}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getScratchSizeValueToProgramMediaVfeState(uint32_t scratchSize) {
    scratchSize >>= static_cast<uint32_t>(MemoryConstants::kiloByteShiftSize);
    uint32_t valueToProgram = 0;
    while (scratchSize >>= 1) {
        valueToProgram++;
    }
    return valueToProgram;
}

template <typename GfxFamily>
bool PreambleHelper<GfxFamily>::isSystolicModeConfigurable(const RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    return productHelper.isSystolicModeConfigurable(hwInfo);
}

} // namespace NEO
