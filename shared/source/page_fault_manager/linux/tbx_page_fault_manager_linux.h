/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/page_fault_manager/linux/cpu_page_fault_manager_linux.h"
#include "shared/source/page_fault_manager/tbx_page_fault_manager.h"

namespace NEO {

class TbxPageFaultManagerLinux final : public PageFaultManagerLinux, public TbxPageFaultManager {};

} // namespace NEO
