/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_context.h"
#include "gtest/gtest.h"

using namespace OCLRT;

class ImageHostPtrTransferTests : public testing::Test {

  public:
    void SetUp() override {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        context.reset(new MockContext(device.get()));
    }

    template <typename ImageTraits>
    void createImageAndSetTestParams() {
        image.reset(ImageHelper<ImageUseHostPtr<ImageTraits>>::create(context.get()));

        imgDesc = &image->getImageDesc();

        hostPtrSlicePitch = image->getHostPtrSlicePitch();
        hostPtrRowPitch = image->getHostPtrRowPitch();
        imageSlicePitch = image->getImageDesc().image_slice_pitch;
        imageRowPitch = image->getImageDesc().image_row_pitch;
        pixelSize = image->getSurfaceFormatInfo().ImageElementSizeInBytes;
    }

    void setExpectedData(uint8_t *dstPtr, size_t slicePitch, size_t rowPitch, std::array<size_t, 3> copyOrigin, std::array<size_t, 3> copyRegion) {
        if (image->getImageDesc().image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY) {
            // For 1DArray type, array region and origin are stored on 2nd position. For 2Darray its on 3rd position.
            std::swap(copyOrigin[1], copyOrigin[2]);
            std::swap(copyRegion[1], copyRegion[2]);
        }

        for (size_t slice = copyOrigin[2]; slice < (copyOrigin[2] + copyRegion[2]); slice++) {
            auto sliceOffset = ptrOffset(dstPtr, slicePitch * slice);
            for (size_t height = copyOrigin[1]; height < (copyOrigin[1] + copyRegion[1]); height++) {
                auto rowOffset = ptrOffset(sliceOffset, rowPitch * height);
                memset(ptrOffset(rowOffset, copyOrigin[0] * pixelSize), 123, copyRegion[0] * pixelSize);
            }
        }
    }

    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Image> image;

    const cl_image_desc *imgDesc = nullptr;
    size_t hostPtrSlicePitch, hostPtrRowPitch, imageSlicePitch, imageRowPitch, pixelSize;
};

TEST_F(ImageHostPtrTransferTests, given3dImageWithoutTilingWhenTransferToHostPtrCalledThenCopyRequestedRegionAndOriginOnly) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceLinearImages.set(true);

    createImageAndSetTestParams<Image3dDefaults>();
    EXPECT_NE(hostPtrSlicePitch, imageSlicePitch);
    EXPECT_NE(hostPtrRowPitch, imageRowPitch);
    EXPECT_NE(image->getCpuAddress(), image->getHostPtr());

    std::array<size_t, 3> copyOrigin = {{imgDesc->image_width / 2, imgDesc->image_height / 2, imgDesc->image_depth / 2}};
    std::array<size_t, 3> copyRegion = copyOrigin;

    std::unique_ptr<uint8_t> expectedHostPtr(new uint8_t[hostPtrSlicePitch * imgDesc->image_depth]);
    memset(image->getHostPtr(), 0, hostPtrSlicePitch * imgDesc->image_depth);
    memset(expectedHostPtr.get(), 0, hostPtrSlicePitch * imgDesc->image_depth);
    memset(image->getCpuAddress(), 123, imageSlicePitch * imgDesc->image_depth);

    setExpectedData(expectedHostPtr.get(), hostPtrSlicePitch, hostPtrRowPitch, copyOrigin, copyRegion);

    image->transferDataToHostPtr(copyRegion, copyOrigin);

    EXPECT_TRUE(memcmp(image->getHostPtr(), expectedHostPtr.get(), hostPtrSlicePitch * imgDesc->image_depth) == 0);
}

TEST_F(ImageHostPtrTransferTests, given3dImageWithoutTilingWhenTransferFromHostPtrCalledThenCopyRequestedRegionAndOriginOnly) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceLinearImages.set(true);

    createImageAndSetTestParams<Image3dDefaults>();
    EXPECT_NE(hostPtrSlicePitch, imageSlicePitch);
    EXPECT_NE(hostPtrRowPitch, imageRowPitch);
    EXPECT_NE(image->getCpuAddress(), image->getHostPtr());

    std::array<size_t, 3> copyOrigin = {{imgDesc->image_width / 2, imgDesc->image_height / 2, imgDesc->image_depth / 2}};
    std::array<size_t, 3> copyRegion = copyOrigin;

    std::unique_ptr<uint8_t> expectedImageData(new uint8_t[imageSlicePitch * imgDesc->image_depth]);
    memset(image->getHostPtr(), 123, hostPtrSlicePitch * imgDesc->image_depth);
    memset(expectedImageData.get(), 0, imageSlicePitch * imgDesc->image_depth);
    memset(image->getCpuAddress(), 0, imageSlicePitch * imgDesc->image_depth);

    setExpectedData(expectedImageData.get(), imageSlicePitch, imageRowPitch, copyOrigin, copyRegion);

    image->transferDataFromHostPtr(copyRegion, copyOrigin);

    EXPECT_TRUE(memcmp(image->getCpuAddress(), expectedImageData.get(), imageSlicePitch * imgDesc->image_depth) == 0);
}

TEST_F(ImageHostPtrTransferTests, given2dArrayImageWithoutTilingWhenTransferToHostPtrCalledThenCopyRequestedRegionAndOriginOnly) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceLinearImages.set(true);

    createImageAndSetTestParams<Image2dArrayDefaults>();
    EXPECT_NE(hostPtrSlicePitch, imageSlicePitch);
    EXPECT_NE(hostPtrRowPitch, imageRowPitch);
    EXPECT_NE(image->getCpuAddress(), image->getHostPtr());

    std::array<size_t, 3> copyOrigin = {{imgDesc->image_width / 2, imgDesc->image_height / 2, imgDesc->image_array_size / 2}};
    std::array<size_t, 3> copyRegion = copyOrigin;

    std::unique_ptr<uint8_t> expectedHostPtr(new uint8_t[hostPtrSlicePitch * imgDesc->image_array_size]);
    memset(image->getHostPtr(), 0, hostPtrSlicePitch * imgDesc->image_array_size);
    memset(expectedHostPtr.get(), 0, hostPtrSlicePitch * imgDesc->image_array_size);
    memset(image->getCpuAddress(), 123, imageSlicePitch * imgDesc->image_array_size);

    setExpectedData(expectedHostPtr.get(), hostPtrSlicePitch, hostPtrRowPitch, copyOrigin, copyRegion);

    image->transferDataToHostPtr(copyRegion, copyOrigin);

    EXPECT_TRUE(memcmp(image->getHostPtr(), expectedHostPtr.get(), hostPtrSlicePitch * imgDesc->image_array_size) == 0);
}

TEST_F(ImageHostPtrTransferTests, given2dArrayImageWithoutTilingWhenTransferFromHostPtrCalledThenCopyRequestedRegionAndOriginOnly) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceLinearImages.set(true);

    createImageAndSetTestParams<Image2dArrayDefaults>();
    EXPECT_NE(hostPtrSlicePitch, imageSlicePitch);
    EXPECT_NE(hostPtrRowPitch, imageRowPitch);
    EXPECT_NE(image->getCpuAddress(), image->getHostPtr());

    std::array<size_t, 3> copyOrigin = {{imgDesc->image_width / 2, imgDesc->image_height / 2, imgDesc->image_array_size / 2}};
    std::array<size_t, 3> copyRegion = copyOrigin;

    std::unique_ptr<uint8_t> expectedImageData(new uint8_t[imageSlicePitch * imgDesc->image_array_size]);
    memset(image->getHostPtr(), 123, hostPtrSlicePitch * imgDesc->image_array_size);
    memset(expectedImageData.get(), 0, imageSlicePitch * imgDesc->image_array_size);
    memset(image->getCpuAddress(), 0, imageSlicePitch * imgDesc->image_array_size);

    setExpectedData(expectedImageData.get(), imageSlicePitch, imageRowPitch, copyOrigin, copyRegion);

    image->transferDataFromHostPtr(copyRegion, copyOrigin);

    EXPECT_TRUE(memcmp(image->getCpuAddress(), expectedImageData.get(), imageSlicePitch * imgDesc->image_array_size) == 0);
}

TEST_F(ImageHostPtrTransferTests, given1dArrayImageWhenTransferToHostPtrCalledThenUseSecondCoordinateAsSlice) {
    createImageAndSetTestParams<Image1dArrayDefaults>();
    std::array<size_t, 3> copyOrigin = {{imgDesc->image_width / 2, imgDesc->image_array_size / 2, 0}};
    std::array<size_t, 3> copyRegion = {{imgDesc->image_width / 2, imgDesc->image_array_size / 2, 1}};

    std::unique_ptr<uint8_t> expectedHostPtr(new uint8_t[hostPtrSlicePitch * imgDesc->image_array_size]);
    memset(image->getHostPtr(), 0, hostPtrSlicePitch * imgDesc->image_array_size);
    memset(expectedHostPtr.get(), 0, hostPtrSlicePitch * imgDesc->image_array_size);
    memset(image->getCpuAddress(), 123, imageSlicePitch * imgDesc->image_array_size);

    setExpectedData(expectedHostPtr.get(), hostPtrSlicePitch, hostPtrRowPitch, copyOrigin, copyRegion);

    image->transferDataToHostPtr(copyRegion, copyOrigin);

    EXPECT_TRUE(memcmp(image->getHostPtr(), expectedHostPtr.get(), hostPtrSlicePitch * imgDesc->image_array_size) == 0);
}

TEST_F(ImageHostPtrTransferTests, given1dArrayImageWhenTransferFromHostPtrCalledThenUseSecondCoordinateAsSlice) {
    createImageAndSetTestParams<Image1dArrayDefaults>();
    std::array<size_t, 3> copyOrigin = {{imgDesc->image_width / 2, imgDesc->image_array_size / 2, 0}};
    std::array<size_t, 3> copyRegion = {{imgDesc->image_width / 2, imgDesc->image_array_size / 2, 1}};

    std::unique_ptr<uint8_t> expectedImageData(new uint8_t[imageSlicePitch * imgDesc->image_array_size]);
    memset(image->getHostPtr(), 123, hostPtrSlicePitch * imgDesc->image_array_size);
    memset(expectedImageData.get(), 0, imageSlicePitch * imgDesc->image_array_size);
    memset(image->getCpuAddress(), 0, imageSlicePitch * imgDesc->image_array_size);

    setExpectedData(expectedImageData.get(), imageSlicePitch, imageRowPitch, copyOrigin, copyRegion);

    image->transferDataFromHostPtr(copyRegion, copyOrigin);

    EXPECT_TRUE(memcmp(image->getCpuAddress(), expectedImageData.get(), imageSlicePitch * imgDesc->image_array_size) == 0);
}
