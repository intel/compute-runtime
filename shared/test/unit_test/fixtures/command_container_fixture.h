/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

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
    }
    void TearDown() {
        cmdContainer.reset();
        DeviceFixture::TearDown();
    }
    std::unique_ptr<MyMockCommandContainer> cmdContainer;
};