/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/image/image_format_desc_helper.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"

namespace NEO {
}

namespace L0 {
namespace ult {

using SetKernelArg = Test<ModuleFixture>;
template <GFXCORE_FAMILY gfxCoreFamily>
struct MyMockImageMediaBlock : public WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>> {
    using WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>::imgInfo;
    void copySurfaceStateToSSH(void *surfaceStateHeap, const uint32_t surfaceStateOffset, bool isMediaBlockArg) override {
        isMediaBlockPassedValue = isMediaBlockArg;
    }
    bool isMediaBlockPassedValue = false;
};

HWTEST2_F(SetKernelArg, givenSupportsMediaBlockAndIsMediaBlockImageHaveUnsuportedMediaBlockFormatThenSetArgReturnErrCode, IsGen12LP) {
    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    createKernel();
    auto argIndex = 3u;
    auto &arg = const_cast<NEO::ArgDescriptor &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex]);
    auto imageHW = std::make_unique<MyMockImageMediaBlock<gfxCoreFamily>>();
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    auto handle = imageHW->toHandle();

    hwInfo->capabilityTable.supportsMediaBlock = true;
    arg.getExtendedTypeInfo().isMediaBlockImage = true;
    SurfaceFormatInfo surfInfo = {GMM_FORMAT_R32G32B32A32_FLOAT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_FLOAT, 0, 4, 4, 16};
    imageHW.get()->imgInfo.surfaceFormat = &surfInfo;
    EXPECT_EQ(kernel->setArgImage(argIndex, sizeof(imageHW.get()), &handle), ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT);
}

HWTEST2_F(SetKernelArg, givenSupportsMediaBlockAndIsMediaBlockImagehaveSuportedMediaBlockFormatThenSetArgReturnSuccess, IsGen12LP) {
    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    createKernel();
    auto argIndex = 3u;
    auto &arg = const_cast<NEO::ArgDescriptor &>(kernel->kernelImmData->getDescriptor().payloadMappings.explicitArgs[argIndex]);
    auto imageHW = std::make_unique<MyMockImageMediaBlock<gfxCoreFamily>>();
    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    auto ret = imageHW->initialize(device, &desc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, ret);
    auto handle = imageHW->toHandle();

    hwInfo->capabilityTable.supportsMediaBlock = true;
    arg.getExtendedTypeInfo().isMediaBlockImage = true;
    SurfaceFormatInfo surfInfo = {GMM_FORMAT_Y216, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8};
    imageHW.get()->imgInfo.surfaceFormat = &surfInfo;
    EXPECT_EQ(kernel->setArgImage(argIndex, sizeof(imageHW.get()), &handle), ZE_RESULT_SUCCESS);
}
} // namespace ult
} // namespace L0