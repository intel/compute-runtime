/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/program/program_info_from_patchtokens.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/compiler_interface/linker_mock.h"
#include "shared/test/unit_test/device_binary_format/patchtokens_tests.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

#include "opencl/source/platform/platform.h"
#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_with_source.h"

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
        pClDevice = pPlatform->getClDevice(0);
        rootDeviceIndex = pClDevice->getRootDeviceIndex();
        cl_device_id device = pClDevice;

        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();

        CreateProgramWithSource(
            pContext,
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

        EXPECT_EQ(nullptr, pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex()));

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

        EXPECT_EQ(nullptr, pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex()));

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
    ClDevice *pClDevice = nullptr;
    uint32_t rootDeviceIndex;
};

void ProgramDataTestBase::buildAndDecodeProgramPatchList() {
    size_t headerSize = sizeof(SProgramBinaryHeader);

    cl_int error = CL_SUCCESS;
    programBinaryHeader.Magic = 0x494E5443;
    programBinaryHeader.Version = CURRENT_ICBE_VERSION;
    programBinaryHeader.Device = defaultHwInfo->platform.eRenderCoreFamily;
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

    auto rootDeviceIndex = pPlatform->getClDevice(0)->getRootDeviceIndex();
    //as we use mock compiler in unit test, replace the genBinary here.
    pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(pProgramData, headerSize + programBinaryHeader.PatchListSize);
    pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = headerSize + programBinaryHeader.PatchListSize;

    error = pProgram->processGenBinary(*pClDevice);
    patchlistDecodeErrorCode = error;
    if (allowDecodeFailure == false) {
        EXPECT_EQ(CL_SUCCESS, error);
    }
    delete[] pProgramData;
}

using ProgramDataTest = ProgramDataTestBase;

TEST_F(ProgramDataTest, GivenEmptyProgramBinaryHeaderWhenBuildingAndDecodingThenSucessIsReturned) {
    buildAndDecodeProgramPatchList();
}

TEST_F(ProgramDataTest, WhenAllocatingConstantMemorySurfaceThenUnderlyingBufferIsSetCorrectly) {

    auto constSize = setupConstantAllocation();

    buildAndDecodeProgramPatchList();

    EXPECT_NE(nullptr, pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(0, memcmp(constValue, pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex())->getUnderlyingBuffer(), constSize));
}

TEST_F(ProgramDataTest, givenProgramWhenAllocatingConstantMemorySurfaceThenProperDeviceBitfieldIsPassed) {
    auto executionEnvironment = pClDevice->getExecutionEnvironment();
    auto memoryManager = new MockMemoryManager(*executionEnvironment);

    std::unique_ptr<MemoryManager> memoryManagerBackup(memoryManager);
    std::swap(memoryManagerBackup, executionEnvironment->memoryManager);
    EXPECT_NE(pClDevice->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);
    setupConstantAllocation();
    buildAndDecodeProgramPatchList();
    EXPECT_NE(nullptr, pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(pClDevice->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);
    std::swap(memoryManagerBackup, executionEnvironment->memoryManager);
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
    this->pProgram->processProgramInfo(programInfo, *pClDevice);

    ASSERT_NE(nullptr, pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_NE(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex())->getGpuAddress())));
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
    this->pProgram->processProgramInfo(programInfo, *pClDevice);

    ASSERT_NE(nullptr, pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(
                           pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex())->getGpuAddress())));
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

    this->pProgram->processProgramInfo(programInfo, *pClDevice);

    pProgram->context = pContext;

    ASSERT_NE(nullptr, pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(
                           pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex())->getGpuAddress())));
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
    this->pProgram->processProgramInfo(programInfo, *pClDevice);

    ASSERT_NE(nullptr, pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_NE(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex())->getGpuAddress())));
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

    this->pProgram->processProgramInfo(programInfo, *pClDevice);

    pProgram->context = pContext;

    ASSERT_NE(nullptr, pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex())->getGpuAddress())));
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
    this->pProgram->processProgramInfo(programInfo, *pClDevice);

    ASSERT_NE(nullptr, pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex())->getGpuAddress())));
}

TEST_F(ProgramDataTest, givenConstantAllocationThatIsInUseByGpuWhenProgramIsBeingDestroyedThenItIsAddedToTemporaryAllocationList) {

    setupConstantAllocation();

    buildAndDecodeProgramPatchList();

    auto &csr = *pPlatform->getClDevice(0)->getDefaultEngine().commandStreamReceiver;
    auto tagAddress = csr.getTagAddress();
    auto constantSurface = pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex());
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

    auto &csr = *pPlatform->getClDevice(0)->getDefaultEngine().commandStreamReceiver;
    auto tagAddress = csr.getTagAddress();
    auto globalSurface = pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex());
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

    EXPECT_NE(nullptr, pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(0, memcmp(constValue, pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex())->getUnderlyingBuffer(), constSize));

    if constexpr (is64bit) {
        EXPECT_TRUE(pProgram->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex())->is32BitAllocation());
    }
}

TEST_F(ProgramDataTest, WhenAllocatingGlobalMemorySurfaceThenUnderlyingBufferIsSetCorrectly) {
    auto globalSize = setupGlobalAllocation();
    buildAndDecodeProgramPatchList();
    EXPECT_NE(nullptr, pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(0, memcmp(globalValue, pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex())->getUnderlyingBuffer(), globalSize));
}
TEST_F(ProgramDataTest, givenProgramWhenAllocatingGlobalMemorySurfaceThenProperDeviceBitfieldIsPassed) {
    auto executionEnvironment = pClDevice->getExecutionEnvironment();
    auto memoryManager = new MockMemoryManager(*executionEnvironment);

    std::unique_ptr<MemoryManager> memoryManagerBackup(memoryManager);
    std::swap(memoryManagerBackup, executionEnvironment->memoryManager);
    EXPECT_NE(pClDevice->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);

    setupGlobalAllocation();
    buildAndDecodeProgramPatchList();
    EXPECT_NE(nullptr, pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(pClDevice->getDeviceBitfield(), memoryManager->recentlyPassedDeviceBitfield);
    std::swap(memoryManagerBackup, executionEnvironment->memoryManager);
}

TEST_F(ProgramDataTest, Given32BitDeviceWhenGlobalMemorySurfaceIsPresentThenItHas32BitStorage) {
    char globalValue[] = "55667788";
    size_t globalSize = strlen(globalValue) + 1;
    this->pContext->getDevice(0)->getMemoryManager()->setForce32BitAllocations(true);
    EXPECT_EQ(nullptr, pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex()));

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

    EXPECT_NE(nullptr, pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(0, memcmp(globalValue, pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex())->getUnderlyingBuffer(), globalSize));
    if constexpr (is64bit) {
        EXPECT_TRUE(pProgram->getGlobalSurface(pContext->getDevice(0)->getRootDeviceIndex())->is32BitAllocation());
    }

    delete[] pAllocateGlobalMemorySurface;
}

TEST(ProgramScopeMetadataTest, WhenPatchingGlobalSurfaceThenPickProperSourceBuffer) {
    MockExecutionEnvironment execEnv;
    execEnv.incRefInternal();
    MockClDevice device{new MockDevice(&execEnv, 0)};
    execEnv.memoryManager = std::make_unique<MockMemoryManager>();
    PatchTokensTestData::ValidProgramWithMixedGlobalVarAndConstSurfacesAndPointers decodedProgram;
    decodedProgram.globalPointerMutable->GlobalPointerOffset = 0U;
    decodedProgram.constantPointerMutable->ConstantPointerOffset = 0U;
    memset(decodedProgram.globalSurfMutable + 1, 0U, sizeof(uintptr_t));
    memset(decodedProgram.constSurfMutable + 1, 0U, sizeof(uintptr_t));
    ProgramInfo programInfo;
    MockProgram program(toClDeviceVector(device));
    NEO::populateProgramInfo(programInfo, decodedProgram);
    program.processProgramInfo(programInfo, device);
    auto &buildInfo = program.buildInfos[device.getRootDeviceIndex()];
    ASSERT_NE(nullptr, buildInfo.globalSurface);
    ASSERT_NE(nullptr, buildInfo.constantSurface);
    ASSERT_NE(nullptr, buildInfo.globalSurface->getUnderlyingBuffer());
    ASSERT_NE(nullptr, buildInfo.constantSurface->getUnderlyingBuffer());
    EXPECT_EQ(static_cast<uintptr_t>(buildInfo.globalSurface->getGpuAddressToPatch()), *reinterpret_cast<uintptr_t *>(buildInfo.constantSurface->getUnderlyingBuffer()));
    EXPECT_EQ(static_cast<uintptr_t>(buildInfo.constantSurface->getGpuAddressToPatch()), *reinterpret_cast<uintptr_t *>(buildInfo.globalSurface->getUnderlyingBuffer()));
}

TEST_F(ProgramDataTest, GivenProgramWith32bitPointerOptWhenProgramScopeConstantBufferPatchTokensAreReadThenConstantPointerOffsetIsPatchedWith32bitPointer) {
    CreateProgramWithSource(pContext, "CopyBuffer_simd16.cl");
    ASSERT_NE(nullptr, pProgram);

    MockProgram *prog = pProgram;

    // simulate case when constant surface was not allocated
    EXPECT_EQ(nullptr, prog->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex()));
    ProgramInfo programInfo;
    programInfo.prepareLinkerInputStorage();

    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.relocationSegment = NEO::SegmentType::GlobalConstants;
    relocInfo.offset = 0U;
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    relocInfo.symbolName = "GlobalConstantPointer";

    NEO::SymbolInfo symbol = {};
    symbol.offset = 0U;
    symbol.size = 8U;
    symbol.segment = NEO::SegmentType::GlobalConstants;

    programInfo.linkerInput->addSymbol("GlobalConstantPointer", symbol);
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

    programInfo.globalConstants.initData = constantSurface.mockGfxAllocation.getUnderlyingBuffer();

    pProgram->setLinkerInput(pClDevice->getRootDeviceIndex(), std::move(programInfo.linkerInput));
    pProgram->linkBinary(&pClDevice->getDevice(), programInfo.globalConstants.initData, programInfo.globalVariables.initData, {}, prog->externalFunctions);
    uint32_t expectedAddr = static_cast<uint32_t>(constantSurface.getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddressToPatch());
    EXPECT_EQ(expectedAddr, constantSurfaceStorage[0]);
    EXPECT_EQ(sentinel, constantSurfaceStorage[1]);
    constantSurface.mockGfxAllocation.set32BitAllocation(false);
    prog->setConstantSurface(nullptr);
}

TEST_F(ProgramDataTest, GivenProgramWith32bitPointerOptWhenProgramScopeGlobalPointerPatchTokensAreReadThenGlobalPointerOffsetIsPatchedWith32bitPointer) {
    CreateProgramWithSource(pContext, "CopyBuffer_simd16.cl");
    ASSERT_NE(nullptr, pProgram);

    MockProgram *prog = pProgram;

    // simulate case when constant surface was not allocated
    EXPECT_EQ(nullptr, prog->getConstantSurface(pContext->getDevice(0)->getRootDeviceIndex()));

    ProgramInfo programInfo;
    programInfo.prepareLinkerInputStorage();
    NEO::LinkerInput::RelocationInfo relocInfo;
    relocInfo.offset = 0U;
    relocInfo.type = NEO::LinkerInput::RelocationInfo::Type::Address;
    relocInfo.relocationSegment = NEO::SegmentType::GlobalVariables;
    relocInfo.symbolName = "GlobalVariablePointer";

    NEO::SymbolInfo symbol = {};
    symbol.offset = 0U;
    symbol.size = 8U;
    symbol.segment = NEO::SegmentType::GlobalVariables;

    programInfo.linkerInput->addSymbol("GlobalVariablePointer", symbol);
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

    programInfo.globalVariables.initData = globalSurface.mockGfxAllocation.getUnderlyingBuffer();

    pProgram->setLinkerInput(pClDevice->getRootDeviceIndex(), std::move(programInfo.linkerInput));
    pProgram->linkBinary(&pClDevice->getDevice(), programInfo.globalConstants.initData, programInfo.globalVariables.initData, {}, prog->externalFunctions);
    uint32_t expectedAddr = static_cast<uint32_t>(globalSurface.getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddressToPatch());
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

    EXPECT_NE(nullptr, pProgram->getLinkerInput(pContext->getDevice(0)->getRootDeviceIndex()));
}

TEST(ProgramLinkBinaryTest, whenLinkerInputEmptyThenLinkSuccessful) {
    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockProgram program{nullptr, false, toClDeviceVector(*device)};
    program.setLinkerInput(device->getRootDeviceIndex(), std::move(linkerInput));
    auto ret = program.linkBinary(&device->getDevice(), nullptr, nullptr, {}, program.externalFunctions);
    EXPECT_EQ(CL_SUCCESS, ret);
}

TEST(ProgramLinkBinaryTest, whenLinkerUnresolvedExternalThenLinkFailedAndBuildLogAvailable) {
    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    NEO::LinkerInput::RelocationInfo relocation = {};
    relocation.symbolName = "A";
    relocation.offset = 0;
    linkerInput->relocations.push_back(NEO::LinkerInput::Relocations{relocation});
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto rootDeviceIndex = device->getRootDeviceIndex();
    MockProgram program{nullptr, false, toClDeviceVector(*device)};
    KernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "onlyKernel";
    std::vector<char> kernelHeap;
    kernelHeap.resize(32, 7);
    kernelInfo.heapInfo.pKernelHeap = kernelHeap.data();
    kernelInfo.heapInfo.KernelHeapSize = static_cast<uint32_t>(kernelHeap.size());
    program.getKernelInfoArray(rootDeviceIndex).push_back(&kernelInfo);
    program.setLinkerInput(rootDeviceIndex, std::move(linkerInput));

    std::string buildLog = program.getBuildLog(device->getRootDeviceIndex());
    EXPECT_TRUE(buildLog.empty());
    auto ret = program.linkBinary(&device->getDevice(), nullptr, nullptr, {}, program.externalFunctions);
    EXPECT_NE(CL_SUCCESS, ret);
    program.getKernelInfoArray(rootDeviceIndex).clear();
    buildLog = program.getBuildLog(rootDeviceIndex);
    EXPECT_FALSE(buildLog.empty());
    Linker::UnresolvedExternals expectedUnresolvedExternals;
    expectedUnresolvedExternals.push_back(Linker::UnresolvedExternal{relocation, 0, false});
    auto expectedError = constructLinkerErrorMessage(expectedUnresolvedExternals, std::vector<std::string>{"kernel : " + kernelInfo.kernelDescriptor.kernelMetadata.kernelName});
    EXPECT_TRUE(hasSubstr(buildLog, expectedError));
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
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockProgram program{nullptr, false, toClDeviceVector(*device)};
    auto &buildInfo = program.buildInfos[device->getRootDeviceIndex()];
    KernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "onlyKernel";
    std::vector<char> kernelHeap;
    kernelHeap.resize(32, 7);
    kernelInfo.heapInfo.pKernelHeap = kernelHeap.data();
    kernelInfo.heapInfo.KernelHeapSize = static_cast<uint32_t>(kernelHeap.size());
    MockGraphicsAllocation kernelIsa(kernelHeap.data(), kernelHeap.size());
    kernelInfo.kernelAllocation = &kernelIsa;
    program.getKernelInfoArray(rootDeviceIndex).push_back(&kernelInfo);
    program.setLinkerInput(device->getRootDeviceIndex(), std::move(linkerInput));

    buildInfo.exportedFunctionsSurface = kernelInfo.kernelAllocation;
    std::vector<char> globalVariablesBuffer;
    globalVariablesBuffer.resize(32, 7);
    std::vector<char> globalConstantsBuffer;
    globalConstantsBuffer.resize(32, 7);
    std::vector<char> globalVariablesInitData{32, 0};
    std::vector<char> globalConstantsInitData{32, 0};
    buildInfo.globalSurface = new MockGraphicsAllocation(globalVariablesBuffer.data(), globalVariablesBuffer.size());
    buildInfo.constantSurface = new MockGraphicsAllocation(globalConstantsBuffer.data(), globalConstantsBuffer.size());

    auto ret = program.linkBinary(&pClDevice->getDevice(), globalConstantsInitData.data(), globalVariablesInitData.data(), {}, program.externalFunctions);
    EXPECT_EQ(CL_SUCCESS, ret);

    linkerInput.reset(static_cast<WhiteBox<LinkerInput> *>(buildInfo.linkerInput.release()));

    for (size_t i = 0; i < linkerInput->relocations.size(); ++i) {
        auto expectedPatch = buildInfo.globalSurface->getGpuAddress() + linkerInput->symbols[linkerInput->relocations[0][0].symbolName].offset;
        auto relocationAddress = kernelHeap.data() + linkerInput->relocations[0][0].offset;

        EXPECT_EQ(static_cast<uintptr_t>(expectedPatch), *reinterpret_cast<uintptr_t *>(relocationAddress)) << i;
    }

    program.getKernelInfoArray(rootDeviceIndex).clear();
    delete buildInfo.globalSurface;
    buildInfo.globalSurface = nullptr;
    delete buildInfo.constantSurface;
    buildInfo.constantSurface = nullptr;
}

TEST_F(ProgramDataTest, whenRelocationsAreNotNeededThenIsaIsPreserved) {
    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->symbols["A"] = NEO::SymbolInfo{4U, 4U, NEO::SegmentType::GlobalVariables};
    linkerInput->symbols["B"] = NEO::SymbolInfo{8U, 4U, NEO::SegmentType::GlobalConstants};

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockProgram program{nullptr, false, toClDeviceVector(*device)};
    auto &buildInfo = program.buildInfos[device->getRootDeviceIndex()];
    KernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "onlyKernel";
    std::vector<char> kernelHeapData;
    kernelHeapData.resize(32, 7);
    std::vector<char> kernelHeap(kernelHeapData.begin(), kernelHeapData.end());
    kernelInfo.heapInfo.pKernelHeap = kernelHeap.data();
    kernelInfo.heapInfo.KernelHeapSize = static_cast<uint32_t>(kernelHeap.size());
    MockGraphicsAllocation kernelIsa(kernelHeap.data(), kernelHeap.size());
    kernelInfo.kernelAllocation = &kernelIsa;
    program.getKernelInfoArray(rootDeviceIndex).push_back(&kernelInfo);
    program.setLinkerInput(rootDeviceIndex, std::move(linkerInput));

    std::vector<char> globalVariablesBuffer;
    globalVariablesBuffer.resize(32, 7);
    std::vector<char> globalConstantsBuffer;
    globalConstantsBuffer.resize(32, 7);
    std::vector<char> globalVariablesInitData{32, 0};
    std::vector<char> globalConstantsInitData{32, 0};
    buildInfo.globalSurface = new MockGraphicsAllocation(globalVariablesBuffer.data(), globalVariablesBuffer.size());
    buildInfo.constantSurface = new MockGraphicsAllocation(globalConstantsBuffer.data(), globalConstantsBuffer.size());

    auto ret = program.linkBinary(&pClDevice->getDevice(), globalConstantsInitData.data(), globalVariablesInitData.data(), {}, program.externalFunctions);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(kernelHeapData, kernelHeap);

    program.getKernelInfoArray(rootDeviceIndex).clear();
    delete buildInfo.globalSurface;
    buildInfo.globalSurface = nullptr;
    delete buildInfo.constantSurface;
    buildInfo.constantSurface = nullptr;
}

TEST(ProgramStringSectionTest, WhenConstStringBufferIsPresentThenUseItForLinking) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto rootDeviceIndex = device->getRootDeviceIndex();

    MockProgram program{nullptr, false, toClDeviceVector(*device)};

    uint8_t kernelHeapData[64] = {};
    MockGraphicsAllocation kernelIsa(kernelHeapData, 64);

    KernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "onlyKernel";
    kernelInfo.heapInfo.pKernelHeap = kernelHeapData;
    kernelInfo.heapInfo.KernelHeapSize = 64;
    kernelInfo.kernelAllocation = &kernelIsa;

    program.getKernelInfoArray(rootDeviceIndex).push_back(&kernelInfo);

    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->relocations.push_back({{".str", 0x8, LinkerInput::RelocationInfo::Type::Address, SegmentType::Instructions}});
    linkerInput->symbols.insert({".str", {0x0, 0x8, SegmentType::GlobalStrings}});
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;

    program.setLinkerInput(rootDeviceIndex, std::move(linkerInput));

    auto isaCpuPtr = reinterpret_cast<char *>(kernelInfo.getGraphicsAllocation()->getUnderlyingBuffer());
    auto patchAddr = ptrOffset(isaCpuPtr, 0x8);

    const char constStringData[] = "Hello World!\n";
    auto stringsAddr = reinterpret_cast<uintptr_t>(constStringData);

    auto ret = program.linkBinary(&device->getDevice(), nullptr, nullptr, {constStringData, sizeof(constStringData)}, program.externalFunctions);
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(static_cast<size_t>(stringsAddr), *reinterpret_cast<size_t *>(patchAddr));

    program.getKernelInfoArray(rootDeviceIndex).clear();
}

TEST(ProgramImplicitArgsTest, givenImplicitRelocationAndStackCallsThenKernelRequiresImplicitArgs) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto rootDeviceIndex = device->getRootDeviceIndex();
    MockProgram program{nullptr, false, toClDeviceVector(*device)};
    KernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "onlyKernel";
    kernelInfo.kernelDescriptor.kernelAttributes.flags.useStackCalls = true;
    uint8_t kernelHeapData[64] = {};
    kernelInfo.heapInfo.pKernelHeap = kernelHeapData;
    kernelInfo.heapInfo.KernelHeapSize = 64;
    MockGraphicsAllocation kernelIsa(kernelHeapData, 64);
    kernelInfo.kernelAllocation = &kernelIsa;
    program.getKernelInfoArray(rootDeviceIndex).push_back(&kernelInfo);

    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->relocations.push_back({{implicitArgsRelocationSymbolNames[0], 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    program.setLinkerInput(rootDeviceIndex, std::move(linkerInput));
    auto ret = program.linkBinary(&device->getDevice(), nullptr, nullptr, {}, program.externalFunctions);
    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_TRUE(kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
    program.getKernelInfoArray(rootDeviceIndex).clear();
}

TEST(ProgramImplicitArgsTest, givenImplicitRelocationAndEnabledDebuggerThenKernelRequiresImplicitArgs) {
    if (!defaultHwInfo->capabilityTable.debuggerSupported) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableMockSourceLevelDebugger.set(1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    EXPECT_NE(nullptr, device->getDebugger());
    auto rootDeviceIndex = device->getRootDeviceIndex();
    MockProgram program{nullptr, false, toClDeviceVector(*device)};
    KernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "onlyKernel";
    kernelInfo.kernelDescriptor.kernelAttributes.flags.useStackCalls = false;
    uint8_t kernelHeapData[64] = {};
    kernelInfo.heapInfo.pKernelHeap = kernelHeapData;
    kernelInfo.heapInfo.KernelHeapSize = 64;
    MockGraphicsAllocation kernelIsa(kernelHeapData, 64);
    kernelInfo.kernelAllocation = &kernelIsa;
    program.getKernelInfoArray(rootDeviceIndex).push_back(&kernelInfo);

    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->relocations.push_back({{implicitArgsRelocationSymbolNames[0], 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    program.setLinkerInput(rootDeviceIndex, std::move(linkerInput));
    auto ret = program.linkBinary(&device->getDevice(), nullptr, nullptr, {}, program.externalFunctions);
    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_TRUE(kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
    program.getKernelInfoArray(rootDeviceIndex).clear();
}

TEST(ProgramImplicitArgsTest, givenImplicitRelocationAndNoStackCallsAndDisabledDebuggerThenKernelDoesntRequireImplicitArgs) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    EXPECT_EQ(nullptr, device->getDebugger());
    auto rootDeviceIndex = device->getRootDeviceIndex();
    MockProgram program{nullptr, false, toClDeviceVector(*device)};
    KernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelMetadata.kernelName = "onlyKernel";
    kernelInfo.kernelDescriptor.kernelAttributes.flags.useStackCalls = false;
    uint8_t kernelHeapData[64] = {};
    kernelInfo.heapInfo.pKernelHeap = kernelHeapData;
    kernelInfo.heapInfo.KernelHeapSize = 64;
    MockGraphicsAllocation kernelIsa(kernelHeapData, 64);
    kernelInfo.kernelAllocation = &kernelIsa;
    program.getKernelInfoArray(rootDeviceIndex).push_back(&kernelInfo);

    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->relocations.push_back({{implicitArgsRelocationSymbolNames[0], 0x8, LinkerInput::RelocationInfo::Type::AddressLow, SegmentType::Instructions}});
    linkerInput->traits.requiresPatchingOfInstructionSegments = true;
    program.setLinkerInput(rootDeviceIndex, std::move(linkerInput));
    auto ret = program.linkBinary(&device->getDevice(), nullptr, nullptr, {}, program.externalFunctions);
    EXPECT_EQ(CL_SUCCESS, ret);

    EXPECT_FALSE(kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs);
    program.getKernelInfoArray(rootDeviceIndex).clear();
}
