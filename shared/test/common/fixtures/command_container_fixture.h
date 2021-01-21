/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/test/common/fixtures/device_fixture.h"

#include "test.h"

namespace NEO {

class CommandEncodeStatesFixture : public DeviceFixture {
  public:
    class MyMockCommandContainer : public CommandContainer {
      public:
        using CommandContainer::dirtyHeaps;
    };

    void SetUp() {
        DeviceFixture::SetUp();
        cmdContainer = std::make_unique<MyMockCommandContainer>();
        cmdContainer->initialize(pDevice);
        cmdContainer->setDirtyStateForAllHeaps(false);
    }
    void TearDown() {
        cmdContainer.reset();
        DeviceFixture::TearDown();
    }
    std::unique_ptr<MyMockCommandContainer> cmdContainer;
    KernelDescriptor descriptor;
};

} // namespace NEO

struct WalkerThreadFixture {
    void SetUp() {
        startWorkGroup[0] = startWorkGroup[1] = startWorkGroup[2] = 0u;
        numWorkGroups[0] = numWorkGroups[1] = numWorkGroups[2] = 1u;
        workGroupSizes[0] = 32u;
        workGroupSizes[1] = workGroupSizes[2] = 1u;
        simd = 32u;
        localIdDimensions = 3u;
        requiredWorkGroupOrder = 0u;
    }
    void TearDown() {}

    uint32_t startWorkGroup[3];
    uint32_t numWorkGroups[3];
    uint32_t workGroupSizes[3];
    uint32_t simd;
    uint32_t localIdDimensions;
    uint32_t requiredWorkGroupOrder;
};

using WalkerThreadTest = Test<WalkerThreadFixture>;
