/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

namespace NEO {
class CommandStreamReceiver;
class GraphicsAllocation;

class TbxPageFaultManager : public virtual CpuPageFaultManager {
  public:
    struct PageFaultDataTbx {
        size_t size;
        bool hasBeenDownloaded = false;
        GraphicsAllocation *gfxAllocation = nullptr;
        uint32_t bank = 0;
        CommandStreamReceiver *csr = nullptr;
    };
    static std::unique_ptr<TbxPageFaultManager> create();

    TbxPageFaultManager() = default;

    using CpuPageFaultManager::insertAllocation;
    using CpuPageFaultManager::removeAllocation;
    void insertAllocation(CommandStreamReceiver *csr, GraphicsAllocation *alloc, uint32_t bank, void *ptr, size_t size) override;
    void removeAllocation(GraphicsAllocation *alloc) override;

    using CpuPageFaultManager::checkFaultHandlerFromPageFaultManager;
    bool verifyAndHandlePageFault(void *ptr, bool handlePageFault) override;

  protected:
    void handlePageFault(void *ptr, PageFaultDataTbx &faultData);

    std::unordered_map<void *, PageFaultDataTbx> memoryDataTbx;
    RecursiveSpinLock mtxTbx;
};

} // namespace NEO
