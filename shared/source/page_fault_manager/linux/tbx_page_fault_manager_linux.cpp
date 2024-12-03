/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/page_fault_manager/linux/tbx_page_fault_manager_linux.h"

namespace NEO {
std::unique_ptr<TbxPageFaultManager> TbxPageFaultManager::create() {
    return std::make_unique<TbxPageFaultManagerLinux>();
}

void TbxPageFaultManagerLinux::handlePageFault(void *ptr, PageFaultData &faultData) {
    TbxPageFaultManager::handlePageFault(ptr, faultData);
}
} // namespace NEO
