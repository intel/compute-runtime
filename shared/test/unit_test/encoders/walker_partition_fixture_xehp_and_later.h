/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "gtest/gtest.h"

using namespace WalkerPartition;

struct WalkerPartitionTests : public ::testing::Test {
    void SetUp() override;

    void TearDown() override;

    template <typename GfxFamily>
    auto createWalker(uint64_t postSyncAddress) {
        using WalkerType = typename GfxFamily::DefaultWalkerType;
        using PostSyncType = decltype(GfxFamily::template getPostSyncType<WalkerType>());

        WalkerType walker;
        walker = GfxFamily::template getInitGpuWalker<WalkerType>();
        walker.setPartitionType(WalkerType::PARTITION_TYPE::PARTITION_TYPE_X);
        auto &postSync = walker.getPostSync();
        postSync.setOperation(PostSyncType::OPERATION::OPERATION_WRITE_TIMESTAMP);
        postSync.setDestinationAddress(postSyncAddress);
        return walker;
    }

    char cmdBuffer[4096u];
    WalkerPartition::WalkerPartitionArgs testArgs = {};
    HardwareInfo testHardwareInfo = {};
    void *cmdBufferAddress = nullptr;
    uint32_t totalBytesProgrammed = 0u;
    bool checkForProperCmdBufferAddressOffset = true;
};
