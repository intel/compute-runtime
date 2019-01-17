/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/preamble.h"
#include "runtime/helpers/string.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/utilities/tag_allocator.h"

namespace OCLRT {
template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::allocateSlbBuffer() {
    auto slbSize = getMinimumSlbSize() + getWaCommandsSize();
    slbSize *= 128; //num of enqueues
    slbSize += sizeof(MI_BATCH_BUFFER_START);
    slbSize = alignUp(slbSize, MemoryConstants::pageSize);
    slbSize += DeviceQueueHw<GfxFamily>::getExecutionModelCleanupSectionSize();
    slbSize += (4 * MemoryConstants::pageSize); // +4 pages spec restriction
    slbSize = alignUp(slbSize, MemoryConstants::pageSize);

    slbBuffer = device->getMemoryManager()->allocateGraphicsMemoryWithProperties({slbSize, GraphicsAllocation::AllocationType::UNDECIDED});
}

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::resetDeviceQueue() {
    auto &caps = device->getDeviceInfo();
    auto igilEventPool = reinterpret_cast<IGIL_EventPool *>(eventPoolBuffer->getUnderlyingBuffer());

    memset(eventPoolBuffer->getUnderlyingBuffer(), 0x0, eventPoolBuffer->getUnderlyingBufferSize());
    igilEventPool->m_size = caps.maxOnDeviceEvents;

    auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(queueBuffer->getUnderlyingBuffer());
    igilQueue = igilCmdQueue;

    igilCmdQueue->m_controls.m_StackSize =
        static_cast<uint32_t>((stackBuffer->getUnderlyingBufferSize() / sizeof(cl_uint)) - 1);
    igilCmdQueue->m_controls.m_StackTop =
        static_cast<uint32_t>((stackBuffer->getUnderlyingBufferSize() / sizeof(cl_uint)) - 1);
    igilCmdQueue->m_controls.m_PreviousHead = IGIL_DEVICE_QUEUE_HEAD_INIT;
    igilCmdQueue->m_controls.m_IDTAfterFirstPhase = 1;
    igilCmdQueue->m_controls.m_CurrentIDToffset = 1;
    igilCmdQueue->m_controls.m_PreviousStorageTop = static_cast<uint32_t>(queueStorageBuffer->getUnderlyingBufferSize());
    igilCmdQueue->m_controls.m_PreviousStackTop =
        static_cast<uint32_t>((stackBuffer->getUnderlyingBufferSize() / sizeof(cl_uint)) - 1);
    igilCmdQueue->m_controls.m_DebugNextBlockID = 0xFFFFFFFF;
    igilCmdQueue->m_controls.m_QstorageSize = static_cast<uint32_t>(queueStorageBuffer->getUnderlyingBufferSize());
    igilCmdQueue->m_controls.m_QstorageTop = static_cast<uint32_t>(queueStorageBuffer->getUnderlyingBufferSize());
    igilCmdQueue->m_controls.m_IsProfilingEnabled = static_cast<uint32_t>(isProfilingEnabled());
    igilCmdQueue->m_controls.m_IsSimulation = static_cast<uint32_t>(device->isSimulation());

    igilCmdQueue->m_controls.m_LastScheduleEventNumber = 0;
    igilCmdQueue->m_controls.m_PreviousNumberOfQueues = 0;
    igilCmdQueue->m_controls.m_EnqueueMarkerScheduled = 0;
    igilCmdQueue->m_controls.m_SecondLevelBatchOffset = 0;
    igilCmdQueue->m_controls.m_TotalNumberOfQueues = 0;
    igilCmdQueue->m_controls.m_EventTimestampAddress = 0;
    igilCmdQueue->m_controls.m_ErrorCode = 0;
    igilCmdQueue->m_controls.m_CurrentScheduleEventNumber = 0;
    igilCmdQueue->m_controls.m_DummyAtomicOperationPlaceholder = 0x00;
    igilCmdQueue->m_controls.m_DebugNextBlockGWS = 0;

    // set first stack element in surface at value "1", it protects Scheduler in corner case when StackTop is empty after Child execution
    auto stack = static_cast<uint32_t *>(stackBuffer->getUnderlyingBuffer());
    stack += ((stackBuffer->getUnderlyingBufferSize() / sizeof(cl_uint)) - 1);
    *stack = 1;

    igilCmdQueue->m_head = IGIL_DEVICE_QUEUE_HEAD_INIT;
    igilCmdQueue->m_size = static_cast<uint32_t>(queueBuffer->getUnderlyingBufferSize() - sizeof(IGIL_CommandQueue));
    igilCmdQueue->m_magic = IGIL_MAGIC_NUMBER;

    igilCmdQueue->m_controls.m_SchedulerEarlyReturn = DebugManager.flags.SchedulerSimulationReturnInstance.get();
    igilCmdQueue->m_controls.m_SchedulerEarlyReturnCounter = 0;

    buildSlbDummyCommands();

    igilCmdQueue->m_controls.m_SLBENDoffsetInBytes = -1;

    igilCmdQueue->m_controls.m_CriticalSection = ExecutionModelCriticalSection::Free;

    resetDSH();
}

template <typename GfxFamily>
size_t DeviceQueueHw<GfxFamily>::getMinimumSlbSize() {
    using MEDIA_STATE_FLUSH = typename GfxFamily::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;

    return sizeof(MEDIA_STATE_FLUSH) +
           sizeof(MEDIA_INTERFACE_DESCRIPTOR_LOAD) +
           sizeof(PIPE_CONTROL) +
           sizeof(GPGPU_WALKER) +
           sizeof(MEDIA_STATE_FLUSH) +
           sizeof(PIPE_CONTROL) +
           DeviceQueueHw<GfxFamily>::getCSPrefetchSize();
}

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::initPipeControl(PIPE_CONTROL *pc) {
    *pc = GfxFamily::cmdInitPipeControl;
    pc->setStateCacheInvalidationEnable(0x1);
    pc->setDcFlushEnable(true);
    pc->setPipeControlFlushEnable(true);
    pc->setTextureCacheInvalidationEnable(true);
    pc->setCommandStreamerStallEnable(true);
}

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::buildSlbDummyCommands() {
    using MEDIA_STATE_FLUSH = typename GfxFamily::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename GfxFamily::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;

    auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(queueBuffer->getUnderlyingBuffer());
    auto slbEndOffset = igilCmdQueue->m_controls.m_SLBENDoffsetInBytes;
    size_t commandsSize = getMinimumSlbSize() + getWaCommandsSize();
    size_t numEnqueues = numberOfDeviceEnqueues;

    // buildSlbDummyCommands is called from resetDeviceQueue() - reset slbCS each time
    slbCS.replaceBuffer(slbBuffer->getUnderlyingBuffer(), slbBuffer->getUnderlyingBufferSize());

    if (slbEndOffset >= 0) {
        DEBUG_BREAK_IF(slbEndOffset % commandsSize != 0);
        //We always overwrite at most one enqueue space with BB_START command pointing to cleanup section
        //if SLBENDoffset is the at the end then BB_START added after scheduler did not corrupt anything so no need to regenerate
        numEnqueues = (slbEndOffset == static_cast<int>(commandsSize)) ? 0 : 1;
        slbCS.getSpace(slbEndOffset);
    }

    for (size_t i = 0; i < numEnqueues; i++) {
        auto mediaStateFlush = slbCS.getSpaceForCmd<MEDIA_STATE_FLUSH>();
        *mediaStateFlush = GfxFamily::cmdInitMediaStateFlush;

        addArbCheckCmdWa();

        addMiAtomicCmdWa((uint64_t)&igilCmdQueue->m_controls.m_DummyAtomicOperationPlaceholder);

        auto mediaIdLoad = slbCS.getSpaceForCmd<MEDIA_INTERFACE_DESCRIPTOR_LOAD>();
        *mediaIdLoad = GfxFamily::cmdInitMediaInterfaceDescriptorLoad;
        mediaIdLoad->setInterfaceDescriptorTotalLength(2048);

        auto dataStartAddress = colorCalcStateSize;

        mediaIdLoad->setInterfaceDescriptorDataStartAddress(dataStartAddress + sizeof(INTERFACE_DESCRIPTOR_DATA) * schedulerIDIndex);

        addLriCmdWa(true);

        if (isProfilingEnabled()) {
            addPipeControlCmdWa();
            auto pipeControl = slbCS.getSpaceForCmd<PIPE_CONTROL>();
            initPipeControl(pipeControl);

        } else {
            auto noop = slbCS.getSpace(sizeof(PIPE_CONTROL));
            memset(noop, 0x0, sizeof(PIPE_CONTROL));
            addPipeControlCmdWa(true);
        }

        auto gpgpuWalker = slbCS.getSpaceForCmd<GPGPU_WALKER>();
        *gpgpuWalker = GfxFamily::cmdInitGpgpuWalker;
        gpgpuWalker->setSimdSize(GPGPU_WALKER::SIMD_SIZE::SIMD_SIZE_SIMD16);
        gpgpuWalker->setThreadGroupIdXDimension(1);
        gpgpuWalker->setThreadGroupIdYDimension(1);
        gpgpuWalker->setThreadGroupIdZDimension(1);
        gpgpuWalker->setRightExecutionMask(0xFFFFFFFF);
        gpgpuWalker->setBottomExecutionMask(0xFFFFFFFF);

        mediaStateFlush = slbCS.getSpaceForCmd<MEDIA_STATE_FLUSH>();
        *mediaStateFlush = GfxFamily::cmdInitMediaStateFlush;

        addArbCheckCmdWa();

        addPipeControlCmdWa();

        auto pipeControl2 = slbCS.getSpaceForCmd<PIPE_CONTROL>();
        initPipeControl(pipeControl2);

        addLriCmdWa(false);

        auto prefetch = slbCS.getSpace(getCSPrefetchSize());
        memset(prefetch, 0x0, getCSPrefetchSize());
    }

    // always the same BBStart position (after 128 enqueues)
    auto bbStartOffset = (commandsSize * 128) - slbCS.getUsed();
    slbCS.getSpace(bbStartOffset);

    auto bbStart = slbCS.getSpaceForCmd<MI_BATCH_BUFFER_START>();
    *bbStart = GfxFamily::cmdInitBatchBufferStart;
    auto slbPtr = reinterpret_cast<uintptr_t>(slbBuffer->getUnderlyingBuffer());
    bbStart->setBatchBufferStartAddressGraphicsaddress472(slbPtr);

    igilCmdQueue->m_controls.m_CleanupSectionSize = 0;
    igilQueue->m_controls.m_CleanupSectionAddress = 0;
}

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::addExecutionModelCleanUpSection(Kernel *parentKernel, TagNode<HwTimeStamps> *hwTimeStamp, uint32_t taskCount) {
    // CleanUp Section
    auto offset = slbCS.getUsed();
    auto alignmentSize = alignUp(offset, MemoryConstants::pageSize) - offset;
    slbCS.getSpace(alignmentSize);
    offset = slbCS.getUsed();

    igilQueue->m_controls.m_CleanupSectionAddress = ptrOffset(slbBuffer->getGpuAddress(), slbCS.getUsed());
    GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(&slbCS, *parentKernel, true);

    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    if (hwTimeStamp != nullptr) {
        uint64_t TimeStampAddress = hwTimeStamp->getGraphicsAllocation()->getGpuAddress() + ptrDiff(&hwTimeStamp->tag->ContextCompleteTS, hwTimeStamp->getGraphicsAllocation()->getUnderlyingBuffer());
        igilQueue->m_controls.m_EventTimestampAddress = TimeStampAddress;

        addProfilingEndCmds(TimeStampAddress);

        //enable preemption
        addLriCmd(false);
    }

    uint64_t criticalSectionAddress = (uint64_t)&igilQueue->m_controls.m_CriticalSection;

    addPipeControlCmdWa();

    PipeControlHelper<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(&slbCS, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, criticalSectionAddress, ExecutionModelCriticalSection::Free);

    uint64_t tagAddress = reinterpret_cast<uint64_t>(device->getDefaultEngine().commandStreamReceiver->getTagAddress());

    addPipeControlCmdWa();

    PipeControlHelper<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(&slbCS, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, tagAddress, taskCount);

    addMediaStateClearCmds();

    auto pBBE = slbCS.getSpaceForCmd<MI_BATCH_BUFFER_END>();
    *pBBE = GfxFamily::cmdInitBatchBufferEnd;

    igilQueue->m_controls.m_CleanupSectionSize = (uint32_t)(slbCS.getUsed() - offset);
}

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::resetDSH() {
    if (heaps[IndirectHeap::DYNAMIC_STATE]) {
        heaps[IndirectHeap::DYNAMIC_STATE]->replaceBuffer(heaps[IndirectHeap::DYNAMIC_STATE]->getCpuBase(), heaps[IndirectHeap::DYNAMIC_STATE]->getMaxAvailableSpace());
        heaps[IndirectHeap::DYNAMIC_STATE]->getSpace(colorCalcStateSize);
    }
}

template <typename GfxFamily>
IndirectHeap *DeviceQueueHw<GfxFamily>::getIndirectHeap(IndirectHeap::Type type) {

    if (!heaps[type]) {
        switch (type) {
        case IndirectHeap::DYNAMIC_STATE: {
            heaps[type] = new IndirectHeap(dshBuffer);
            // get space for colorCalc and 2 ID tables at the beginning
            heaps[type]->getSpace(colorCalcStateSize);
            break;
        }
        default:
            break;
        }
    }
    return heaps[type];
}

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::setupIndirectState(IndirectHeap &surfaceStateHeap, IndirectHeap &dynamicStateHeap, Kernel *parentKernel, uint32_t parentIDCount) {
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;
    void *pDSH = dynamicStateHeap.getCpuBase();

    // Set scheduler ID to last entry in first table, it will have ID == 0, blocks will have following entries.
    auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(queueBuffer->getUnderlyingBuffer());
    igilCmdQueue->m_controls.m_IDTstart = colorCalcStateSize + sizeof(INTERFACE_DESCRIPTOR_DATA) * (interfaceDescriptorEntries - 2);

    // Parent's dsh is located after ColorCalcState and 2 ID tables
    igilCmdQueue->m_controls.m_DynamicHeapStart = offsetDsh + alignUp((uint32_t)parentKernel->getDynamicStateHeapSize(), GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
    igilCmdQueue->m_controls.m_DynamicHeapSizeInBytes = (uint32_t)dshBuffer->getUnderlyingBufferSize();

    igilCmdQueue->m_controls.m_CurrentDSHoffset = igilCmdQueue->m_controls.m_DynamicHeapStart;
    igilCmdQueue->m_controls.m_ParentDSHOffset = offsetDsh;

    uint32_t blockIndex = parentIDCount;

    pDSH = ptrOffset(pDSH, colorCalcStateSize);

    INTERFACE_DESCRIPTOR_DATA *pIDDestination = static_cast<INTERFACE_DESCRIPTOR_DATA *>(pDSH);

    BlockKernelManager *blockManager = parentKernel->getProgram()->getBlockKernelManager();
    uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

    uint32_t maxBindingTableCount = 0;
    uint32_t totalBlockSSHSize = 0;

    igilCmdQueue->m_controls.m_StartBlockID = blockIndex;

    for (uint32_t i = 0; i < blockCount; i++) {
        const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);

        auto blockAllocation = pBlockInfo->getGraphicsAllocation();
        DEBUG_BREAK_IF(!blockAllocation);

        auto gpuAddress = blockAllocation ? blockAllocation->getGpuAddressToPatch() : 0llu;

        auto bindingTableCount = pBlockInfo->patchInfo.bindingTableState->Count;
        maxBindingTableCount = std::max(maxBindingTableCount, bindingTableCount);

        totalBlockSSHSize += alignUp(pBlockInfo->heapInfo.pKernelHeader->SurfaceStateHeapSize, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

        auto btOffset = KernelCommandsHelper<GfxFamily>::pushBindingTableAndSurfaceStates(surfaceStateHeap, *pBlockInfo);

        parentKernel->setReflectionSurfaceBlockBtOffset(i, static_cast<uint32_t>(btOffset));

        // Determine SIMD size
        uint32_t simd = pBlockInfo->getMaxSimdSize();
        DEBUG_BREAK_IF(pBlockInfo->patchInfo.interfaceDescriptorData == nullptr);

        uint32_t idOffset = pBlockInfo->patchInfo.interfaceDescriptorData->Offset;
        const INTERFACE_DESCRIPTOR_DATA *pBlockID = static_cast<const INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(pBlockInfo->heapInfo.pDsh, idOffset));

        pIDDestination[blockIndex + i] = *pBlockID;
        pIDDestination[blockIndex + i].setKernelStartPointerHigh(gpuAddress >> 32);
        pIDDestination[blockIndex + i].setKernelStartPointer((uint32_t)gpuAddress);
        pIDDestination[blockIndex + i].setBarrierEnable(pBlockInfo->patchInfo.executionEnvironment->HasBarriers > 0);
        pIDDestination[blockIndex + i].setDenormMode(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL);

        // Set offset to sampler states, block's DHSOffset is added by scheduler
        pIDDestination[blockIndex + i].setSamplerStatePointer(static_cast<uint32_t>(pBlockInfo->getBorderColorStateSize()));

        auto threadPayload = pBlockInfo->patchInfo.threadPayload;
        DEBUG_BREAK_IF(nullptr == threadPayload);

        auto numChannels = PerThreadDataHelper::getNumLocalIdChannels(*threadPayload);
        auto sizePerThreadData = getPerThreadSizeLocalIDs(simd, numChannels);

        auto numGrfPerThreadData = static_cast<uint32_t>(sizePerThreadData / sizeof(GRF));

        // HW requires a minimum of 1 GRF of perThreadData for each thread in a thread group
        // when sizeCrossThreadData != 0
        numGrfPerThreadData = std::max(numGrfPerThreadData, 1u);
        pIDDestination[blockIndex + i].setConstantIndirectUrbEntryReadLength(numGrfPerThreadData);
    }

    igilCmdQueue->m_controls.m_BTmaxSize = alignUp(maxBindingTableCount * (uint32_t)sizeof(BINDING_TABLE_STATE), INTERFACE_DESCRIPTOR_DATA::BINDINGTABLEPOINTER::BINDINGTABLEPOINTER_ALIGN_SIZE);
    igilCmdQueue->m_controls.m_BTbaseOffset = alignUp((uint32_t)surfaceStateHeap.getUsed(), INTERFACE_DESCRIPTOR_DATA::BINDINGTABLEPOINTER::BINDINGTABLEPOINTER_ALIGN_SIZE);
    igilCmdQueue->m_controls.m_CurrentSSHoffset = igilCmdQueue->m_controls.m_BTbaseOffset;
}

template <typename GfxFamily>
size_t DeviceQueueHw<GfxFamily>::setSchedulerCrossThreadData(SchedulerKernel &scheduler) {
    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    size_t offset = dshBuffer->getUnderlyingBufferSize() - scheduler.getCurbeSize() - 4096; // Page size padding

    auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(queueBuffer->getUnderlyingBuffer());
    igilCmdQueue->m_controls.m_SchedulerDSHOffset = (uint32_t)offset;
    igilCmdQueue->m_controls.m_SchedulerConstantBufferSize = (uint32_t)scheduler.getCurbeSize();

    return offset;
}

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::dispatchScheduler(CommandQueue &cmdQ, SchedulerKernel &scheduler, PreemptionMode preemptionMode, IndirectHeap *ssh, IndirectHeap *dsh) {
    GpgpuWalkerHelper<GfxFamily>::dispatchScheduler(cmdQ,
                                                    *this,
                                                    preemptionMode,
                                                    scheduler,
                                                    ssh,
                                                    dsh);
    return;
}

template <typename GfxFamily>
size_t DeviceQueueHw<GfxFamily>::getCSPrefetchSize() {
    return 512;
}

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::addLriCmd(bool setArbCheck) {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    auto lri = slbCS.getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *lri = GfxFamily::cmdInitLoadRegisterImm;
    lri->setRegisterOffset(0x2248); // CTXT_PREMP_DBG offset
    if (setArbCheck)
        lri->setDataDword(0x00000100); // set only bit 8 (Preempt On MI_ARB_CHK Only)
    else
        lri->setDataDword(0x0);
}

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::addMediaStateClearCmds() {
    typedef typename GfxFamily::MEDIA_VFE_STATE MEDIA_VFE_STATE;

    addPipeControlCmdWa();

    auto pipeControl = slbCS.getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = GfxFamily::cmdInitPipeControl;
    pipeControl->setGenericMediaStateClear(true);
    pipeControl->setCommandStreamerStallEnable(true);

    addDcFlushToPipeControlWa(pipeControl);

    PreambleHelper<GfxFamily>::programVFEState(&slbCS, device->getHardwareInfo(), 0, 0);
}

template <typename GfxFamily>
size_t DeviceQueueHw<GfxFamily>::getMediaStateClearCmdsSize() {
    using MEDIA_VFE_STATE = typename GfxFamily::MEDIA_VFE_STATE;
    // PC with GenreicMediaStateClear + WA PC
    size_t size = 2 * sizeof(PIPE_CONTROL);

    // VFE state cmds
    size += sizeof(PIPE_CONTROL);
    size += sizeof(MEDIA_VFE_STATE);
    return size;
}

template <typename GfxFamily>
size_t DeviceQueueHw<GfxFamily>::getExecutionModelCleanupSectionSize() {
    size_t totalSize = 0;
    totalSize += sizeof(PIPE_CONTROL) +
                 2 * sizeof(MI_LOAD_REGISTER_REG) +
                 sizeof(MI_LOAD_REGISTER_IMM) +
                 sizeof(PIPE_CONTROL) +
                 sizeof(MI_MATH) +
                 NUM_ALU_INST_FOR_READ_MODIFY_WRITE * sizeof(MI_MATH_ALU_INST_INLINE);

    totalSize += getProfilingEndCmdsSize();
    totalSize += getMediaStateClearCmdsSize();

    totalSize += 4 * sizeof(PIPE_CONTROL);
    totalSize += sizeof(MI_BATCH_BUFFER_END);
    return totalSize;
}

template <typename GfxFamily>
size_t DeviceQueueHw<GfxFamily>::getProfilingEndCmdsSize() {
    size_t size = 0;
    size += sizeof(PIPE_CONTROL) + sizeof(MI_STORE_REGISTER_MEM);
    size += sizeof(MI_LOAD_REGISTER_IMM);
    return size;
}

template <typename GfxFamily>
void DeviceQueueHw<GfxFamily>::addDcFlushToPipeControlWa(PIPE_CONTROL *pc) {}

} // namespace OCLRT
