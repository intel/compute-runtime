/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_sip.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

#include "common/StateSaveAreaHeader.h"

namespace L0 {
namespace ult {

struct DebugApiFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
        mockBuiltins = new MockBuiltins();
        mockBuiltins->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
        MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltins);
    }

    void setUpV3Header() {
        mockBuiltins->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(3);
    }
    void setUpV3HeaderWithoutHeapless() {
        mockBuiltins->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(3);
        auto pStateSaveArea = reinterpret_cast<NEO::StateSaveAreaHeader *>(mockBuiltins->stateSaveAreaHeader.data());
        pStateSaveArea->regHeaderV3.sip_flags = 0;
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    MockBuiltins *mockBuiltins = nullptr;
};

size_t threadSlotOffset(SIP::StateSaveAreaHeader *pStateSaveAreaHeader, int slice, int subslice, int eu, int thread);

size_t regOffsetInThreadSlot(const SIP::regset_desc *regdesc, uint32_t start);

void initStateSaveArea(std::vector<char> &stateSaveArea, SIP::version version, L0::Device *device);

template <typename GfxFamily>
struct MockL0GfxCoreHelperSupportsThreadControlStopped : L0::L0GfxCoreHelperHw<GfxFamily> {
    bool isThreadControlStoppedSupported() const override { return true; }
};

} // namespace ult
} // namespace L0
