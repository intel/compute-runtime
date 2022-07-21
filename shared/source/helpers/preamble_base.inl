/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_helper.h"
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
    bool debuggingEnabled = device.getDebugger() != nullptr || device.isDebuggerActive();
    totalSize += getKernelDebuggingCommandsSize(debuggingEnabled);
    return totalSize;
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(const HardwareInfo &hwInfo) {
    size_t size = 0;
    using PIPELINE_SELECT = typename GfxFamily::PIPELINE_SELECT;
    size += sizeof(PIPELINE_SELECT);
    if (MemorySynchronizationCommands<GfxFamily>::isBarrierlPriorToPipelineSelectWaRequired(hwInfo)) {
        size += sizeof(PIPE_CONTROL);
    }
    return size;
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreamble(LinearStream *pCommandStream, Device &device, uint32_t l3Config,
                                                GraphicsAllocation *preemptionCsr, LogicalStateHelper *logicalStateHelper) {
    programL3(pCommandStream, l3Config);
    programPreemption(pCommandStream, device, preemptionCsr, logicalStateHelper);
    if (device.isDebuggerActive()) {
        programKernelDebugging(pCommandStream);
    }
    programGenSpecificPreambleWorkArounds(pCommandStream, device.getHardwareInfo());
    programSemaphoreDelay(pCommandStream);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreemption(LinearStream *pCommandStream, Device &device, GraphicsAllocation *preemptionCsr, LogicalStateHelper *logicalStateHelper) {
    PreemptionHelper::programCsrBaseAddress<GfxFamily>(*pCommandStream, device, preemptionCsr, logicalStateHelper);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programKernelDebugging(LinearStream *pCommandStream) {
    LriHelper<GfxFamily>::program(pCommandStream,
                                  DebugModeRegisterOffset<GfxFamily>::registerOffset,
                                  DebugModeRegisterOffset<GfxFamily>::debugEnabledValue,
                                  true);

    LriHelper<GfxFamily>::program(pCommandStream,
                                  TdDebugControlRegisterOffset<GfxFamily>::registerOffset,
                                  TdDebugControlRegisterOffset<GfxFamily>::debugEnabledValue,
                                  false);
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getKernelDebuggingCommandsSize(bool debuggingActive) {
    if (debuggingActive) {
        return 2 * sizeof(MI_LOAD_REGISTER_IMM);
    }
    return 0;
}

template <typename GfxFamily>
bool PreambleHelper<GfxFamily>::isL3Configurable(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programAdditionalFieldsInVfeState(VFE_STATE_TYPE *mediaVfeState, const HardwareInfo &hwInfo, bool disableEUFusion) {
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::appendProgramVFEState(const HardwareInfo &hwInfo, const StreamProperties &streamProperties, void *cmd) {}

template <typename GfxFamily>
uint32_t PreambleHelper<GfxFamily>::getScratchSizeValueToProgramMediaVfeState(uint32_t scratchSize) {
    scratchSize >>= static_cast<uint32_t>(MemoryConstants::kiloByteShiftSize);
    uint32_t valueToProgram = 0;
    while (scratchSize >>= 1) {
        valueToProgram++;
    }
    return valueToProgram;
}

} // namespace NEO
