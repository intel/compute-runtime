/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

class CommandEncoderTests : public DeviceFixture,
                            public ::testing::Test {

  public:
    void SetUp() override {
        ::testing::Test::SetUp();
        DeviceFixture::SetUp();
    }
    void TearDown() override {
        DeviceFixture::TearDown();
        ::testing::Test::TearDown();
    }
};

HWTEST_F(CommandEncoderTests, givenImmDataWriteWhenProgrammingMiFlushDwThenSetAllRequiredFields) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));

    uint64_t gpuAddress = 0x1230000;
    uint64_t immData = 456;

    EncodeMiFlushDW<FamilyType>::programMiFlushDw(linearStream, gpuAddress, immData);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    EXPECT_EQ(sizeof(MI_FLUSH_DW), linearStream.getUsed());
    EXPECT_EQ(MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD, miFlushDwCmd->getPostSyncOperation());
    EXPECT_EQ(gpuAddress, miFlushDwCmd->getDestinationAddress());
    EXPECT_EQ(immData, miFlushDwCmd->getImmediateData());
}
