/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gpu_page_fault_helper.h"

namespace NEO::GpuPageFaultHelpers {

std::string faultTypeToString(FaultType type) {
    switch (type) {
    case FaultType::notPresent:
        return "NotPresent";
    case FaultType::writeAccessViolation:
        return "WriteAccessViolation";
    case FaultType::atomicAccessViolation:
        return "AtomicAccessViolation";
    default:
        return "Unknown";
    }
}

std::string faultAccessToString(FaultAccess access) {
    switch (access) {
    case FaultAccess::read:
        return "Read";
    case FaultAccess::write:
        return "Write";
    case FaultAccess::atomic:
        return "Atomic";
    default:
        return "Unknown";
    }
}

std::string faultLevelToString(FaultLevel level) {
    switch (level) {
    case FaultLevel::pte:
        return "PTE";
    case FaultLevel::pde:
        return "PDE";
    case FaultLevel::pdp:
        return "PDP";
    case FaultLevel::pml4:
        return "PML4";
    case FaultLevel::pml5:
        return "PML5";
    default:
        return "Unknown";
    }
}

} // namespace NEO::GpuPageFaultHelpers
