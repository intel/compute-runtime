/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_arg_descriptor.h"

#include "test.h"

#include <gtest/gtest.h>

#include <limits>

TEST(Undefined, GivenAnyTypeThenReturnsMaxValue) {
    EXPECT_EQ(std::numeric_limits<uint8_t>::max(), NEO::undefined<uint8_t>);
    EXPECT_EQ(std::numeric_limits<uint16_t>::max(), NEO::undefined<uint16_t>);
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), NEO::undefined<uint32_t>);
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), NEO::undefined<uint64_t>);
}

TEST(IsUndefinedOffset, GivenAnyTypeThenComparesAgainstUndefined) {
    EXPECT_TRUE(NEO::isUndefinedOffset(NEO::undefined<uint8_t>));
    EXPECT_TRUE(NEO::isUndefinedOffset(NEO::undefined<uint16_t>));
    EXPECT_TRUE(NEO::isUndefinedOffset(NEO::undefined<uint32_t>));
    EXPECT_TRUE(NEO::isUndefinedOffset(NEO::undefined<uint64_t>));

    EXPECT_FALSE(NEO::isUndefinedOffset<uint64_t>(NEO::undefined<uint8_t>));
    EXPECT_FALSE(NEO::isUndefinedOffset<uint64_t>(NEO::undefined<uint16_t>));
    EXPECT_FALSE(NEO::isUndefinedOffset<uint64_t>(NEO::undefined<uint32_t>));

    EXPECT_FALSE(NEO::isUndefinedOffset<uint8_t>(0));
    EXPECT_FALSE(NEO::isUndefinedOffset<uint8_t>(0));
    EXPECT_FALSE(NEO::isUndefinedOffset<uint8_t>(0));
    EXPECT_FALSE(NEO::isUndefinedOffset<uint8_t>(0));
}

TEST(IsValidOffset, GivenAnyTypeThenComparesAgainstUndefined) {
    EXPECT_FALSE(NEO::isValidOffset(NEO::undefined<uint8_t>));
    EXPECT_FALSE(NEO::isValidOffset(NEO::undefined<uint16_t>));
    EXPECT_FALSE(NEO::isValidOffset(NEO::undefined<uint32_t>));
    EXPECT_FALSE(NEO::isValidOffset(NEO::undefined<uint64_t>));

    EXPECT_TRUE(NEO::isValidOffset<uint64_t>(NEO::undefined<uint8_t>));
    EXPECT_TRUE(NEO::isValidOffset<uint64_t>(NEO::undefined<uint16_t>));
    EXPECT_TRUE(NEO::isValidOffset<uint64_t>(NEO::undefined<uint32_t>));

    EXPECT_TRUE(NEO::isValidOffset<uint8_t>(0));
    EXPECT_TRUE(NEO::isValidOffset<uint8_t>(0));
    EXPECT_TRUE(NEO::isValidOffset<uint8_t>(0));
    EXPECT_TRUE(NEO::isValidOffset<uint8_t>(0));
}

TEST(ArgDescPointer, WhenDefaultInitializedThenOffsetsAreUndefined) {
    NEO::ArgDescPointer argPtr;
    EXPECT_TRUE(NEO::isUndefinedOffset(argPtr.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(argPtr.stateless));
    EXPECT_TRUE(NEO::isUndefinedOffset(argPtr.bindless));
    EXPECT_TRUE(NEO::isUndefinedOffset(argPtr.bufferOffset));
    EXPECT_TRUE(NEO::isUndefinedOffset(argPtr.slmOffset));

    EXPECT_EQ(0U, argPtr.requiredSlmAlignment);
    EXPECT_EQ(0U, argPtr.pointerSize);
    EXPECT_TRUE(argPtr.accessedUsingStatelessAddressingMode);
}

TEST(ArgDescPointerIsPureStateful, WhenQueriedThenReturnsTrueIfPointerIsNotAccessedInStatelessManner) {
    NEO::ArgDescPointer argPtr;
    argPtr.accessedUsingStatelessAddressingMode = true;
    EXPECT_FALSE(argPtr.isPureStateful());

    argPtr.accessedUsingStatelessAddressingMode = false;
    EXPECT_TRUE(argPtr.isPureStateful());
}

TEST(ArgDescImage, WhenDefaultInitializedThenOffsetsAreUndefined) {
    NEO::ArgDescImage argImage;
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.bindless));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.imgWidth));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.imgHeight));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.imgDepth));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.channelDataType));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.channelOrder));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.arraySize));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.numSamples));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.numMipLevels));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.flatBaseOffset));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.flatWidth));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.flatHeight));
    EXPECT_TRUE(NEO::isUndefinedOffset(argImage.metadataPayload.flatPitch));
}

TEST(ArgDescSampler, WhenDefaultInitializedThenOffsetsAreUndefined) {
    NEO::ArgDescSampler argSampler;
    EXPECT_EQ(0U, argSampler.samplerType);
    EXPECT_TRUE(NEO::isUndefinedOffset(argSampler.bindful));
    EXPECT_TRUE(NEO::isUndefinedOffset(argSampler.bindless));
    EXPECT_TRUE(NEO::isUndefinedOffset(argSampler.metadataPayload.samplerSnapWa));
    EXPECT_TRUE(NEO::isUndefinedOffset(argSampler.metadataPayload.samplerAddressingMode));
    EXPECT_TRUE(NEO::isUndefinedOffset(argSampler.metadataPayload.samplerNormalizedCoords));
}

TEST(ArgDescValue, WhenDefaultInitializedThenOffsetsAreUndefined) {
    NEO::ArgDescValue argValue;
    EXPECT_TRUE(argValue.elements.empty());

    NEO::ArgDescValue::Element argValueElement;
    EXPECT_TRUE(NEO::isUndefinedOffset(argValueElement.offset));
    EXPECT_EQ(0U, argValueElement.size);
    EXPECT_EQ(0U, argValueElement.sourceOffset);
}

TEST(ArgDescriptor, WhenDefaultInitializedThenTypeIsUnknown) {
    NEO::ArgDescriptor arg;
    EXPECT_EQ(NEO::ArgDescriptor::ArgTUnknown, arg.type);
}

TEST(ArgDescriptorExtendedTypeInfo, WhenDefaultInitializedThenFlagsAreCleared) {
    NEO::ArgDescriptor::ExtendedTypeInfo argExtendedTypeInfo;
    EXPECT_EQ(0U, argExtendedTypeInfo.packed);

    NEO::ArgDescriptor arg;
    EXPECT_EQ(0U, arg.getExtendedTypeInfo().packed);
    EXPECT_EQ(&arg.getExtendedTypeInfo(), &const_cast<const NEO::ArgDescriptor &>(arg).getExtendedTypeInfo());
}

TEST(ArgDescriptorGetTraits, WhenDefaultInitializedThenTraitsAreCleared) {
    NEO::ArgTypeTraits expected;
    NEO::ArgDescriptor arg;
    NEO::ArgTypeTraits &got = arg.getTraits();
    EXPECT_EQ(expected.accessQualifier, got.accessQualifier);
    EXPECT_EQ(expected.addressQualifier, got.addressQualifier);
    EXPECT_EQ(expected.argByValSize, got.argByValSize);
    EXPECT_EQ(expected.typeQualifiers.packed, got.typeQualifiers.packed);
    EXPECT_EQ(&arg.getTraits(), &const_cast<const NEO::ArgDescriptor &>(arg).getTraits());
}

TEST(ArgDescriptorIsReadOnly, GivenImageArgWhenAccessQualifierIsReadOnlyThenReturnsTrue) {
    NEO::ArgDescriptor arg;
    arg.as<NEO::ArgDescImage>(true);
    arg.getTraits().accessQualifier = NEO::KernelArgMetadata::AccessReadOnly;
    EXPECT_TRUE(arg.isReadOnly());

    arg.getTraits().accessQualifier = NEO::KernelArgMetadata::AccessNone;
    EXPECT_FALSE(arg.isReadOnly());
    arg.getTraits().accessQualifier = NEO::KernelArgMetadata::AccessReadWrite;
    EXPECT_FALSE(arg.isReadOnly());
    arg.getTraits().accessQualifier = NEO::KernelArgMetadata::AccessWriteOnly;
    EXPECT_FALSE(arg.isReadOnly());
    arg.getTraits().accessQualifier = NEO::KernelArgMetadata::AccessUnknown;
    EXPECT_FALSE(arg.isReadOnly());
}

TEST(ArgDescriptorIsReadOnly, GivenPointerArgWhenConstQualifiedThenReturnsTrue) {
    NEO::ArgDescriptor arg;
    arg.as<NEO::ArgDescPointer>(true);
    arg.getTraits().typeQualifiers.constQual = true;
    EXPECT_TRUE(arg.isReadOnly());

    arg.getTraits().typeQualifiers.constQual = false;
    EXPECT_FALSE(arg.isReadOnly());
}

TEST(ArgDescriptorIsReadOnly, GivenPointerArgWhenConstantAddressSpaceThenReturnsTrue) {
    NEO::ArgDescriptor arg;
    arg.as<NEO::ArgDescPointer>(true);
    arg.getTraits().addressQualifier = NEO::KernelArgMetadata::AddrConstant;
    EXPECT_TRUE(arg.isReadOnly());

    arg.getTraits().addressQualifier = NEO::KernelArgMetadata::AddrGlobal;
    EXPECT_FALSE(arg.isReadOnly());

    arg.getTraits().addressQualifier = NEO::KernelArgMetadata::AddrLocal;
    EXPECT_FALSE(arg.isReadOnly());

    arg.getTraits().addressQualifier = NEO::KernelArgMetadata::AddrPrivate;
    EXPECT_FALSE(arg.isReadOnly());

    arg.getTraits().addressQualifier = NEO::KernelArgMetadata::AddrUnknown;
    EXPECT_FALSE(arg.isReadOnly());
}

TEST(ArgDescriptorIsReadOnly, GivenSamplerArgThenReturnsTrue) {
    NEO::ArgDescriptor arg;
    arg.as<NEO::ArgDescSampler>(true);
    EXPECT_TRUE(arg.isReadOnly());
}

TEST(ArgDescriptorIsReadOnly, GivenValueArgThenReturnsTrue) {
    NEO::ArgDescriptor arg;
    arg.as<NEO::ArgDescValue>(true);
    EXPECT_TRUE(arg.isReadOnly());
}

TEST(ArgDescriptorIs, WhenQueriedThenComparesAgainstStoredArgType) {
    NEO::ArgDescriptor args[] = {{NEO::ArgDescriptor::ArgTPointer},
                                 {NEO::ArgDescriptor::ArgTImage},
                                 {NEO::ArgDescriptor::ArgTSampler},
                                 {NEO::ArgDescriptor::ArgTValue}};
    for (const auto &arg : args) {
        EXPECT_EQ(arg.type == NEO::ArgDescriptor::ArgTPointer, arg.is<NEO::ArgDescriptor::ArgTPointer>());
        EXPECT_EQ(arg.type == NEO::ArgDescriptor::ArgTImage, arg.is<NEO::ArgDescriptor::ArgTImage>());
        EXPECT_EQ(arg.type == NEO::ArgDescriptor::ArgTSampler, arg.is<NEO::ArgDescriptor::ArgTSampler>());
        EXPECT_EQ(arg.type == NEO::ArgDescriptor::ArgTValue, arg.is<NEO::ArgDescriptor::ArgTValue>());
    }
}

TEST(ArgDescriptorAs, GivenUninitializedArgWhenInitializationRequestedThenInitializesTheArg) {
    NEO::ArgDescriptor argPointer;
    NEO::ArgDescriptor argImage;
    NEO::ArgDescriptor argSampler;
    NEO::ArgDescriptor argValue;

    argPointer.as<NEO::ArgDescPointer>(true);
    argImage.as<NEO::ArgDescImage>(true);
    argSampler.as<NEO::ArgDescSampler>(true);
    argValue.as<NEO::ArgDescValue>(true);
    EXPECT_EQ(NEO::ArgDescriptor::ArgTPointer, argPointer.type);
    EXPECT_EQ(NEO::ArgDescriptor::ArgTImage, argImage.type);
    EXPECT_EQ(NEO::ArgDescriptor::ArgTSampler, argSampler.type);
    EXPECT_EQ(NEO::ArgDescriptor::ArgTValue, argValue.type);
}

TEST(ArgDescriptorAs, GivenUninitializedArgWhenInitializationNotRequestedThenAborts) {
    NEO::ArgDescriptor argPointer;
    NEO::ArgDescriptor argImage;
    NEO::ArgDescriptor argSampler;
    NEO::ArgDescriptor argValue;

    EXPECT_THROW(argPointer.as<NEO::ArgDescPointer>(false), std::exception);
    EXPECT_THROW(argImage.as<NEO::ArgDescImage>(false), std::exception);
    EXPECT_THROW(argSampler.as<NEO::ArgDescSampler>(false), std::exception);
    EXPECT_THROW(argValue.as<NEO::ArgDescValue>(false), std::exception);
    EXPECT_EQ(NEO::ArgDescriptor::ArgTUnknown, argPointer.type);
    EXPECT_EQ(NEO::ArgDescriptor::ArgTUnknown, argImage.type);
    EXPECT_EQ(NEO::ArgDescriptor::ArgTUnknown, argSampler.type);
    EXPECT_EQ(NEO::ArgDescriptor::ArgTUnknown, argValue.type);
}

TEST(ArgDescriptorAs, GivenMismatchedArgTypeThenAborts) {
    NEO::ArgDescriptor argPointer;
    NEO::ArgDescriptor argImage;
    NEO::ArgDescriptor argSampler;
    NEO::ArgDescriptor argValue;

    argPointer.as<NEO::ArgDescPointer>(true);
    argImage.as<NEO::ArgDescImage>(true);
    argSampler.as<NEO::ArgDescSampler>(true);
    argValue.as<NEO::ArgDescValue>(true);

    EXPECT_NO_THROW(argPointer.as<NEO::ArgDescPointer>());
    EXPECT_NO_THROW(argImage.as<NEO::ArgDescImage>());
    EXPECT_NO_THROW(argSampler.as<NEO::ArgDescSampler>());
    EXPECT_NO_THROW(argValue.as<NEO::ArgDescValue>());

    EXPECT_THROW(argPointer.as<NEO::ArgDescImage>(), std::exception);
    EXPECT_THROW(argPointer.as<NEO::ArgDescSampler>(), std::exception);
    EXPECT_THROW(argPointer.as<NEO::ArgDescValue>(), std::exception);

    EXPECT_THROW(argImage.as<NEO::ArgDescPointer>(), std::exception);
    EXPECT_THROW(argImage.as<NEO::ArgDescSampler>(), std::exception);
    EXPECT_THROW(argImage.as<NEO::ArgDescValue>(), std::exception);

    EXPECT_THROW(argSampler.as<NEO::ArgDescPointer>(), std::exception);
    EXPECT_THROW(argSampler.as<NEO::ArgDescImage>(), std::exception);
    EXPECT_THROW(argSampler.as<NEO::ArgDescValue>(), std::exception);

    EXPECT_THROW(argValue.as<NEO::ArgDescPointer>(), std::exception);
    EXPECT_THROW(argValue.as<NEO::ArgDescImage>(), std::exception);
    EXPECT_THROW(argValue.as<NEO::ArgDescSampler>(), std::exception);

    EXPECT_NO_THROW(const_cast<NEO::ArgDescriptor &>(argPointer).as<NEO::ArgDescPointer>());
    EXPECT_NO_THROW(const_cast<NEO::ArgDescriptor &>(argImage).as<NEO::ArgDescImage>());
    EXPECT_NO_THROW(const_cast<NEO::ArgDescriptor &>(argSampler).as<NEO::ArgDescSampler>());
    EXPECT_NO_THROW(const_cast<NEO::ArgDescriptor &>(argValue).as<NEO::ArgDescValue>());

    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argPointer).as<NEO::ArgDescImage>(), std::exception);
    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argPointer).as<NEO::ArgDescSampler>(), std::exception);
    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argPointer).as<NEO::ArgDescValue>(), std::exception);

    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argImage).as<NEO::ArgDescPointer>(), std::exception);
    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argImage).as<NEO::ArgDescSampler>(), std::exception);
    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argImage).as<NEO::ArgDescValue>(), std::exception);

    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argSampler).as<NEO::ArgDescPointer>(), std::exception);
    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argSampler).as<NEO::ArgDescImage>(), std::exception);
    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argSampler).as<NEO::ArgDescValue>(), std::exception);

    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argValue).as<NEO::ArgDescPointer>(), std::exception);
    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argValue).as<NEO::ArgDescImage>(), std::exception);
    EXPECT_THROW(const_cast<NEO::ArgDescriptor &>(argValue).as<NEO::ArgDescSampler>(), std::exception);
}

TEST(ArgDescriptorCopyAssign, WhenCopyAssignedThenCopiesExtendedTypeInfo) {
    NEO::ArgDescriptor arg0;
    arg0.getExtendedTypeInfo().isAccelerator = true;
    arg0.getExtendedTypeInfo().hasDeviceSideEnqueueExtendedDescriptor = true;

    NEO::ArgDescriptor arg1{arg0};
    NEO::ArgDescriptor arg2;
    arg2 = arg1;
    EXPECT_EQ(arg0.getExtendedTypeInfo().packed, arg2.getExtendedTypeInfo().packed);
}

TEST(ArgDescriptorCopyAssign, WhenCopyAssignedThenCopiesTraits) {
    NEO::ArgDescriptor arg0;
    arg0.getTraits().accessQualifier = NEO::KernelArgMetadata::AccessWriteOnly;
    arg0.getTraits().addressQualifier = NEO::KernelArgMetadata::AddrLocal;
    arg0.getTraits().argByValSize = 3;
    arg0.getTraits().typeQualifiers.restrictQual = true;

    NEO::ArgDescriptor arg2;
    arg2 = arg0;
    EXPECT_EQ(arg0.getTraits().accessQualifier, arg2.getTraits().accessQualifier);
    EXPECT_EQ(arg0.getTraits().addressQualifier, arg2.getTraits().addressQualifier);
    EXPECT_EQ(arg0.getTraits().argByValSize, arg2.getTraits().argByValSize);
    EXPECT_EQ(arg0.getTraits().typeQualifiers.packed, arg2.getTraits().typeQualifiers.packed);
}

TEST(ArgDescriptorCopyAssign, GivenPointerArgWhenCopyAssignedThenCopiesDataBasedOnArgType) {
    NEO::ArgDescriptor arg0;
    auto &argPointer = arg0.as<NEO::ArgDescPointer>(true);
    argPointer.bindful = 2;
    argPointer.stateless = 3;
    argPointer.bindless = 5;
    argPointer.bufferOffset = 7;
    argPointer.slmOffset = 11;
    argPointer.requiredSlmAlignment = 13;
    argPointer.pointerSize = 17;
    argPointer.accessedUsingStatelessAddressingMode = false;

    NEO::ArgDescriptor arg2;
    arg2 = arg0;
    EXPECT_EQ(argPointer.bindful, arg2.as<NEO::ArgDescPointer>().bindful);
    EXPECT_EQ(argPointer.stateless, arg2.as<NEO::ArgDescPointer>().stateless);
    EXPECT_EQ(argPointer.bindless, arg2.as<NEO::ArgDescPointer>().bindless);
    EXPECT_EQ(argPointer.bufferOffset, arg2.as<NEO::ArgDescPointer>().bufferOffset);
    EXPECT_EQ(argPointer.slmOffset, arg2.as<NEO::ArgDescPointer>().slmOffset);
    EXPECT_EQ(argPointer.requiredSlmAlignment, arg2.as<NEO::ArgDescPointer>().requiredSlmAlignment);
    EXPECT_EQ(argPointer.pointerSize, arg2.as<NEO::ArgDescPointer>().pointerSize);
    EXPECT_EQ(argPointer.accessedUsingStatelessAddressingMode, arg2.as<NEO::ArgDescPointer>().accessedUsingStatelessAddressingMode);
}

TEST(ArgDescriptorCopyAssign, GivenImageArgWhenCopyAssignedThenCopiesDataBasedOnArgType) {
    NEO::ArgDescriptor arg0;
    auto &argImage = arg0.as<NEO::ArgDescImage>(true);
    argImage.bindful = 2;
    argImage.bindless = 3;

    argImage.metadataPayload.imgWidth = 5;
    argImage.metadataPayload.imgHeight = 7;
    argImage.metadataPayload.imgDepth = 11;
    argImage.metadataPayload.channelDataType = 13;
    argImage.metadataPayload.channelOrder = 17;

    argImage.metadataPayload.arraySize = 19;
    argImage.metadataPayload.numSamples = 23;
    argImage.metadataPayload.numMipLevels = 29;

    argImage.metadataPayload.flatBaseOffset = 31;
    argImage.metadataPayload.flatWidth = 37;
    argImage.metadataPayload.flatHeight = 41;
    argImage.metadataPayload.flatPitch = 43;

    NEO::ArgDescriptor arg2;
    arg2 = arg0;
    EXPECT_EQ(argImage.metadataPayload.imgWidth, arg2.as<NEO::ArgDescImage>().metadataPayload.imgWidth);
    EXPECT_EQ(argImage.metadataPayload.imgHeight, arg2.as<NEO::ArgDescImage>().metadataPayload.imgHeight);
    EXPECT_EQ(argImage.metadataPayload.imgDepth, arg2.as<NEO::ArgDescImage>().metadataPayload.imgDepth);
    EXPECT_EQ(argImage.metadataPayload.channelDataType, arg2.as<NEO::ArgDescImage>().metadataPayload.channelDataType);
    EXPECT_EQ(argImage.metadataPayload.channelOrder, arg2.as<NEO::ArgDescImage>().metadataPayload.channelOrder);
    EXPECT_EQ(argImage.metadataPayload.arraySize, arg2.as<NEO::ArgDescImage>().metadataPayload.arraySize);
    EXPECT_EQ(argImage.metadataPayload.numSamples, arg2.as<NEO::ArgDescImage>().metadataPayload.numSamples);
    EXPECT_EQ(argImage.metadataPayload.numMipLevels, arg2.as<NEO::ArgDescImage>().metadataPayload.numMipLevels);
    EXPECT_EQ(argImage.metadataPayload.flatBaseOffset, arg2.as<NEO::ArgDescImage>().metadataPayload.flatBaseOffset);
    EXPECT_EQ(argImage.metadataPayload.flatWidth, arg2.as<NEO::ArgDescImage>().metadataPayload.flatWidth);
    EXPECT_EQ(argImage.metadataPayload.flatHeight, arg2.as<NEO::ArgDescImage>().metadataPayload.flatHeight);
    EXPECT_EQ(argImage.metadataPayload.flatPitch, arg2.as<NEO::ArgDescImage>().metadataPayload.flatPitch);
}

TEST(ArgDescriptorCopyAssign, GivenSamplerArgWhenCopyAssignedThenCopiesDataBasedOnArgType) {
    NEO::ArgDescriptor arg0;
    auto &argSampler = arg0.as<NEO::ArgDescSampler>(true);
    argSampler.samplerType = 2;
    argSampler.bindful = 3;
    argSampler.bindless = 5;
    argSampler.metadataPayload.samplerSnapWa = 7;
    argSampler.metadataPayload.samplerAddressingMode = 11;
    argSampler.metadataPayload.samplerNormalizedCoords = 13;

    NEO::ArgDescriptor arg2;
    arg2 = arg0;
    EXPECT_EQ(argSampler.samplerType, arg2.as<NEO::ArgDescSampler>().samplerType);
    EXPECT_EQ(argSampler.bindful, arg2.as<NEO::ArgDescSampler>().bindful);
    EXPECT_EQ(argSampler.bindless, arg2.as<NEO::ArgDescSampler>().bindless);
    EXPECT_EQ(argSampler.metadataPayload.samplerSnapWa, arg2.as<NEO::ArgDescSampler>().metadataPayload.samplerSnapWa);
    EXPECT_EQ(argSampler.metadataPayload.samplerAddressingMode, arg2.as<NEO::ArgDescSampler>().metadataPayload.samplerAddressingMode);
    EXPECT_EQ(argSampler.metadataPayload.samplerNormalizedCoords, arg2.as<NEO::ArgDescSampler>().metadataPayload.samplerNormalizedCoords);
}

TEST(ArgDescriptorCopyAssign, GivenValueArgWhenCopyAssignedThenCopiesDataBasedOnArgType) {
    NEO::ArgDescValue::Element element0;
    element0.offset = 2;
    element0.size = 3;
    element0.sourceOffset = 5;

    NEO::ArgDescValue::Element element1;
    element1.offset = 7;
    element1.size = 11;
    element1.sourceOffset = 13;

    NEO::ArgDescriptor arg0;
    auto &argValue = arg0.as<NEO::ArgDescValue>(true);
    argValue.elements.push_back(element0);
    argValue.elements.push_back(element1);

    NEO::ArgDescriptor arg2;
    arg2 = arg0;
    ASSERT_EQ(argValue.elements.size(), arg2.as<NEO::ArgDescValue>().elements.size());
    for (size_t i = 0; i < argValue.elements.size(); ++i) {
        EXPECT_EQ(argValue.elements[i].offset, arg2.as<NEO::ArgDescValue>().elements[i].offset) << i;
        EXPECT_EQ(argValue.elements[i].offset, arg2.as<NEO::ArgDescValue>().elements[i].offset) << i;
        EXPECT_EQ(argValue.elements[i].offset, arg2.as<NEO::ArgDescValue>().elements[i].offset) << i;
    }
}

TEST(SetOffsetsVec, GivenArrayOfCrossThreadDataThenCopiesProperAmountOfElements) {
    NEO::CrossThreadDataOffset src[3] = {2, 3, 5};
    NEO::CrossThreadDataOffset dst[3] = {7, 11, 13};
    NEO::setOffsetsVec(dst, src);
    EXPECT_EQ(dst[0], src[0]);
    EXPECT_EQ(dst[1], src[1]);
    EXPECT_EQ(dst[2], src[2]);
}

TEST(PatchNonPointer, GivenUndefinedOffsetThenReturnsFalse) {
    uint8_t buffer[64];
    uint32_t value = 7;
    EXPECT_FALSE(NEO::patchNonPointer(buffer, NEO::undefined<NEO::CrossThreadDataOffset>, value));
}

TEST(PatchNonPointer, GivenOutOfBoundsOffsetThenAbort) {
    uint8_t buffer[64];
    uint32_t value = 7;
    EXPECT_THROW(NEO::patchNonPointer(buffer, sizeof(buffer), value), std::exception);
    EXPECT_THROW(NEO::patchNonPointer(buffer, sizeof(buffer) - sizeof(value) + 1, value), std::exception);
}

TEST(PatchNonPointer, GivenValidOffsetThenPatchProperly) {
    alignas(8) uint8_t buffer[64];
    memset(buffer, 3, sizeof(buffer));
    uint32_t value32 = 7;
    uint64_t value64 = 13;
    EXPECT_TRUE(NEO::patchNonPointer(buffer, 0, value32));
    EXPECT_TRUE(NEO::patchNonPointer(buffer, 8, value64));
    EXPECT_TRUE(NEO::patchNonPointer(buffer, sizeof(buffer) - sizeof(value64), value64));

    alignas(8) uint8_t expected[64];
    memset(expected, 3, sizeof(expected));
    *reinterpret_cast<uint32_t *>(expected) = value32;
    *reinterpret_cast<uint64_t *>(expected + 8) = value64;
    *reinterpret_cast<uint64_t *>(expected + sizeof(expected) - sizeof(value64)) = value64;
    EXPECT_EQ(0, memcmp(expected, buffer, sizeof(buffer)));
}

TEST(PatchVecNonPointer, GivenArrayOfOffsetsThenReturnsNumberOfValuesProperlyPatched) {
    alignas(8) uint8_t buffer[64];
    memset(buffer, 3, sizeof(buffer));
    NEO::CrossThreadDataOffset offsets[] = {0, 4, sizeof(buffer) - sizeof(uint32_t), NEO::undefined<NEO::CrossThreadDataOffset>};
    uint32_t values[] = {7, 11, 13, 17};
    auto numPatched = NEO::patchVecNonPointer(buffer, offsets, values);
    EXPECT_EQ(3U, numPatched);
    alignas(8) uint8_t expected[64];
    memset(expected, 3, sizeof(expected));
    *reinterpret_cast<uint32_t *>(expected) = 7;
    *reinterpret_cast<uint32_t *>(expected + 4) = 11;
    *reinterpret_cast<uint32_t *>(expected + sizeof(expected) - sizeof(uint32_t)) = 13;
    EXPECT_EQ(0, memcmp(expected, buffer, sizeof(buffer)));
}

TEST(PatchPointer, GivenUnhandledPointerSizeThenAborts) {
    alignas(8) uint8_t buffer[64];
    memset(buffer, 3, sizeof(buffer));
    NEO::ArgDescPointer ptrArg;
    uintptr_t ptrValue = reinterpret_cast<uintptr_t>(&ptrArg);
    ptrArg.pointerSize = 5;
    EXPECT_THROW(patchPointer(buffer, ptrArg, ptrValue), std::exception);
}

TEST(PatchPointer, Given32bitPointerSizeThenPatchesOnly32bits) {
    alignas(8) uint8_t buffer[64];
    memset(buffer, 3, sizeof(buffer));
    NEO::ArgDescPointer ptrArg;
    uintptr_t ptrValue = reinterpret_cast<uintptr_t>(&ptrArg);
    ptrArg.stateless = 0U;
    ptrArg.pointerSize = 4;
    EXPECT_TRUE(patchPointer(buffer, ptrArg, ptrValue));
    alignas(8) uint8_t expected[64];
    memset(expected, 3, sizeof(expected));
    *reinterpret_cast<uint32_t *>(expected) = static_cast<uint32_t>(ptrValue);
}

TEST(PatchPointer, Given64bitPointerSizeThenPatchesAll64bits) {
    alignas(8) uint8_t buffer[64];
    memset(buffer, 3, sizeof(buffer));
    NEO::ArgDescPointer ptrArg;
    uintptr_t ptrValue = reinterpret_cast<uintptr_t>(&ptrArg);
    ptrArg.stateless = 0U;
    ptrArg.pointerSize = 8;
    EXPECT_TRUE(patchPointer(buffer, ptrArg, ptrValue));
    alignas(8) uint8_t expected[64];
    memset(expected, 3, sizeof(expected));
    *reinterpret_cast<uint64_t *>(expected) = static_cast<uint64_t>(ptrValue);
}
