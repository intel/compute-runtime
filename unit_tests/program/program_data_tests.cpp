/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/string.h"
#include "core/unit_tests/compiler_interface/linker_mock.h"
#include "runtime/memory_manager/allocations_list.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/unified_memory_manager.h"
#include "runtime/platform/platform.h"
#include "runtime/program/program.h"
#include "test.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/program/program_with_source.h"

using namespace NEO;

using namespace iOpenCL;
static const char constValue[] = "11223344";
static const char globalValue[] = "55667788";

template <typename ProgramType>
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
        cl_device_id device = pPlatform->getDevice(0);
        ContextFixture::SetUp(1, &device);
        ProgramFixture::SetUp();

        CreateProgramWithSource<ProgramType>(
            pContext,
            &device,
            "CopyBuffer_simd8.cl");
    }

    void TearDown() override {
        deleteDataReadFromFile(knownSource);
        ProgramFixture::TearDown();
        ContextFixture::TearDown();
        PlatformFixture::TearDown();
    }

    size_t setupConstantAllocation() {
        size_t constSize = strlen(constValue) + 1;

        EXPECT_EQ(nullptr, pProgram->getConstantSurface());

        SPatchAllocateConstantMemorySurfaceProgramBinaryInfo allocateConstMemorySurface;
        allocateConstMemorySurface.Token = PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
        allocateConstMemorySurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo) + constSize);

        allocateConstMemorySurface.ConstantBufferIndex = 0;
        allocateConstMemorySurface.InlineDataSize = static_cast<uint32_t>(constSize);

        pAllocateConstMemorySurface.reset(new cl_char[allocateConstMemorySurface.Size]);

        memcpy_s(pAllocateConstMemorySurface.get(),
                 sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo),
                 &allocateConstMemorySurface,
                 sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo));

        memcpy_s((cl_char *)pAllocateConstMemorySurface.get() + sizeof(allocateConstMemorySurface), constSize, constValue, constSize);

        pProgramPatchList = (void *)pAllocateConstMemorySurface.get();
        programPatchListSize = allocateConstMemorySurface.Size;
        return constSize;
    }

    size_t setupGlobalAllocation() {
        size_t globalSize = strlen(globalValue) + 1;

        EXPECT_EQ(nullptr, pProgram->getGlobalSurface());

        SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo allocateGlobalMemorySurface;
        allocateGlobalMemorySurface.Token = PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
        allocateGlobalMemorySurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo) + globalSize);

        allocateGlobalMemorySurface.GlobalBufferIndex = 0;
        allocateGlobalMemorySurface.InlineDataSize = static_cast<uint32_t>(globalSize);

        pAllocateGlobalMemorySurface.reset(new cl_char[allocateGlobalMemorySurface.Size]);

        memcpy_s(pAllocateGlobalMemorySurface.get(),
                 sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo),
                 &allocateGlobalMemorySurface,
                 sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo));

        memcpy_s((cl_char *)pAllocateGlobalMemorySurface.get() + sizeof(allocateGlobalMemorySurface), globalSize, globalValue, globalSize);

        pProgramPatchList = pAllocateGlobalMemorySurface.get();
        programPatchListSize = allocateGlobalMemorySurface.Size;
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

template <typename ProgramType>
void ProgramDataTestBase<ProgramType>::buildAndDecodeProgramPatchList() {
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
    pProgram->storeGenBinary(pProgramData, headerSize + programBinaryHeader.PatchListSize);

    error = pProgram->processGenBinary();
    patchlistDecodeErrorCode = error;
    if (allowDecodeFailure == false) {
        EXPECT_EQ(CL_SUCCESS, error);
    }
    delete[] pProgramData;
}

using ProgramDataTest = ProgramDataTestBase<NEO::Program>;
using MockProgramDataTest = ProgramDataTestBase<MockProgram>;

TEST_F(ProgramDataTest, EmptyProgramBinaryHeader) {
    buildAndDecodeProgramPatchList();
}

TEST_F(ProgramDataTest, AllocateConstantMemorySurfaceProgramBinaryInfo) {

    auto constSize = setupConstantAllocation();

    buildAndDecodeProgramPatchList();

    EXPECT_NE(nullptr, pProgram->getConstantSurface());
    EXPECT_EQ(0, memcmp(constValue, reinterpret_cast<char *>(pProgram->getConstantSurface()->getUnderlyingBuffer()), constSize));
}

TEST_F(MockProgramDataTest, whenGlobalConstantsAreExportedThenAllocateSurfacesAsSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    setupConstantAllocation();
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalConstants = true;
    static_cast<MockProgram *>(pProgram)->linkerInput = std::move(mockLinkerInput);

    buildAndDecodeProgramPatchList();

    ASSERT_NE(nullptr, pProgram->getConstantSurface());
    EXPECT_NE(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getConstantSurface()->getGpuAddress())));
}

TEST_F(MockProgramDataTest, whenGlobalConstantsAreNotExportedThenAllocateSurfacesAsNonSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    setupConstantAllocation();
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalConstants = false;
    static_cast<MockProgram *>(pProgram)->linkerInput = std::move(mockLinkerInput);
    static_cast<MockProgram *>(pProgram)->context = nullptr;

    buildAndDecodeProgramPatchList();

    static_cast<MockProgram *>(pProgram)->context = pContext;

    ASSERT_NE(nullptr, pProgram->getConstantSurface());
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getConstantSurface()->getGpuAddress())));
}

TEST_F(MockProgramDataTest, whenGlobalConstantsAreExportedButContextUnavailableThenAllocateSurfacesAsNonSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    setupConstantAllocation();
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalConstants = true;
    static_cast<MockProgram *>(pProgram)->linkerInput = std::move(mockLinkerInput);
    static_cast<MockProgram *>(pProgram)->context = nullptr;

    buildAndDecodeProgramPatchList();

    static_cast<MockProgram *>(pProgram)->context = pContext;

    ASSERT_NE(nullptr, pProgram->getConstantSurface());
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getConstantSurface()->getGpuAddress())));
}

TEST_F(MockProgramDataTest, whenGlobalVariablesAreExportedThenAllocateSurfacesAsSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }
    setupGlobalAllocation();
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalVariables = true;
    static_cast<MockProgram *>(pProgram)->linkerInput = std::move(mockLinkerInput);

    buildAndDecodeProgramPatchList();

    ASSERT_NE(nullptr, pProgram->getGlobalSurface());
    EXPECT_NE(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getGlobalSurface()->getGpuAddress())));
}

TEST_F(MockProgramDataTest, whenGlobalVariablesAreExportedButContextUnavailableThenAllocateSurfacesAsNonSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    setupGlobalAllocation();
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalVariables = true;
    static_cast<MockProgram *>(pProgram)->linkerInput = std::move(mockLinkerInput);
    static_cast<MockProgram *>(pProgram)->context = nullptr;

    buildAndDecodeProgramPatchList();

    static_cast<MockProgram *>(pProgram)->context = pContext;

    ASSERT_NE(nullptr, pProgram->getGlobalSurface());
    EXPECT_EQ(nullptr, this->pContext->getSVMAllocsManager()->getSVMAlloc(reinterpret_cast<const void *>(pProgram->getGlobalSurface()->getGpuAddress())));
}

TEST_F(MockProgramDataTest, whenGlobalVariablesAreNotExportedThenAllocateSurfacesAsNonSvm) {
    if (this->pContext->getSVMAllocsManager() == nullptr) {
        return;
    }

    setupGlobalAllocation();
    std::unique_ptr<WhiteBox<NEO::LinkerInput>> mockLinkerInput = std::make_unique<WhiteBox<NEO::LinkerInput>>();
    mockLinkerInput->traits.exportsGlobalVariables = false;
    static_cast<MockProgram *>(pProgram)->linkerInput = std::move(mockLinkerInput);
    static_cast<MockProgram *>(pProgram)->context = nullptr;

    buildAndDecodeProgramPatchList();

    static_cast<MockProgram *>(pProgram)->context = pContext;

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
    EXPECT_EQ(0, memcmp(constValue, reinterpret_cast<char *>(pProgram->getConstantSurface()->getUnderlyingBuffer()), constSize));

    if (is64bit) {
        EXPECT_TRUE(pProgram->getConstantSurface()->is32BitAllocation());
    }
}

TEST_F(ProgramDataTest, AllocateGlobalMemorySurfaceProgramBinaryInfo) {
    auto globalSize = setupGlobalAllocation();
    buildAndDecodeProgramPatchList();
    EXPECT_NE(nullptr, pProgram->getGlobalSurface());
    EXPECT_EQ(0, memcmp(globalValue, reinterpret_cast<char *>(pProgram->getGlobalSurface()->getUnderlyingBuffer()), globalSize));
}

TEST_F(ProgramDataTest, GlobalPointerProgramBinaryInfo) {
    char globalValue;
    char *pGlobalPointerValue = &globalValue;
    size_t globalPointerSize = sizeof(pGlobalPointerValue);
    char *ptr;

    // simulate case when global surface was not allocated
    EXPECT_EQ(nullptr, pProgram->getGlobalSurface());

    SPatchGlobalPointerProgramBinaryInfo globalPointer;
    globalPointer.Token = PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO;
    globalPointer.Size = sizeof(SPatchGlobalPointerProgramBinaryInfo);

    globalPointer.GlobalBufferIndex = 0;
    globalPointer.GlobalPointerOffset = 0;
    globalPointer.BufferIndex = 0;
    globalPointer.BufferType = PROGRAM_SCOPE_GLOBAL_BUFFER;

    cl_char *pGlobalPointer = new cl_char[globalPointer.Size];
    memcpy_s(pGlobalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo),
             &globalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pGlobalPointer;
    programPatchListSize = globalPointer.Size;

    this->allowDecodeFailure = true;
    buildAndDecodeProgramPatchList();
    EXPECT_EQ(nullptr, pProgram->getGlobalSurface());
    EXPECT_EQ(CL_INVALID_BINARY, this->patchlistDecodeErrorCode);
    this->allowDecodeFailure = false;

    delete[] pGlobalPointer;

    // regular case - global surface exists
    SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo allocateGlobalMemorySurface;
    allocateGlobalMemorySurface.Token = PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
    allocateGlobalMemorySurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo) + globalPointerSize);
    allocateGlobalMemorySurface.GlobalBufferIndex = 0;
    allocateGlobalMemorySurface.InlineDataSize = static_cast<uint32_t>(globalPointerSize);

    cl_char *pAllocateGlobalMemorySurface = new cl_char[allocateGlobalMemorySurface.Size];

    memcpy_s(pAllocateGlobalMemorySurface,
             sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo),
             &allocateGlobalMemorySurface,
             sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo));
    memcpy_s((cl_char *)pAllocateGlobalMemorySurface + sizeof(allocateGlobalMemorySurface), globalPointerSize, &pGlobalPointerValue, globalPointerSize);

    pProgramPatchList = (void *)pAllocateGlobalMemorySurface;
    programPatchListSize = allocateGlobalMemorySurface.Size;
    buildAndDecodeProgramPatchList();

    EXPECT_NE(nullptr, pProgram->getGlobalSurface());

    auto globalSurface = pProgram->getGlobalSurface();

    globalSurface->setCpuPtrAndGpuAddress(globalSurface->getUnderlyingBuffer(), globalSurface->getGpuAddress() + 1);
    EXPECT_NE(reinterpret_cast<uint64_t>(globalSurface->getUnderlyingBuffer()), globalSurface->getGpuAddress());
    EXPECT_EQ(0, memcmp(&pGlobalPointerValue, reinterpret_cast<char *>(globalSurface->getUnderlyingBuffer()), globalPointerSize));

    delete[] pAllocateGlobalMemorySurface;

    // global pointer to global surface - simulate invalid GlobalBufferIndex
    globalPointer.Token = PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO;
    globalPointer.Size = sizeof(SPatchGlobalPointerProgramBinaryInfo);

    globalPointer.GlobalBufferIndex = 10;
    globalPointer.GlobalPointerOffset = 0;
    globalPointer.BufferIndex = 0;
    globalPointer.BufferType = PROGRAM_SCOPE_GLOBAL_BUFFER;

    pGlobalPointer = new cl_char[globalPointer.Size];
    memcpy_s(pGlobalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo),
             &globalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pGlobalPointer;
    programPatchListSize = globalPointer.Size;

    buildAndDecodeProgramPatchList();

    EXPECT_EQ(0, memcmp(&pGlobalPointerValue, reinterpret_cast<char *>(globalSurface->getUnderlyingBuffer()), globalPointerSize));

    delete[] pGlobalPointer;

    // global pointer to global surface - simulate invalid BufferIndex
    globalPointer.Token = PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO;
    globalPointer.Size = sizeof(SPatchGlobalPointerProgramBinaryInfo);

    globalPointer.GlobalBufferIndex = 0;
    globalPointer.GlobalPointerOffset = 0;
    globalPointer.BufferIndex = 10;
    globalPointer.BufferType = PROGRAM_SCOPE_GLOBAL_BUFFER;

    pGlobalPointer = new cl_char[globalPointer.Size];
    memcpy_s(pGlobalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo),
             &globalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pGlobalPointer;
    programPatchListSize = globalPointer.Size;

    buildAndDecodeProgramPatchList();

    EXPECT_EQ(0, memcmp(&pGlobalPointerValue, reinterpret_cast<char *>(globalSurface->getUnderlyingBuffer()), globalPointerSize));

    delete[] pGlobalPointer;

    // global pointer to global surface - simulate invalid BufferType
    globalPointer.Token = PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO;
    globalPointer.Size = sizeof(SPatchGlobalPointerProgramBinaryInfo);

    globalPointer.GlobalBufferIndex = 0;
    globalPointer.GlobalPointerOffset = 0;
    globalPointer.BufferIndex = 0;
    globalPointer.BufferType = PROGRAM_SCOPE_CONSTANT_BUFFER;

    pGlobalPointer = new cl_char[globalPointer.Size];
    memcpy_s(pGlobalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo),
             &globalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pGlobalPointer;
    programPatchListSize = globalPointer.Size;

    buildAndDecodeProgramPatchList();

    EXPECT_EQ(0, memcmp(&pGlobalPointerValue, reinterpret_cast<char *>(globalSurface->getUnderlyingBuffer()), globalPointerSize));

    delete[] pGlobalPointer;

    // regular case - global pointer to global surface - all parameters valid
    globalPointer.Token = PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO;
    globalPointer.Size = sizeof(SPatchGlobalPointerProgramBinaryInfo);

    globalPointer.GlobalBufferIndex = 0;
    globalPointer.GlobalPointerOffset = 0;
    globalPointer.BufferIndex = 0;
    globalPointer.BufferType = PROGRAM_SCOPE_GLOBAL_BUFFER;

    pGlobalPointer = new cl_char[globalPointer.Size];
    memcpy_s(pGlobalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo),
             &globalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo));

    pProgramPatchList = (void *)pGlobalPointer;
    programPatchListSize = globalPointer.Size;
    buildAndDecodeProgramPatchList();

    if (!globalSurface->is32BitAllocation()) {
        EXPECT_NE(0, memcmp(&pGlobalPointerValue, reinterpret_cast<char *>(globalSurface->getUnderlyingBuffer()), globalPointerSize));
        ptr = pGlobalPointerValue + (globalSurface->getGpuAddressToPatch());
        EXPECT_EQ(0, memcmp(&ptr, reinterpret_cast<char *>(globalSurface->getUnderlyingBuffer()), globalPointerSize));
    }
    delete[] pGlobalPointer;
}

TEST_F(ProgramDataTest, Given32BitDeviceWhenGlobalMemorySurfaceIsPresentThenItHas32BitStorage) {
    char globalValue[] = "55667788";
    size_t globalSize = strlen(globalValue) + 1;
    this->pContext->getDevice(0)->getMemoryManager()->setForce32BitAllocations(true);
    EXPECT_EQ(nullptr, pProgram->getGlobalSurface());

    SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo allocateGlobalMemorySurface;
    allocateGlobalMemorySurface.Token = PATCH_TOKEN_ALLOCATE_GLOBAL_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
    allocateGlobalMemorySurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo) + globalSize);

    allocateGlobalMemorySurface.GlobalBufferIndex = 0;
    allocateGlobalMemorySurface.InlineDataSize = static_cast<uint32_t>(globalSize);

    cl_char *pAllocateGlobalMemorySurface = new cl_char[allocateGlobalMemorySurface.Size];

    memcpy_s(pAllocateGlobalMemorySurface,
             sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo),
             &allocateGlobalMemorySurface,
             sizeof(SPatchAllocateGlobalMemorySurfaceProgramBinaryInfo));

    memcpy_s((cl_char *)pAllocateGlobalMemorySurface + sizeof(allocateGlobalMemorySurface), globalSize, globalValue, globalSize);

    pProgramPatchList = (void *)pAllocateGlobalMemorySurface;
    programPatchListSize = allocateGlobalMemorySurface.Size;

    buildAndDecodeProgramPatchList();

    EXPECT_NE(nullptr, pProgram->getGlobalSurface());
    EXPECT_EQ(0, memcmp(globalValue, reinterpret_cast<char *>(pProgram->getGlobalSurface()->getUnderlyingBuffer()), globalSize));
    if (is64bit) {
        EXPECT_TRUE(pProgram->getGlobalSurface()->is32BitAllocation());
    }

    delete[] pAllocateGlobalMemorySurface;
}

TEST_F(ProgramDataTest, ConstantPointerProgramBinaryInfo) {
    const char *pConstantData = "01234567";
    size_t constantDataLen = strlen(pConstantData);

    // simulate case when constant surface was not allocated
    EXPECT_EQ(nullptr, pProgram->getConstantSurface());

    SPatchConstantPointerProgramBinaryInfo constantPointer;
    constantPointer.Token = PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO;
    constantPointer.Size = sizeof(SPatchConstantPointerProgramBinaryInfo);
    constantPointer.ConstantBufferIndex = 0;
    constantPointer.ConstantPointerOffset = 0;
    constantPointer.BufferIndex = 0;
    constantPointer.BufferType = PROGRAM_SCOPE_CONSTANT_BUFFER;

    cl_char *pConstantPointer = new cl_char[constantPointer.Size];
    memcpy_s(pConstantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo),
             &constantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pConstantPointer;
    programPatchListSize = constantPointer.Size;

    this->allowDecodeFailure = true;
    buildAndDecodeProgramPatchList();
    EXPECT_EQ(nullptr, pProgram->getConstantSurface());
    EXPECT_EQ(CL_INVALID_BINARY, this->patchlistDecodeErrorCode);
    this->allowDecodeFailure = false;

    delete[] pConstantPointer;

    // regular case - constant surface exists
    SPatchAllocateConstantMemorySurfaceProgramBinaryInfo allocateConstMemorySurface;
    allocateConstMemorySurface.Token = PATCH_TOKEN_ALLOCATE_CONSTANT_MEMORY_SURFACE_PROGRAM_BINARY_INFO;
    // note : + sizeof(uint64_t) is to accomodate for constant buffer offset
    allocateConstMemorySurface.Size = static_cast<uint32_t>(sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo) + constantDataLen + sizeof(uint64_t));

    allocateConstMemorySurface.ConstantBufferIndex = 0;
    allocateConstMemorySurface.InlineDataSize = static_cast<uint32_t>(constantDataLen + sizeof(uint64_t));

    auto pAllocateConstMemorySurface = std::unique_ptr<char>(new char[allocateConstMemorySurface.Size]);

    // copy the token header
    memcpy_s(pAllocateConstMemorySurface.get(),
             sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo),
             &allocateConstMemorySurface,
             sizeof(SPatchAllocateConstantMemorySurfaceProgramBinaryInfo));

    // copy the constant data
    memcpy_s((char *)pAllocateConstMemorySurface.get() + sizeof(allocateConstMemorySurface), constantDataLen, pConstantData, constantDataLen);

    // zero-out the constant buffer offset (will be patched during gen binary decoding)
    size_t constantBufferOffsetPatchOffset = constantDataLen;
    *(uint64_t *)((char *)pAllocateConstMemorySurface.get() + sizeof(allocateConstMemorySurface) + constantBufferOffsetPatchOffset) = 0U;

    pProgramPatchList = (void *)pAllocateConstMemorySurface.get();
    programPatchListSize = allocateConstMemorySurface.Size;

    buildAndDecodeProgramPatchList();

    EXPECT_NE(nullptr, pProgram->getConstantSurface());

    auto constantSurface = pProgram->getConstantSurface();
    constantSurface->setCpuPtrAndGpuAddress(constantSurface->getUnderlyingBuffer(), constantSurface->getGpuAddress() + 1);
    EXPECT_NE(reinterpret_cast<uint64_t>(constantSurface->getUnderlyingBuffer()), constantSurface->getGpuAddress());

    EXPECT_EQ(0, memcmp(pConstantData, reinterpret_cast<char *>(constantSurface->getUnderlyingBuffer()), constantDataLen));
    // there was no PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO, so constant buffer offset should be still 0
    EXPECT_EQ(0U, *(uint64_t *)(reinterpret_cast<char *>(constantSurface->getUnderlyingBuffer()) + constantBufferOffsetPatchOffset));
    // once finally constant buffer offset gets patched - the patch value depends on the bitness of the compute kernel
    auto patchOffsetValueStorage = std::unique_ptr<uint64_t>(new uint64_t); // 4bytes for 32-bit compute kernel, full 8byte for 64-bit compute kernel
    uint64_t *patchOffsetValue = patchOffsetValueStorage.get();
    if (constantSurface->is32BitAllocation() || (sizeof(void *) == 4)) {
        reinterpret_cast<uint32_t *>(patchOffsetValue)[0] = static_cast<uint32_t>(constantSurface->getGpuAddressToPatch());
        reinterpret_cast<uint32_t *>(patchOffsetValue)[1] = 0; // just pad with 0
    } else {
        // full 8 bytes
        *reinterpret_cast<uint64_t *>(patchOffsetValue) = constantSurface->getGpuAddressToPatch();
    }

    // constant pointer to constant surface - simulate invalid GlobalBufferIndex
    constantPointer.Token = PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO;
    constantPointer.Size = sizeof(SPatchConstantPointerProgramBinaryInfo);
    constantPointer.ConstantBufferIndex = 10;
    constantPointer.ConstantPointerOffset = 0;
    constantPointer.BufferIndex = 0;
    constantPointer.BufferType = PROGRAM_SCOPE_CONSTANT_BUFFER;

    pConstantPointer = new cl_char[constantPointer.Size];
    memcpy_s(pConstantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo),
             &constantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pConstantPointer;
    programPatchListSize = constantPointer.Size;

    buildAndDecodeProgramPatchList();
    EXPECT_EQ(0, memcmp(pConstantData, reinterpret_cast<char *>(constantSurface->getUnderlyingBuffer()), constantDataLen));
    // check that constant pointer offset was not patched
    EXPECT_EQ(0U, *(uint64_t *)(reinterpret_cast<char *>(constantSurface->getUnderlyingBuffer()) + constantBufferOffsetPatchOffset));
    // reset the constant pointer offset
    *(uint64_t *)((char *)constantSurface->getUnderlyingBuffer() + constantBufferOffsetPatchOffset) = 0U;

    delete[] pConstantPointer;

    // constant pointer to constant surface - simulate invalid BufferIndex
    constantPointer.Token = PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO;
    constantPointer.Size = sizeof(SPatchConstantPointerProgramBinaryInfo);
    constantPointer.ConstantBufferIndex = 0;
    constantPointer.ConstantPointerOffset = 0;
    constantPointer.BufferIndex = 10;
    constantPointer.BufferType = PROGRAM_SCOPE_CONSTANT_BUFFER;

    pConstantPointer = new cl_char[constantPointer.Size];
    memcpy_s(pConstantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo),
             &constantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pConstantPointer;
    programPatchListSize = constantPointer.Size;

    buildAndDecodeProgramPatchList();
    EXPECT_EQ(0, memcmp(pConstantData, reinterpret_cast<char *>(constantSurface->getUnderlyingBuffer()), constantDataLen));
    // check that constant pointer offset was not patched
    EXPECT_EQ(0U, *(uint64_t *)(reinterpret_cast<char *>(constantSurface->getUnderlyingBuffer()) + constantBufferOffsetPatchOffset));
    // reset the constant pointer offset
    *(uint64_t *)((char *)constantSurface->getUnderlyingBuffer() + constantBufferOffsetPatchOffset) = 0U;

    delete[] pConstantPointer;

    // constant pointer to constant surface - simulate invalid BufferType
    constantPointer.Token = PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO;
    constantPointer.Size = sizeof(SPatchConstantPointerProgramBinaryInfo);
    constantPointer.ConstantBufferIndex = 0;
    constantPointer.ConstantPointerOffset = 0;
    constantPointer.BufferIndex = 0;
    constantPointer.BufferType = PROGRAM_SCOPE_GLOBAL_BUFFER;

    pConstantPointer = new cl_char[constantPointer.Size];
    memcpy_s(pConstantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo),
             &constantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pConstantPointer;
    programPatchListSize = constantPointer.Size;

    buildAndDecodeProgramPatchList();
    EXPECT_EQ(0, memcmp(pConstantData, reinterpret_cast<char *>(constantSurface->getUnderlyingBuffer()), constantDataLen));
    // check that constant pointer offset was not patched
    EXPECT_EQ(0U, *(uint64_t *)(reinterpret_cast<char *>(constantSurface->getUnderlyingBuffer()) + constantBufferOffsetPatchOffset));
    // reset the constant pointer offset
    *(uint64_t *)((char *)constantSurface->getUnderlyingBuffer() + constantBufferOffsetPatchOffset) = 0U;

    delete[] pConstantPointer;

    // regular case - constant pointer to constant surface - all parameters valid
    constantPointer.Token = PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO;
    constantPointer.Size = sizeof(SPatchConstantPointerProgramBinaryInfo);
    constantPointer.ConstantBufferIndex = 0;
    constantPointer.ConstantPointerOffset = constantDataLen;
    constantPointer.BufferIndex = 0;
    constantPointer.BufferType = PROGRAM_SCOPE_CONSTANT_BUFFER;

    pConstantPointer = new cl_char[constantPointer.Size];
    memcpy_s(pConstantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo),
             &constantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pConstantPointer;
    programPatchListSize = constantPointer.Size;

    buildAndDecodeProgramPatchList();

    EXPECT_EQ(0, memcmp(pConstantData, reinterpret_cast<char *>(constantSurface->getUnderlyingBuffer()), constantDataLen));
    // check that constant pointer offset was patched
    EXPECT_EQ(*reinterpret_cast<uint64_t *>(patchOffsetValue), *(uint64_t *)(reinterpret_cast<char *>(constantSurface->getUnderlyingBuffer()) + constantBufferOffsetPatchOffset));

    delete[] pConstantPointer;
}

TEST_F(ProgramDataTest, GivenProgramWith32bitPointerOptWhenProgramScopeConstantBufferPatchTokensAreReadThenConstantPointerOffsetIsPatchedWith32bitPointer) {
    cl_device_id device = pPlatform->getDevice(0);
    CreateProgramWithSource<MockProgram>(pContext, &device, "CopyBuffer_simd8.cl");
    ASSERT_NE(nullptr, pProgram);

    MockProgram *prog = static_cast<MockProgram *>(pProgram);

    // simulate case when constant surface was not allocated
    EXPECT_EQ(nullptr, prog->getConstantSurface());

    SPatchConstantPointerProgramBinaryInfo constantPointer;
    constantPointer.Token = PATCH_TOKEN_CONSTANT_POINTER_PROGRAM_BINARY_INFO;
    constantPointer.Size = sizeof(SPatchConstantPointerProgramBinaryInfo);
    constantPointer.ConstantBufferIndex = 0;
    constantPointer.ConstantPointerOffset = 0;
    constantPointer.BufferIndex = 0;
    constantPointer.BufferType = PROGRAM_SCOPE_CONSTANT_BUFFER;

    auto pConstantPointer = std::unique_ptr<char[]>(new char[constantPointer.Size]);
    memcpy_s(pConstantPointer.get(),
             sizeof(SPatchConstantPointerProgramBinaryInfo),
             &constantPointer,
             sizeof(SPatchConstantPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pConstantPointer.get();
    programPatchListSize = constantPointer.Size;

    MockBuffer constantSurface;
    ASSERT_LT(8U, constantSurface.getSize());
    prog->setConstantSurface(&constantSurface.mockGfxAllocation);
    constantSurface.mockGfxAllocation.set32BitAllocation(true);
    uint32_t *constantSurfaceStorage = reinterpret_cast<uint32_t *>(constantSurface.getCpuAddress());
    uint32_t sentinel = 0x17192329U;
    constantSurfaceStorage[0] = 0U;
    constantSurfaceStorage[1] = sentinel;
    buildAndDecodeProgramPatchList();
    uint32_t expectedAddr = static_cast<uint32_t>(constantSurface.getGraphicsAllocation()->getGpuAddressToPatch());
    EXPECT_EQ(expectedAddr, constantSurfaceStorage[0]);
    EXPECT_EQ(sentinel, constantSurfaceStorage[1]);
    constantSurface.mockGfxAllocation.set32BitAllocation(false);
    prog->setConstantSurface(nullptr);
}

TEST_F(ProgramDataTest, GivenProgramWith32bitPointerOptWhenProgramScopeGlobalPointerPatchTokensAreReadThenGlobalPointerOffsetIsPatchedWith32bitPointer) {
    cl_device_id device = pPlatform->getDevice(0);
    CreateProgramWithSource<MockProgram>(pContext, &device, "CopyBuffer_simd8.cl");
    ASSERT_NE(nullptr, pProgram);

    MockProgram *prog = static_cast<MockProgram *>(pProgram);

    // simulate case when constant surface was not allocated
    EXPECT_EQ(nullptr, prog->getConstantSurface());

    SPatchGlobalPointerProgramBinaryInfo globalPointer;
    globalPointer.Token = PATCH_TOKEN_GLOBAL_POINTER_PROGRAM_BINARY_INFO;
    globalPointer.Size = sizeof(SPatchConstantPointerProgramBinaryInfo);
    globalPointer.GlobalBufferIndex = 0;
    globalPointer.GlobalPointerOffset = 0;
    globalPointer.BufferIndex = 0;
    globalPointer.BufferType = PROGRAM_SCOPE_GLOBAL_BUFFER;

    auto pGlobalPointer = std::unique_ptr<char[]>(new char[globalPointer.Size]);
    memcpy_s(pGlobalPointer.get(),
             sizeof(SPatchGlobalPointerProgramBinaryInfo),
             &globalPointer,
             sizeof(SPatchGlobalPointerProgramBinaryInfo));
    pProgramPatchList = (void *)pGlobalPointer.get();
    programPatchListSize = globalPointer.Size;

    MockBuffer globalSurface;
    ASSERT_LT(8U, globalSurface.getSize());
    prog->setGlobalSurface(&globalSurface.mockGfxAllocation);
    globalSurface.mockGfxAllocation.set32BitAllocation(true);
    uint32_t *globalSurfaceStorage = reinterpret_cast<uint32_t *>(globalSurface.getCpuAddress());
    uint32_t sentinel = 0x17192329U;
    globalSurfaceStorage[0] = 0U;
    globalSurfaceStorage[1] = sentinel;
    buildAndDecodeProgramPatchList();
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

TEST(ProgramLinkBinaryTest, whenPrepareLinkerInputStorageGetsCalledTwiceThenLinkerInputStorageIsReused) {
    ExecutionEnvironment execEnv;
    MockProgram program{execEnv};
    EXPECT_EQ(nullptr, program.linkerInput);
    program.prepareLinkerInputStorage();
    EXPECT_NE(nullptr, program.linkerInput);
    auto prevLinkerInput = program.getLinkerInput();
    program.prepareLinkerInputStorage();
    EXPECT_EQ(prevLinkerInput, program.linkerInput.get());
}

TEST_F(ProgramDataTest, whenLinkerInputValidThenIsaIsProperlyPatched) {
    auto linkerInput = std::make_unique<WhiteBox<LinkerInput>>();
    linkerInput->symbols["A"] = NEO::SymbolInfo{4U, 4U, NEO::SymbolInfo::GlobalVariable};
    linkerInput->symbols["B"] = NEO::SymbolInfo{8U, 4U, NEO::SymbolInfo::GlobalConstant};
    linkerInput->symbols["C"] = NEO::SymbolInfo{16U, 4U, NEO::SymbolInfo::Function};

    linkerInput->relocations.push_back({NEO::LinkerInput::RelocationInfo{"A", 8U}, NEO::LinkerInput::RelocationInfo{"B", 16U}, NEO::LinkerInput::RelocationInfo{"C", 24U}});
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
    linkerInput->symbols["A"] = NEO::SymbolInfo{4U, 4U, NEO::SymbolInfo::GlobalVariable};
    linkerInput->symbols["B"] = NEO::SymbolInfo{8U, 4U, NEO::SymbolInfo::GlobalConstant};

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
