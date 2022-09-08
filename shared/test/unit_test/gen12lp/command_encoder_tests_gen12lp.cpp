/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using Gen12LpCommandEncodeTest = testing::Test;
GEN12LPTEST_F(Gen12LpCommandEncodeTest, givenGen12LpPlatformWhenDoBindingTablePrefetchIsCalledThenReturnsTrue) {
    EXPECT_FALSE(EncodeSurfaceState<FamilyType>::doBindingTablePrefetch());
}

template <bool rcs>
class MyCommandStreamReceiverMock : public MockCommandStreamReceiver {
  public:
    MyCommandStreamReceiverMock(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield) : MockCommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {}
    bool isRcs() const override {
        return rcs;
    }
};

GEN12LPTEST_F(Gen12LpCommandEncodeTest, WhenDefaultEngineIsRcsAnd3DPipelineSelectWaRequiredThenAdditionalPipelineSelectSizeEqualsTwoPipelineSelectSize) {
    MockDevice device;
    auto csr = std::make_unique<MyCommandStreamReceiverMock<true>>(*device.getExecutionEnvironment(), 0, device.getDeviceBitfield());
    auto oldCsr = device.getDefaultEngine().commandStreamReceiver;
    device.getDefaultEngine().commandStreamReceiver = csr.get();

    auto &hwInfoConfig = *HwInfoConfig::get(device.getHardwareInfo().platform.eProductFamily);
    if (hwInfoConfig.is3DPipelineSelectWARequired()) {
        EXPECT_EQ(2 * PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(device.getHardwareInfo()), EncodeWA<FamilyType>::getAdditionalPipelineSelectSize(device, csr->isRcs()));
    } else {
        EXPECT_EQ(0u, EncodeWA<FamilyType>::getAdditionalPipelineSelectSize(device, csr->isRcs()));
    }

    device.getDefaultEngine().commandStreamReceiver = oldCsr;
}

GEN12LPTEST_F(Gen12LpCommandEncodeTest, givenGen12LpPlatformWhenDefaultEngineIsNotRcsThenAdditionalPipelineSelectSizeEqualZero) {
    MockDevice device;
    auto csr = std::make_unique<MyCommandStreamReceiverMock<false>>(*device.getExecutionEnvironment(), 0, device.getDeviceBitfield());
    auto oldCsr = device.getDefaultEngine().commandStreamReceiver;
    device.getDefaultEngine().commandStreamReceiver = csr.get();
    EXPECT_EQ(0u, EncodeWA<FamilyType>::getAdditionalPipelineSelectSize(device, csr->isRcs()));
    device.getDefaultEngine().commandStreamReceiver = oldCsr;
}
