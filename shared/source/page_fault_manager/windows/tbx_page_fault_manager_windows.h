/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/page_fault_manager/tbx_page_fault_manager.h"
#include "shared/source/page_fault_manager/windows/cpu_page_fault_manager_windows.h"

// Known false positive for valid virtual inheritance
#pragma warning(disable : 4250)

namespace NEO {

class TbxPageFaultManagerWindows final : public TbxPageFaultManager, public PageFaultManagerWindows {};

} // namespace NEO
