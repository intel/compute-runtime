/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/image_helper.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"

using namespace NEO;

namespace L0 {
namespace ult {

struct ImageHelperFixture : Test<CommandListFixture> {
    CmdListMemoryCopyParams copyParams = {};
};

HWTEST_F(ImageHelperFixture, whenImagesCheckedForPackageFormatThenFalseIsReturned) {
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    zeDesc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    zeDesc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    zeDesc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    zeDesc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    zeDesc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    zeDesc.width = 4;
    zeDesc.height = 4;
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);
    EXPECT_FALSE(ImageHelper::areImagesCompatibleWithPackedFormat(device->getProductHelper(), imageHWSrc->getImageInfo(), imageHWDst->getImageInfo(), imageHWSrc->getAllocation(), imageHWDst->getAllocation(), 5));
}

HWTEST_F(ImageHelperFixture, givenPackedSurfaceStateWhenCopyingImageThenSurfaceStateIsNotModified) {
    auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
    auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
    mockBuiltinKernel->setArgRedescribedImageCallBase = false;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);

    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<FamilyType::gfxCoreFamily>>>();

    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.type = ZE_IMAGE_TYPE_2D;
    zeDesc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    zeDesc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    zeDesc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    zeDesc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_G;
    zeDesc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_B;
    zeDesc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_A;
    zeDesc.width = 4;
    zeDesc.height = 4;

    imageHWSrc->initialize(device, &zeDesc);
    imageHWDst->initialize(device, &zeDesc);

    EXPECT_EQ(imageHWSrc->packedSurfaceState.getSurfaceFormat(), 0u);
    EXPECT_EQ(imageHWSrc->packedSurfaceState.getWidth(), 1u);
    EXPECT_EQ(imageHWSrc->packedSurfaceState.getHeight(), 1u);

    EXPECT_EQ(imageHWDst->packedSurfaceState.getSurfaceFormat(), 0u);
    EXPECT_EQ(imageHWDst->packedSurfaceState.getWidth(), 1u);
    EXPECT_EQ(imageHWDst->packedSurfaceState.getHeight(), 1u);
}

} // namespace ult
} // namespace L0