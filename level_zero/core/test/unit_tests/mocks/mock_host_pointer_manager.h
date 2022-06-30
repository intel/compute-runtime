/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/driver/host_pointer_manager.h"
#include "level_zero/core/test/unit_tests/white_box.h"

namespace L0 {
namespace ult {

template <>
struct WhiteBox<::L0::HostPointerManager> : public ::L0::HostPointerManager {
    using ::L0::HostPointerManager::createHostPointerAllocation;
    using ::L0::HostPointerManager::hostPointerAllocations;
    using ::L0::HostPointerManager::memoryManager;
};

using HostPointerManager = WhiteBox<::L0::HostPointerManager>;

} // namespace ult
} // namespace L0
