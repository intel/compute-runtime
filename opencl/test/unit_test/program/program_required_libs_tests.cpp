/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/linker.h"
#include "shared/source/program/program_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

#include <cstring>
#include <limits>
#include <string>

namespace NEO {

struct ProgramRequiredLibsTest : public ClDeviceFixture, public ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::setUp();
        program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));
    }

    void TearDown() override {
        program.reset();
        ClDeviceFixture::tearDown();
    }

    std::unique_ptr<MockProgram> program;
};

TEST_F(ProgramRequiredLibsTest, givenEmptyRequiredLibsWhenResolveCalledThenSuccess) {
    ProgramInfo programInfo;

    auto retVal = program->resolveRequiredLibs(*pClDevice, programInfo);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(program->buildInfos[pClDevice->getRootDeviceIndex()].requiredLibPrograms.empty());
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

    auto &reqLibs = program->buildInfos[pClDevice->getRootDeviceIndex()].requiredLibPrograms;
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

    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
    EXPECT_TRUE(program->buildInfos[rootDeviceIndex].requiredLibPrograms.empty());

    std::string buildLog{program->getBuildLog(rootDeviceIndex)};
    EXPECT_NE(std::string::npos, buildLog.find("libMissing.zebin"));
}

TEST_F(ProgramRequiredLibsTest, givenLibProvidesSymbolWhenLinkAgainstRequiredLibsCalledThenLinkedFully) {
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();

    auto lib = std::make_unique<MockProgram>(nullptr, true, toClDeviceVector(*pClDevice));
    Linker::RelocatedSymbolsMap libSymbols;
    Linker::RelocatedSymbol<SymbolInfo> sym{};
    sym.gpuAddress = 0xDEADBEEF00ULL;
    libSymbols.emplace("foo", sym);
    lib->setSymbols(rootDeviceIndex, std::move(libSymbols));

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.push_back(lib.get());

    std::vector<char> isaBytes(64, 0);
    Linker::PatchableSegments isaSegments;
    isaSegments.push_back(Linker::PatchableSegment{isaBytes.data(),
                                                   reinterpret_cast<uint64_t>(isaBytes.data()),
                                                   isaBytes.size()});

    Linker::UnresolvedExternals unresolved;
    Linker::UnresolvedExternal ext{};
    ext.unresolvedRelocation.symbolName = "foo";
    ext.unresolvedRelocation.offset = 0;
    ext.unresolvedRelocation.type = LinkerInput::RelocationInfo::Type::address;
    ext.instructionsSegmentId = 0;
    unresolved.push_back(ext);

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedFully, status);
    EXPECT_TRUE(unresolved.empty());

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.clear();
}

TEST_F(ProgramRequiredLibsTest, givenLibDoesNotProvideSymbolWhenLinkAgainstRequiredLibsCalledThenLinkedPartially) {
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();

    auto lib = std::make_unique<MockProgram>(nullptr, true, toClDeviceVector(*pClDevice));
    program->buildInfos[rootDeviceIndex].requiredLibPrograms.push_back(lib.get());

    std::vector<char> isaBytes(64, 0);
    Linker::PatchableSegments isaSegments;
    isaSegments.push_back(Linker::PatchableSegment{isaBytes.data(),
                                                   reinterpret_cast<uint64_t>(isaBytes.data()),
                                                   isaBytes.size()});

    Linker::UnresolvedExternals unresolved;
    Linker::UnresolvedExternal ext{};
    ext.unresolvedRelocation.symbolName = "missing";
    ext.unresolvedRelocation.offset = 0;
    ext.unresolvedRelocation.type = LinkerInput::RelocationInfo::Type::address;
    ext.instructionsSegmentId = 0;
    unresolved.push_back(ext);

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedPartially, status);
    EXPECT_EQ(1U, unresolved.size());

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.clear();
}

TEST_F(ProgramRequiredLibsTest, givenLibProvidesSymbolWhenLinkAgainstRequiredLibsAndErrorMessageIsConstructedThenLogDoesNotMentionUnresolvedSymbol) {
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();

    constexpr auto resolvableSymbolName = "resolvable_symbol";

    auto lib = std::make_unique<MockProgram>(nullptr, true, toClDeviceVector(*pClDevice));
    Linker::RelocatedSymbolsMap libSymbols;
    Linker::RelocatedSymbol<SymbolInfo> sym{};
    sym.gpuAddress = 0xDEADBEEF00ULL;
    libSymbols.emplace(resolvableSymbolName, sym);
    lib->setSymbols(rootDeviceIndex, std::move(libSymbols));

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.push_back(lib.get());

    std::vector<char> isaBytes(64, 0);
    Linker::PatchableSegments isaSegments;
    isaSegments.push_back(Linker::PatchableSegment{isaBytes.data(),
                                                   reinterpret_cast<uint64_t>(isaBytes.data()),
                                                   isaBytes.size()});

    Linker::UnresolvedExternals unresolved;
    Linker::UnresolvedExternal ext{};
    ext.unresolvedRelocation.symbolName = resolvableSymbolName;
    ext.unresolvedRelocation.offset = 0;
    ext.unresolvedRelocation.type = LinkerInput::RelocationInfo::Type::address;
    ext.instructionsSegmentId = 0;
    unresolved.push_back(ext);

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

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.clear();
}

TEST_F(ProgramRequiredLibsTest, givenMixedResolvedAndUnresolvedSymbolsWhenLinkAgainstRequiredLibsThenOnlyTrulyUnresolvedAreReportedInErrorLog) {
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();

    constexpr auto resolvableSymbolName = "resolvable_symbol";
    constexpr auto missingSymbolName = "missing_symbol";

    auto lib = std::make_unique<MockProgram>(nullptr, true, toClDeviceVector(*pClDevice));
    Linker::RelocatedSymbolsMap libSymbols;
    Linker::RelocatedSymbol<SymbolInfo> sym{};
    sym.gpuAddress = 0xDEADBEEF00ULL;
    libSymbols.emplace(resolvableSymbolName, sym);
    lib->setSymbols(rootDeviceIndex, std::move(libSymbols));

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.push_back(lib.get());

    std::vector<char> isaBytes(64, 0);
    Linker::PatchableSegments isaSegments;
    isaSegments.push_back(Linker::PatchableSegment{isaBytes.data(),
                                                   reinterpret_cast<uint64_t>(isaBytes.data()),
                                                   isaBytes.size()});

    Linker::UnresolvedExternals unresolved;
    Linker::UnresolvedExternal resolvableExt{};
    resolvableExt.unresolvedRelocation.symbolName = resolvableSymbolName;
    resolvableExt.unresolvedRelocation.offset = 0;
    resolvableExt.unresolvedRelocation.type = LinkerInput::RelocationInfo::Type::address;
    resolvableExt.instructionsSegmentId = 0;
    unresolved.push_back(resolvableExt);

    Linker::UnresolvedExternal missingExt{};
    missingExt.unresolvedRelocation.symbolName = missingSymbolName;
    missingExt.unresolvedRelocation.offset = 32;
    missingExt.unresolvedRelocation.type = LinkerInput::RelocationInfo::Type::address;
    missingExt.instructionsSegmentId = 0;
    unresolved.push_back(missingExt);

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

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.clear();
}

TEST_F(ProgramRequiredLibsTest, givenSymbolReferencedWithAddendWhenLinkAgainstRequiredLibsThenPatchedValueIncludesAddend) {
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();

    auto lib = std::make_unique<MockProgram>(nullptr, true, toClDeviceVector(*pClDevice));
    Linker::RelocatedSymbolsMap libSymbols;
    Linker::RelocatedSymbol<SymbolInfo> sym{};
    sym.gpuAddress = 0xDEAD000000ULL;
    libSymbols.emplace("foo", sym);
    lib->setSymbols(rootDeviceIndex, std::move(libSymbols));

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.push_back(lib.get());

    std::vector<uint8_t> isaBytes(64, 0);
    Linker::PatchableSegments isaSegments;
    isaSegments.push_back(Linker::PatchableSegment{isaBytes.data(),
                                                   reinterpret_cast<uint64_t>(isaBytes.data()),
                                                   isaBytes.size()});

    constexpr int64_t addend = 0x10;
    Linker::UnresolvedExternals unresolved;
    Linker::UnresolvedExternal ext{};
    ext.unresolvedRelocation.symbolName = "foo";
    ext.unresolvedRelocation.offset = 0;
    ext.unresolvedRelocation.type = LinkerInput::RelocationInfo::Type::address;
    ext.unresolvedRelocation.addend = addend;
    ext.instructionsSegmentId = 0;
    unresolved.push_back(ext);

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedFully, status);
    EXPECT_TRUE(unresolved.empty());

    uint64_t patched = 0;
    memcpy(&patched, isaBytes.data(), sizeof(uint64_t));
    EXPECT_EQ(sym.gpuAddress + static_cast<uint64_t>(addend), patched);

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.clear();
}

TEST_F(ProgramRequiredLibsTest, givenUnresolvedExternalWithOutOfRangeInstructionsSegmentIdWhenLinkAgainstRequiredLibsThenEntryIsLeftUnresolvedAndNoCrash) {
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();

    auto lib = std::make_unique<MockProgram>(nullptr, true, toClDeviceVector(*pClDevice));
    Linker::RelocatedSymbolsMap libSymbols;
    Linker::RelocatedSymbol<SymbolInfo> sym{};
    sym.gpuAddress = 0xDEADBEEF00ULL;
    libSymbols.emplace("foo", sym);
    lib->setSymbols(rootDeviceIndex, std::move(libSymbols));

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.push_back(lib.get());

    std::vector<uint8_t> isaBytes(64, 0);
    Linker::PatchableSegments isaSegments;
    isaSegments.push_back(Linker::PatchableSegment{isaBytes.data(),
                                                   reinterpret_cast<uint64_t>(isaBytes.data()),
                                                   isaBytes.size()});

    Linker::UnresolvedExternals unresolved;
    Linker::UnresolvedExternal ext{};
    ext.unresolvedRelocation.symbolName = "foo";
    ext.unresolvedRelocation.offset = 0;
    ext.unresolvedRelocation.type = LinkerInput::RelocationInfo::Type::address;
    ext.instructionsSegmentId = std::numeric_limits<uint32_t>::max();
    unresolved.push_back(ext);

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedPartially, status);
    ASSERT_EQ(1U, unresolved.size());
    EXPECT_STREQ("foo", unresolved[0].unresolvedRelocation.symbolName.c_str());

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.clear();
}

TEST_F(ProgramRequiredLibsTest, givenUnresolvedExternalWithOffsetBeyondSegmentSizeWhenLinkAgainstRequiredLibsThenEntryIsLeftUnresolvedAndNoOutOfBoundsWrite) {
    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();

    auto lib = std::make_unique<MockProgram>(nullptr, true, toClDeviceVector(*pClDevice));
    Linker::RelocatedSymbolsMap libSymbols;
    Linker::RelocatedSymbol<SymbolInfo> sym{};
    sym.gpuAddress = 0xDEADBEEF00ULL;
    libSymbols.emplace("foo", sym);
    lib->setSymbols(rootDeviceIndex, std::move(libSymbols));

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.push_back(lib.get());

    constexpr size_t segmentSize = 16U;
    std::vector<uint8_t> isaBytes(segmentSize, 0xCC);
    Linker::PatchableSegments isaSegments;
    isaSegments.push_back(Linker::PatchableSegment{isaBytes.data(),
                                                   reinterpret_cast<uint64_t>(isaBytes.data()),
                                                   isaBytes.size()});

    Linker::UnresolvedExternals unresolved;
    Linker::UnresolvedExternal ext{};
    ext.unresolvedRelocation.symbolName = "foo";
    ext.unresolvedRelocation.offset = segmentSize - 2U;
    ext.unresolvedRelocation.type = LinkerInput::RelocationInfo::Type::address;
    ext.instructionsSegmentId = 0;
    unresolved.push_back(ext);

    auto status = program->linkAgainstRequiredLibs(rootDeviceIndex, isaSegments, unresolved);
    EXPECT_EQ(LinkingStatus::linkedPartially, status);
    ASSERT_EQ(1U, unresolved.size());
    for (auto byte : isaBytes) {
        EXPECT_EQ(0xCC, byte);
    }

    program->buildInfos[rootDeviceIndex].requiredLibPrograms.clear();
}

} // namespace NEO
