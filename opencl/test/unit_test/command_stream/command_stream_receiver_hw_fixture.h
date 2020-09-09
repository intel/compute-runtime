/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "test.h"
using namespace NEO;
struct BcsTests : public Test<ClDeviceFixture> {
    void SetUp() override {
        Test<ClDeviceFixture>::SetUp();

        auto &csr = pDevice->getGpgpuCommandStreamReceiver();
        auto engine = csr.getMemoryManager()->getRegisteredEngineForCsr(&csr);
        auto contextId = engine->osContext->getContextId();

        delete engine->osContext;
        engine->osContext = OsContext::create(nullptr, contextId, pDevice->getDeviceBitfield(), aub_stream::EngineType::ENGINE_BCS, PreemptionMode::Disabled,
                                              false, false, false);
        engine->osContext->incRefInternal();
        csr.setupContext(*engine->osContext);

        context = std::make_unique<MockContext>(pClDevice);
    }

    void TearDown() override {
        context.reset();
        Test<ClDeviceFixture>::TearDown();
    }

    uint32_t blitBuffer(CommandStreamReceiver *bcsCsr, const BlitProperties &blitProperties, bool blocking) {
        BlitPropertiesContainer container;
        container.push_back(blitProperties);

        return bcsCsr->blitBuffer(container, blocking, false);
    }

    TimestampPacketContainer timestampPacketContainer;
    CsrDependencies csrDependencies;
    std::unique_ptr<MockContext> context;
};