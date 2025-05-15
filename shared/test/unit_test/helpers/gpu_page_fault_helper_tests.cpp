/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gpu_page_fault_helper.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(GpuPageFaultHelperTest, givenValidAndInvalidFaultTypesWhenGettingStringRepresentationThenItIsCorrect) {
    EXPECT_EQ(std::string{"NotPresent"}, GpuPageFaultHelpers::faultTypeToString(FaultType::notPresent));
    EXPECT_EQ(std::string{"WriteAccessViolation"}, GpuPageFaultHelpers::faultTypeToString(FaultType::writeAccessViolation));
    EXPECT_EQ(std::string{"AtomicAccessViolation"}, GpuPageFaultHelpers::faultTypeToString(FaultType::atomicAccessViolation));
    EXPECT_EQ(std::string{"Unknown"}, GpuPageFaultHelpers::faultTypeToString(static_cast<FaultType>(0xcc))); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

TEST(GpuPageFaultHelperTest, givenValidAndInvalidFaultAccessesWhenGettingStringRepresentationThenItIsCorrect) {
    EXPECT_EQ(std::string{"Read"}, GpuPageFaultHelpers::faultAccessToString(FaultAccess::read));
    EXPECT_EQ(std::string{"Write"}, GpuPageFaultHelpers::faultAccessToString(FaultAccess::write));
    EXPECT_EQ(std::string{"Atomic"}, GpuPageFaultHelpers::faultAccessToString(FaultAccess::atomic));
    EXPECT_EQ(std::string{"Unknown"}, GpuPageFaultHelpers::faultAccessToString(static_cast<FaultAccess>(0xcc))); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}

TEST(GpuPageFaultHelperTest, givenValidAndInvalidFaultLevelWhenGettingStringRepresentationThenItIsCorrect) {
    EXPECT_EQ(std::string{"PTE"}, GpuPageFaultHelpers::faultLevelToString(FaultLevel::pte));
    EXPECT_EQ(std::string{"PDE"}, GpuPageFaultHelpers::faultLevelToString(FaultLevel::pde));
    EXPECT_EQ(std::string{"PDP"}, GpuPageFaultHelpers::faultLevelToString(FaultLevel::pdp));
    EXPECT_EQ(std::string{"PML4"}, GpuPageFaultHelpers::faultLevelToString(FaultLevel::pml4));
    EXPECT_EQ(std::string{"PML5"}, GpuPageFaultHelpers::faultLevelToString(FaultLevel::pml5));
    EXPECT_EQ(std::string{"Unknown"}, GpuPageFaultHelpers::faultLevelToString(static_cast<FaultLevel>(0xcc))); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}
