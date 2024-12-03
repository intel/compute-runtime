/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/windows/tbx_page_fault_manager_windows.h"

namespace NEO {
std::unique_ptr<TbxPageFaultManager> TbxPageFaultManager::create() {
    return std::make_unique<TbxPageFaultManagerWindows>();
}

void TbxPageFaultManagerWindows::handlePageFault(void *ptr, PageFaultData &faultData) {
    TbxPageFaultManager::handlePageFault(ptr, faultData);
}

} // namespace NEO
