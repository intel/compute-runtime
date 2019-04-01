/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/linear_stream.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/indirect_heap/indirect_heap.h"
#include "runtime/kernel/kernel.h"
#include "runtime/program/program.h"
#include "runtime/scheduler/scheduler_kernel.h"

#include "hw_cmds.h"

namespace NEO {

template <typename GfxFamily>
class DeviceQueueHw : public DeviceQueue {
    using BaseClass = DeviceQueue;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using MI_MATH = typename GfxFamily::MI_MATH;
    using MI_MATH_ALU_INST_INLINE = typename GfxFamily::MI_MATH_ALU_INST_INLINE;

  public:
    DeviceQueueHw(Context *context,
                  Device *device,
                  cl_queue_properties &properties) : BaseClass(context, device, properties) {
        allocateSlbBuffer();
        offsetDsh = colorCalcStateSize + (uint32_t)sizeof(INTERFACE_DESCRIPTOR_DATA) * interfaceDescriptorEntries * numberOfIDTables;
        igilQueue = reinterpret_cast<IGIL_CommandQueue *>(queueBuffer->getUnderlyingBuffer());
    }

    static DeviceQueue *create(Context *context,
                               Device *device,
                               cl_queue_properties &properties) {
        return new (std::nothrow) DeviceQueueHw<GfxFamily>(context, device, properties);
    }

    IndirectHeap *getIndirectHeap(IndirectHeap::Type type) override;

    LinearStream *getSlbCS() { return &slbCS; }
    void resetDSH();

    size_t setSchedulerCrossThreadData(SchedulerKernel &scheduler);

    void setupIndirectState(IndirectHeap &surfaceStateHeap, IndirectHeap &dynamicStateHeap, Kernel *parentKernel, uint32_t parentIDCount) override;

    void addExecutionModelCleanUpSection(Kernel *parentKernel, TagNode<HwTimeStamps> *hwTimeStamp, uint32_t taskCount) override;
    void resetDeviceQueue() override;
    void dispatchScheduler(LinearStream &commandStream, SchedulerKernel &scheduler, PreemptionMode preemptionMode, IndirectHeap *ssh, IndirectHeap *dsh) override;

    uint32_t getSchedulerReturnInstance() {
        return igilQueue->m_controls.m_SchedulerEarlyReturn;
    }

    static size_t getCSPrefetchSize();

  protected:
    void allocateSlbBuffer();
    size_t getMinimumSlbSize();
    size_t getWaCommandsSize();
    void addArbCheckCmdWa();
    void addMiAtomicCmdWa(uint64_t atomicOpPlaceholder);
    void addLriCmdWa(bool setArbCheck);
    void addLriCmd(bool setArbCheck);
    void addPipeControlCmdWa(bool isNoopCmd = false);
    void initPipeControl(PIPE_CONTROL *pc);
    void buildSlbDummyCommands();
    void addDcFlushToPipeControlWa(PIPE_CONTROL *pc);

    void addProfilingEndCmds(uint64_t timestampAddress);
    static size_t getProfilingEndCmdsSize();

    MOCKABLE_VIRTUAL void addMediaStateClearCmds();
    static size_t getMediaStateClearCmdsSize();

    static size_t getExecutionModelCleanupSectionSize();

    LinearStream slbCS;
    IGIL_CommandQueue *igilQueue = nullptr;
};
} // namespace NEO
