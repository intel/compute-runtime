/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/linker.h"
#include "shared/source/program/program_info.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

#include <cstring>
#include <limits>
#include <string>

namespace NEO {

struct ProgramRequiredLibsTest : public Test<ClDeviceFixture> {
    void SetUp() override {
        Test<ClDeviceFixture>::SetUp();
        program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));
    }

    void TearDown() override {
        program.reset();
        libs.clear();
        Test<ClDeviceFixture>::TearDown();
    }

    // registers a required lib exporting a single symbol; the lib outlives the program (destroyed in TearDown)
    void addLibWithSymbol(const char *symbolName, uint64_t gpuAddress) {
        auto lib = std::make_unique<MockProgram>(nullptr, true, toClDeviceVector(*pClDevice));
        if (symbolName != nullptr) {
            Linker::RelocatedSymbolsMap libSymbols;
            Linker::RelocatedSymbol<SymbolInfo> sym{};
            sym.gpuAddress = gpuAddress;
            libSymbols.emplace(symbolName, sym);
            lib->setSymbols(rootDeviceIndex, std::move(libSymbols));
        }
        program->buildInfos[rootDeviceIndex].requiredLibPrograms.push_back(lib.get());
        libs.push_back(std::move(lib));
    }

    Linker::PatchableSegments makeIsaSegment(size_t segmentSize, uint8_t fillValue = 0U) {
        isaBytes.assign(segmentSize, fillValue);
        Linker::PatchableSegments isaSegments;
        isaSegments.push_back(Linker::PatchableSegment{isaBytes.data(),
                                                       reinterpret_cast<uint64_t>(isaBytes.data()),
                                                       isaBytes.size()});
        return isaSegments;
    }

    static Linker::UnresolvedExternal makeExternal(const char *symbolName, uint64_t offset, uint32_t instructionsSegmentId = 0U, int64_t addend = 0) {
        Linker::UnresolvedExternal ext{};
        ext.unresolvedRelocation.symbolName = symbolName;
        ext.unresolvedRelocation.offset = offset;
        ext.unresolvedRelocation.type = LinkerInput::RelocationInfo::Type::address;
        ext.unresolvedRelocation.addend = addend;
        ext.instructionsSegmentId = instructionsSegmentId;
        return ext;
    }

    static constexpr uint64_t libSymbolGpuAddress = 0xDEADBEEF00ULL;

    std::unique_ptr<MockProgram> program;
    std::vector<std::unique_ptr<MockProgram>> libs;
    std::vector<uint8_t> isaBytes;
};

TEST_F(ProgramRequiredLibsTest, givenEmptyRequiredLibsWhenResolveCalledThenSuccess) {
    ProgramInfo programInfo;

    auto retVal = program->resolveRequiredLibs(*pClDevice, programInfo);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(program->getRequiredLibPrograms(rootDeviceIndex).empty());
}

TEST_F(ProgramRequiredLibsTest, givenCachedLibWhenResolveCalledThenStoredInBuildInfo) {
    constexpr auto libName = "libFoo.zebin";
    auto *fakeLib = new MockProgram(nullptr, true, toClDeviceVector(*pClDevice));
    {
        auto lock = pClDevice->requiredLibsRegistry.lock();
        pClDevice->requiredLibsRegistry->emplace(libName, std::unique_ptr<Program>(fakeLib));
    }

    ProgramInfo programInfo;
    programInfo.requiredLibs.emplace_back(libName);

    auto retVal = program->resolveRequiredLibs(*pClDevice, programInfo);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto &reqLibs = program->getRequiredLibPrograms(rootDeviceIndex);
    ASSERT_EQ(1U, reqLibs.size());
    EXPECT_EQ(static_cast<Program *>(fakeLib), reqLibs[0]);
}

TEST_F(ProgramRequiredLibsTest, givenMissingLibWhenResolveCalledThenFailureAndBuildLogPopulated) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RequiredLibsBinarySearchPath.set("/no/such/path/exists");

    ProgramInfo programInfo;
    programInfo.requiredLibs.emplace_back("libMissing.zebin");

    auto retVal = program->resolveRequiredLibs(*pClDevice, programInfo);
    EXPECT_NE(CL_SUCCESS, retVal);
    EXPECT_TRUE(program->getRequiredLibPrograms(rootDeviceIndex).empty());

    std::string buildLog{program->getBuildLog(rootDeviceIndex)};
    EXPECT_NE(std::string::npos, buildLog.find("libMissing.zebin"));
}

TEST_F(ProgramRequiredLibsTest, givenLibProvidesSymbolWhenLinkAgainstRequiredLibsCalledThenLinkedFully) {
    addLibWithSymbol("foo", libSymbolGpuAddress);
    auto isaSegments = makeIsaSegment(64U);

    Linker::UnresolvedExternals unresolved{makeExternal("foo", 0U)};

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedFully, status);
    EXPECT_TRUE(unresolved.empty());
}

TEST_F(ProgramRequiredLibsTest, givenLibDoesNotProvideSymbolWhenLinkAgainstRequiredLibsCalledThenLinkedPartially) {
    addLibWithSymbol(nullptr, 0U);
    auto isaSegments = makeIsaSegment(64U);

    Linker::UnresolvedExternals unresolved{makeExternal("missing", 0U)};

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedPartially, status);
    EXPECT_EQ(1U, unresolved.size());
}

TEST_F(ProgramRequiredLibsTest, givenLibProvidesSymbolWhenLinkAgainstRequiredLibsAndErrorMessageIsConstructedThenLogDoesNotMentionUnresolvedSymbol) {
    constexpr auto resolvableSymbolName = "resolvable_symbol";

    addLibWithSymbol(resolvableSymbolName, libSymbolGpuAddress);
    auto isaSegments = makeIsaSegment(64U);

    Linker::UnresolvedExternals unresolved{makeExternal(resolvableSymbolName, 0U)};

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedFully, status);
    EXPECT_TRUE(unresolved.empty());

    auto error = constructLinkerErrorMessage(unresolved, {"kernel : k0"});
    if (!error.empty()) {
        program->updateBuildLog(rootDeviceIndex, error.c_str(), error.size());
    }

    std::string buildLog{program->getBuildLog(rootDeviceIndex)};
    EXPECT_EQ(std::string::npos, buildLog.find("Unresolved"))
        << "Build log must not mention unresolved symbols when all symbols were resolved via required_libs. "
        << "Log was: '" << buildLog << "'";
    EXPECT_EQ(std::string::npos, buildLog.find(resolvableSymbolName))
        << "Build log must not mention a symbol name when that symbol was successfully resolved. "
        << "Log was: '" << buildLog << "'";
}

TEST_F(ProgramRequiredLibsTest, givenMixedResolvedAndUnresolvedSymbolsWhenLinkAgainstRequiredLibsThenOnlyTrulyUnresolvedAreReportedInErrorLog) {
    constexpr auto resolvableSymbolName = "resolvable_symbol";
    constexpr auto missingSymbolName = "missing_symbol";

    addLibWithSymbol(resolvableSymbolName, libSymbolGpuAddress);
    auto isaSegments = makeIsaSegment(64U);

    Linker::UnresolvedExternals unresolved{makeExternal(resolvableSymbolName, 0U),
                                           makeExternal(missingSymbolName, 32U)};

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedPartially, status);
    ASSERT_EQ(1U, unresolved.size());
    EXPECT_STREQ(missingSymbolName, unresolved[0].unresolvedRelocation.symbolName.c_str());

    auto error = constructLinkerErrorMessage(unresolved, {"kernel : k0"});
    program->updateBuildLog(rootDeviceIndex, error.c_str(), error.size());

    std::string buildLog{program->getBuildLog(rootDeviceIndex)};
    EXPECT_NE(std::string::npos, buildLog.find(missingSymbolName))
        << "Build log must mention the truly unresolved symbol. Log was: '" << buildLog << "'";
    EXPECT_EQ(std::string::npos, buildLog.find(resolvableSymbolName))
        << "Build log must not mention a symbol name when that symbol was successfully resolved via required_libs. "
        << "Log was: '" << buildLog << "'";
}

TEST_F(ProgramRequiredLibsTest, givenSymbolReferencedWithAddendWhenLinkAgainstRequiredLibsThenPatchedValueIncludesAddend) {
    constexpr uint64_t symbolGpuAddress = 0xDEAD000000ULL;
    constexpr int64_t addend = 0x10;

    addLibWithSymbol("foo", symbolGpuAddress);
    auto isaSegments = makeIsaSegment(64U);

    Linker::UnresolvedExternals unresolved{makeExternal("foo", 0U, 0U, addend)};

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedFully, status);
    EXPECT_TRUE(unresolved.empty());

    uint64_t patched = 0;
    memcpy(&patched, isaBytes.data(), sizeof(uint64_t));
    EXPECT_EQ(symbolGpuAddress + static_cast<uint64_t>(addend), patched);
}

TEST_F(ProgramRequiredLibsTest, givenUnresolvedExternalWithOutOfRangeInstructionsSegmentIdWhenLinkAgainstRequiredLibsThenEntryIsLeftUnresolvedAndNoCrash) {
    addLibWithSymbol("foo", libSymbolGpuAddress);
    auto isaSegments = makeIsaSegment(64U);

    Linker::UnresolvedExternals unresolved{makeExternal("foo", 0U, std::numeric_limits<uint32_t>::max())};

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedPartially, status);
    ASSERT_EQ(1U, unresolved.size());
    EXPECT_STREQ("foo", unresolved[0].unresolvedRelocation.symbolName.c_str());
}

TEST_F(ProgramRequiredLibsTest, givenUnresolvedExternalWithOffsetBeyondSegmentSizeWhenLinkAgainstRequiredLibsThenEntryIsLeftUnresolvedAndNoOutOfBoundsWrite) {
    constexpr size_t segmentSize = 16U;
    constexpr uint8_t fillValue = 0xCC;

    addLibWithSymbol("foo", libSymbolGpuAddress);
    auto isaSegments = makeIsaSegment(segmentSize, fillValue);

    Linker::UnresolvedExternals unresolved{makeExternal("foo", segmentSize - 2U)};

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedPartially, status);
    ASSERT_EQ(1U, unresolved.size());
    for (auto byte : isaBytes) {
        EXPECT_EQ(fillValue, byte);
    }
}

TEST_F(ProgramRequiredLibsTest, givenUnresolvedExternalWithSegmentSmallerThanRelocationWhenLinkAgainstRequiredLibsThenEntryIsLeftUnresolvedAndNoOutOfBoundsWrite) {
    const auto segmentSize = addressSizeInBytes(LinkerInput::RelocationInfo::Type::address) - 1U;
    constexpr uint8_t fillValue = 0xCC;

    addLibWithSymbol("foo", libSymbolGpuAddress);
    auto isaSegments = makeIsaSegment(segmentSize, fillValue);

    Linker::UnresolvedExternals unresolved{makeExternal("foo", 0U)};

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedPartially, status);
    ASSERT_EQ(1U, unresolved.size());
    for (auto byte : isaBytes) {
        EXPECT_EQ(fillValue, byte);
    }
}

TEST_F(ProgramRequiredLibsTest, givenRequiredLibProvidingUnresolvedSymbolWhenLinkBinaryThenPatchesIsaAndLinksFully) {
    addLibWithSymbol("foo", libSymbolGpuAddress);

    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->textRelocations.push_back({{"foo", 0U, LinkerInput::RelocationInfo::Type::address}});
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;

    std::vector<char> kernelHeap(64U);
    MockGraphicsAllocation kernelIsa(kernelHeap.data(), kernelHeap.size());
    KernelInfo kernelInfo;
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "kernel";
    kernelInfo.heapInfo.pKernelHeap = kernelHeap.data();
    kernelInfo.heapInfo.kernelHeapSize = static_cast<uint32_t>(kernelHeap.size());
    kernelInfo.kernelAllocation = &kernelIsa;
    program->getKernelInfoArray(rootDeviceIndex).push_back(&kernelInfo);
    program->setLinkerInput(rootDeviceIndex, std::move(linkerInput));

    auto retVal = program->linkBinary(&pClDevice->getDevice(), nullptr, 0U, nullptr, 0U, {}, program->externalFunctions);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(libSymbolGpuAddress, *reinterpret_cast<uint64_t *>(kernelHeap.data()));

    program->getKernelInfoArray(rootDeviceIndex).clear();
}

} // namespace NEO
