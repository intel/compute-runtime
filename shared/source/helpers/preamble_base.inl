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
#include "shared/source/helpers/hw_cmds.h"
#include "shared/source/helpers/preamble.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/kernel/kernel.h"

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
uint32_t PreambleHelper<GfxFamily>::getDefaultThreadArbitrationPolicy() {
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
    if (HardwareCommandsHelper<GfxFamily>::isPipeControlPriorToPipelineSelectWArequired(hwInfo)) {
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
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreemption(LinearStream *pCommandStream, Device &device, GraphicsAllocation *preemptionCsr) {
    PreemptionHelper::programCsrBaseAddress<GfxFamily>(*pCommandStream, device, preemptionCsr);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programKernelDebugging(LinearStream *pCommandStream) {
    LriHelper<GfxFamily>::program(pCommandStream, DebugModeRegisterOffset<GfxFamily>::registerOffset,
                                  DebugModeRegisterOffset<GfxFamily>::debugEnabledValue);
    LriHelper<GfxFamily>::program(pCommandStream, TdDebugControlRegisterOffset::registerOffset,
                                  TdDebugControlRegisterOffset::debugEnabledValue);
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

} // namespace NEO
