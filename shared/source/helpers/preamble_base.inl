/*
 * Copyright (C) 2018-2020 Intel Corporation
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

#include "hw_cmds.h"
#include "reg_configs_common.h"

#include <cstddef>

namespace NEO {

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programThreadArbitration(LinearStream *pCommandStream, uint32_t requiredThreadArbitrationPolicy) {
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getThreadArbitrationCommandsSize() {
    return 0;
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programGenSpecificPreambleWorkArounds(LinearStream *pCommandStream, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPerDssBackedBuffer(LinearStream *pCommandStream, const HardwareInfo &hwInfo, GraphicsAllocation *perDssBackBufferOffset) {
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getPerDssBackedBufferCommandsSize(const HardwareInfo &hwInfo) {
    return 0;
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
    totalSize += getKernelDebuggingCommandsSize(device.isDebuggerActive());
    return totalSize;
}

template <typename GfxFamily>
size_t PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(const HardwareInfo &hwInfo) {
    size_t size = 0;
    using PIPELINE_SELECT = typename GfxFamily::PIPELINE_SELECT;
    size += sizeof(PIPELINE_SELECT);
    if (MemorySynchronizationCommands<GfxFamily>::isPipeControlPriorToPipelineSelectWArequired(hwInfo)) {
        size += sizeof(PIPE_CONTROL);
    }
    return size;
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreamble(LinearStream *pCommandStream, Device &device, uint32_t l3Config,
                                                uint32_t requiredThreadArbitrationPolicy, GraphicsAllocation *preemptionCsr, GraphicsAllocation *perDssBackedBuffer) {
    programL3(pCommandStream, l3Config);
    programThreadArbitration(pCommandStream, requiredThreadArbitrationPolicy);
    programPreemption(pCommandStream, device, preemptionCsr);
    if (device.isDebuggerActive()) {
        programKernelDebugging(pCommandStream);
    }
    programGenSpecificPreambleWorkArounds(pCommandStream, device.getHardwareInfo());
    if (perDssBackedBuffer != nullptr) {
        programPerDssBackedBuffer(pCommandStream, device.getHardwareInfo(), perDssBackedBuffer);
    }
    programSemaphoreDelay(pCommandStream);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreemption(LinearStream *pCommandStream, Device &device, GraphicsAllocation *preemptionCsr) {
    PreemptionHelper::programCsrBaseAddress<GfxFamily>(*pCommandStream, device, preemptionCsr);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programKernelDebugging(LinearStream *pCommandStream) {
    LriHelper<GfxFamily>::program(pCommandStream,
                                  DebugModeRegisterOffset<GfxFamily>::registerOffset,
                                  DebugModeRegisterOffset<GfxFamily>::debugEnabledValue,
                                  false);

    LriHelper<GfxFamily>::program(pCommandStream,
                                  TdDebugControlRegisterOffset::registerOffset,
                                  TdDebugControlRegisterOffset::debugEnabledValue,
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
void PreambleHelper<GfxFamily>::programAdditionalFieldsInVfeState(VFE_STATE_TYPE *mediaVfeState, const HardwareInfo &hwInfo) {
}

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
