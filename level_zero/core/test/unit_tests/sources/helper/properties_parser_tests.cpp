/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/helpers/properties_parser.h"

namespace L0 {
namespace ult {
struct ImageStaticFunctionConvertTypeTest : public testing::TestWithParam<std::pair<ze_image_type_t, NEO::ImageType>> {
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_P(ImageStaticFunctionConvertTypeTest, givenZeImageFormatTypeWhenConvertTypeThenCorrectImageTypeReturned) {
    auto params = GetParam();
    EXPECT_EQ(convertType(params.first), params.second);
}

std::pair<ze_image_type_t, NEO::ImageType> validTypes[] = {
    {ZE_IMAGE_TYPE_2D, NEO::ImageType::image2D},
    {ZE_IMAGE_TYPE_3D, NEO::ImageType::image3D},
    {ZE_IMAGE_TYPE_2DARRAY, NEO::ImageType::image2DArray},
    {ZE_IMAGE_TYPE_1D, NEO::ImageType::image1D},
    {ZE_IMAGE_TYPE_1DARRAY, NEO::ImageType::image1DArray},
    {ZE_IMAGE_TYPE_BUFFER, NEO::ImageType::image1DBuffer}};

INSTANTIATE_TEST_SUITE_P(
    imageTypeFlags,
    ImageStaticFunctionConvertTypeTest,
    testing::ValuesIn(validTypes));

TEST(ImageStaticFunctionConvertInvalidType, givenInvalidZeImageFormatTypeWhenConvertTypeThenInvalidFormatIsRetrurned) {
    EXPECT_EQ(convertType(ZE_IMAGE_TYPE_FORCE_UINT32), NEO::ImageType::invalid);
}

TEST(ConvertDescriptorTest, givenZeImageDescWhenConvertDescriptorThenCorrectImageDescriptorReturned) {
    ze_image_desc_t zeDesc = {};
    zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    zeDesc.arraylevels = 1u;
    zeDesc.depth = 1u;
    zeDesc.height = 1u;
    zeDesc.width = 1u;
    zeDesc.miplevels = 1u;
    zeDesc.type = ZE_IMAGE_TYPE_2DARRAY;

    NEO::ImageDescriptor desc = convertDescriptor(zeDesc);
    EXPECT_EQ(desc.fromParent, false);
    EXPECT_EQ(desc.imageArraySize, zeDesc.arraylevels);
    EXPECT_EQ(desc.imageDepth, zeDesc.depth);
    EXPECT_EQ(desc.imageHeight, zeDesc.height);
    EXPECT_EQ(desc.imageRowPitch, 0u);
    EXPECT_EQ(desc.imageSlicePitch, 0u);
    EXPECT_EQ(desc.imageType, NEO::ImageType::image2DArray);
    EXPECT_EQ(desc.imageWidth, zeDesc.width);
    EXPECT_EQ(desc.numMipLevels, zeDesc.miplevels);
    EXPECT_EQ(desc.numSamples, 0u);
}

TEST(L0StructuresLookupTableTests, givenL0StructuresWithOpaqueFDWhenPrepareLookupTableThenProperFieldsInLookupTableAreSet) {
    ze_image_desc_t imageDesc = {};
    imageDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    imageDesc.height = 10;
    imageDesc.width = 10;
    imageDesc.depth = 10;
    ze_external_memory_import_fd_t fdStructure = {};
    fdStructure.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    fdStructure.fd = 1;
    fdStructure.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_FD;
    ze_image_view_planar_exp_desc_t imageView = {};
    imageView.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC;
    imageView.planeIndex = 1u;

    imageDesc.pNext = &fdStructure;
    fdStructure.pNext = &imageView;
    imageView.pNext = nullptr;

    StructuresLookupTable l0LookupTable = {};
    auto result = prepareL0StructuresLookupTable(l0LookupTable, &imageDesc);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_TRUE(l0LookupTable.isSharedHandle);

    EXPECT_TRUE(l0LookupTable.sharedHandleType.isSupportedHandle);
    EXPECT_TRUE(l0LookupTable.sharedHandleType.isOpaqueFDHandle);
    EXPECT_EQ(l0LookupTable.sharedHandleType.fd, fdStructure.fd);

    EXPECT_TRUE(l0LookupTable.areImageProperties);

    EXPECT_EQ(l0LookupTable.imageProperties.planeIndex, imageView.planeIndex);
    EXPECT_EQ(l0LookupTable.imageProperties.imageDescriptor.imageWidth, imageDesc.width);
    EXPECT_EQ(l0LookupTable.imageProperties.imageDescriptor.imageHeight, imageDesc.height);
    EXPECT_EQ(l0LookupTable.imageProperties.imageDescriptor.imageDepth, imageDesc.depth);
}

TEST(L0StructuresLookupTableTests, givenL0StructuresWithDmaBufWhenPrepareLookupTableThenProperFieldsInLookupTableAreSet) {
    ze_image_desc_t imageDesc = {};
    imageDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    imageDesc.height = 10;
    imageDesc.width = 10;
    imageDesc.depth = 10;
    ze_external_memory_import_fd_t fdStructure = {};
    fdStructure.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    fdStructure.fd = 1;
    fdStructure.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;
    ze_image_view_planar_exp_desc_t imageView = {};
    imageView.stype = ZE_STRUCTURE_TYPE_IMAGE_VIEW_PLANAR_EXP_DESC;
    imageView.planeIndex = 1u;

    imageDesc.pNext = &fdStructure;
    fdStructure.pNext = &imageView;
    imageView.pNext = nullptr;

    StructuresLookupTable l0LookupTable = {};
    auto result = prepareL0StructuresLookupTable(l0LookupTable, &imageDesc);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_TRUE(l0LookupTable.isSharedHandle);

    EXPECT_TRUE(l0LookupTable.sharedHandleType.isSupportedHandle);
    EXPECT_TRUE(l0LookupTable.sharedHandleType.isDMABUFHandle);
    EXPECT_EQ(l0LookupTable.sharedHandleType.fd, fdStructure.fd);

    EXPECT_TRUE(l0LookupTable.areImageProperties);

    EXPECT_EQ(l0LookupTable.imageProperties.planeIndex, imageView.planeIndex);
    EXPECT_EQ(l0LookupTable.imageProperties.imageDescriptor.imageWidth, imageDesc.width);
    EXPECT_EQ(l0LookupTable.imageProperties.imageDescriptor.imageHeight, imageDesc.height);
    EXPECT_EQ(l0LookupTable.imageProperties.imageDescriptor.imageDepth, imageDesc.depth);
}

TEST(L0StructuresLookupTableTests, givenL0StructuresWithUnsupportedImportHandlesWhenPrepareLookupTableThenUnsuppoertedErrorIsReturned) {
    ze_external_memory_import_win32_handle_t exportStruct = {};
    exportStruct.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_FD;
    exportStruct.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;

    StructuresLookupTable l0LookupTable = {};
    auto result = prepareL0StructuresLookupTable(l0LookupTable, &exportStruct);
    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION);
}

TEST(L0StructuresLookupTableTests, givenL0StructuresWithNTHandleWhenPrepareLookupTableThenProperFieldsInLookupTableAreSet) {
    uint64_t handle = 0x02;
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    importNTHandle.handle = &handle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;

    StructuresLookupTable l0LookupTable = {};
    auto result = prepareL0StructuresLookupTable(l0LookupTable, &importNTHandle);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_TRUE(l0LookupTable.isSharedHandle);
    EXPECT_TRUE(l0LookupTable.sharedHandleType.isSupportedHandle);
    EXPECT_TRUE(l0LookupTable.sharedHandleType.isNTHandle);
    EXPECT_EQ(l0LookupTable.sharedHandleType.ntHandle, importNTHandle.handle);
}

TEST(L0StructuresLookupTableTests, givenL0StructuresWithSupportedExportHandlesWhenPrepareLookupTableThenProperFieldsInLookupTableAreSet) {
    ze_external_memory_import_win32_handle_t exportStruct = {};
    exportStruct.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportStruct.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;

    StructuresLookupTable l0LookupTable = {};
    auto result = prepareL0StructuresLookupTable(l0LookupTable, &exportStruct);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_TRUE(l0LookupTable.exportMemory);
    exportStruct.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_DMA_BUF;

    l0LookupTable = {};
    result = prepareL0StructuresLookupTable(l0LookupTable, &exportStruct);

    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_TRUE(l0LookupTable.exportMemory);
}

TEST(L0StructuresLookupTableTests, givenL0StructuresWithSupportedCompressionHintsWhenPrepareLookupTableThenProperFieldsInLookupTableAreSet) {
    {
        ze_external_memory_import_win32_handle_t exportStruct = {};
        exportStruct.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        exportStruct.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED;

        StructuresLookupTable l0LookupTable = {};
        auto result = prepareL0StructuresLookupTable(l0LookupTable, &exportStruct);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_TRUE(l0LookupTable.compressedHint);
        EXPECT_FALSE(l0LookupTable.uncompressedHint);
    }

    {
        ze_external_memory_import_win32_handle_t exportStruct = {};
        exportStruct.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        exportStruct.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_UNCOMPRESSED;

        StructuresLookupTable l0LookupTable = {};
        auto result = prepareL0StructuresLookupTable(l0LookupTable, &exportStruct);

        EXPECT_EQ(result, ZE_RESULT_SUCCESS);
        EXPECT_FALSE(l0LookupTable.compressedHint);
        EXPECT_TRUE(l0LookupTable.uncompressedHint);
    }

    {
        ze_external_memory_import_win32_handle_t exportStruct = {};
        exportStruct.stype = ZE_STRUCTURE_TYPE_MEMORY_COMPRESSION_HINTS_EXT_DESC;
        exportStruct.flags = ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_COMPRESSED | ZE_MEMORY_COMPRESSION_HINTS_EXT_FLAG_UNCOMPRESSED;

        StructuresLookupTable l0LookupTable = {};
        auto result = prepareL0StructuresLookupTable(l0LookupTable, &exportStruct);

        EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION);
    }
}

TEST(L0StructuresLookupTableTests, givenL0StructuresWithUnsupportedExportHandlesWhenPrepareLookupTableThenUnsuppoertedErrorIsReturned) {
    ze_external_memory_import_win32_handle_t exportStruct = {};
    exportStruct.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportStruct.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_D3D11_TEXTURE;

    StructuresLookupTable l0LookupTable = {};
    auto result = prepareL0StructuresLookupTable(l0LookupTable, &exportStruct);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION);

    EXPECT_FALSE(l0LookupTable.exportMemory);
    exportStruct.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32_KMT;

    l0LookupTable = {};
    result = prepareL0StructuresLookupTable(l0LookupTable, &exportStruct);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION);

    EXPECT_FALSE(l0LookupTable.exportMemory);
}

TEST(L0StructuresLookupTableTests, givenL0StructuresWithSupportedExportHandlesAndImageDescWhenPrepareLookupTableThenUnsupportedErrorIsReturned) {
    ze_image_desc_t imageDesc = {};
    imageDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;

    ze_external_memory_import_win32_handle_t exportStruct = {};
    exportStruct.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_EXPORT_DESC;
    exportStruct.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32;

    exportStruct.pNext = &imageDesc;

    StructuresLookupTable l0LookupTable = {};
    auto result = prepareL0StructuresLookupTable(l0LookupTable, &exportStruct);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION);

    EXPECT_TRUE(l0LookupTable.exportMemory);
    EXPECT_TRUE(l0LookupTable.areImageProperties);
}

TEST(L0StructuresLookupTableTests, givenL0StructuresWithUnsupportedOptionsWhenPrepareLookupTableThenProperFieldsInLookupTableAreSet) {
    uint64_t handle = 0x02;
    ze_external_memory_import_win32_handle_t importNTHandle = {};
    importNTHandle.stype = ZE_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMPORT_WIN32;
    importNTHandle.handle = &handle;
    importNTHandle.flags = ZE_EXTERNAL_MEMORY_TYPE_FLAG_OPAQUE_WIN32_KMT;

    StructuresLookupTable l0LookupTable = {};
    auto result = prepareL0StructuresLookupTable(l0LookupTable, &importNTHandle);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION);

    EXPECT_TRUE(l0LookupTable.isSharedHandle);
    EXPECT_FALSE(l0LookupTable.sharedHandleType.isSupportedHandle);

    l0LookupTable = {};
    result = prepareL0StructuresLookupTable(l0LookupTable, &importNTHandle);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION);

    EXPECT_TRUE(l0LookupTable.isSharedHandle);
    EXPECT_FALSE(l0LookupTable.sharedHandleType.isSupportedHandle);

    l0LookupTable = {};

    result = prepareL0StructuresLookupTable(l0LookupTable, &importNTHandle);

    EXPECT_EQ(result, ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION);
}
} // namespace ult
} // namespace L0
