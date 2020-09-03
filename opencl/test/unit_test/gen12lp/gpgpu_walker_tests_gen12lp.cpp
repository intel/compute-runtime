/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "test.h"

namespace NEO {
struct GpgpuWalkerTests : public ::testing::Test {
    void SetUp() override {
    }

    void TearDown() override {
    }
};

GEN12LPTEST_F(GpgpuWalkerTests, givenMiStoreRegMemWhenAdjustMiStoreRegMemModeThenMmioRemapEnableIsSet) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    MI_STORE_REGISTER_MEM cmd = FamilyType::cmdInitStoreRegisterMem;

    GpgpuWalkerHelper<FamilyType>::adjustMiStoreRegMemMode(&cmd);

    EXPECT_EQ(true, cmd.getMmioRemapEnable());
}
} // namespace NEO