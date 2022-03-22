/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/aub_tests/command_stream/copy_engine_aub_tests_xehp_and_later.h"

using namespace NEO;

template <uint32_t numTiles, typename FamilyType, bool useLocalMemory = true>
struct CopyEnginesPvcFixture : public CopyEngineXeHPAndLater<numTiles, useLocalMemory>, public ::testing::WithParamInterface<uint32_t /* EngineType */> {
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

using OneTilePvcTests = CopyEnginesPvcFixture<1, XE_HPC_COREFamily>;

INSTANTIATE_TEST_CASE_P(
    MemCopyBcsCmd,
    OneTilePvcTests,
    testing::ValuesIn(allSupportedCopyEngines));

XE_HPC_CORETEST_P(OneTilePvcTests, givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolve) {
    givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolveImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopied) {
    givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenOffsetsWhenBltExecutedThenCopiedDataIsValid) {
    givenOffsetsWhenBltExecutedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompress) {
    givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompressImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenCopyBufferRectWithBigSizesWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenCopyBufferRectWithBigSizesWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

using OneTileSystemMemoryPvcTests = CopyEnginesPvcFixture<1, XE_HPC_COREFamily, false>;

INSTANTIATE_TEST_CASE_P(
    MemCopyBcsCmd,
    OneTileSystemMemoryPvcTests,
    testing::ValuesIn(allSupportedCopyEngines));

XE_HPC_CORETEST_P(OneTileSystemMemoryPvcTests, givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValid) {
    givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTilePvcTests, givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}
