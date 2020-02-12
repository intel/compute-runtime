/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/string.h"
#include "core/memory_manager/allocations_list.h"
#include "core/memory_manager/graphics_allocation.h"
#include "core/memory_manager/unified_memory_manager.h"
#include "core/program/program_info_from_patchtokens.h"
#include "core/unit_tests/compiler_interface/linker_mock.h"
#include "core/unit_tests/device_binary_format/patchtokens_tests.h"
#include "runtime/platform/platform.h"
#include "runtime/program/program.h"
#include "test.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/program/program_with_source.h"

using namespace NEO;

using namespace iOpenCL;
static const char constValue[] = "11223344";
static const char globalValue[] = "55667788";

class ProgramDataTestBase : public testing::Test,
                            public ContextFixture,
                            public PlatformFixture,
                            public ProgramFixture {

    using ContextFixture::SetUp;
    using PlatformFixture::SetUp;

  public:
    ProgramDataTestBase() {
        memset(&programBinaryHeader, 0x00, sizeof(SProgramBinaryHeader));
        pCurPtr = nullptr;
        pProgramPatchList = nullptr;
        programPatchListSize = 0;
    }

    void buildAndDecodeProgramPatchList();

    void SetUp() override {
        PlatformFixture::SetUp();
        cl_device_id device = pPlatform->getClDevice(0);
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();

        CreateProgramWithSource(
            pContext,
            &device,
            "CopyBuffer_simd16.cl");
    }

    void TearDown() override {
        knownSource.reset();
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
    }

    size_t setupConstantAllocation() {
        size_t constSize = strlen(constValue) + 1;

        EXPECT_EQ(nullptr, pProgram->getConstantSurface());

        SPatchAllocateConstantMemorySurfaceProgramBinaryInfo allocateConstMemorySurface;
        allocateConstMemorySurface.Token = PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
        allocateConstMemorySurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo));

        allocateConstMemorySurface.ConstantBufferIndex = 0;
        allocateConstMemorySurface.InlineDataSize = static_cast<uint32_t>(constSize);

        pAllocateConstMemorySurface.reset(new cl_char[allocateConstMemorySurface.Size + constSize]);

        memcpy_s(pAllocateConstMemorySurface.get(),
                 sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo),
                 &allocateConstMemorySurface,
                 sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo));

        memcpy_s((cl_char *)pAllocateConstMemorySurface.get() + sizeof(allocateConstMemorySurface), constSize, constValue, constSize);

        pProgramPatchList = (void *)pAllocateConstMemorySurface.get();
        programPatchListSize = static_cast<uint32_t>(allocateConstMemorySurface.Size + constSize);
        return constSize;
    }

    size_t setupGlobalAllocation() {
        size_t globalSize = strlen(globalValue) + 1;

        EXPECT_EQ(nullptr, pProgram->getGlobalSurface());

        SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo allocateGlobalMemorySurface;
        allocateGlobalMemorySurface.Token = PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
        allocateGlobalMemorySurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo));

        allocateGlobalMemorySurface.GlobalBufferIndex = 0;
        allocateGlobalMemorySurface.InlineDataSize = static_cast<uint32_t>(globalSize);

        pAllocateGlobalMemorySurface.reset(new cl_char[allocateGlobalMemorySurface.Size + globalSize]);

        memcpy_s(pAllocateGlobalMemorySurface.get(),
                 sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo),
                 &allocateGlobalMemorySurface,
                 sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo));

        memcpy_s((cl_char *)pAllocateGlobalMemorySurface.get() + sizeof(allocateGlobalMemorySurface), globalSize, globalValue, globalSize);

        pProgramPatchList = pAllocateGlobalMemorySurface.get();
        programPatchListSize = static_cast<uint32_t>(allocateGlobalMemorySurface.Size + globalSize);
        return globalSize;
    }
    std::unique_ptr<cl_char[]> pAllocateConstMemorySurface;
    std::unique_ptr<cl_char[]> pAllocateGlobalMemorySurface;
    char *pCurPtr;
    SProgramBinaryHeader programBinaryHeader;
    void *pProgramPatchList;
    uint32_t programPatchListSize;
    cl_int patchlistDecodeErrorCode = 0;
    bool allowDecodeFailure = false;
};

void ProgramDataTestBase::buildAndDecodeProgramPatchList() {
    size_t headerSize = sizeof(SProgramBinaryHeader);

    cl_int error = CL_SUCCESS;
    programBinaryHeader.Magic = 0x494E5443;
    programBinaryHeader.Version = CURRENT_ICBE_VERSION;
    programBinaryHeader.Device = platformDevices[0]->platform.eRenderCoreFamily;
    programBinaryHeader.GPUPointerSizeInBytes = 8;
    programBinaryHeader.NumberOfKernels = 0;
    programBinaryHeader.PatchListSize = programPatchListSize;

    char *pProgramData = new char[headerSize + programBinaryHeader.PatchListSize];
    ASSERT_NE(nullptr, pProgramData);

    pCurPtr = pProgramData;

    // program header
    memset(pCurPtr, 0, sizeof(SProgramBinaryHeader));
    *(SProgramBinaryHeader *)pCurPtr = programBinaryHeader;

    pCurPtr += sizeof(SProgramBinaryHeader);

    // patch list
    memcpy_s(pCurPtr, programPatchListSize, pProgramPatchList, programPatchListSize);
    pCurPtr += programPatchListSize;

    //as we use mock compiler in unit test, replace the genBinary here.
    pProgram->unpackedDeviceBinary = makeCopy(pProgramData, headerSize + programBinaryHeader.PatchListSize);
    pProgram->unpackedDeviceBinarySize = headerSize + programBinaryHeader.PatchListSize;

    error = pProgram->processGenBinary();
    patchlistDecodeErrorCode = error;
    if (allowDecodeFailure == false) {
        EXPECT_EQ(CL_SUCCESS, error);
    }
    delete[] pProgramData;
}

using ProgramDataTest = ProgramDataTestBase;

TEST_F(ProgramDataTest, EmptyProgramBinaryHeader) {
    buildAndDecodeProgramPatchList();
}

TEST_F(ProgramDataTest, AllocateConstantMemorySurfaceProgramBinaryInfo) {

    auto constSize = setupConstantAllocation();

    buildAndDecodeProgramPatchList();

    EXPECT_NE(nullptr, pProgram->getConstantSurface());
    EXPECT_EQ(0, memcmp(constValue, pProgram->getConstantSurface()->getUnderlyingBuffer(), constSize));
}

TEST_F(ProgramDataTest, whenGlobalConstantsAreExportedThenAllocateSurfacesAsSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    char constantData[128] = {};
    ProgramInfo programInfo;
    programInfo.globalConstants.initData = constantData;
    programInfo.globalConstants.size = sizeof(constantData);
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalConstants = true;
    programInfo.linkerInput = std::move(mockLinkerInput);
    this->pProgram->processProgramInfo(programInfo);

    ASSERT_NE(nullptr, pProgram->getConstantSurface());
    EXPECT_NE(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getConstantSurface()->getGpuAddress())));
}

TEST_F(ProgramDataTest, whenGlobalConstantsAreNotExportedThenAllocateSurfacesAsNonSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    char constantData[128] = {};
    ProgramInfo programInfo;
    programInfo.globalConstants.initData = constantData;
    programInfo.globalConstants.size = sizeof(constantData);
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalConstants = false;
    programInfo.linkerInput = std::move(mockLinkerInput);
    this->pProgram->processProgramInfo(programInfo);

    ASSERT_NE(nullptr, pProgram->getConstantSurface());
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getConstantSurface()->getGpuAddress())));
}

TEST_F(ProgramDataTest, whenGlobalConstantsAreExportedButContextUnavailableThenAllocateSurfacesAsNonSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    char constantData[128] = {};
    ProgramInfo programInfo;
    programInfo.globalConstants.initData = constantData;
    programInfo.globalConstants.size = sizeof(constantData);
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalConstants = true;
    programInfo.linkerInput = std::move(mockLinkerInput);

    pProgram->context = nullptr;

    this->pProgram->processProgramInfo(programInfo);

    pProgram->context = pContext;

    ASSERT_NE(nullptr, pProgram->getConstantSurface());
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getConstantSurface()->getGpuAddress())));
}

TEST_F(ProgramDataTest, whenGlobalVariablesAreExportedThenAllocateSurfacesAsSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }
    char globalData[128] = {};
    ProgramInfo programInfo;
    programInfo.globalVariables.initData = globalData;
    programInfo.globalVariables.size = sizeof(globalData);
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalVariables = true;
    programInfo.linkerInput = std::move(mockLinkerInput);
    this->pProgram->processProgramInfo(programInfo);

    ASSERT_NE(nullptr, pProgram->getGlobalSurface());
    EXPECT_NE(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getGlobalSurface()->getGpuAddress())));
}

TEST_F(ProgramDataTest, whenGlobalVariablesAreExportedButContextUnavailableThenAllocateSurfacesAsNonSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    char globalData[128] = {};
    ProgramInfo programInfo;
    programInfo.globalVariables.initData = globalData;
    programInfo.globalVariables.size = sizeof(globalData);
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalVariables = true;
    programInfo.linkerInput = std::move(mockLinkerInput);

    pProgram->context = nullptr;

    this->pProgram->processProgramInfo(programInfo);

    pProgram->context = pContext;

    ASSERT_NE(nullptr, pProgram->getGlobalSurface());
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getGlobalSurface()->getGpuAddress())));
}

TEST_F(ProgramDataTest, whenGlobalVariablesAreNotExportedThenAllocateSurfacesAsNonSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    char globalData[128] = {};
    ProgramInfo programInfo;
    programInfo.globalVariables.initData = globalData;
    programInfo.globalVariables.size = sizeof(globalData);
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalVariables = false;
    programInfo.linkerInput = std::move(mockLinkerInput);
    this->pProgram->processProgramInfo(programInfo);

    ASSERT_NE(nullptr, pProgram->getGlobalSurface());
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getGlobalSurface()->getGpuAddress())));
}

TEST_F(ProgramDataTest, givenConstantAllocationThatIsInUseByGpuWhenProgramIsBeingDestroyedThenItIsAddedToTemporaryAllocationList) {

    setupConstantAllocation();

    buildAndDecodeProgramPatchList();

    auto &csr = *pPlatform->getDevice(0)->getDefaultEngine().commandStreamReceiver;
    auto tagAddress = csr.getTagAddress();
    auto constantSurface = pProgram->getConstantSurface();
    constantSurface->updateTaskCount(*tagAddress + 1, csr.getOsContext().getContextId());

    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    delete pProgram;
    pProgram = nullptr;
    EXPECT_FALSE(csr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_EQ(constantSurface, csr.getTemporaryAllocations().peekHead());
}

TEST_F(ProgramDataTest, givenGlobalAllocationThatIsInUseByGpuWhenProgramIsBeingDestroyedThenItIsAddedToTemporaryAllocationList) {
    setupGlobalAllocation();

    buildAndDecodeProgramPatchList();

    auto &csr = *pPlatform->getDevice(0)->getDefaultEngine().commandStreamReceiver;
    auto tagAddress = csr.getTagAddress();
    auto globalSurface = pProgram->getGlobalSurface();
    globalSurface->updateTaskCount(*tagAddress + 1, csr.getOsContext().getContextId());

    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    delete pProgram;
    pProgram = nullptr;
    EXPECT_FALSE(csr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_EQ(globalSurface, csr.getTemporaryAllocations().peekHead());
}

TEST_F(ProgramDataTest, GivenDeviceForcing32BitMessagesWhenConstAllocationIsPresentInProgramBinariesThen32BitStorageIsAllocated) {
    auto constSize = setupConstantAllocation();
    this->pContext->getDevice(0)->getMemoryManager()->setForce32BitAllocations(true);

    buildAndDecodeProgramPatchList();

    EXPECT_NE(nullptr, pProgram->getConstantSurface());
    EXPECT_EQ(0, memcmp(constValue, pProgram->getConstantSurface()->getUnderlyingBuffer(), constSize));

    if (is64bit) {
        EXPECT_TRUE(pProgram->getConstantSurface()->is32BitAllocation());
    }
}

TEST_F(ProgramDataTest, AllocateGlobalMemorySurfaceProgramBinaryInfo) {
    auto globalSize = setupGlobalAllocation();
    buildAndDecodeProgramPatchList();
    EXPECT_NE(nullptr, pProgram->getGlobalSurface());
    EXPECT_EQ(0, memcmp(globalValue, pProgram->getGlobalSurface()->getUnderlyingBuffer(), globalSize));
}

TEST_F(ProgramDataTest, Given32BitDeviceWhenGlobalMemorySurfaceIsPresentThenItHas32BitStorage) {
    char globalValue[] = "55667788";
    size_t globalSize = strlen(globalValue) + 1;
    this->pContext->getDevice(0)->getMemoryManager()->setForce32BitAllocations(true);
    EXPECT_EQ(nullptr, pProgram->getGlobalSurface());

    SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo allocateGlobalMemorySurface;
    allocateGlobalMemorySurface.Token = PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
    allocateGlobalMemorySurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo));

    allocateGlobalMemorySurface.GlobalBufferIndex = 0;
    allocateGlobalMemorySurface.InlineDataSize = static_cast<uint32_t>(globalSize);

    cl_char *pAllocateGlobalMemorySurface = new cl_char[allocateGlobalMemorySurface.Size + globalSize];

    memcpy_s(pAllocateGlobalMemorySurface,
             sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo),
             &allocateGlobalMemorySurface,
             sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo));

    memcpy_s((cl_char *)pAllocateGlobalMemorySurface + sizeof(allocateGlobalMemorySurface), globalSize, globalValue, globalSize);

    pProgramPatchList = (void *)pAllocateGlobalMemorySurface;
    programPatchListSize = static_cast<uint32_t>(allocateGlobalMemorySurface.Size + globalSize);

    buildAndDecodeProgramPatchList();

    EXPECT_NE(nullptr, pProgram->getGlobalSurface());
    EXPECT_EQ(0, memcmp(globalValue, pProgram->getGlobalSurface()->getUnderlyingBuffer(), globalSize));
    if (is64bit) {
        EXPECT_TRUE(pProgram->getGlobalSurface()->is32BitAllocation());
    }

    delete[] pAllocateGlobalMemorySurface;
}

TEST(ProgramScopeMetadataTest, WhenPatchingGlobalSurfaceThenPickProperSourceBuffer) {
    MockExecutionEnvironment execEnv;
    MockClDevice device{new MockDevice};
    execEnv.memoryManager = std::make_unique<MockMemoryManager>();
    PatchTokensTestData::ValidProgramWithMixedGlobalVarAndConstSurfacesAndPointers decodedProgram;
    decodedProgram.globalPointerMutable->GlobalPointerOffset = 0U;
    decodedProgram.constantPointerMutable->ConstantPointerOffset = 0U;
    memset(decodedProgram.globalSurfMutable + 1, 0U, sizeof(uintptr_t));
    memset(decodedProgram.constSurfMutable + 1, 0U, sizeof(uintptr_t));
    ProgramInfo programInfo;
    MockProgram program(execEnv);
    program.pDevice = &device;
    NEO::populateProgramInfo(programInfo, decodedProgram);
    program.processProgramInfo(programInfo);
    ASSERT_NE(nullptr, program.globalSurface);
    ASSERT_NE(nullptr, program.constantSurface);
    ASSERT_NE(nullptr, program.globalSurface->getUnderlyingBuffer());
    ASSERT_NE(nullptr, program.constantSurface->getUnderlyingBuffer());
    EXPECT_EQ(static_cast<uintptr_t>(program.globalSurface->getGpuAddressToPatch()), *reinterpret_cast<uintptr_t *>(program.constantSurface->getUnderlyingBuffer()));
    EXPECT_EQ(static_cast<uintptr_t>(program.constantSurface->getGpuAddressToPatch()), *reinterpret_cast<uintptr_t *>(program.globalSurface->getUnderlyingBuffer()));
}

TEST_F(ProgramDataTest, GivenProgramWith32bitPointerOptWhenProgramScopeConstantBufferPatchTokensAreReadThenConstantPointerOffsetIsPatchedWith32bitPointer) {
    cl_device_id device = pPlatform->getClDevice(0);
    CreateProgramWithSource(pContext, &device, "CopyBuffer_simd16.cl");
    ASSERT_NE(nullptr, pProgram);

    MockProgram *prog = pProgram;

    // simulate case when constant surface was not allocated
    EXPECT_EQ(nullptr, prog->getConstantSurface());
    ProgramInfo programInfo;
    programInfo.prepareLinkerInputStorage();
    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.relocationSegment = NEO::SegmentType::GlobalConstants;
    relocInfo.symbolSegment = NEO::SegmentType::GlobalConstants;
    relocInfo.offset = 0U;
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    programInfo.linkerInput->addDataRelocationInfo(relocInfo);
    programInfo.linkerInput->setPointerSize(LinkerInput::Traits::PointerSize::Ptr32bit);

    MockBuffer constantSurface;
    ASSERT_LT(8U, constantSurface.getSize());
    prog->setConstantSurface(&constantSurface.mockGfxAllocation);
    constantSurface.mockGfxAllocation.set32BitAllocation(true);
    uint32_t *constantSurfaceStorage = reinterpret_cast<uint32_t *>(constantSurface.getCpuAddress());
    uint32_t sentinel = 0x17192329U;
    constantSurfaceStorage[0] = 0U;
    constantSurfaceStorage[1] = sentinel;

    pProgram->linkerInput = std::move(programInfo.linkerInput);
    pProgram->linkBinary();
    uint32_t expectedAddr = static_cast<uint32_t>(constantSurface.getGraphicsAllocation()->getGpuAddressToPatch());
    EXPECT_EQ(expectedAddr, constantSurfaceStorage[0]);
    EXPECT_EQ(sentinel, constantSurfaceStorage[1]);
    constantSurface.mockGfxAllocation.set32BitAllocation(false);
    prog->setConstantSurface(nullptr);
}

TEST_F(ProgramDataTest, GivenProgramWith32bitPointerOptWhenProgramScopeGlobalPointerPatchTokensAreReadThenGlobalPointerOffsetIsPatchedWith32bitPointer) {
    cl_device_id device = pPlatform->getClDevice(0);
    CreateProgramWithSource(pContext, &device, "CopyBuffer_simd16.cl");
    ASSERT_NE(nullptr, pProgram);

    MockProgram *prog = pProgram;

    // simulate case when constant surface was not allocated
    EXPECT_EQ(nullptr, prog->getConstantSurface());

    ProgramInfo programInfo;
    programInfo.prepareLinkerInputStorage();
    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.relocationSegment = NEO::SegmentType::GlobalVariables;
    relocInfo.symbolSegment = NEO::SegmentType::GlobalVariables;
    relocInfo.offset = 0U;
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    programInfo.linkerInput->addDataRelocationInfo(relocInfo);
    programInfo.linkerInput->setPointerSize(LinkerInput::Traits::PointerSize::Ptr32bit);

    MockBuffer globalSurface;
    ASSERT_LT(8U, globalSurface.getSize());
    prog->setGlobalSurface(&globalSurface.mockGfxAllocation);
    globalSurface.mockGfxAllocation.set32BitAllocation(true);
    uint32_t *globalSurfaceStorage = reinterpret_cast<uint32_t *>(globalSurface.getCpuAddress());
    uint32_t sentinel = 0x17192329U;
    globalSurfaceStorage[0] = 0U;
    globalSurfaceStorage[1] = sentinel;

    pProgram->linkerInput = std::move(programInfo.linkerInput);
    pProgram->linkBinary();
    uint32_t expectedAddr = static_cast<uint32_t>(globalSurface.getGraphicsAllocation()->getGpuAddressToPatch());
    EXPECT_EQ(expectedAddr, globalSurfaceStorage[0]);
    EXPECT_EQ(sentinel, globalSurfaceStorage[1]);
    globalSurface.mockGfxAllocation.set32BitAllocation(false);
    prog->setGlobalSurface(nullptr);
}

TEST_F(ProgramDataTest, givenSymbolTablePatchTokenThenLinkerInputIsCreated) {
    SPatchFunctionTableInfo token;
    token.Token = PATCH_TOKEN_PROGRAM_SYMBOL_TABLE;
    token.Size = static_cast<uint32_t>(sizeof(SPatchFunctionTableInfo));
    token.NumEntries = 0;

    pProgramPatchList = &token;
    programPatchListSize = token.Size;

    buildAndDecodeProgramPatchList();

    EXPECT_NE(nullptr, pProgram->getLinkerInput());
}

TEST(ProgramLinkBinaryTest, whenLinkerInputEmptyThenLinkSuccessful) {
    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    NEO::ExecutionEnvironment env;
    MockProgram program{env};
    program.linkerInput = std::move(linkerInput);
    auto ret = program.linkBinary();
    EXPECT_EQ(CL_SUCCESS, ret);
}

TEST(ProgramLinkBinaryTest, whenLinkerUnresolvedExternalThenLinkFailedAndBuildLogAvailable) {
    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    NEO::LinkerInput::RelocationInfo relocation = {};
    relocation.symbolName = "A";
    relocation.offset = 0;
    linkerInput->relocations.push_back(NEO::LinkerInput::Relocations{relocation});
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    NEO::ExecutionEnvironment env;
    MockProgram program{env};
    KernelInfo kernelInfo = {};
    kernelInfo.name = "onlyKernel";
    std::vector<char> kernelHeap;
    kernelHeap.resize(32, 7);
    kernelInfo.heapInfo.pKernelHeap = kernelHeap.data();
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader = {};
    kernelHeader.KernelHeapSize = static_cast<uint32_t>(kernelHeap.size());
    kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
    program.getKernelInfoArray().push_back(&kernelInfo);
    program.linkerInput = std::move(linkerInput);

    EXPECT_EQ(nullptr, program.getBuildLog(nullptr));
    auto ret = program.linkBinary();
    EXPECT_NE(CL_SUCCESS, ret);
    program.getKernelInfoArray().clear();
    auto buildLog = program.getBuildLog(nullptr);
    ASSERT_NE(nullptr, buildLog);
    Linker::UnresolvedExternals expectedUnresolvedExternals;
    expectedUnresolvedExternals.push_back(Linker::UnresolvedExternal{relocation, 0, false});
    auto expectedError = constructLinkerErrorMessage(expectedUnresolvedExternals, std::vector<std::string>{"kernel : " + kernelInfo.name});
    EXPECT_THAT(buildLog, ::testing::HasSubstr(expectedError));
}

TEST_F(ProgramDataTest, whenLinkerInputValidThenIsaIsProperlyPatched) {
    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->symbols["A"] = NEO::SymbolInfo{4U, 4U, NEO::SegmentType::GlobalVariables};
    linkerInput->symbols["B"] = NEO::SymbolInfo{8U, 4U, NEO::SegmentType::GlobalConstants};
    linkerInput->symbols["C"] = NEO::SymbolInfo{16U, 4U, NEO::SegmentType::Instructions};

    auto relocationType = NEO::LinkerInput::RelocationInfo::Type::Address;
    linkerInput->relocations.push_back({NEO::LinkerInput::RelocationInfo{"A", 8U, relocationType},
                                        NEO::LinkerInput::RelocationInfo{"B", 16U, relocationType},
                                        NEO::LinkerInput::RelocationInfo{"C", 24U, relocationType}});
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    linkerInput->exportedFunctionsSegmentId = 0;
    NEO::ExecutionEnvironment env;
    MockProgram program{env};
    KernelInfo kernelInfo = {};
    kernelInfo.name = "onlyKernel";
    std::vector<char> kernelHeap;
    kernelHeap.resize(32, 7);
    kernelInfo.heapInfo.pKernelHeap = kernelHeap.data();
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader = {};
    kernelHeader.KernelHeapSize = static_cast<uint32_t>(kernelHeap.size());
    kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
    MockGraphicsAllocation kernelIsa(kernelHeap.data(), kernelHeap.size());
    kernelInfo.kernelAllocation = &kernelIsa;
    program.getKernelInfoArray().push_back(&kernelInfo);
    program.linkerInput = std::move(linkerInput);

    program.exportedFunctionsSurface = kernelInfo.kernelAllocation;
    std::vector<char> globalVariablesBuffer;
    globalVariablesBuffer.resize(32, 7);
    std::vector<char> globalConstantsBuffer;
    globalConstantsBuffer.resize(32, 7);
    program.globalSurface = new MockGraphicsAllocation(globalVariablesBuffer.data(), globalVariablesBuffer.size());
    program.constantSurface = new MockGraphicsAllocation(globalConstantsBuffer.data(), globalConstantsBuffer.size());

    program.pDevice = this->pContext->getDevice(0);

    auto ret = program.linkBinary();
    EXPECT_EQ(CL_SUCCESS, ret);

    linkerInput.reset(static_cast<WhiteBox<LinkerInput> *>(program.linkerInput.release()));

    for (size_t i = 0; i < linkerInput->relocations.size(); ++i) {
        auto expectedPatch = program.globalSurface->getGpuAddress() + linkerInput->symbols[linkerInput->relocations[0][0].symbolName].offset;
        auto relocationAddress = kernelHeap.data() + linkerInput->relocations[0][0].offset;

        EXPECT_EQ(static_cast<uintptr_t>(expectedPatch), *reinterpret_cast<uintptr_t *>(relocationAddress)) << i;
    }

    program.getKernelInfoArray().clear();
    delete program.globalSurface;
    program.globalSurface = nullptr;
    delete program.constantSurface;
    program.constantSurface = nullptr;
}

TEST_F(ProgramDataTest, whenRelocationsAreNotNeededThenIsaIsPreserved) {
    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->symbols["A"] = NEO::SymbolInfo{4U, 4U, NEO::SegmentType::GlobalVariables};
    linkerInput->symbols["B"] = NEO::SymbolInfo{8U, 4U, NEO::SegmentType::GlobalConstants};

    NEO::ExecutionEnvironment env;
    MockProgram program{env};
    KernelInfo kernelInfo = {};
    kernelInfo.name = "onlyKernel";
    std::vector<char> kernelHeapData;
    kernelHeapData.resize(32, 7);
    std::vector<char> kernelHeap(kernelHeapData.begin(), kernelHeapData.end());
    kernelInfo.heapInfo.pKernelHeap = kernelHeap.data();
    iOpenCL::SKernelBinaryHeaderCommon kernelHeader = {};
    kernelHeader.KernelHeapSize = static_cast<uint32_t>(kernelHeap.size());
    kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
    MockGraphicsAllocation kernelIsa(kernelHeap.data(), kernelHeap.size());
    kernelInfo.kernelAllocation = &kernelIsa;
    program.getKernelInfoArray().push_back(&kernelInfo);
    program.linkerInput = std::move(linkerInput);

    std::vector<char> globalVariablesBuffer;
    globalVariablesBuffer.resize(32, 7);
    std::vector<char> globalConstantsBuffer;
    globalConstantsBuffer.resize(32, 7);
    program.globalSurface = new MockGraphicsAllocation(globalVariablesBuffer.data(), globalVariablesBuffer.size());
    program.constantSurface = new MockGraphicsAllocation(globalConstantsBuffer.data(), globalConstantsBuffer.size());

    program.pDevice = this->pContext->getDevice(0);

    auto ret = program.linkBinary();
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(kernelHeapData, kernelHeap);

    program.getKernelInfoArray().clear();
    delete program.globalSurface;
    program.globalSurface = nullptr;
    delete program.constantSurface;
    program.constantSurface = nullptr;
}
