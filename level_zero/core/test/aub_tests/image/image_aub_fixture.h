/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/image/image.h"
#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

#include "aubstream/hardware_context.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <tuple>
#include <vector>

namespace L0 {
namespace ult {

struct ImageAubChannelType {
    ze_image_format_type_t type;
    uint32_t bytesPerChannel;
};

struct ImageAubRegionParams {
    ze_image_type_t imageType;
    uint32_t origin[3];
};

inline constexpr ImageAubChannelType imageAubChannelTypes[] = {
    {ZE_IMAGE_FORMAT_TYPE_UNORM, 1u},
    {ZE_IMAGE_FORMAT_TYPE_SINT, 2u},
    {ZE_IMAGE_FORMAT_TYPE_UINT, 4u},
    {ZE_IMAGE_FORMAT_TYPE_FLOAT, 2u},
    {ZE_IMAGE_FORMAT_TYPE_FLOAT, 4u}};

inline constexpr ImageAubRegionParams imageAubRegion1D = {ZE_IMAGE_TYPE_1D, {0u, 0u, 0u}};
inline constexpr ImageAubRegionParams imageAubRegion1DWithOrigin = {ZE_IMAGE_TYPE_1D, {1u, 0u, 0u}};
inline constexpr ImageAubRegionParams imageAubRegion2D = {ZE_IMAGE_TYPE_2D, {0u, 0u, 0u}};
inline constexpr ImageAubRegionParams imageAubRegion2DWithOrigin = {ZE_IMAGE_TYPE_2D, {1u, 2u, 0u}};
inline constexpr ImageAubRegionParams imageAubRegion3D = {ZE_IMAGE_TYPE_3D, {0u, 0u, 0u}};
inline constexpr ImageAubRegionParams imageAubRegion3DWithOrigin = {ZE_IMAGE_TYPE_3D, {1u, 2u, 3u}};

struct ImageAubChannelCountRegions {
    uint32_t numChannels;
    ImageAubRegionParams regions[2];
};

inline constexpr ImageAubChannelCountRegions imageAubChannelCountRegions[] = {
    {1u, {imageAubRegion1D, imageAubRegion2DWithOrigin}},
    {2u, {imageAubRegion2D, imageAubRegion3DWithOrigin}},
    {4u, {imageAubRegion3D, imageAubRegion1DWithOrigin}}};

inline ze_image_format_layout_t getImageAubFormatLayout(uint32_t bytesPerChannel, uint32_t numChannels) {
    const ze_image_format_layout_t layouts[3][3] = {
        {ZE_IMAGE_FORMAT_LAYOUT_8, ZE_IMAGE_FORMAT_LAYOUT_8_8, ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8},
        {ZE_IMAGE_FORMAT_LAYOUT_16, ZE_IMAGE_FORMAT_LAYOUT_16_16, ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16},
        {ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_LAYOUT_32_32, ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32}};
    const uint32_t sizeIndex = bytesPerChannel == 1u ? 0u : (bytesPerChannel == 2u ? 1u : 2u);
    const uint32_t channelIndex = numChannels == 1u ? 0u : (numChannels == 2u ? 1u : 2u);
    return layouts[sizeIndex][channelIndex];
}

inline ze_image_format_t makeImageAubFormat(ze_image_format_type_t type, uint32_t bytesPerChannel, uint32_t numChannels) {
    ze_image_format_t format = {};
    format.layout = getImageAubFormatLayout(bytesPerChannel, numChannels);
    format.type = type;
    format.x = ZE_IMAGE_FORMAT_SWIZZLE_R;
    format.y = numChannels > 1u ? ZE_IMAGE_FORMAT_SWIZZLE_G : ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.z = numChannels > 2u ? ZE_IMAGE_FORMAT_SWIZZLE_B : ZE_IMAGE_FORMAT_SWIZZLE_0;
    format.w = numChannels > 2u ? ZE_IMAGE_FORMAT_SWIZZLE_A : ZE_IMAGE_FORMAT_SWIZZLE_1;
    return format;
}

inline bool isPixelInImageAubRegion(const ze_image_region_t &region, size_t x, size_t y, size_t z) {
    return x >= region.originX && x < region.originX + region.width &&
           y >= region.originY && y < region.originY + region.height &&
           z >= region.originZ && z < region.originZ + region.depth;
}

struct ImageAubFixtureL0 : public AUBFixtureL0 {
    void setUp(bool useCopyEngine, ze_image_type_t imageType) {
        if (!NEO::defaultHwInfo->capabilityTable.supportsImages) {
            GTEST_SKIP();
        }

        auto hwInfo = *NEO::defaultHwInfo;
        if (useCopyEngine) {
            NEO::MockExecutionEnvironment mockExecutionEnvironment{};
            const auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<NEO::ProductHelper>();
            if (!productHelper.isBlitterForImagesSupported()) {
                GTEST_SKIP();
            }
            if (imageType == ZE_IMAGE_TYPE_3D && !productHelper.isTile64With3DSurfaceOnBCSSupported(hwInfo)) {
                GTEST_SKIP();
            }
            hwInfo.capabilityTable.blitterOperationsSupported = true;
        }

        AUBFixtureL0::setUp(&hwInfo, false);

        if (!this->device->getDeviceInfo().imageSupport) {
            GTEST_SKIP();
        }

        const auto &engineGroups = this->neoDevice->getRegularEngineGroups();
        bool engineGroupFound = false;
        uint32_t ordinal = 0;
        NEO::EngineGroupType engineGroupType = NEO::EngineGroupType::compute;
        for (uint32_t groupIndex = 0; groupIndex < engineGroups.size(); groupIndex++) {
            const auto groupType = engineGroups[groupIndex].engineGroupType;
            const bool isMatchingGroup = useCopyEngine ? (groupType == NEO::EngineGroupType::copy)
                                                       : (groupType == NEO::EngineGroupType::compute || groupType == NEO::EngineGroupType::renderCompute);
            if (isMatchingGroup) {
                ordinal = groupIndex;
                engineGroupType = groupType;
                engineGroupFound = true;
                break;
            }
        }
        if (!engineGroupFound) {
            GTEST_SKIP();
        }

        ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
        queueDesc.ordinal = ordinal;
        queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
        ze_result_t returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
        this->immCmdList.reset(CommandList::createImmediate(this->device, &queueDesc, false, engineGroupType, returnValue));
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        ASSERT_NE(nullptr, this->immCmdList.get());
        EXPECT_EQ(useCopyEngine, this->immCmdList->isCopyOnly(false));
        this->immCmdListCsr = this->immCmdList->getCsr(false);
    }

    void tearDown() {
        this->immCmdList.reset();
        for (auto image : this->images) {
            Image::fromHandle(image)->destroy();
        }
        AUBFixtureL0::tearDown();
    }

    uint8_t *allocateHostBytes(size_t size) {
        return static_cast<uint8_t *>(this->allocateHostMemory(size, 0u));
    }

    ze_image_handle_t createImage(ze_image_type_t imageType, const ze_image_format_t &format, uint64_t width, uint32_t height, uint32_t depth, uint32_t arrayLevels) {
        ze_image_desc_t imageDesc = {ZE_STRUCTURE_TYPE_IMAGE_DESC};
        imageDesc.flags = ZE_IMAGE_FLAG_KERNEL_WRITE;
        imageDesc.type = imageType;
        imageDesc.format = format;
        imageDesc.width = width;
        imageDesc.height = height;
        imageDesc.depth = depth;
        imageDesc.arraylevels = arrayLevels;
        imageDesc.miplevels = 0;
        return this->createImage(imageDesc);
    }

    ze_image_handle_t createImage(const ze_image_desc_t &imageDesc) {
        ze_image_handle_t image = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeImageCreate(this->context->toHandle(), this->device->toHandle(), &imageDesc, &image));
        if (image != nullptr) {
            this->images.push_back(image);
        }
        return image;
    }

    void synchronize() {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListHostSynchronize(this->immCmdList->toHandle(), std::numeric_limits<uint64_t>::max()));
    }

    void expectImmCmdListMemory(void *gfxAddress, const void *srcAddress, size_t length) {
        EXPECT_TRUE(this->immCmdListCsr->expectMemory(gfxAddress, srcAddress, length, aub_stream::CompareOperationValues::CompareEqual));
    }

    void expectImmCmdListMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length) {
        EXPECT_TRUE(this->immCmdListCsr->expectMemory(gfxAddress, srcAddress, length, aub_stream::CompareOperationValues::CompareNotEqual));
    }

    ze_command_list_handle_t getCmdListHandle() {
        return this->immCmdList->toHandle();
    }

    NEO::DestroyableZeUniquePtr<L0::CommandList> immCmdList;
    std::vector<ze_image_handle_t> images;
    NEO::CommandStreamReceiver *immCmdListCsr = nullptr;
};

template <typename TestBase>
struct ImageAubTestL0 : public ImageAubFixtureL0, public TestBase {
    void TearDown() override {
        ImageAubFixtureL0::tearDown();
    }
};

struct ImageAubCopyEngineTestL0 : public ImageAubTestL0<::testing::TestWithParam<bool>> {
    void SetUp() override {
        ImageAubFixtureL0::setUp(GetParam(), ZE_IMAGE_TYPE_2D);
    }
};

using ImageAubUnalignedCopyParams = std::tuple<ImageAubChannelType, uint32_t, ImageAubRegionParams, bool>;

inline std::vector<ImageAubUnalignedCopyParams> getImageAubUnalignedCopyParams() {
    std::vector<ImageAubUnalignedCopyParams> params;
    for (const auto &channelType : imageAubChannelTypes) {
        for (const auto &channelCountRegions : imageAubChannelCountRegions) {
            for (const auto &region : channelCountRegions.regions) {
                for (const bool useCopyEngine : {false, true}) {
                    params.emplace_back(channelType, channelCountRegions.numChannels, region, useCopyEngine);
                }
            }
        }
    }
    return params;
}

struct ImageAubUnalignedCopyTestL0 : public ImageAubTestL0<::testing::TestWithParam<ImageAubUnalignedCopyParams>> {
    void SetUp() override {
        ImageAubFixtureL0::setUp(std::get<3>(GetParam()), std::get<2>(GetParam()).imageType);
    }

    void createImageWithRegion() {
        const auto channelType = std::get<0>(GetParam());
        const auto numChannels = std::get<1>(GetParam());
        const auto &regionParams = std::get<2>(GetParam());

        this->imageWidth = 5;
        this->imageHeight = regionParams.imageType != ZE_IMAGE_TYPE_1D ? 5 : 1;
        this->imageDepth = regionParams.imageType == ZE_IMAGE_TYPE_3D ? 5 : 1;
        this->elementSize = channelType.bytesPerChannel * numChannels;
        this->rowPitch = static_cast<uint32_t>(this->imageWidth * this->elementSize);
        this->slicePitch = this->rowPitch * this->imageHeight;
        this->imageDataSize = static_cast<size_t>(this->slicePitch) * this->imageDepth;

        this->image = this->createImage(regionParams.imageType, makeImageAubFormat(channelType.type, channelType.bytesPerChannel, numChannels),
                                        this->imageWidth, this->imageHeight, this->imageDepth, 0u);
        this->region = {regionParams.origin[0], regionParams.origin[1], regionParams.origin[2],
                        std::max(this->imageWidth / 2, 1u), std::max(this->imageHeight / 2, 1u), std::max(this->imageDepth / 2, 1u)};
    }

    static constexpr size_t unalignedOffset = 4;
    uint32_t imageWidth = 0;
    uint32_t imageHeight = 0;
    uint32_t imageDepth = 0;
    size_t elementSize = 0;
    uint32_t rowPitch = 0;
    uint32_t slicePitch = 0;
    size_t imageDataSize = 0;
    ze_image_handle_t image = nullptr;
    ze_image_region_t region = {};
};

} // namespace ult
} // namespace L0
