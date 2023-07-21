/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"

namespace NEO {
std::unique_ptr<PageFaultManager> PageFaultManager::create() {
    auto pageFaultManager = std::make_unique<MockPageFaultManager>();

    pageFaultManager->selectGpuDomainHandler();
    return pageFaultManager;
}
} // namespace NEO
