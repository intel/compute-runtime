/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "test.h"

namespace NEO {

class CommandEncodeStatesFixture : public ClDeviceFixture {
  public:
    class MyMockCommandContainer : public CommandContainer {
      public:
        using CommandContainer::dirtyHeaps;
    };

    void SetUp() {
        ClDeviceFixture::SetUp();
        cmdContainer = std::make_unique<MyMockCommandContainer>();
        cmdContainer->initialize(pDevice);
    }
    void TearDown() {
        cmdContainer.reset();
        ClDeviceFixture::TearDown();
    }
    std::unique_ptr<MyMockCommandContainer> cmdContainer;
};

} // namespace NEO
