/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
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

    static const unsigned int maxTagCount = 200;
    static const unsigned int maxTagHeapSize = 16384;
    unsigned int currentCallCount = 0;
    unsigned int getCurrentHeapOffset() { return currentHeapOffset; }

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
    auto ptr = reinterpret_cast<SWTags::BXMLHeapInfo *>(memoryManager->lockResource(bxmlHeap));
    EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                      bxmlHeap->getGpuAddress(),
                                                      ptr->magicNumber,
                                                      0,
                                                      false,
                                                      false);
    memoryManager->unlockResource(bxmlHeap);
}

template <typename GfxFamily>
void SWTagsManager::insertSWTagHeapAddress(LinearStream &cmdStream) {
    auto ptr = reinterpret_cast<SWTags::BXMLHeapInfo *>(memoryManager->lockResource(tagHeap));
    EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream,
                                                      tagHeap->getGpuAddress(),
                                                      ptr->magicNumber,
                                                      0,
                                                      false,
                                                      false);
    memoryManager->unlockResource(tagHeap);
}

template <typename GfxFamily, typename Tag, typename... Params>
void SWTagsManager::insertTag(LinearStream &cmdStream, Device &device, Params... params) {
    using MI_NOOP = typename GfxFamily::MI_NOOP;

    unsigned int tagSize = sizeof(Tag);

    if (currentTagCount >= maxTagCount || getCurrentHeapOffset() + tagSize > maxTagHeapSize) {
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
    using MI_NOOP = typename GfxFamily::MI_NOOP;

    return 2 * EncodeStoreMemory<GfxFamily>::getStoreDataImmSize() + 2 * maxTagCount * sizeof(MI_NOOP);
}

} // namespace NEO
