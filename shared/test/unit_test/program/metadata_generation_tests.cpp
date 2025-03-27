/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/zebin_decoder.h"
#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/metadata_generation.h"
#include "shared/test/common/mocks/mock_elf.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MetadataGenerationTest, givenNonZebinaryFormatWhenCallingPopulateZebinExtendedArgsMetadataThenMetadataIsNotPopulated) {
    MetadataGeneration metadataGeneration;

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    std::vector<NEO::KernelInfo *> kernelInfos;
    kernelInfos.push_back(&kernelInfo);

    ASSERT_TRUE(kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.empty());
    metadataGeneration.callPopulateZebinExtendedArgsMetadataOnce(ArrayRef<const uint8_t>(), 0, kernelInfos);
    EXPECT_TRUE(kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.empty());
}

TEST(MetadataGenerationTest, givenZebinaryFormatWithInvalidZeInfoWhenCallingPopulateExtendedArgsMetadataThenReturnWithoutPopulatingMetadata) {
    MetadataGeneration metadataGeneration;
    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    std::vector<NEO::KernelInfo *> kernelInfos;
    kernelInfos.push_back(&kernelInfo);

    NEO::ConstStringRef zeInfo = R"===(
kernels:
  - name:            some_kernel
    simd_size:       32
kernels_misc_info:
  - name:            some_kernel
    args_info:
      - name:            a
        index:           0
        address_qualifier: __global
...
)===";
    constexpr auto numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64;
    MockElfEncoder<numBits> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_REL;
    elfEncoder.appendSection(Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));
    auto storage = elfEncoder.encode();

    ASSERT_TRUE(kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.empty());
    metadataGeneration.callPopulateZebinExtendedArgsMetadataOnce(storage, std::string::npos, kernelInfos);
    EXPECT_TRUE(kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.empty());
}

TEST(MetadataGenerationTest, givenZebinaryFormatWithValidZeInfoWhenCallingPopulateExtendedArgsMetadataThenMetadataIsPopulated) {
    MetadataGeneration metadataGeneration;
    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    std::vector<NEO::KernelInfo *> kernelInfos;
    kernelInfos.push_back(&kernelInfo);

    NEO::ConstStringRef zeInfo = R"===(
kernels:
  - name:            some_kernel
    simd_size:       32
kernels_misc_info:
  - name:            some_kernel
    args_info:
      - name:            a
        index:           0
        address_qualifier: __global
...
)===";
    constexpr auto numBits = is32bit ? Elf::EI_CLASS_32 : Elf::EI_CLASS_64;
    MockElfEncoder<numBits> elfEncoder;
    elfEncoder.getElfFileHeader().type = NEO::Elf::ET_REL;
    elfEncoder.appendSection(Zebin::Elf::SectionHeaderTypeZebin::SHT_ZEBIN_ZEINFO, Zebin::Elf::SectionNames::zeInfo, ArrayRef<const uint8_t>::fromAny(zeInfo.data(), zeInfo.size()));
    auto storage = elfEncoder.encode();
    auto kernelMiscInfoPos = zeInfo.str().find(Zebin::ZeInfo::Tags::kernelMiscInfo.str());
    ASSERT_NE(std::string::npos, kernelMiscInfoPos);

    ASSERT_TRUE(kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.empty());
    metadataGeneration.callPopulateZebinExtendedArgsMetadataOnce(storage, kernelMiscInfoPos, kernelInfos);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());
}

TEST(MetadataGenerationTest, givenNativeBinaryWhenCallingGenerateDefaultExtendedArgsMetadataThenGenerateMetadataForEachExplicitArgForEachKernel) {
    MetadataGeneration metadataGeneration;

    KernelInfo kernelInfo1, kernelInfo2;
    kernelInfo1.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    kernelInfo2.kernelDescriptor.kernelMetadata.kernelName = "another_kernel";
    std::vector<NEO::KernelInfo *> kernelInfos;
    kernelInfos.push_back(&kernelInfo1);
    kernelInfos.push_back(&kernelInfo2);

    kernelInfo1.kernelDescriptor.payloadMappings.explicitArgs.resize(2);
    kernelInfo1.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTPointer;
    auto &ptr = kernelInfo1.kernelDescriptor.payloadMappings.explicitArgs.at(0).as<ArgDescPointer>();
    ptr.pointerSize = 8u;

    kernelInfo1.kernelDescriptor.payloadMappings.explicitArgs.at(1).type = ArgDescriptor::argTImage;
    auto &img = kernelInfo1.kernelDescriptor.payloadMappings.explicitArgs.at(1).as<ArgDescImage>();
    img.imageType = NEOImageType::imageType2D;

    kernelInfo2.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelInfo2.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTSampler;

    metadataGeneration.callGenerateDefaultExtendedArgsMetadataOnce(kernelInfos);
    EXPECT_EQ(2u, kernelInfo1.kernelDescriptor.explicitArgsExtendedMetadata.size());
    EXPECT_EQ(1u, kernelInfo2.kernelDescriptor.explicitArgsExtendedMetadata.size());

    const auto &argMetadata1 = kernelInfo1.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ("arg0", argMetadata1.argName.c_str());
    auto expectedTypeName = std::string("__opaque_ptr;" + std::to_string(ptr.pointerSize));
    EXPECT_STREQ(expectedTypeName.c_str(), argMetadata1.type.c_str());

    const auto &argMetadata2 = kernelInfo1.kernelDescriptor.explicitArgsExtendedMetadata[1];
    EXPECT_STREQ("arg1", argMetadata2.argName.c_str());
    EXPECT_STREQ("image2d_t", argMetadata2.type.c_str());

    const auto &argMetadata3 = kernelInfo2.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ("arg0", argMetadata3.argName.c_str());
    EXPECT_STREQ("sampler_t", argMetadata3.type.c_str());
}

TEST(MetadataGenerationTest, whenGeneratingDefaultMetadataForArgByValueWithManyElementsThenGenerateProperMetadata) {
    MetadataGeneration metadataGeneration;

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    std::vector<NEO::KernelInfo *> kernelInfos;
    kernelInfos.push_back(&kernelInfo);

    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTValue;
    auto &argAsVal = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).as<ArgDescValue>();
    argAsVal.elements.resize(3u);

    argAsVal.elements[0].sourceOffset = 0u;
    argAsVal.elements[0].size = 8u;
    argAsVal.elements[1].sourceOffset = 16u;
    argAsVal.elements[1].size = 8u;
    argAsVal.elements[2].sourceOffset = 8u;
    argAsVal.elements[2].size = 8u;

    metadataGeneration.callGenerateDefaultExtendedArgsMetadataOnce(kernelInfos);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());

    const auto &argMetadata = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ("arg0", argMetadata.argName.c_str());

    auto expectedSize = argAsVal.elements[1].sourceOffset + argAsVal.elements[1].size;
    auto expectedTypeName = std::string("__opaque_var;" + std::to_string(expectedSize));
    EXPECT_STREQ(expectedTypeName.c_str(), argMetadata.type.c_str());

    const auto &argTypeTraits = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).getTraits();
    EXPECT_EQ(KernelArgMetadata::AddrPrivate, argTypeTraits.addressQualifier);
    EXPECT_EQ(KernelArgMetadata::AccessNone, argTypeTraits.accessQualifier);
    EXPECT_TRUE(argTypeTraits.typeQualifiers.empty());
}

TEST(MetadataGenerationTest, whenGeneratingDefaultMetadataForArgByValueWithSingleElementEachThenGenerateProperMetadata) {
    MetadataGeneration metadataGeneration;

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    std::vector<NEO::KernelInfo *> kernelInfos;
    kernelInfos.push_back(&kernelInfo);

    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTValue;
    auto &argAsVal = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).as<ArgDescValue>();
    argAsVal.elements.resize(1u);
    argAsVal.elements[0].size = 16u;

    metadataGeneration.callGenerateDefaultExtendedArgsMetadataOnce(kernelInfos);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());

    const auto &argMetadata = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ("arg0", argMetadata.argName.c_str());

    auto expectedTypeName = std::string("__opaque;" + std::to_string(argAsVal.elements[0].size));
    EXPECT_STREQ(expectedTypeName.c_str(), argMetadata.type.c_str());

    const auto &argTypeTraits = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).getTraits();
    EXPECT_EQ(KernelArgMetadata::AddrPrivate, argTypeTraits.addressQualifier);
    EXPECT_EQ(KernelArgMetadata::AccessNone, argTypeTraits.accessQualifier);
    EXPECT_TRUE(argTypeTraits.typeQualifiers.empty());
}

std::array<std::pair<NEOImageType, std::string>, 12> imgTypes{
    std::make_pair<>(NEOImageType::imageTypeBuffer, "image1d_buffer_t"),
    std::make_pair<>(NEOImageType::imageType1D, "image1d_t"),
    std::make_pair<>(NEOImageType::imageType1DArray, "image1d_array_t"),
    std::make_pair<>(NEOImageType::imageType2DArray, "image2d_array_t"),
    std::make_pair<>(NEOImageType::imageType3D, "image3d_t"),
    std::make_pair<>(NEOImageType::imageType2DDepth, "image2d_depth_t"),
    std::make_pair<>(NEOImageType::imageType2DArrayDepth, "image2d_array_depth_t"),
    std::make_pair<>(NEOImageType::imageType2DMSAA, "image2d_msaa_t"),
    std::make_pair<>(NEOImageType::imageType2DMSAADepth, "image2d_msaa_depth_t"),
    std::make_pair<>(NEOImageType::imageType2DArrayMSAA, "image2d_array_msaa_t"),
    std::make_pair<>(NEOImageType::imageType2DArrayMSAADepth, "image2d_array_msaa_depth_t"),
    std::make_pair<>(NEOImageType::imageType2D, "image2d_t")};

using MetadataGenerationDefaultArgsMetadataImagesTest = ::testing::TestWithParam<std::pair<NEOImageType, std::string>>;

TEST_P(MetadataGenerationDefaultArgsMetadataImagesTest, whenGeneratingDefaultMetadataForImageArgThenSetProperCorrespondingTypeName) {
    MetadataGeneration metadataGeneration;

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";

    std::vector<NEO::KernelInfo *> kernelInfos;
    kernelInfos.push_back(&kernelInfo);

    const auto &imgTypeTypenamePair = GetParam();

    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    auto &arg = kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[0];
    arg.type = ArgDescriptor::argTImage;
    arg.as<ArgDescImage>().imageType = imgTypeTypenamePair.first;

    metadataGeneration.callGenerateDefaultExtendedArgsMetadataOnce(kernelInfos);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());
    const auto &argMetadata = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ(argMetadata.type.c_str(), imgTypeTypenamePair.second.c_str());
}

INSTANTIATE_TEST_SUITE_P(
    MetadataGenerationDefaultArgsMetadataImagesTestValues,
    MetadataGenerationDefaultArgsMetadataImagesTest,
    ::testing::ValuesIn(imgTypes));

TEST(MetadataGenerationDefaultArgsMetadataImagesTest, whenGeneratingDefaultMetadataForSamplerArgThenSetProperTypeName) {
    MetadataGeneration metadataGeneration;

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    std::vector<NEO::KernelInfo *> kernelInfos;
    kernelInfos.push_back(&kernelInfo);

    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTSampler;

    metadataGeneration.callGenerateDefaultExtendedArgsMetadataOnce(kernelInfos);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());

    const auto &argMetadata = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_STREQ("sampler_t", argMetadata.type.c_str());
}

TEST(MetadataGenerationDefaultArgsMetadataImagesTest, whenGeneratingDefaultMetadataForUnknownArgThenDontGenerateMetadata) {
    MetadataGeneration metadataGeneration;

    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "some_kernel";
    std::vector<NEO::KernelInfo *> kernelInfos;
    kernelInfos.push_back(&kernelInfo);

    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.at(0).type = ArgDescriptor::argTUnknown;

    metadataGeneration.callGenerateDefaultExtendedArgsMetadataOnce(kernelInfos);
    EXPECT_EQ(1u, kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata.size());

    const auto &argMetadata = kernelInfo.kernelDescriptor.explicitArgsExtendedMetadata[0];
    EXPECT_TRUE(argMetadata.type.empty());
}