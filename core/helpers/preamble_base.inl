/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/linear_stream.h"
#include "core/command_stream/preemption.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/hw_cmds.h"
#include "core/helpers/preamble.h"
#include "runtime/device/device.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/kernel/kernel.h"

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
    totalSize += getKernelDebuggingCommandsSize(device.isSourceLevelDebuggerActive());
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
    if (device.isSourceLevelDebuggerActive()) {
        programKernelDebugging(pCommandStream);
    }
    programGenSpecificPreambleWorkArounds(pCommandStream, device.getHardwareInfo());
    if (DebugManager.flags.ForcePerDssBackedBufferProgramming.get()) {
        programPerDssBackedBuffer(pCommandStream, device.getHardwareInfo(), perDssBackedBuffer);
    }
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programPreemption(LinearStream *pCommandStream, Device &device, GraphicsAllocation *preemptionCsr) {
    PreemptionHelper::programCsrBaseAddress<GfxFamily>(*pCommandStream, device, preemptionCsr);
}

template <typename GfxFamily>
void PreambleHelper<GfxFamily>::programKernelDebugging(LinearStream *pCommandStream) {
    auto pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM)));
    *pCmd = GfxFamily::cmdInitLoadRegisterImm;
    pCmd->setRegisterOffset(DebugModeRegisterOffset<GfxFamily>::registerOffset);
    pCmd->setDataDword(DebugModeRegisterOffset<GfxFamily>::debugEnabledValue);

    auto pCmd2 = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(pCommandStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM)));
    *pCmd2 = GfxFamily::cmdInitLoadRegisterImm;
    pCmd2->setRegisterOffset(TdDebugControlRegisterOffset::registerOffset);
    pCmd2->setDataDword(TdDebugControlRegisterOffset::debugEnabledValue);
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
