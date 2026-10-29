/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

namespace L0 {

void transferAndUnprotectMemoryWithHints(NEO::CpuPageFaultManager *pageFaultHandler, void *allocPtr, NEO::CpuPageFaultManager::PageFaultData &pageFaultData);

} // namespace L0
