/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/aub_tests/command_stream/copy_engine_aub_tests_xehp_and_later.h"

using namespace NEO;

template <uint32_t numTiles, typename FamilyType, bool useLocalMemory = true>
struct CopyEnginesXeHpcFixture : public CopyEngineXeHPAndLater<numTiles, useLocalMemory>, public ::testing::WithParamInterface<uint32_t /* EngineType */> {
    using MEM_COPY = typename FamilyType::MEM_COPY;

    void SetUp() override {
        this->bcsEngineType = static_cast<aub_stream::EngineType>(GetParam());
        CopyEngineXeHPAndLater<numTiles, useLocalMemory>::SetUp();
    }
};
constexpr uint32_t allSupportedCopyEngines[] = {
    aub_stream::EngineType::ENGINE_BCS,
    aub_stream::EngineType::ENGINE_BCS1,
    aub_stream::EngineType::ENGINE_BCS2,
    aub_stream::EngineType::ENGINE_BCS3,
    aub_stream::EngineType::ENGINE_BCS4,
    aub_stream::EngineType::ENGINE_BCS5,
    aub_stream::EngineType::ENGINE_BCS6,
    aub_stream::EngineType::ENGINE_BCS7,
    aub_stream::EngineType::ENGINE_BCS8,
};

using OneTileXeHpcTests = CopyEnginesXeHpcFixture<1, XeHpcCoreFamily>;

INSTANTIATE_TEST_CASE_P(
    MemCopyBcsCmd,
    OneTileXeHpcTests,
    testing::ValuesIn(allSupportedCopyEngines));

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolve) {
    givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolveImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopied) {
    givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenOffsetsWhenBltExecutedThenCopiedDataIsValid) {
    givenOffsetsWhenBltExecutedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompress) {
    givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompressImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenCopyBufferRectWithBigSizesWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenCopyBufferRectWithBigSizesWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

using OneTileSystemMemoryXeHpcTests = CopyEnginesXeHpcFixture<1, XeHpcCoreFamily, false>;

INSTANTIATE_TEST_CASE_P(
    MemCopyBcsCmd,
    OneTileSystemMemoryXeHpcTests,
    testing::ValuesIn(allSupportedCopyEngines));

XE_HPC_CORETEST_P(OneTileSystemMemoryXeHpcTests, givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}
