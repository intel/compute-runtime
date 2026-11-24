/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/fixtures/front_window_fixture.h"

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

void MemManagerFixture::setUp() {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
    DeviceFixture::setUp();
    memManager = std::make_unique<MockMemoryManager>(*pDevice->getExecutionEnvironment());
}

void MemManagerFixture::tearDown() {
    DeviceFixture::tearDown();
}

BindlessCommandEncodeStatesFixture::~BindlessCommandEncodeStatesFixture() {
}

void BindlessCommandEncodeStatesFixture::setUp() {
    MemManagerFixture::setUp();

    auto &productHelper = pDevice->getProductHelper();
    this->l1CachePolicyData.init(productHelper);

    cmdContainer = std::make_unique<CommandContainer>();
    cmdContainer->l1CachePolicyDataRef() = &l1CachePolicyData;
}

void BindlessCommandEncodeStatesFixture::tearDown() {
    cmdContainer.reset(nullptr);
    MemManagerFixture::tearDown();
}
