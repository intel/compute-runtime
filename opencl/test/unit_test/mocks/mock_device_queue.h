/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/device_queue/device_queue.h"
#include "opencl/source/device_queue/device_queue_hw.h"
#include "opencl/source/helpers/hardware_commands_helper.h"

namespace NEO {
template <typename GfxFamily>
class MockDeviceQueueHw : public DeviceQueueHw<GfxFamily> {
    using BaseClass = DeviceQueueHw<GfxFamily>;
    using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using MI_ARB_CHECK = typename GfxFamily::MI_ARB_CHECK;
    using MEDIA_STATE_FLUSH = typename GfxFamily::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;

  public:
    using BaseClass::addArbCheckCmdWa;
    using BaseClass::addLriCmd;
    using BaseClass::addLriCmdWa;
    using BaseClass::addMediaStateClearCmds;
    using BaseClass::addMiAtomicCmdWa;
    using BaseClass::addPipeControlCmdWa;
    using BaseClass::addProfilingEndCmds;
    using BaseClass::buildSlbDummyCommands;
    using BaseClass::getBlockKernelStartPointer;
    using BaseClass::getCSPrefetchSize;
    using BaseClass::getExecutionModelCleanupSectionSize;
    using BaseClass::getMediaStateClearCmdsSize;
    using BaseClass::getMinimumSlbSize;
    using BaseClass::getProfilingEndCmdsSize;
    using BaseClass::getSlbCS;
    using BaseClass::getWaCommandsSize;
    using BaseClass::offsetDsh;

    bool arbCheckWa;
    bool miAtomicWa;
    bool lriWa;
    bool pipeControlWa;

    struct ExpectedCmds {
        MEDIA_STATE_FLUSH mediaStateFlush;
        MI_ARB_CHECK arbCheck;
        MI_ATOMIC miAtomic;
        MEDIA_INTERFACE_DESCRIPTOR_LOAD mediaIdLoad;
        MI_LOAD_REGISTER_IMM lriTrue;
        MI_LOAD_REGISTER_IMM lriFalse;
        PIPE_CONTROL pipeControl;
        PIPE_CONTROL noopedPipeControl;
        GPGPU_WALKER gpgpuWalker;
        uint8_t *prefetch;
        MI_BATCH_BUFFER_START bbStart;
    } expectedCmds;

    MockDeviceQueueHw(Context *context,
                      ClDevice *device,
                      cl_queue_properties &properties) : BaseClass(context, device, properties) {
        auto slb = this->getSlbBuffer();
        LinearStream *slbCS = getSlbCS();
        slbCS->replaceBuffer(slb->getUnderlyingBuffer(), slb->getUnderlyingBufferSize());
        size_t size = slbCS->getUsed();

        lriWa = false;
        addLriCmdWa(true);
        if (slbCS->getUsed() > size) {
            size = slbCS->getUsed();
            lriWa = true;
        }
        pipeControlWa = false;
        addPipeControlCmdWa();
        if (slbCS->getUsed() > size) {
            size = slbCS->getUsed();
            pipeControlWa = true;
        }
        arbCheckWa = false;
        addArbCheckCmdWa();
        if (slbCS->getUsed() > size) {
            size = slbCS->getUsed();
            arbCheckWa = true;
        }
        miAtomicWa = false;
        addMiAtomicCmdWa(0);
        if (slbCS->getUsed() > size) {
            size = slbCS->getUsed();
            miAtomicWa = true;
        }
        slbCS->replaceBuffer(slb->getUnderlyingBuffer(), slb->getUnderlyingBufferSize()); // reset

        setupExpectedCmds();
    };

    ~MockDeviceQueueHw() override {
        if (expectedCmds.prefetch)
            delete expectedCmds.prefetch;
    }

    MI_ATOMIC getExpectedMiAtomicCmd() {
        auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(this->queueBuffer->getUnderlyingBuffer());
        auto placeholder = (uint64_t)&igilCmdQueue->m_controls.m_DummyAtomicOperationPlaceholder;

        MI_ATOMIC miAtomic = GfxFamily::cmdInitAtomic;
        miAtomic.setReturnDataControl(0x1);
        miAtomic.setCsStall(0x1);
        HardwareCommandsHelper<GfxFamily>::programMiAtomic(miAtomic, placeholder, MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_8B_INCREMENT,
                                                           MI_ATOMIC::DATA_SIZE::DATA_SIZE_QWORD);

        return miAtomic;
    };

    MI_LOAD_REGISTER_IMM getExpectedLriCmd(bool arbCheck) {
        MI_LOAD_REGISTER_IMM lri = GfxFamily::cmdInitLoadRegisterImm;
        lri.setRegisterOffset(0x2248); // CTXT_PREMP_DBG offset
        if (arbCheck)
            lri.setDataDword(0x00000100); // set only bit 8 (Preempt On MI_ARB_CHK Only)
        else
            lri.setDataDword(0x0); // default value

        return lri;
    }

    PIPE_CONTROL getExpectedPipeControlCmd() {
        PIPE_CONTROL pc;
        this->initPipeControl(&pc);
        return pc;
    }

    MI_ARB_CHECK getExpectedArbCheckCmd() {
        return GfxFamily::cmdInitArbCheck;
    }

    void setupExpectedCmds() {
        expectedCmds.mediaStateFlush = GfxFamily::cmdInitMediaStateFlush;
        expectedCmds.arbCheck = getExpectedArbCheckCmd();
        expectedCmds.miAtomic = getExpectedMiAtomicCmd();
        expectedCmds.mediaIdLoad = GfxFamily::cmdInitMediaInterfaceDescriptorLoad;
        expectedCmds.mediaIdLoad.setInterfaceDescriptorTotalLength(2048);

        auto dataStartAddress = DeviceQueue::colorCalcStateSize;

        // add shift to second table ( 62 index of first ID table with scheduler )
        dataStartAddress += sizeof(INTERFACE_DESCRIPTOR_DATA) * DeviceQueue::schedulerIDIndex;

        expectedCmds.mediaIdLoad.setInterfaceDescriptorDataStartAddress(dataStartAddress);
        expectedCmds.lriTrue = getExpectedLriCmd(true);
        expectedCmds.lriFalse = getExpectedLriCmd(false);
        expectedCmds.pipeControl = getExpectedPipeControlCmd();
        memset(&expectedCmds.noopedPipeControl, 0x0, sizeof(PIPE_CONTROL));
        expectedCmds.gpgpuWalker = GfxFamily::cmdInitGpgpuWalker;
        expectedCmds.gpgpuWalker.setSimdSize(GPGPU_WALKER::SIMD_SIZE::SIMD_SIZE_SIMD16);
        expectedCmds.gpgpuWalker.setThreadGroupIdXDimension(1);
        expectedCmds.gpgpuWalker.setThreadGroupIdYDimension(1);
        expectedCmds.gpgpuWalker.setThreadGroupIdZDimension(1);
        expectedCmds.gpgpuWalker.setRightExecutionMask(0xFFFFFFFF);
        expectedCmds.gpgpuWalker.setBottomExecutionMask(0xFFFFFFFF);
        expectedCmds.prefetch = new uint8_t[DeviceQueueHw<GfxFamily>::getCSPrefetchSize()];
        memset(expectedCmds.prefetch, 0x0, DeviceQueueHw<GfxFamily>::getCSPrefetchSize());
        expectedCmds.bbStart = GfxFamily::cmdInitBatchBufferStart;
        auto slbPtr = reinterpret_cast<uintptr_t>(this->getSlbBuffer()->getUnderlyingBuffer());
        expectedCmds.bbStart.setBatchBufferStartAddressGraphicsaddress472(slbPtr);
    }

    IGIL_CommandQueue *getIgilQueue() {
        auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(DeviceQueue::queueBuffer->getUnderlyingBuffer());
        return igilCmdQueue;
    }
};
} // namespace NEO
