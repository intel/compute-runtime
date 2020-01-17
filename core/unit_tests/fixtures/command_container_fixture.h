/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/command_container/command_encoder.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

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