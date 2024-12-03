/*
 * Copyright (C) 2024 Intel Corporation
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
    static std::unique_ptr<TbxPageFaultManager> create();

    using CpuPageFaultManager::insertAllocation;
    using CpuPageFaultManager::removeAllocation;
    void insertAllocation(CommandStreamReceiver *csr, GraphicsAllocation *alloc, uint32_t bank, void *ptr, size_t size) override;
    void removeAllocation(GraphicsAllocation *alloc) override;

    using CpuPageFaultManager::checkFaultHandlerFromPageFaultManager;
    using CpuPageFaultManager::verifyAndHandlePageFault;

  protected:
    void handlePageFault(void *ptr, PageFaultData &faultData) override;
};

} // namespace NEO
