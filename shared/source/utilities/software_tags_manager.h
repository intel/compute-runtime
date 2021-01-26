/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/software_tags.h"

namespace NEO {

class Device;
class GraphicsAllocation;
class LinearStream;

class SWTagsManager {
  public:
    SWTagsManager() = default;

    void initialize(Device &device);
    void shutdown();
    bool isInitialized() const { return initialized; }

    GraphicsAllocation *getBXMLHeapAllocation() { return bxmlHeap; }
    GraphicsAllocation *getSWTagHeapAllocation() { return tagHeap; }

    template <typename GfxFamily>
    void insertBXMLHeapAddress(LinearStream &cmdStream);
    template <typename GfxFamily>
    void insertSWTagHeapAddress(LinearStream &cmdStream);
    template <typename GfxFamily, typename Tag, typename... Params>
    void insertTag(LinearStream &cmdStream, Device &device, Params... params);

    template <typename GfxFamily>
    static size_t estimateSpaceForSWTags();

    static const unsigned int MAX_TAG_COUNT = 20;
    static const unsigned int MAX_TAG_HEAP_SIZE = 1024;

  private:
    void allocateBXMLHeap(Device &device);
    void allocateSWTagHeap(Device &device);

    MemoryManager *memoryManager;
    GraphicsAllocation *tagHeap = nullptr;
    GraphicsAllocation *bxmlHeap = nullptr;
    unsigned int currentHeapOffset = 0;
    unsigned int currentTagCount = 0;
    bool initialized = false;
};

template <typename GfxFamily>
void SWTagsManager::insertBXMLHeapAddress(LinearStream &cmdStream) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;

    auto ptr = reinterpret_cast<SWTags::BXMLHeapInfo *>(memoryManager->lockResource(bxmlHeap));
    MI_STORE_DATA_IMM storeDataImm = GfxFamily::cmdInitStoreDataImm;
    storeDataImm.setAddress(bxmlHeap->getGpuAddress());
    storeDataImm.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD);
    storeDataImm.setDataDword0(ptr->magicNumber);
    memoryManager->unlockResource(bxmlHeap);

    auto sdiSpace = cmdStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
    *sdiSpace = storeDataImm;
}

template <typename GfxFamily>
void SWTagsManager::insertSWTagHeapAddress(LinearStream &cmdStream) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;

    auto ptr = reinterpret_cast<SWTags::BXMLHeapInfo *>(memoryManager->lockResource(tagHeap));
    MI_STORE_DATA_IMM storeDataImm = GfxFamily::cmdInitStoreDataImm;
    storeDataImm.setAddress(tagHeap->getGpuAddress());
    storeDataImm.setDwordLength(MI_STORE_DATA_IMM::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD);
    storeDataImm.setDataDword0(ptr->magicNumber);
    memoryManager->unlockResource(tagHeap);

    auto sdiSpace = cmdStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
    *sdiSpace = storeDataImm;
}

template <typename GfxFamily, typename Tag, typename... Params>
void SWTagsManager::insertTag(LinearStream &cmdStream, Device &device, Params... params) {
    using MI_NOOP = typename GfxFamily::MI_NOOP;

    unsigned int tagSize = sizeof(Tag);

    if (currentTagCount >= MAX_TAG_COUNT || currentHeapOffset + tagSize > MAX_TAG_HEAP_SIZE) {
        return;
    }
    ++currentTagCount;

    Tag tag(std::forward<Params>(params)...);

    MemoryTransferHelper::transferMemoryToAllocation(false, device, tagHeap, currentHeapOffset, &tag, tagSize);

    MI_NOOP marker = GfxFamily::cmdInitNoop;
    marker.setIdentificationNumber(tag.getMarkerNoopID(tag.getOpCode()));
    marker.setIdentificationNumberRegisterWriteEnable(true);

    MI_NOOP offset = GfxFamily::cmdInitNoop;
    offset.setIdentificationNumber(tag.getOffsetNoopID(currentHeapOffset));
    currentHeapOffset += tagSize;

    MI_NOOP *pNoop = cmdStream.getSpaceForCmd<MI_NOOP>();
    *pNoop = marker;
    pNoop = cmdStream.getSpaceForCmd<MI_NOOP>();
    *pNoop = offset;
}

template <typename GfxFamily>
size_t SWTagsManager::estimateSpaceForSWTags() {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    using MI_NOOP = typename GfxFamily::MI_NOOP;

    return 2 * sizeof(MI_STORE_DATA_IMM) + 2 * MAX_TAG_COUNT * sizeof(MI_NOOP);
}

} // namespace NEO
