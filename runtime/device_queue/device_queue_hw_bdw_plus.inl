/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device_queue/device_queue_hw_base.inl"

namespace NEO {

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
void DeviceQueueHw<GfxFamily>::addMediaStateClearCmds() {
    typedef typename GfxFamily::MEDIA_VFE_STATE MEDIA_VFE_STATE;

    addPipeControlCmdWa();

    auto pipeControl = slbCS.getSpaceForCmd<PIPE_CONTROL>();
    *pipeControl = GfxFamily::cmdInitPipeControl;
    pipeControl->setGenericMediaStateClear(true);
    pipeControl->setCommandStreamerStallEnable(true);

    addDcFlushToPipeControlWa(pipeControl);

    PreambleHelper<GfxFamily>::programVFEState(&slbCS, device->getHardwareInfo(), 0, 0, device->getDeviceInfo().maxFrontEndThreads);
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

        auto btOffset = HardwareCommandsHelper<GfxFamily>::pushBindingTableAndSurfaceStates(surfaceStateHeap, *pBlockInfo);

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

} // namespace NEO
