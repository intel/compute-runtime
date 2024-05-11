/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <string>

namespace NEO {

enum class FaultType : uint16_t {
    notPresent = 0b00,
    writeAccessViolation = 0b01,
    atomicAccessViolation = 0b10
};
enum class FaultAccess : uint16_t {
    read = 0b00,
    write = 0b01,
    atomic = 0b10
};
enum class FaultLevel : uint16_t {
    pte = 0b000,
    pde = 0b001,
    pdp = 0b010,
    pml4 = 0b011,
    pml5 = 0b0100
};

namespace GpuPageFaultHelpers {

std::string faultTypeToString(FaultType type);
std::string faultAccessToString(FaultAccess access);
std::string faultLevelToString(FaultLevel level);

} // namespace GpuPageFaultHelpers

} // namespace NEO
