/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/aub_tests/command_stream/copy_engine_aub_tests_xehp_and_later.h"

namespace L0 {
namespace ult {

template <uint32_t numTiles, bool useLocalMemory>
struct CopyEnginesXeHpcFixture : public CopyEngineXeHPAndLater<numTiles, useLocalMemory>, public ::testing::WithParamInterface<uint32_t> {
    aub_stream::EngineType getBcsEngineTypeToUse(const NEO::ProductHelper &productHelper) const override {
        return static_cast<aub_stream::EngineType>(this->GetParam());
    }
};

constexpr uint32_t allSupportedCopyEngines[] = {
    aub_stream::EngineType::ENGINE_BCS1,
    aub_stream::EngineType::ENGINE_BCS2,
    aub_stream::EngineType::ENGINE_BCS3,
    aub_stream::EngineType::ENGINE_BCS4,
    aub_stream::EngineType::ENGINE_BCS5,
    aub_stream::EngineType::ENGINE_BCS6,
    aub_stream::EngineType::ENGINE_BCS7,
    aub_stream::EngineType::ENGINE_BCS8,
};

using OneTileXeHpcTests = CopyEnginesXeHpcFixture<1, true>;

INSTANTIATE_TEST_SUITE_P(
    MemCopyBcsCmd,
    OneTileXeHpcTests,
    testing::ValuesIn(allSupportedCopyEngines));

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolve) {
    this->givenNotCompressedBufferWhenBltExecutedThenCompressDataAndResolveImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    this->givenHostPtrWhenBlitCommandToCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    this->givenDstHostPtrWhenBlitCommandFromCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    this->givenDstHostPtrWhenBlitCommandFromNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    this->givenSrcHostPtrWhenBlitCommandToNotCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopied) {
    this->givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedFromHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    this->givenBufferWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenOffsetsWhenBltExecutedThenCopiedDataIsValid) {
    this->givenOffsetsWhenBltExecutedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValid) {
    this->givenSrcCompressedBufferWhenBlitCommandToDstCompressedBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompress) {
    this->givenCompressedBufferWhenAuxTranslationCalledThenResolveAndCompressImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenCopyBufferRectWithBigSizesWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    this->givenCopyBufferRectWithBigSizesWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

using OneTileSystemMemoryXeHpcTests = CopyEnginesXeHpcFixture<1, false>;

INSTANTIATE_TEST_SUITE_P(
    MemCopyBcsCmd,
    OneTileSystemMemoryXeHpcTests,
    testing::ValuesIn(allSupportedCopyEngines));

XE_HPC_CORETEST_P(OneTileSystemMemoryXeHpcTests, givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValid) {
    this->givenSrcSystemBufferWhenBlitCommandToDstSystemBufferIsDispatchedThenCopiedDataIsValidImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    this->givenReadBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    this->givenWriteBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

XE_HPC_CORETEST_P(OneTileXeHpcTests, givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopied) {
    this->givenCopyBufferRectWithOffsetWhenHostPtrBlitCommandIsDispatchedToHostPtrThenDataIsCorrectlyCopiedImpl<FamilyType>();
}

} // namespace ult
} // namespace L0
