/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hash.h"
#include "runtime/compiler_interface/patchtokens_decoder.h"
#include "test.h"

#include "patchtokens_tests.h"

#include <vector>

bool hasEmptyHeaps(const NEO::PatchTokenBinary::KernelFromPatchtokens &kernel) {
    return kernel.heaps.generalState.empty() && kernel.heaps.dynamicState.empty() && kernel.heaps.surfaceState.empty();
}

bool hasEmptyTokensInfo(const NEO::PatchTokenBinary::KernelFromPatchtokens &kernel) {
    auto &toks = kernel.tokens;
    bool empty = true;
    empty &= nullptr == toks.samplerStateArray;
    empty &= nullptr == toks.bindingTableState;
    empty &= nullptr == toks.allocateLocalSurface;
    empty &= nullptr == toks.mediaVfeState[0];
    empty &= nullptr == toks.mediaVfeState[1];
    empty &= nullptr == toks.mediaInterfaceDescriptorLoad;
    empty &= nullptr == toks.interfaceDescriptorData;
    empty &= nullptr == toks.threadPayload;
    empty &= nullptr == toks.executionEnvironment;
    empty &= nullptr == toks.dataParameterStream;
    empty &= nullptr == toks.kernelAttributesInfo;
    empty &= nullptr == toks.allocateStatelessPrivateSurface;
    empty &= nullptr == toks.allocateStatelessConstantMemorySurfaceWithInitialization;
    empty &= nullptr == toks.allocateStatelessGlobalMemorySurfaceWithInitialization;
    empty &= nullptr == toks.allocateStatelessPrintfSurface;
    empty &= nullptr == toks.allocateStatelessEventPoolSurface;
    empty &= nullptr == toks.allocateStatelessDefaultDeviceQueueSurface;
    empty &= nullptr == toks.inlineVmeSamplerInfo;
    empty &= nullptr == toks.gtpinFreeGrfInfo;
    empty &= nullptr == toks.stateSip;
    empty &= nullptr == toks.allocateSystemThreadSurface;
    empty &= nullptr == toks.gtpinInfo;
    empty &= nullptr == toks.programSymbolTable;
    empty &= nullptr == toks.programRelocationTable;
    empty &= toks.kernelArgs.empty();
    empty &= toks.strings.empty();
    for (int i = 0; i < 3; ++i) {
        empty &= nullptr == toks.crossThreadPayloadArgs.localWorkSize[i];
        empty &= nullptr == toks.crossThreadPayloadArgs.localWorkSize[i];
        empty &= nullptr == toks.crossThreadPayloadArgs.localWorkSize2[i];
        empty &= nullptr == toks.crossThreadPayloadArgs.enqueuedLocalWorkSize[i];
        empty &= nullptr == toks.crossThreadPayloadArgs.numWorkGroups[i];
        empty &= nullptr == toks.crossThreadPayloadArgs.globalWorkOffset[i];
        empty &= nullptr == toks.crossThreadPayloadArgs.globalWorkSize[i];
    }
    empty &= nullptr == toks.crossThreadPayloadArgs.maxWorkGroupSize;
    empty &= nullptr == toks.crossThreadPayloadArgs.workDimensions;
    empty &= nullptr == toks.crossThreadPayloadArgs.simdSize;
    empty &= nullptr == toks.crossThreadPayloadArgs.parentEvent;
    empty &= nullptr == toks.crossThreadPayloadArgs.privateMemoryStatelessSize;
    empty &= nullptr == toks.crossThreadPayloadArgs.localMemoryStatelessWindowSize;
    empty &= nullptr == toks.crossThreadPayloadArgs.localMemoryStatelessWindowStartAddress;
    empty &= nullptr == toks.crossThreadPayloadArgs.preferredWorkgroupMultiple;
    empty &= toks.crossThreadPayloadArgs.childBlockSimdSize.empty();
    return empty;
}

bool hasEmptyTokensInfo(const NEO::PatchTokenBinary::ProgramFromPatchtokens &program) {
    auto &toks = program.programScopeTokens;
    bool empty = true;
    empty &= toks.allocateConstantMemorySurface.empty();
    empty &= toks.allocateGlobalMemorySurface.empty();
    empty &= toks.constantPointer.empty();
    empty &= toks.globalPointer.empty();
    empty &= nullptr == toks.symbolTable;
    return empty;
}

template <typename TokenT>
uint32_t pushBackToken(iOpenCL::PATCH_TOKEN token, std::vector<uint8_t> &storage) {
    auto offset = storage.size();
    TokenT tok = PatchTokensTestData::initToken<TokenT>(token);
    storage.insert(storage.end(), reinterpret_cast<uint8_t *>(&tok), reinterpret_cast<uint8_t *>((&tok) + 1));
    return static_cast<uint32_t>(offset);
}

template <typename TokenT>
uint32_t pushBackToken(const TokenT &token, std::vector<uint8_t> &storage) {
    auto offset = storage.size();
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&token), reinterpret_cast<const uint8_t *>(&token) + token.Size);
    return static_cast<uint32_t>(offset);
}

uint32_t pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_TOKEN type, std::vector<uint8_t> &storage, uint32_t sourceIndex = 0, uint32_t argNum = 0) {
    iOpenCL::SPatchDataParameterBuffer tok = PatchTokensTestData::initDataParameterBufferToken(type, sourceIndex, argNum);

    auto offset = storage.size();
    storage.insert(storage.end(), reinterpret_cast<const uint8_t *>(&tok), reinterpret_cast<const uint8_t *>(&tok) + tok.Size);
    return static_cast<uint32_t>(offset);
}

bool tokenOffsetMatched(const uint8_t *base, size_t tokenOffset, const iOpenCL::SPatchItemHeader *expectedToken) {
    return (base + tokenOffset) == reinterpret_cast<const uint8_t *>(expectedToken);
}

TEST(GetInlineData, GivenConstantMemorySurfaceTokenThenReturnProperOffsetToInlineData) {
    iOpenCL::SPatchAllocateConstantMemorySurfaceProgramBinaryInfo surfTok[2];
    EXPECT_EQ(reinterpret_cast<uint8_t *>(&surfTok[1]), NEO::PatchTokenBinary::getInlineData(&surfTok[0]));
}

TEST(GetInlineData, GivenGlobalMemorySurfaceTokenThenReturnProperOffsetToInlineData) {
    iOpenCL::SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo surfTok[2];
    EXPECT_EQ(reinterpret_cast<uint8_t *>(&surfTok[1]), NEO::PatchTokenBinary::getInlineData(&surfTok[0]));
}

TEST(GetInlineData, GivenStringTokenThenReturnProperOffsetToInlineData) {
    iOpenCL::SPatchString surfTok[2];
    EXPECT_EQ(reinterpret_cast<uint8_t *>(&surfTok[1]), NEO::PatchTokenBinary::getInlineData(&surfTok[0]));
}

TEST(GetInlineData, GivenKernelArgumentInfoTokenThenReturnDecodedInlineData) {
    std::vector<uint8_t> storage;
    std::string addressQualifier = "__global";
    std::string accessQualifier = "read_write";
    std::string argName = "custom_arg";
    std::string typeName = "int*;";
    std::string typeQualifier = "const";

    PatchTokensTestData::pushBackArgInfoToken(storage, 0U, addressQualifier, accessQualifier, argName, typeName, typeQualifier);

    auto inlineData = NEO::PatchTokenBinary::getInlineData(reinterpret_cast<iOpenCL::SPatchKernelArgumentInfo *>(storage.data()));
    EXPECT_STREQ(addressQualifier.c_str(), std::string(inlineData.addressQualifier.begin(), inlineData.addressQualifier.end()).c_str());
    EXPECT_STREQ(accessQualifier.c_str(), std::string(inlineData.accessQualifier.begin(), inlineData.accessQualifier.end()).c_str());
    EXPECT_STREQ(argName.c_str(), std::string(inlineData.argName.begin(), inlineData.argName.end()).c_str());
    EXPECT_STREQ(typeName.c_str(), std::string(inlineData.typeName.begin(), inlineData.typeName.end()).c_str());
    EXPECT_STREQ(typeQualifier.c_str(), std::string(inlineData.typeQualifiers.begin(), inlineData.typeQualifiers.end()).c_str());
}

TEST(GetInlineData, GivenKernelArgumentInfoTokenWhenNotEnoughDataThenArrayIsBoundsProtected) {
    iOpenCL::SPatchKernelArgumentInfo tokInline = {};
    tokInline.AddressQualifierSize = 4;
    tokInline.AccessQualifierSize = 8;
    tokInline.ArgumentNameSize = 32;
    tokInline.TypeNameSize = 16;
    tokInline.TypeQualifierSize = 6;
    tokInline.Size = sizeof(iOpenCL::SPatchKernelArgumentInfo);
    auto inlineData = NEO::PatchTokenBinary::getInlineData(&tokInline);
    EXPECT_EQ(0U, inlineData.addressQualifier.size());
    EXPECT_EQ(0U, inlineData.accessQualifier.size());
    EXPECT_EQ(0U, inlineData.argName.size());
    EXPECT_EQ(0U, inlineData.typeName.size());
    EXPECT_EQ(0U, inlineData.typeQualifiers.size());
}

TEST(KernelChecksum, GivenKernelBlobThenChecksumIsCalculatedBasedOnDataAfterKernelHeader) {
    std::vector<uint8_t> storage;
    auto kernel = PatchTokensTestData::ValidEmptyKernel::create(storage);
    auto calculatedChecksum = NEO::PatchTokenBinary::calcKernelChecksum(kernel.blobs.kernelInfo);

    auto dataToHash = ArrayRef<const uint8_t>(ptrOffset(storage.data(), sizeof(iOpenCL::SKernelBinaryHeaderCommon)), ptrOffset(storage.data(), storage.size()));
    uint64_t hashValue = NEO::Hash::hash(reinterpret_cast<const char *>(dataToHash.begin()), dataToHash.size());
    uint32_t expectedChecksum = hashValue & 0xFFFFFFFF;
    EXPECT_EQ(expectedChecksum, calculatedChecksum);
}

TEST(KernelChecksum, GivenKernelWithProperChecksumThenValidationSucceeds) {
    std::vector<uint8_t> storage;
    auto kernel = PatchTokensTestData::ValidEmptyKernel::create(storage);
    auto calculatedChecksum = NEO::PatchTokenBinary::calcKernelChecksum(kernel.blobs.kernelInfo);
    EXPECT_EQ(kernel.header->CheckSum, calculatedChecksum);
    EXPECT_FALSE(NEO::PatchTokenBinary::hasInvalidChecksum(kernel));
}

TEST(KernelChecksum, GivenKernelWithInvalidChecksumThenValidationFails) {
    std::vector<uint8_t> storage;
    auto kernel = PatchTokensTestData::ValidEmptyKernel::create(storage);
    auto calculatedChecksum = NEO::PatchTokenBinary::calcKernelChecksum(kernel.blobs.kernelInfo);
    EXPECT_EQ(kernel.header->CheckSum, calculatedChecksum);

    ASSERT_EQ(storage.data(), kernel.blobs.kernelInfo.begin());
    reinterpret_cast<iOpenCL::SKernelBinaryHeader *>(storage.data())->CheckSum += 1;

    EXPECT_TRUE(NEO::PatchTokenBinary::hasInvalidChecksum(kernel));
}

TEST(KernelDecoder, GivenValidEmptyKernelThenDecodingOfHeaderSucceeds) {
    std::vector<uint8_t> storage;
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);
    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(kernelToEncode.blobs.kernelInfo, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    ASSERT_NE(nullptr, decodedKernel.header);
    EXPECT_EQ(kernelToEncode.header, decodedKernel.header);
    EXPECT_EQ(kernelToEncode.name, decodedKernel.name);
    EXPECT_EQ(kernelToEncode.blobs.kernelInfo, decodedKernel.blobs.kernelInfo);
    EXPECT_EQ(0U, decodedKernel.isa.size());
    EXPECT_TRUE(hasEmptyHeaps(decodedKernel));
    EXPECT_EQ(0U, decodedKernel.unhandledTokens.size());
    EXPECT_TRUE(hasEmptyTokensInfo(decodedKernel));
}

TEST(KernelDecoder, GivenEmptyKernelWhenBlobSmallerThanKernelHeaderThenDecodingFails) {
    std::vector<uint8_t> storage;
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);
    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    auto brokenBlob = ArrayRef<const uint8_t>(kernelToEncode.blobs.kernelInfo.begin(),
                                              kernelToEncode.blobs.kernelInfo.begin() + sizeof(iOpenCL::SKernelBinaryHeader) - 1);
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(brokenBlob, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
}

TEST(KernelDecoder, GivenValidKernelWithHeapsThenDecodingSucceedsAndHeapsAreProperlySet) {
    std::vector<uint8_t> storage;
    storage.reserve(512);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());

    size_t isaOffset = storage.size();
    kernelHeader->KernelHeapSize = 16U;
    storage.resize(storage.size() + kernelHeader->KernelHeapSize);

    size_t generalStateHeapOffset = storage.size();
    kernelHeader->GeneralStateHeapSize = 24U;
    storage.resize(storage.size() + kernelHeader->GeneralStateHeapSize);

    size_t dynamicStateHeapOffset = storage.size();
    kernelHeader->DynamicStateHeapSize = 8U;
    storage.resize(storage.size() + kernelHeader->DynamicStateHeapSize);

    size_t surfaceStateHeapOffset = storage.size();
    kernelHeader->SurfaceStateHeapSize = 32U;
    storage.resize(storage.size() + kernelHeader->SurfaceStateHeapSize);

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);

    EXPECT_EQ(kernelToEncode.header, decodedKernel.header);
    EXPECT_EQ(0U, decodedKernel.unhandledTokens.size());
    EXPECT_TRUE(hasEmptyTokensInfo(decodedKernel));

    EXPECT_EQ(kernelToEncode.name, decodedKernel.name);
    EXPECT_EQ(ArrayRef<const uint8_t>(storage.data() + isaOffset, kernelHeader->KernelHeapSize), decodedKernel.isa);
    EXPECT_EQ(ArrayRef<const uint8_t>(storage.data() + generalStateHeapOffset, kernelHeader->GeneralStateHeapSize), decodedKernel.heaps.generalState);
    EXPECT_EQ(ArrayRef<const uint8_t>(storage.data() + dynamicStateHeapOffset, kernelHeader->DynamicStateHeapSize), decodedKernel.heaps.dynamicState);
    EXPECT_EQ(ArrayRef<const uint8_t>(storage.data() + surfaceStateHeapOffset, kernelHeader->SurfaceStateHeapSize), decodedKernel.heaps.surfaceState);
}

TEST(KernelDecoder, GivenEmptyKernelWhenBlobDoesntHaveEnoughSpaceForHeaderDataThenDecodingFails) {
    std::vector<uint8_t> storage;
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);
    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    iOpenCL::SKernelBinaryHeaderCommon originalHeader = *kernelToEncode.header;
    uint32_t outOfBoundsSize = static_cast<uint32_t>(storage.size());

    decodedKernel = {};
    kernelHeader->KernelNameSize = outOfBoundsSize;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(kernelToEncode.blobs.kernelInfo, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
    kernelHeader->KernelNameSize = originalHeader.KernelNameSize;

    kernelHeader->KernelHeapSize = outOfBoundsSize;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(kernelToEncode.blobs.kernelInfo, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
    kernelHeader->KernelHeapSize = originalHeader.KernelHeapSize;

    kernelHeader->GeneralStateHeapSize = outOfBoundsSize;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(kernelToEncode.blobs.kernelInfo, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
    kernelHeader->GeneralStateHeapSize = originalHeader.GeneralStateHeapSize;

    kernelHeader->DynamicStateHeapSize = outOfBoundsSize;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(kernelToEncode.blobs.kernelInfo, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
    kernelHeader->DynamicStateHeapSize = originalHeader.DynamicStateHeapSize;

    kernelHeader->SurfaceStateHeapSize = outOfBoundsSize;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(kernelToEncode.blobs.kernelInfo, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
    kernelHeader->SurfaceStateHeapSize = originalHeader.SurfaceStateHeapSize;

    kernelHeader->PatchListSize = outOfBoundsSize;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(kernelToEncode.blobs.kernelInfo, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
    kernelHeader->PatchListSize = originalHeader.PatchListSize;
}

TEST(KernelDecoder, GivenKernelWithValidKernelPatchtokensThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    using namespace iOpenCL;

    std::vector<uint8_t> storage;
    storage.reserve(1024);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());
    auto samplerStateArrayOff = pushBackToken<SPatchSamplerStateArray>(PATCH_TOKEN_SAMPLER_STATE_ARRAY, storage);
    auto bindingTableStateOff = pushBackToken<SPatchBindingTableState>(PATCH_TOKEN_BINDING_TABLE_STATE, storage);
    auto allocateLocalSurfaceOff = pushBackToken<SPatchAllocateLocalSurface>(PATCH_TOKEN_ALLOCATE_LOCAL_SURFACE, storage);
    auto mediaVfeState0Off = pushBackToken<SPatchMediaVFEState>(PATCH_TOKEN_MEDIA_VFE_STATE, storage);
    auto mediaVfeState1Off = pushBackToken<SPatchMediaVFEState>(PATCH_TOKEN_MEDIA_VFE_STATE_SLOT1, storage);
    auto mediaInterfaceDescriptorLoadOff = pushBackToken<SPatchMediaInterfaceDescriptorLoad>(PATCH_TOKEN_MEDIA_INTERFACE_DESCRIPTOR_LOAD, storage);
    auto interfaceDescriptorDataOff = pushBackToken<SPatchInterfaceDescriptorData>(PATCH_TOKEN_INTERFACE_DESCRIPTOR_DATA, storage);
    auto threadPayloadOff = pushBackToken<SPatchThreadPayload>(PATCH_TOKEN_THREAD_PAYLOAD, storage);
    auto executionEnvironmentOff = pushBackToken<SPatchExecutionEnvironment>(PATCH_TOKEN_EXECUTION_ENVIRONMENT, storage);
    auto kernelAttributesInfoOff = pushBackToken<SPatchKernelAttributesInfo>(PATCH_TOKEN_KERNEL_ATTRIBUTES_INFO, storage);
    auto allocatedStatelessPrivateMemoryOff = pushBackToken<SPatchKernelAttributesInfo>(PATCH_TOKEN_ALLOCATE_STATELESS_PRIVATE_MEMORY, storage);
    auto allocateStatelessConstantMemorySurfaceWithInitializationOff = pushBackToken<SPatchAllocateStatelessConstantMemorySurfaceWithInitialization>(PATCH_TOKEN_ALLOCATE_STATELESS_CONSTANT_MEMORY_SURFACE_WITH_INITIALIZATION, storage);
    auto allocateStatelessGlobalMemorySurfaceWithInitializationOff = pushBackToken<SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization>(PATCH_TOKEN_ALLOCATE_STATELESS_GLOBAL_MEMORY_SURFACE_WITH_INITIALIZATION, storage);
    auto allocateStatelessPrintfSurfaceOff = pushBackToken<SPatchAllocateStatelessPrintfSurface>(PATCH_TOKEN_ALLOCATE_STATELESS_PRINTF_SURFACE, storage);
    auto allocateStatelessEventPoolSurfaceOff = pushBackToken<SPatchAllocateStatelessEventPoolSurface>(PATCH_TOKEN_ALLOCATE_STATELESS_EVENT_POOL_SURFACE, storage);
    auto allocateStatelessDefaultDeviceQueueSurfaceOff = pushBackToken<SPatchAllocateStatelessDefaultDeviceQueueSurface>(PATCH_TOKEN_ALLOCATE_STATELESS_DEFAULT_DEVICE_QUEUE_SURFACE, storage);
    auto inlineVmeSamplerInfoOff = pushBackToken<SPatchInlineVMESamplerInfo>(PATCH_TOKEN_INLINE_VME_SAMPLER_INFO, storage);
    auto gtpinFreeGrfInfoOff = pushBackToken<SPatchGtpinFreeGRFInfo>(PATCH_TOKEN_GTPIN_FREE_GRF_INFO, storage);
    auto gtpinInfoOff = pushBackToken<SPatchItemHeader>(PATCH_TOKEN_GTPIN_INFO, storage);
    auto stateSipOff = pushBackToken<SPatchStateSIP>(PATCH_TOKEN_STATE_SIP, storage);
    auto programSymbolTableOff = pushBackToken<SPatchFunctionTableInfo>(PATCH_TOKEN_PROGRAM_SYMBOL_TABLE, storage);
    auto programRelocationTableOff = pushBackToken<SPatchFunctionTableInfo>(PATCH_TOKEN_PROGRAM_RELOCATION_TABLE, storage);
    auto dataParameterStreamOff = pushBackToken<SPatchDataParameterStream>(PATCH_TOKEN_DATA_PARAMETER_STREAM, storage);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());
    EXPECT_EQ(ptrOffset(storage.data(), patchListOffset), decodedKernel.blobs.patchList.begin());
    EXPECT_EQ(ptrOffset(storage.data(), storage.size()), decodedKernel.blobs.patchList.end());

    auto base = storage.data();
    EXPECT_TRUE(tokenOffsetMatched(base, samplerStateArrayOff, decodedKernel.tokens.samplerStateArray));
    EXPECT_TRUE(tokenOffsetMatched(base, bindingTableStateOff, decodedKernel.tokens.bindingTableState));
    EXPECT_TRUE(tokenOffsetMatched(base, allocateLocalSurfaceOff, decodedKernel.tokens.allocateLocalSurface));
    EXPECT_TRUE(tokenOffsetMatched(base, mediaVfeState0Off, decodedKernel.tokens.mediaVfeState[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, mediaVfeState1Off, decodedKernel.tokens.mediaVfeState[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, mediaInterfaceDescriptorLoadOff, decodedKernel.tokens.mediaInterfaceDescriptorLoad));
    EXPECT_TRUE(tokenOffsetMatched(base, interfaceDescriptorDataOff, decodedKernel.tokens.interfaceDescriptorData));
    EXPECT_TRUE(tokenOffsetMatched(base, threadPayloadOff, decodedKernel.tokens.threadPayload));
    EXPECT_TRUE(tokenOffsetMatched(base, executionEnvironmentOff, decodedKernel.tokens.executionEnvironment));
    EXPECT_TRUE(tokenOffsetMatched(base, kernelAttributesInfoOff, decodedKernel.tokens.kernelAttributesInfo));
    EXPECT_TRUE(tokenOffsetMatched(base, allocatedStatelessPrivateMemoryOff, decodedKernel.tokens.allocateStatelessPrivateSurface));
    EXPECT_TRUE(tokenOffsetMatched(base, allocateStatelessConstantMemorySurfaceWithInitializationOff, decodedKernel.tokens.allocateStatelessConstantMemorySurfaceWithInitialization));
    EXPECT_TRUE(tokenOffsetMatched(base, allocateStatelessGlobalMemorySurfaceWithInitializationOff, decodedKernel.tokens.allocateStatelessGlobalMemorySurfaceWithInitialization));
    EXPECT_TRUE(tokenOffsetMatched(base, allocateStatelessPrintfSurfaceOff, decodedKernel.tokens.allocateStatelessPrintfSurface));
    EXPECT_TRUE(tokenOffsetMatched(base, allocateStatelessEventPoolSurfaceOff, decodedKernel.tokens.allocateStatelessEventPoolSurface));
    EXPECT_TRUE(tokenOffsetMatched(base, allocateStatelessDefaultDeviceQueueSurfaceOff, decodedKernel.tokens.allocateStatelessDefaultDeviceQueueSurface));
    EXPECT_TRUE(tokenOffsetMatched(base, inlineVmeSamplerInfoOff, decodedKernel.tokens.inlineVmeSamplerInfo));
    EXPECT_TRUE(tokenOffsetMatched(base, gtpinFreeGrfInfoOff, decodedKernel.tokens.gtpinFreeGrfInfo));
    EXPECT_TRUE(tokenOffsetMatched(base, gtpinInfoOff, decodedKernel.tokens.gtpinInfo));
    EXPECT_TRUE(tokenOffsetMatched(base, stateSipOff, decodedKernel.tokens.stateSip));
    EXPECT_TRUE(tokenOffsetMatched(base, programSymbolTableOff, decodedKernel.tokens.programSymbolTable));
    EXPECT_TRUE(tokenOffsetMatched(base, programRelocationTableOff, decodedKernel.tokens.programRelocationTable));
    EXPECT_TRUE(tokenOffsetMatched(base, dataParameterStreamOff, decodedKernel.tokens.dataParameterStream));
}

TEST(KernelDecoder, GivenKernelWithValidStringPatchtokensThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    std::vector<uint8_t> storage;
    storage.reserve(512);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchString stringTok = {};
    stringTok.Token = iOpenCL::PATCH_TOKEN::PATCH_TOKEN_STRING;

    auto patchListOffset = static_cast<uint32_t>(storage.size());
    auto string1Off = PatchTokensTestData::pushBackStringToken("str1", 1, storage);
    auto string2Off = PatchTokensTestData::pushBackStringToken("str2", 2, storage);
    auto string0Off = PatchTokensTestData::pushBackStringToken("str0", 0, storage);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());

    auto base = storage.data();
    ASSERT_EQ(3U, decodedKernel.tokens.strings.size());
    EXPECT_TRUE(tokenOffsetMatched(base, string0Off, decodedKernel.tokens.strings[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, string1Off, decodedKernel.tokens.strings[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, string2Off, decodedKernel.tokens.strings[2]));
}

TEST(KernelDecoder, GivenKernelWithValidArgInfoPatchtokensThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    std::vector<uint8_t> storage;
    storage.reserve(512);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    iOpenCL::SPatchKernelArgumentInfo argInfoTok = {};
    argInfoTok.Token = iOpenCL::PATCH_TOKEN::PATCH_TOKEN_KERNEL_ARGUMENT_INFO;

    auto patchListOffset = static_cast<uint32_t>(storage.size());

    auto arg1Off = static_cast<uint32_t>(storage.size());
    argInfoTok.ArgumentNumber = 1;
    auto additionalDataSize = 8;
    argInfoTok.Size = sizeof(argInfoTok) + additionalDataSize;
    storage.insert(storage.end(), reinterpret_cast<uint8_t *>(&argInfoTok), reinterpret_cast<uint8_t *>((&argInfoTok) + 1));
    storage.resize(storage.size() + additionalDataSize);

    auto arg2Off = static_cast<uint32_t>(storage.size());
    argInfoTok.ArgumentNumber = 2;
    additionalDataSize = 16;
    argInfoTok.Size = sizeof(argInfoTok) + additionalDataSize;
    storage.insert(storage.end(), reinterpret_cast<uint8_t *>(&argInfoTok), reinterpret_cast<uint8_t *>((&argInfoTok) + 1));
    storage.resize(storage.size() + additionalDataSize);

    auto arg0Off = static_cast<uint32_t>(storage.size());
    argInfoTok.ArgumentNumber = 0;
    additionalDataSize = 24;
    argInfoTok.Size = sizeof(argInfoTok) + additionalDataSize;
    storage.insert(storage.end(), reinterpret_cast<uint8_t *>(&argInfoTok), reinterpret_cast<uint8_t *>((&argInfoTok) + 1));
    storage.resize(storage.size() + additionalDataSize);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());

    auto base = storage.data();
    ASSERT_EQ(3U, decodedKernel.tokens.kernelArgs.size());
    EXPECT_TRUE(tokenOffsetMatched(base, arg0Off, decodedKernel.tokens.kernelArgs[0].argInfo));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1Off, decodedKernel.tokens.kernelArgs[1].argInfo));
    EXPECT_TRUE(tokenOffsetMatched(base, arg2Off, decodedKernel.tokens.kernelArgs[2].argInfo));
}

TEST(KernelDecoder, GivenKernelWithValidObjectArgPatchtokensThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    std::vector<uint8_t> storage;
    storage.reserve(512);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());

    iOpenCL::SPatchSamplerKernelArgument samplerTok = {};
    samplerTok.Token = iOpenCL::PATCH_TOKEN::PATCH_TOKEN_SAMPLER_KERNEL_ARGUMENT;
    samplerTok.Size = sizeof(samplerTok);
    samplerTok.ArgumentNumber = 3;

    iOpenCL::SPatchImageMemoryObjectKernelArgument imageTok = {};
    imageTok.Token = iOpenCL::PATCH_TOKEN::PATCH_TOKEN_IMAGE_MEMORY_OBJECT_KERNEL_ARGUMENT;
    imageTok.Size = sizeof(imageTok);
    imageTok.ArgumentNumber = 1;

    iOpenCL::SPatchGlobalMemoryObjectKernelArgument globalMemTok = {};
    globalMemTok.Token = iOpenCL::PATCH_TOKEN::PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT;
    globalMemTok.Size = sizeof(globalMemTok);
    globalMemTok.ArgumentNumber = 2;

    iOpenCL::SPatchStatelessGlobalMemoryObjectKernelArgument statelessGlobalMemTok = {};
    statelessGlobalMemTok.Token = iOpenCL::PATCH_TOKEN::PATCH_TOKEN_STATELESS_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT;
    statelessGlobalMemTok.Size = sizeof(statelessGlobalMemTok);
    statelessGlobalMemTok.ArgumentNumber = 0;

    iOpenCL::SPatchStatelessConstantMemoryObjectKernelArgument statelessConstantMemTok = {};
    statelessConstantMemTok.Token = iOpenCL::PATCH_TOKEN::PATCH_TOKEN_STATELESS_CONSTANT_MEMORY_OBJECT_KERNEL_ARGUMENT;
    statelessConstantMemTok.Size = sizeof(statelessConstantMemTok);
    statelessConstantMemTok.ArgumentNumber = 5;

    iOpenCL::SPatchStatelessDeviceQueueKernelArgument statelessDeviceQueueTok = {};
    statelessDeviceQueueTok.Token = iOpenCL::PATCH_TOKEN::PATCH_TOKEN_STATELESS_DEVICE_QUEUE_KERNEL_ARGUMENT;
    statelessDeviceQueueTok.Size = sizeof(iOpenCL::SPatchGlobalMemoryObjectKernelArgument);
    statelessDeviceQueueTok.ArgumentNumber = 4;

    auto samplerOff = pushBackToken(samplerTok, storage);
    auto imageOff = pushBackToken(imageTok, storage);
    auto globalMemOff = pushBackToken(globalMemTok, storage);
    auto statelessGlobalMemOff = pushBackToken(statelessGlobalMemTok, storage);
    auto statelessConstantMemOff = pushBackToken(statelessConstantMemTok, storage);
    auto statelessDeviceQueueOff = pushBackToken(statelessDeviceQueueTok, storage);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());

    auto base = storage.data();
    ASSERT_EQ(6U, decodedKernel.tokens.kernelArgs.size());
    EXPECT_TRUE(tokenOffsetMatched(base, samplerOff, decodedKernel.tokens.kernelArgs[samplerTok.ArgumentNumber].objectArg));
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectType::Sampler, decodedKernel.tokens.kernelArgs[samplerTok.ArgumentNumber].objectType);
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[samplerTok.ArgumentNumber].objectTypeSpecialized);

    EXPECT_TRUE(tokenOffsetMatched(base, imageOff, decodedKernel.tokens.kernelArgs[imageTok.ArgumentNumber].objectArg));
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectType::Image, decodedKernel.tokens.kernelArgs[imageTok.ArgumentNumber].objectType);
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[imageTok.ArgumentNumber].objectTypeSpecialized);

    EXPECT_TRUE(tokenOffsetMatched(base, globalMemOff, decodedKernel.tokens.kernelArgs[globalMemTok.ArgumentNumber].objectArg));
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectType::Buffer, decodedKernel.tokens.kernelArgs[globalMemTok.ArgumentNumber].objectType);
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[globalMemTok.ArgumentNumber].objectTypeSpecialized);

    EXPECT_TRUE(tokenOffsetMatched(base, statelessGlobalMemOff, decodedKernel.tokens.kernelArgs[statelessGlobalMemTok.ArgumentNumber].objectArg));
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectType::Buffer, decodedKernel.tokens.kernelArgs[statelessGlobalMemTok.ArgumentNumber].objectType);
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[statelessGlobalMemTok.ArgumentNumber].objectTypeSpecialized);

    EXPECT_TRUE(tokenOffsetMatched(base, statelessConstantMemOff, decodedKernel.tokens.kernelArgs[statelessConstantMemTok.ArgumentNumber].objectArg));
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectType::Buffer, decodedKernel.tokens.kernelArgs[statelessConstantMemTok.ArgumentNumber].objectType);
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[statelessConstantMemTok.ArgumentNumber].objectTypeSpecialized);

    EXPECT_TRUE(tokenOffsetMatched(base, statelessDeviceQueueOff, decodedKernel.tokens.kernelArgs[statelessDeviceQueueTok.ArgumentNumber].objectArg));
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectType::Buffer, decodedKernel.tokens.kernelArgs[statelessDeviceQueueTok.ArgumentNumber].objectType);
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[statelessDeviceQueueTok.ArgumentNumber].objectTypeSpecialized);

    for (int i = 0; i < 6; ++i) {
        EXPECT_EQ(nullptr, decodedKernel.tokens.kernelArgs[i].argInfo);
        EXPECT_EQ(0U, decodedKernel.tokens.kernelArgs[i].byValMap.size());
        EXPECT_EQ(nullptr, decodedKernel.tokens.kernelArgs[i].objectId);
        EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[i].objectTypeSpecialized);
    }
}

TEST(KernelDecoder, GivenKernelWithValidNonArgCrossThreadDataPatchtokensThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    using namespace iOpenCL;

    std::vector<uint8_t> storage;
    storage.reserve(2048);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());
    auto localWorkSize0Off = pushBackDataParameterToken(DATA_PARAMETER_LOCAL_WORK_SIZE, storage, 0U);
    auto localWorkSize20Off = pushBackDataParameterToken(DATA_PARAMETER_LOCAL_WORK_SIZE, storage, 0U);
    auto localWorkSize1Off = pushBackDataParameterToken(DATA_PARAMETER_LOCAL_WORK_SIZE, storage, 1U);
    auto localWorkSize2Off = pushBackDataParameterToken(DATA_PARAMETER_LOCAL_WORK_SIZE, storage, 2U);
    auto localWorkSize21Off = pushBackDataParameterToken(DATA_PARAMETER_LOCAL_WORK_SIZE, storage, 1U);
    auto localWorkSize22Off = pushBackDataParameterToken(DATA_PARAMETER_LOCAL_WORK_SIZE, storage, 2U);
    auto globalWorkOffset0Off = pushBackDataParameterToken(DATA_PARAMETER_GLOBAL_WORK_OFFSET, storage, 0U);
    auto globalWorkOffset1Off = pushBackDataParameterToken(DATA_PARAMETER_GLOBAL_WORK_OFFSET, storage, 1U);
    auto globalWorkOffset2Off = pushBackDataParameterToken(DATA_PARAMETER_GLOBAL_WORK_OFFSET, storage, 2U);
    auto enqueuedLocalWorkSize0Off = pushBackDataParameterToken(DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE, storage, 0U);
    auto enqueuedLocalWorkSize1Off = pushBackDataParameterToken(DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE, storage, 1U);
    auto enqueuedLocalWorkSize2Off = pushBackDataParameterToken(DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE, storage, 2U);
    auto globalWorkSize0Off = pushBackDataParameterToken(DATA_PARAMETER_GLOBAL_WORK_SIZE, storage, 0U);
    auto globalWorkSize1Off = pushBackDataParameterToken(DATA_PARAMETER_GLOBAL_WORK_SIZE, storage, 1U);
    auto globalWorkSize2Off = pushBackDataParameterToken(DATA_PARAMETER_GLOBAL_WORK_SIZE, storage, 2U);
    auto numWorkGroups0Off = pushBackDataParameterToken(DATA_PARAMETER_NUM_WORK_GROUPS, storage, 0U);
    auto numWorkGroups1Off = pushBackDataParameterToken(DATA_PARAMETER_NUM_WORK_GROUPS, storage, 1U);
    auto numWorkGroups2Off = pushBackDataParameterToken(DATA_PARAMETER_NUM_WORK_GROUPS, storage, 2U);
    auto maxWorkGroupsOff = pushBackDataParameterToken(DATA_PARAMETER_MAX_WORKGROUP_SIZE, storage);
    auto workDimensionsOff = pushBackDataParameterToken(DATA_PARAMETER_WORK_DIMENSIONS, storage);
    auto simdSizeOff = pushBackDataParameterToken(DATA_PARAMETER_SIMD_SIZE, storage);
    auto privateMemoryStatelessSizeOff = pushBackDataParameterToken(DATA_PARAMETER_PRIVATE_MEMORY_STATELESS_SIZE, storage);
    auto localMemoryStatelessWindowSizeOff = pushBackDataParameterToken(DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_SIZE, storage);
    auto localMemoryStatelessWindowStartAddrOff = pushBackDataParameterToken(DATA_PARAMETER_LOCAL_MEMORY_STATELESS_WINDOW_START_ADDRESS, storage);
    auto parentEventOff = pushBackDataParameterToken(DATA_PARAMETER_PARENT_EVENT, storage);
    auto preferredWorkgroupMultipleOff = pushBackDataParameterToken(DATA_PARAMETER_PREFERRED_WORKGROUP_MULTIPLE, storage);
    auto childBlockSimdSize0Off = pushBackDataParameterToken(DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE, storage);
    auto childBlockSimdSize1Off = pushBackDataParameterToken(DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE, storage);
    auto childBlockSimdSize2Off = pushBackDataParameterToken(DATA_PARAMETER_CHILD_BLOCK_SIMD_SIZE, storage);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());

    auto base = storage.data();
    EXPECT_TRUE(tokenOffsetMatched(base, localWorkSize0Off, decodedKernel.tokens.crossThreadPayloadArgs.localWorkSize[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, localWorkSize20Off, decodedKernel.tokens.crossThreadPayloadArgs.localWorkSize2[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, localWorkSize1Off, decodedKernel.tokens.crossThreadPayloadArgs.localWorkSize[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, localWorkSize2Off, decodedKernel.tokens.crossThreadPayloadArgs.localWorkSize[2]));
    EXPECT_TRUE(tokenOffsetMatched(base, localWorkSize21Off, decodedKernel.tokens.crossThreadPayloadArgs.localWorkSize2[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, localWorkSize22Off, decodedKernel.tokens.crossThreadPayloadArgs.localWorkSize2[2]));
    EXPECT_TRUE(tokenOffsetMatched(base, globalWorkOffset0Off, decodedKernel.tokens.crossThreadPayloadArgs.globalWorkOffset[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, globalWorkOffset1Off, decodedKernel.tokens.crossThreadPayloadArgs.globalWorkOffset[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, globalWorkOffset2Off, decodedKernel.tokens.crossThreadPayloadArgs.globalWorkOffset[2]));
    EXPECT_TRUE(tokenOffsetMatched(base, enqueuedLocalWorkSize0Off, decodedKernel.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, enqueuedLocalWorkSize1Off, decodedKernel.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, enqueuedLocalWorkSize2Off, decodedKernel.tokens.crossThreadPayloadArgs.enqueuedLocalWorkSize[2]));
    EXPECT_TRUE(tokenOffsetMatched(base, globalWorkSize0Off, decodedKernel.tokens.crossThreadPayloadArgs.globalWorkSize[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, globalWorkSize1Off, decodedKernel.tokens.crossThreadPayloadArgs.globalWorkSize[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, globalWorkSize2Off, decodedKernel.tokens.crossThreadPayloadArgs.globalWorkSize[2]));
    EXPECT_TRUE(tokenOffsetMatched(base, numWorkGroups0Off, decodedKernel.tokens.crossThreadPayloadArgs.numWorkGroups[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, numWorkGroups1Off, decodedKernel.tokens.crossThreadPayloadArgs.numWorkGroups[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, numWorkGroups2Off, decodedKernel.tokens.crossThreadPayloadArgs.numWorkGroups[2]));
    EXPECT_TRUE(tokenOffsetMatched(base, maxWorkGroupsOff, decodedKernel.tokens.crossThreadPayloadArgs.maxWorkGroupSize));
    EXPECT_TRUE(tokenOffsetMatched(base, workDimensionsOff, decodedKernel.tokens.crossThreadPayloadArgs.workDimensions));
    EXPECT_TRUE(tokenOffsetMatched(base, simdSizeOff, decodedKernel.tokens.crossThreadPayloadArgs.simdSize));
    EXPECT_TRUE(tokenOffsetMatched(base, privateMemoryStatelessSizeOff, decodedKernel.tokens.crossThreadPayloadArgs.privateMemoryStatelessSize));
    EXPECT_TRUE(tokenOffsetMatched(base, localMemoryStatelessWindowSizeOff, decodedKernel.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowSize));
    EXPECT_TRUE(tokenOffsetMatched(base, localMemoryStatelessWindowStartAddrOff, decodedKernel.tokens.crossThreadPayloadArgs.localMemoryStatelessWindowStartAddress));
    EXPECT_TRUE(tokenOffsetMatched(base, parentEventOff, decodedKernel.tokens.crossThreadPayloadArgs.parentEvent));
    EXPECT_TRUE(tokenOffsetMatched(base, preferredWorkgroupMultipleOff, decodedKernel.tokens.crossThreadPayloadArgs.preferredWorkgroupMultiple));
    ASSERT_EQ(3U, decodedKernel.tokens.crossThreadPayloadArgs.childBlockSimdSize.size());
    EXPECT_TRUE(tokenOffsetMatched(base, childBlockSimdSize0Off, decodedKernel.tokens.crossThreadPayloadArgs.childBlockSimdSize[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, childBlockSimdSize1Off, decodedKernel.tokens.crossThreadPayloadArgs.childBlockSimdSize[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, childBlockSimdSize2Off, decodedKernel.tokens.crossThreadPayloadArgs.childBlockSimdSize[2]));
}

TEST(KernelDecoder, GivenKernelWithArgCrossThreadDataPatchtokensWhenSourceIndexIsGreaterThan2ThenThenDecodingSucceedsButTokenIsMarkedAsUnhandled) {
    std::vector<uint8_t> storage;
    storage.reserve(128);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());

    auto localWorkSize3Off = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_LOCAL_WORK_SIZE, storage, 3U);
    auto globalWorkOffset3Off = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_GLOBAL_WORK_OFFSET, storage, 3U);
    auto enqueuedLocalWorkSize3Off = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE, storage, 3U);
    auto globalWorkSize3Off = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_GLOBAL_WORK_SIZE, storage, 3U);
    auto numWorkGroups3Off = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_NUM_WORK_GROUPS, storage, 3U);
    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    ASSERT_EQ(5U, decodedKernel.unhandledTokens.size());

    auto base = storage.data();
    EXPECT_TRUE(tokenOffsetMatched(base, localWorkSize3Off, decodedKernel.unhandledTokens[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, globalWorkOffset3Off, decodedKernel.unhandledTokens[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, enqueuedLocalWorkSize3Off, decodedKernel.unhandledTokens[2]));
    EXPECT_TRUE(tokenOffsetMatched(base, globalWorkSize3Off, decodedKernel.unhandledTokens[3]));
    EXPECT_TRUE(tokenOffsetMatched(base, numWorkGroups3Off, decodedKernel.unhandledTokens[4]));
}

TEST(KernelDecoder, GivenKernelWithUnkownPatchtokensThenDecodingSucceedsButTokenIsMarkedAsUnhandled) {
    std::vector<uint8_t> storage;
    storage.reserve(128);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());
    auto unknownTokOff = pushBackToken<iOpenCL::SPatchItemHeader>(iOpenCL::NUM_PATCH_TOKENS, storage);
    auto unknownCrossThreadTokOff = pushBackDataParameterToken(iOpenCL::NUM_DATA_PARAMETER_TOKENS, storage);
    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    ASSERT_EQ(2U, decodedKernel.unhandledTokens.size());

    auto base = storage.data();
    EXPECT_TRUE(tokenOffsetMatched(base, unknownTokOff, decodedKernel.unhandledTokens[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, unknownCrossThreadTokOff, decodedKernel.unhandledTokens[1]));
}

TEST(KernelDecoder, GivenKernelWithValidObjectArgMetadataPatchtokensThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    std::vector<uint8_t> storage;
    storage.reserve(1024);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());

    auto arg0ObjectIdOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_OBJECT_ID, storage, 0U, 0U);
    auto arg0BufferOffsetOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_BUFFER_OFFSET, storage, 0U, 0U);
    auto arg0BufferStatefulOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL, storage, 0U, 0U);

    auto arg1ObjectIdOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_OBJECT_ID, storage, 0U, 1U);
    auto arg1ImageWidthOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_IMAGE_WIDTH, storage, 0U, 1U);
    auto arg1ImageHeightOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_IMAGE_HEIGHT, storage, 0U, 1U);
    auto arg1ImageDepthOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_IMAGE_DEPTH, storage, 0U, 1U);
    auto arg1ImageChannelDataTypeOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE, storage, 0U, 1U);
    auto arg1ImageChannelOrderOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_IMAGE_CHANNEL_ORDER, storage, 0U, 1U);
    auto arg1ImageArraySizeOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_IMAGE_ARRAY_SIZE, storage, 0U, 1U);
    auto arg1ImageNumSamplesOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_IMAGE_NUM_SAMPLES, storage, 0U, 1U);
    auto arg1ImageNumMipLevelOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS, storage, 0U, 1U);

    auto arg2SamplerCoordinateSnapWaRequiredOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED, storage, 0U, 2U);
    auto arg2SamplerAddressModeOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_SAMPLER_ADDRESS_MODE, storage, 0U, 2U);
    auto arg2SamplerNormalizedCoordsOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS, storage, 0U, 2U);

    auto arg3SlmTokenOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES, storage, 0U, 3U);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());

    ASSERT_EQ(4U, decodedKernel.tokens.kernelArgs.size());
    ASSERT_EQ(NEO::PatchTokenBinary::ArgObjectType::Buffer, decodedKernel.tokens.kernelArgs[0].objectType);
    ASSERT_EQ(NEO::PatchTokenBinary::ArgObjectType::Image, decodedKernel.tokens.kernelArgs[1].objectType);
    ASSERT_EQ(NEO::PatchTokenBinary::ArgObjectType::Sampler, decodedKernel.tokens.kernelArgs[2].objectType);
    ASSERT_EQ(NEO::PatchTokenBinary::ArgObjectType::Slm, decodedKernel.tokens.kernelArgs[3].objectType);

    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[0].objectTypeSpecialized);
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[1].objectTypeSpecialized);
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[2].objectTypeSpecialized);
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::None, decodedKernel.tokens.kernelArgs[3].objectTypeSpecialized);

    auto base = storage.data();
    EXPECT_TRUE(tokenOffsetMatched(base, arg0ObjectIdOff, decodedKernel.tokens.kernelArgs[0].objectId));
    EXPECT_TRUE(tokenOffsetMatched(base, arg0BufferOffsetOff, decodedKernel.tokens.kernelArgs[0].metadata.buffer.bufferOffset));
    EXPECT_TRUE(tokenOffsetMatched(base, arg0BufferStatefulOff, decodedKernel.tokens.kernelArgs[0].metadata.buffer.pureStateful));

    EXPECT_TRUE(tokenOffsetMatched(base, arg1ObjectIdOff, decodedKernel.tokens.kernelArgs[1].objectId));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1ImageWidthOff, decodedKernel.tokens.kernelArgs[1].metadata.image.width));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1ImageHeightOff, decodedKernel.tokens.kernelArgs[1].metadata.image.height));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1ImageDepthOff, decodedKernel.tokens.kernelArgs[1].metadata.image.depth));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1ImageChannelDataTypeOff, decodedKernel.tokens.kernelArgs[1].metadata.image.channelDataType));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1ImageChannelOrderOff, decodedKernel.tokens.kernelArgs[1].metadata.image.channelOrder));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1ImageArraySizeOff, decodedKernel.tokens.kernelArgs[1].metadata.image.arraySize));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1ImageNumSamplesOff, decodedKernel.tokens.kernelArgs[1].metadata.image.numSamples));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1ImageNumMipLevelOff, decodedKernel.tokens.kernelArgs[1].metadata.image.numMipLevels));

    EXPECT_TRUE(tokenOffsetMatched(base, arg2SamplerCoordinateSnapWaRequiredOff, decodedKernel.tokens.kernelArgs[2].metadata.sampler.coordinateSnapWaRequired));
    EXPECT_TRUE(tokenOffsetMatched(base, arg2SamplerAddressModeOff, decodedKernel.tokens.kernelArgs[2].metadata.sampler.addressMode));
    EXPECT_TRUE(tokenOffsetMatched(base, arg2SamplerNormalizedCoordsOff, decodedKernel.tokens.kernelArgs[2].metadata.sampler.normalizedCoords));

    EXPECT_TRUE(tokenOffsetMatched(base, arg3SlmTokenOff, decodedKernel.tokens.kernelArgs[3].metadata.slm.token));
}

TEST(KernelDecoder, GivenKernelWithMismatchedArgMetadataPatchtokensThenDecodingFails) {
    std::vector<uint8_t> storage;
    storage.reserve(128);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());

    auto arg0Metadata0Off = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL, storage, 0U, 0U);
    auto arg0Metadata1Off = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_BUFFER_OFFSET, storage, 0U, 0U);

    iOpenCL::SPatchDataParameterBuffer *arg0Metadata0 = reinterpret_cast<iOpenCL::SPatchDataParameterBuffer *>(storage.data() + arg0Metadata0Off);
    iOpenCL::SPatchDataParameterBuffer *arg0Metadata1 = reinterpret_cast<iOpenCL::SPatchDataParameterBuffer *>(storage.data() + arg0Metadata1Off);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_IMAGE_WIDTH;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_SAMPLER_ADDRESS_MODE;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_IMAGE_WIDTH;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_IMAGE_WIDTH;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_SAMPLER_ADDRESS_MODE;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_IMAGE_WIDTH;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_SAMPLER_ADDRESS_MODE;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_SAMPLER_ADDRESS_MODE;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_IMAGE_WIDTH;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_SAMPLER_ADDRESS_MODE;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_IMAGE_WIDTH;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_SAMPLER_ADDRESS_MODE;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);

    decodedKernel = {};
    arg0Metadata0->Type = iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES;
    arg0Metadata1->Type = iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL;
    decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
}

TEST(KernelDecoder, GivenKernelWithMismatchedArgMetadataPatchtokensThenDecodingFailsAndStops) {
    std::vector<uint8_t> storage;
    storage.reserve(128);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());

    pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_BUFFER_STATEFUL, storage, 0U, 0U);
    pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_IMAGE_DEPTH, storage, 0U, 0U);
    auto unhandledTokenAfterInvalidOff = pushBackDataParameterToken(iOpenCL::NUM_DATA_PARAMETER_TOKENS, storage, 0U, 0U);
    (void)unhandledTokenAfterInvalidOff;

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());
}

TEST(KernelDecoder, GivenKernelWithByValArgMetadataPatchtokensThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    std::vector<uint8_t> storage;
    storage.reserve(128);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());

    auto arg0Val0Off = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_KERNEL_ARGUMENT, storage, 0U, 0U);
    auto arg0Val1Off = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_KERNEL_ARGUMENT, storage, 0U, 0U);
    auto arg1SlmOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES, storage, 0U, 1U);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());

    ASSERT_EQ(2U, decodedKernel.tokens.kernelArgs.size());
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectType::None, decodedKernel.tokens.kernelArgs[0].objectType);
    ASSERT_EQ(NEO::PatchTokenBinary::ArgObjectType::Slm, decodedKernel.tokens.kernelArgs[1].objectType);

    ASSERT_EQ(2U, decodedKernel.tokens.kernelArgs[0].byValMap.size());
    ASSERT_EQ(1U, decodedKernel.tokens.kernelArgs[1].byValMap.size());
    auto base = storage.data();
    EXPECT_TRUE(tokenOffsetMatched(base, arg0Val0Off, decodedKernel.tokens.kernelArgs[0].byValMap[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, arg0Val1Off, decodedKernel.tokens.kernelArgs[0].byValMap[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1SlmOff, decodedKernel.tokens.kernelArgs[1].metadata.slm.token));
    EXPECT_TRUE(tokenOffsetMatched(base, arg1SlmOff, decodedKernel.tokens.kernelArgs[1].byValMap[0]));
}

TEST(KernelDecoder, GivenKernelWithVmeMetadataPatchtokensThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    std::vector<uint8_t> storage;
    storage.reserve(128);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());

    auto arg0VmeBlockTypeOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_VME_MB_BLOCK_TYPE, storage, 0U, 0U);
    auto arg0VmeSubpixelModeOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_VME_SUBPIXEL_MODE, storage, 0U, 0U);
    auto arg0VmeSadAdjustModeOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_VME_SAD_ADJUST_MODE, storage, 0U, 0U);
    auto arg0VmeSearchPathTypeOff = pushBackDataParameterToken(iOpenCL::DATA_PARAMETER_VME_SEARCH_PATH_TYPE, storage, 0U, 0U);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());

    ASSERT_EQ(1U, decodedKernel.tokens.kernelArgs.size());
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectType::None, decodedKernel.tokens.kernelArgs[0].objectType);
    EXPECT_EQ(NEO::PatchTokenBinary::ArgObjectTypeSpecialized::Vme, decodedKernel.tokens.kernelArgs[0].objectTypeSpecialized);
    ;

    auto base = storage.data();
    EXPECT_TRUE(tokenOffsetMatched(base, arg0VmeBlockTypeOff, decodedKernel.tokens.kernelArgs[0].metadataSpecialized.vme.mbBlockType));
    EXPECT_TRUE(tokenOffsetMatched(base, arg0VmeSubpixelModeOff, decodedKernel.tokens.kernelArgs[0].metadataSpecialized.vme.subpixelMode));
    EXPECT_TRUE(tokenOffsetMatched(base, arg0VmeSadAdjustModeOff, decodedKernel.tokens.kernelArgs[0].metadataSpecialized.vme.sadAdjustMode));
    EXPECT_TRUE(tokenOffsetMatched(base, arg0VmeSearchPathTypeOff, decodedKernel.tokens.kernelArgs[0].metadataSpecialized.vme.searchPathType));
}

TEST(KernelDecoder, GivenKernelWithOutOfBoundsTokenThenDecodingFails) {
    std::vector<uint8_t> storage;
    storage.reserve(128);
    auto kernelToEncode = PatchTokensTestData::ValidEmptyKernel::create(storage);

    auto patchListOffset = static_cast<uint32_t>(storage.size());
    pushBackToken<iOpenCL::SPatchSamplerStateArray>(iOpenCL::PATCH_TOKEN_SAMPLER_STATE_ARRAY, storage);

    ASSERT_EQ(storage.data(), kernelToEncode.blobs.kernelInfo.begin());
    auto kernelHeader = reinterpret_cast<iOpenCL::SKernelBinaryHeaderCommon *>(storage.data());
    kernelHeader->PatchListSize = static_cast<uint32_t>(storage.size()) - patchListOffset;
    kernelHeader->PatchListSize -= 1;

    NEO::PatchTokenBinary::KernelFromPatchtokens decodedKernel;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeKernelFromPatchtokensBlob(storage, decodedKernel);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedKernel.decodeStatus);
}

TEST(ProgramDecoder, GivenValidEmptyProgramThenDecodingOfHeaderSucceeds) {
    std::vector<uint8_t> storage;
    PatchTokensTestData::ValidEmptyProgram programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedProgram.decodeStatus);
    EXPECT_TRUE(decodedProgram.unhandledTokens.empty());
    ASSERT_NE(nullptr, decodedProgram.header);
    EXPECT_EQ(programToEncode.header, decodedProgram.header);
    EXPECT_EQ(programToEncode.blobs.programInfo, decodedProgram.blobs.programInfo);
    EXPECT_TRUE(decodedProgram.blobs.kernelsInfo.empty());
    EXPECT_TRUE(decodedProgram.blobs.patchList.empty());
    EXPECT_TRUE(decodedProgram.kernels.empty());
    EXPECT_TRUE(hasEmptyTokensInfo(decodedProgram));
}

TEST(ProgramDecoder, GivenProgramWhenBlobSmallerThanProgramHeaderThenDecodingFails) {
    PatchTokensTestData::ValidEmptyProgram programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    auto brokenBlob = ArrayRef<const uint8_t>(programToEncode.blobs.programInfo.begin(),
                                              programToEncode.blobs.programInfo.begin() + sizeof(iOpenCL::SProgramBinaryHeader) - 1);
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(brokenBlob, decodedProgram);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedProgram.decodeStatus);
}

TEST(ProgramDecoder, GivenProgramWithInvaidProgramMagicThenDecodingFails) {
    PatchTokensTestData::ValidEmptyProgram programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    programToEncode.headerMutable->Magic += 1;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedProgram.decodeStatus);
}

TEST(ProgramDecoder, GivenProgramWhenBlobDoesntHaveEnoughSpaceForPatchListThenDecodingFails) {
    PatchTokensTestData::ValidEmptyProgram programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    programToEncode.headerMutable->PatchListSize = static_cast<uint32_t>(programToEncode.blobs.patchList.size() + 1);
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedProgram.decodeStatus);
}

TEST(ProgramDecoder, GivenValidProgramWithConstantSurfacesThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    PatchTokensTestData::ValidProgramWithConstantSurface programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedProgram.decodeStatus);
    EXPECT_TRUE(decodedProgram.unhandledTokens.empty());
    ASSERT_EQ(1U, decodedProgram.programScopeTokens.allocateConstantMemorySurface.size());
    EXPECT_EQ(programToEncode.programScopeTokens.allocateConstantMemorySurface[0], decodedProgram.programScopeTokens.allocateConstantMemorySurface[0]);

    decodedProgram = {};
    auto inlineSize = programToEncode.programScopeTokens.allocateConstantMemorySurface[0]->InlineDataSize;
    auto secondConstantSurfaceOff = programToEncode.storage.size();
    programToEncode.storage.insert(programToEncode.storage.end(), reinterpret_cast<uint8_t *>(programToEncode.constSurfMutable),
                                   reinterpret_cast<uint8_t *>(programToEncode.constSurfMutable + 1));
    programToEncode.storage.resize(programToEncode.storage.size() + inlineSize);
    programToEncode.recalcTokPtr();
    decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedProgram.decodeStatus);
    EXPECT_TRUE(decodedProgram.unhandledTokens.empty());

    auto base = programToEncode.storage.data();
    ASSERT_EQ(2U, decodedProgram.programScopeTokens.allocateConstantMemorySurface.size());
    EXPECT_EQ(programToEncode.programScopeTokens.allocateConstantMemorySurface[0], decodedProgram.programScopeTokens.allocateConstantMemorySurface[0]);
    EXPECT_TRUE(tokenOffsetMatched(base, secondConstantSurfaceOff, decodedProgram.programScopeTokens.allocateConstantMemorySurface[1]));
}

TEST(ProgramDecoder, GivenProgramWithConstantSurfaceWhenBlobSmallerThanNeededForInlineDataThenDecodingFails) {
    PatchTokensTestData::ValidProgramWithConstantSurface programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    programToEncode.headerMutable->PatchListSize -= 1;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedProgram.decodeStatus);
}

TEST(ProgramDecoder, GivenValidProgramWithGlobalSurfacesThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    PatchTokensTestData::ValidProgramWithGlobalSurface programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedProgram.decodeStatus);
    EXPECT_TRUE(decodedProgram.unhandledTokens.empty());
    ASSERT_EQ(1U, decodedProgram.programScopeTokens.allocateGlobalMemorySurface.size());
    EXPECT_EQ(programToEncode.programScopeTokens.allocateGlobalMemorySurface[0], decodedProgram.programScopeTokens.allocateGlobalMemorySurface[0]);

    decodedProgram = {};
    auto inlineSize = programToEncode.programScopeTokens.allocateGlobalMemorySurface[0]->InlineDataSize;
    auto secondGlobalSurfaceOff = programToEncode.storage.size();
    programToEncode.storage.insert(programToEncode.storage.end(), reinterpret_cast<uint8_t *>(programToEncode.globalSurfMutable),
                                   reinterpret_cast<uint8_t *>(programToEncode.globalSurfMutable + 1));
    programToEncode.storage.resize(programToEncode.storage.size() + inlineSize);
    programToEncode.recalcTokPtr();
    decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedProgram.decodeStatus);
    EXPECT_TRUE(decodedProgram.unhandledTokens.empty());

    auto base = programToEncode.storage.data();
    ASSERT_EQ(2U, decodedProgram.programScopeTokens.allocateGlobalMemorySurface.size());
    EXPECT_EQ(programToEncode.programScopeTokens.allocateGlobalMemorySurface[0], decodedProgram.programScopeTokens.allocateGlobalMemorySurface[0]);
    EXPECT_TRUE(tokenOffsetMatched(base, secondGlobalSurfaceOff, decodedProgram.programScopeTokens.allocateGlobalMemorySurface[1]));
}

TEST(ProgramDecoder, GivenProgramWithGlobalSurfaceWhenBlobSmallerThanNeededForInlineDataThenDecodingFails) {
    PatchTokensTestData::ValidProgramWithGlobalSurface programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    programToEncode.headerMutable->PatchListSize -= 1;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedProgram.decodeStatus);
}

TEST(ProgramDecoder, GivenValidProgramWithPatchtokensThenDecodingSucceedsAndTokensAreProperlyAssinged) {
    using namespace iOpenCL;

    PatchTokensTestData::ValidProgramWithConstantSurfaceAndPointer programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    auto constPointer1Off = pushBackToken<SPatchConstantPointerProgramBinaryInfo>(PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO, programToEncode.storage);
    auto constPointer2Off = pushBackToken<SPatchConstantPointerProgramBinaryInfo>(PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO, programToEncode.storage);
    auto globalPointer0Off = pushBackToken<SPatchGlobalPointerProgramBinaryInfo>(PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO, programToEncode.storage);
    auto globalPointer1Off = pushBackToken<SPatchGlobalPointerProgramBinaryInfo>(PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO, programToEncode.storage);
    auto symbolTableOff = pushBackToken<SPatchFunctionTableInfo>(PATCH_TOKEN_PROGRAM_SYMBOL_TABLE, programToEncode.storage);
    programToEncode.recalcTokPtr();
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedProgram.decodeStatus);
    EXPECT_TRUE(decodedProgram.unhandledTokens.empty());
    auto base = programToEncode.storage.data();

    EXPECT_EQ(1U, programToEncode.programScopeTokens.constantPointer.size());
    ASSERT_EQ(3U, decodedProgram.programScopeTokens.constantPointer.size());
    ASSERT_EQ(2U, decodedProgram.programScopeTokens.globalPointer.size());
    EXPECT_TRUE(tokenOffsetMatched(base, constPointer1Off, decodedProgram.programScopeTokens.constantPointer[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, constPointer2Off, decodedProgram.programScopeTokens.constantPointer[2]));
    EXPECT_TRUE(tokenOffsetMatched(base, globalPointer0Off, decodedProgram.programScopeTokens.globalPointer[0]));
    EXPECT_TRUE(tokenOffsetMatched(base, globalPointer1Off, decodedProgram.programScopeTokens.globalPointer[1]));
    EXPECT_TRUE(tokenOffsetMatched(base, symbolTableOff, decodedProgram.programScopeTokens.symbolTable));
}

TEST(ProgramDecoder, GivenProgramWithUnkownPatchtokensThenDecodingSucceedsButTokenIsMarkedAsUnhandled) {
    PatchTokensTestData::ValidProgramWithConstantSurface programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;

    programToEncode.constSurfMutable->Token = iOpenCL::NUM_PATCH_TOKENS;
    programToEncode.constSurfMutable->Size += programToEncode.constSurfMutable->InlineDataSize;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedProgram.decodeStatus);
    ASSERT_EQ(1U, decodedProgram.unhandledTokens.size());
    EXPECT_EQ(programToEncode.constSurfMutable, decodedProgram.unhandledTokens[0]);
}

TEST(ProgramDecoder, GivenValidProgramWithKernelThenDecodingSucceedsAndTokensAreProperlyAssigned) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm programToEncode;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.blobs.programInfo, decodedProgram);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedProgram.decodeStatus);
    EXPECT_TRUE(decodedProgram.unhandledTokens.empty());
    ASSERT_EQ(1U, decodedProgram.header->NumberOfKernels);
    ASSERT_EQ(1U, decodedProgram.kernels.size());
    auto decodedKernel = decodedProgram.kernels[0];
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel.decodeStatus);
    EXPECT_TRUE(decodedKernel.unhandledTokens.empty());
    EXPECT_NE(nullptr, decodedKernel.tokens.allocateLocalSurface);
}

TEST(ProgramDecoder, GivenValidProgramWithTwoKernelsWhenThenDecodingSucceeds) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm programToEncode;
    programToEncode.headerMutable->NumberOfKernels = 2;
    programToEncode.storage.insert(programToEncode.storage.end(), programToEncode.kernels[0].blobs.kernelInfo.begin(), programToEncode.kernels[0].blobs.kernelInfo.end());
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.storage, decodedProgram);
    EXPECT_TRUE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedProgram.decodeStatus);
    EXPECT_TRUE(decodedProgram.unhandledTokens.empty());
    EXPECT_EQ(2U, decodedProgram.header->NumberOfKernels);
    ASSERT_EQ(2U, decodedProgram.kernels.size());

    auto decodedKernel0 = decodedProgram.kernels[0];
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel0.decodeStatus);
    EXPECT_TRUE(decodedKernel0.unhandledTokens.empty());
    EXPECT_NE(nullptr, decodedKernel0.tokens.allocateLocalSurface);

    auto decodedKernel1 = decodedProgram.kernels[0];
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::Success, decodedKernel1.decodeStatus);
    EXPECT_TRUE(decodedKernel1.unhandledTokens.empty());
    EXPECT_NE(nullptr, decodedKernel1.tokens.allocateLocalSurface);
}

TEST(ProgramDecoder, GivenPatchTokenWithZeroSizeThenDecodingFailsAndStops) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm programToEncode;
    programToEncode.slmMutable->Size = 0U;
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.storage, decodedProgram);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedProgram.decodeStatus);
}

TEST(ProgramDecoder, GivenProgramWithMultipleKernelsWhenFailsToDecodeKernelThenDecodingFailsAndStops) {
    PatchTokensTestData::ValidProgramWithKernelUsingSlm programToEncode;
    programToEncode.slmMutable->Size = 0U;
    programToEncode.headerMutable->NumberOfKernels = 2;
    programToEncode.storage.insert(programToEncode.storage.end(), programToEncode.kernels[0].blobs.kernelInfo.begin(), programToEncode.kernels[0].blobs.kernelInfo.end());
    NEO::PatchTokenBinary::ProgramFromPatchtokens decodedProgram;
    bool decodeSuccess = NEO::PatchTokenBinary::decodeProgramFromPatchtokensBlob(programToEncode.storage, decodedProgram);
    EXPECT_FALSE(decodeSuccess);
    EXPECT_EQ(NEO::PatchTokenBinary::DecoderError::InvalidBinary, decodedProgram.decodeStatus);
    EXPECT_TRUE(decodedProgram.unhandledTokens.empty());
    EXPECT_EQ(2U, decodedProgram.header->NumberOfKernels);
    EXPECT_EQ(1U, decodedProgram.kernels.size());
}
