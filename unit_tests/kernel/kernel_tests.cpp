/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/helpers/options.h"
#include "runtime/kernel/kernel.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/execution_model_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/gtest_helpers.h"
#include "test.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/program/program_from_binary.h"
#include "unit_tests/program/program_tests.h"

#include <memory>

using namespace OCLRT;

class KernelTest : public ProgramFromBinaryTest {
  public:
    KernelTest() {
    }

    ~KernelTest() override = default;

  protected:
    void SetUp() override {
        ProgramFromBinaryTest::SetUp();
        ASSERT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        cl_device_id device = pDevice;
        retVal = pProgram->build(
            1,
            &device,
            nullptr,
            nullptr,
            nullptr,
            false);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // create a kernel
        pKernel = Kernel::create(
            pProgram,
            *pProgram->getKernelInfo(KernelName),
            &retVal);

        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pKernel);
    }

    void TearDown() override {
        delete pKernel;
        pKernel = nullptr;
        deleteDataReadFromFile(knownSource);
        ProgramFromBinaryTest::TearDown();
    }

    Kernel *pKernel = nullptr;
    cl_int retVal = CL_SUCCESS;
};

TEST(KernelTest, isMemObj) {
    EXPECT_TRUE(Kernel::isMemObj(Kernel::BUFFER_OBJ));
    EXPECT_TRUE(Kernel::isMemObj(Kernel::IMAGE_OBJ));
    EXPECT_TRUE(Kernel::isMemObj(Kernel::PIPE_OBJ));

    EXPECT_FALSE(Kernel::isMemObj(Kernel::SAMPLER_OBJ));
    EXPECT_FALSE(Kernel::isMemObj(Kernel::ACCELERATOR_OBJ));
    EXPECT_FALSE(Kernel::isMemObj(Kernel::NONE_OBJ));
    EXPECT_FALSE(Kernel::isMemObj(Kernel::SVM_ALLOC_OBJ));
}

TEST_P(KernelTest, getKernelHeap) {
    EXPECT_EQ(pKernel->getKernelInfo().heapInfo.pKernelHeap, pKernel->getKernelHeap());
    EXPECT_EQ(pKernel->getKernelInfo().heapInfo.pKernelHeader->KernelHeapSize, pKernel->getKernelHeapSize());
}

TEST_P(KernelTest, Create_Simple) {
    // included in the setup of fixture
}

TEST_P(KernelTest, GetInfo_InvalidParamName) {
    size_t paramValueSizeRet = 0;

    // get size
    retVal = pKernel->getInfo(
        0,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_P(KernelTest, GetInfo_Name) {
    cl_kernel_info paramName = CL_KERNEL_FUNCTION_NAME;
    size_t paramValueSize = 0;
    char *paramValue = nullptr;
    size_t paramValueSizeRet = 0;

    // get size
    retVal = pKernel->getInfo(
        paramName,
        paramValueSize,
        nullptr,
        &paramValueSizeRet);
    EXPECT_NE(0u, paramValueSizeRet);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // allocate space for name
    paramValue = new char[paramValueSizeRet];

    // get the name
    paramValueSize = paramValueSizeRet;

    retVal = pKernel->getInfo(
        paramName,
        paramValueSize,
        paramValue,
        nullptr);

    EXPECT_NE(nullptr, paramValue);
    EXPECT_EQ(0, strcmp(paramValue, KernelName));
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] paramValue;
}

TEST_P(KernelTest, GetInfo_BinaryProgramIntel) {
    cl_kernel_info paramName = CL_KERNEL_BINARY_PROGRAM_INTEL;
    size_t paramValueSize = 0;
    char *paramValue = nullptr;
    size_t paramValueSizeRet = 0;
    const char *pKernelData = reinterpret_cast<const char *>(pKernel->getKernelHeap());
    EXPECT_NE(nullptr, pKernelData);

    // get size of kernel binary
    retVal = pKernel->getInfo(
        paramName,
        paramValueSize,
        nullptr,
        &paramValueSizeRet);
    EXPECT_NE(0u, paramValueSizeRet);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // allocate space for kernel binary
    paramValue = new char[paramValueSizeRet];

    // get kernel binary
    paramValueSize = paramValueSizeRet;
    retVal = pKernel->getInfo(
        paramName,
        paramValueSize,
        paramValue,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, paramValue);
    EXPECT_EQ(0, memcmp(paramValue, pKernelData, paramValueSize));

    delete[] paramValue;
}

TEST_P(KernelTest, GetInfo_NumArgs) {
    cl_kernel_info paramName = CL_KERNEL_NUM_ARGS;
    size_t paramValueSize = sizeof(cl_uint);
    cl_uint paramValue = 0;
    size_t paramValueSizeRet = 0;

    // get size
    retVal = pKernel->getInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(sizeof(cl_uint), paramValueSizeRet);
    EXPECT_EQ(2u, paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_P(KernelTest, GetInfo_Program) {
    cl_kernel_info paramName = CL_KERNEL_PROGRAM;
    size_t paramValueSize = sizeof(cl_program);
    cl_program paramValue = 0;
    size_t paramValueSizeRet = 0;
    cl_program prog = pProgram;

    // get size
    retVal = pKernel->getInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_program), paramValueSizeRet);
    EXPECT_EQ(prog, paramValue);
}

TEST_P(KernelTest, GetInfo_Context) {
    cl_kernel_info paramName = CL_KERNEL_CONTEXT;
    cl_context paramValue = 0;
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;
    cl_context context = pContext;

    // get size
    retVal = pKernel->getInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
    EXPECT_EQ(context, paramValue);
}

TEST_P(KernelTest, GetWorkGroupInfo_WorkgroupSize) {
    cl_kernel_info paramName = CL_KERNEL_WORK_GROUP_SIZE;
    size_t paramValue = 0;
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    retVal = pKernel->getWorkGroupInfo(
        pDevice,
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
    EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize, paramValue);
}

TEST_P(KernelTest, GetWorkGroupInfo_CompileWorkgroupSize) {
    cl_kernel_info paramName = CL_KERNEL_COMPILE_WORK_GROUP_SIZE;
    size_t paramValue[3];
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    retVal = pKernel->getWorkGroupInfo(
        pDevice,
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
}

INSTANTIATE_TEST_CASE_P(KernelTests,
                        KernelTest,
                        ::testing::Combine(
                            ::testing::ValuesIn(BinaryFileNames),
                            ::testing::ValuesIn(KernelNames)));

class KernelFromBinaryTest : public ProgramFromBinarySimpleTest,
                             public MemoryManagementFixture {
  public:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        ProgramFromBinarySimpleTest::SetUp();
    }
    void TearDown() override {
        ProgramFromBinarySimpleTest::TearDown();
        MemoryManagementFixture::TearDown();
    }
};
typedef Test<KernelFromBinaryTest> KernelFromBinaryTests;

TEST_F(KernelFromBinaryTests, getInfo_NumArgs) {
    cl_device_id device = pDevice;

    CreateProgramFromBinary<Program>(pContext, &device, "kernel_num_args");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("test");

    // create a kernel
    auto pKernel = Kernel::create(
        pProgram,
        *pKernelInfo,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;
    size_t paramValueSizeRet = 0;

    // get size
    retVal = pKernel->getInfo(
        CL_KERNEL_NUM_ARGS,
        sizeof(cl_uint),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint), paramValueSizeRet);
    EXPECT_EQ(3u, paramValue);

    delete pKernel;
}

TEST_F(KernelFromBinaryTests, BuiltInIsSetToFalseForRegularKernels) {
    cl_device_id device = pDevice;

    CreateProgramFromBinary<Program>(pContext, &device, "simple_kernels");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("simple_kernel_0");

    // create a kernel
    auto pKernel = Kernel::create(
        pProgram,
        *pKernelInfo,
        &retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, pKernel);

    // get builtIn property
    bool isBuiltIn = pKernel->isBuiltIn;

    EXPECT_FALSE(isBuiltIn);

    delete pKernel;
}

TEST(PatchInfo, Constructor) {
    PatchInfo patchInfo;
    EXPECT_EQ(nullptr, patchInfo.interfaceDescriptorDataLoad);
    EXPECT_EQ(nullptr, patchInfo.localsurface);
    EXPECT_EQ(nullptr, patchInfo.mediavfestate);
    EXPECT_EQ(nullptr, patchInfo.interfaceDescriptorData);
    EXPECT_EQ(nullptr, patchInfo.samplerStateArray);
    EXPECT_EQ(nullptr, patchInfo.bindingTableState);
    EXPECT_EQ(nullptr, patchInfo.dataParameterStream);
    EXPECT_EQ(nullptr, patchInfo.threadPayload);
    EXPECT_EQ(nullptr, patchInfo.executionEnvironment);
    EXPECT_EQ(nullptr, patchInfo.pKernelAttributesInfo);
    EXPECT_EQ(nullptr, patchInfo.pAllocateStatelessPrivateSurface);
    EXPECT_EQ(nullptr, patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization);
    EXPECT_EQ(nullptr, patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization);
    EXPECT_EQ(nullptr, patchInfo.pAllocateStatelessPrintfSurface);
    EXPECT_EQ(nullptr, patchInfo.pAllocateStatelessEventPoolSurface);
    EXPECT_EQ(nullptr, patchInfo.pAllocateStatelessDefaultDeviceQueueSurface);
}

typedef Test<DeviceFixture> KernelPrivateSurfaceTest;
typedef Test<DeviceFixture> KernelGlobalSurfaceTest;
typedef Test<DeviceFixture> KernelConstantSurfaceTest;

struct KernelWithDeviceQueueFixture : public DeviceFixture,
                                      public DeviceQueueFixture,
                                      public testing::Test {
    void SetUp() override {
        DeviceFixture::SetUp();
        DeviceQueueFixture::SetUp(&context, pDevice);
    }
    void TearDown() override {
        DeviceQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    MockContext context;
};

typedef KernelWithDeviceQueueFixture KernelDefaultDeviceQueueSurfaceTest;
typedef KernelWithDeviceQueueFixture KernelEventPoolSurfaceTest;

class CommandStreamReceiverMock : public CommandStreamReceiver {
    typedef CommandStreamReceiver BaseClass;

  public:
    CommandStreamReceiverMock() : BaseClass() {
    }

    void makeResident(GraphicsAllocation &graphicsAllocation) override {
        residency[graphicsAllocation.getUnderlyingBuffer()] = graphicsAllocation.getUnderlyingBufferSize();
        CommandStreamReceiver::makeResident(graphicsAllocation);
    }

    void makeNonResident(GraphicsAllocation &graphicsAllocation) override {
        residency.erase(graphicsAllocation.getUnderlyingBuffer());
        CommandStreamReceiver::makeNonResident(graphicsAllocation);
    }

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineOrdinal, ResidencyContainer *allocationsForResidency) override {
        return flushStamp->peekStamp();
    }

    void addPipeControl(LinearStream &commandStream, bool dcFlush) override {
    }

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait) override {
    }

    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const LinearStream &dsh,
        const LinearStream &ih,
        const LinearStream &ioh,
        const LinearStream &ssh,
        uint32_t taskLevel,
        DispatchFlags &dispatchFlags) override {
        CompletionStamp cs = {};
        return cs;
    }

    void flushBatchedSubmissions() override {}

    std::map<const void *, size_t> residency;
};

TEST_F(KernelPrivateSurfaceTest, testPrivateSurface) {
    ASSERT_NE(nullptr, pDevice);

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    // setup private memory
    SPatchAllocateStatelessPrivateSurface tokenSPS;
    tokenSPS.SurfaceStateHeapOffset = 64;
    tokenSPS.DataParamOffset = 40;
    tokenSPS.DataParamSize = 8;
    tokenSPS.PerThreadPrivateMemorySize = 112;
    pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = &tokenSPS;

    SPatchDataParameterStream tokenDPS;
    tokenDPS.DataParameterStreamSize = 64;
    pKernelInfo->patchInfo.dataParameterStream = &tokenDPS;

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // create kernel
    MockContext context;
    MockProgram program(&context);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    // Test it
    std::unique_ptr<OsAgnosticMemoryManager> memoryManager(new OsAgnosticMemoryManager());
    std::unique_ptr<CommandStreamReceiverMock> csr(new CommandStreamReceiverMock());
    csr->setMemoryManager(memoryManager.get());
    csr->residency.clear();
    EXPECT_EQ(0u, csr->residency.size());

    pKernel->makeResident(*csr.get());
    EXPECT_EQ(1u, csr->residency.size());

    csr->makeSurfacePackNonResident();
    pKernel->updateWithCompletionStamp(*csr.get(), nullptr);
    EXPECT_EQ(0u, csr->residency.size());

    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelPrivateSurfaceTest, testPrivateSurfaceAllocationFailure) {
    ASSERT_NE(nullptr, pDevice);

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    // setup private memory
    SPatchAllocateStatelessPrivateSurface tokenSPS;
    tokenSPS.SurfaceStateHeapOffset = 64;
    tokenSPS.DataParamOffset = 40;
    tokenSPS.DataParamSize = 8;
    tokenSPS.PerThreadPrivateMemorySize = 112;
    pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = &tokenSPS;

    SPatchDataParameterStream tokenDPS;
    tokenDPS.DataParameterStreamSize = 64;
    pKernelInfo->patchInfo.dataParameterStream = &tokenDPS;

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // create kernel
    MockContext context;
    MockProgram program(&context);
    MemoryManagementFixture::InjectedFunction method = [&](size_t failureIndex) {
        MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

        if (MemoryManagementFixture::nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, pKernel->initialize());
        } else {
            EXPECT_EQ(CL_OUT_OF_RESOURCES, pKernel->initialize());
        }
        delete pKernel;
    };
    auto f = new MemoryManagementFixture();
    f->SetUp();
    f->injectFailures(method);
    f->TearDown();
    delete f;

    delete pKernelInfo;
}

TEST_F(KernelPrivateSurfaceTest, given32BitDeviceWhenKernelIsCreatedThenPrivateSurfaceIs32BitAllocation) {
    if (is64bit) {
        pDevice->getMemoryManager()->setForce32BitAllocations(true);

        // define kernel info
        KernelInfo *pKernelInfo = KernelInfo::create();

        // setup private memory
        SPatchAllocateStatelessPrivateSurface tokenSPS;
        tokenSPS.SurfaceStateHeapOffset = 64;
        tokenSPS.DataParamOffset = 40;
        tokenSPS.DataParamSize = 4;
        tokenSPS.PerThreadPrivateMemorySize = 112;
        pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = &tokenSPS;

        SPatchDataParameterStream tokenDPS;
        tokenDPS.DataParameterStreamSize = 64;
        pKernelInfo->patchInfo.dataParameterStream = &tokenDPS;

        SPatchExecutionEnvironment tokenEE;
        tokenEE.CompiledSIMD8 = false;
        tokenEE.CompiledSIMD16 = false;
        tokenEE.CompiledSIMD32 = true;
        pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

        // create kernel
        MockContext context;
        MockProgram program(&context);
        MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        EXPECT_TRUE(pKernel->getPrivateSurface()->is32BitAllocation);

        delete pKernel;
        delete pKernelInfo;
    }
}

HWTEST_F(KernelPrivateSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenPrivateMemorySurfaceStateIsPatchedWithCpuAddress) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup constant memory
    SPatchAllocateStatelessPrivateSurface AllocateStatelessPrivateMemorySurface;
    AllocateStatelessPrivateMemorySurface.SurfaceStateHeapOffset = 0;
    AllocateStatelessPrivateMemorySurface.DataParamOffset = 0;
    AllocateStatelessPrivateMemorySurface.DataParamSize = 8;
    AllocateStatelessPrivateMemorySurface.PerThreadPrivateMemorySize = 16;

    pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = &AllocateStatelessPrivateMemorySurface;

    MockContext context;
    MockProgram program(&context);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // setup surface state heap
    char surfaceStateHeap[0x80];
    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

    // define stateful path
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    auto bufferAddress = pKernel->getPrivateSurface()->getGpuAddress();

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface->SurfaceStateHeapOffset));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(bufferAddress, surfaceAddress);

    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelPrivateSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenPrivateMemorySurfaceStateIsNotPatched) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup global memory
    char buffer[16];
    GraphicsAllocation gfxAlloc(buffer, sizeof(buffer));

    MockContext context;
    MockProgram program(&context);
    program.setConstantSurface(&gfxAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());

    program.setConstantSurface(nullptr);
    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelPrivateSurfaceTest, givenNullDataParameterStreamGetConstantBufferSizeReturnsZero) {
    KernelInfo *pKernelInfo = KernelInfo::create();

    EXPECT_EQ(0u, pKernelInfo->getConstantBufferSize());

    delete pKernelInfo;
}

TEST_F(KernelPrivateSurfaceTest, givenNonNullDataParameterStreamGetConstantBufferSizeReturnsCorrectSize) {
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchDataParameterStream tokenDPS;
    tokenDPS.DataParameterStreamSize = 64;
    pKernelInfo->patchInfo.dataParameterStream = &tokenDPS;

    EXPECT_EQ(64u, pKernelInfo->getConstantBufferSize());

    delete pKernelInfo;
}

TEST_F(KernelGlobalSurfaceTest, givenBuiltInKernelWhenKernelIsCreatedThenGlobalSurfaceIsPatchedWithCpuAddress) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    // setup global memory
    SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization AllocateStatelessGlobalMemorySurfaceWithInitialization;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = &AllocateStatelessGlobalMemorySurfaceWithInitialization;

    SPatchDataParameterStream tempSPatchDataParameterStream;
    tempSPatchDataParameterStream.DataParameterStreamSize = 16;
    pKernelInfo->patchInfo.dataParameterStream = &tempSPatchDataParameterStream;

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    char buffer[16];

    GraphicsAllocation gfxAlloc((void *)buffer, (uint64_t)buffer - 8u, 8);
    uint64_t bufferAddress = (uint64_t)gfxAlloc.getUnderlyingBuffer();

    // create kernel
    MockContext context;
    MockProgram program(&context);
    program.setGlobalSurface(&gfxAlloc);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    pKernel->isBuiltIn = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)pKernel->getCrossThreadData());

    program.setGlobalSurface(nullptr);
    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelGlobalSurfaceTest, givenNDRangeKernelWhenKernelIsCreatedThenGlobalSurfaceIsPatchedWithBaseAddressOffset) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    // setup global memory
    SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization AllocateStatelessGlobalMemorySurfaceWithInitialization;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = &AllocateStatelessGlobalMemorySurfaceWithInitialization;

    SPatchDataParameterStream tempSPatchDataParameterStream;
    tempSPatchDataParameterStream.DataParameterStreamSize = 16;
    pKernelInfo->patchInfo.dataParameterStream = &tempSPatchDataParameterStream;

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    char buffer[16];

    GraphicsAllocation gfxAlloc((void *)buffer, (uint64_t)buffer - 8u, 8);
    uint64_t bufferAddress = gfxAlloc.getGpuAddress();

    // create kernel
    MockProgram program;
    program.setGlobalSurface(&gfxAlloc);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)pKernel->getCrossThreadData());

    program.setGlobalSurface(nullptr);

    delete pKernel;
    delete pKernelInfo;
}

HWTEST_F(KernelGlobalSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenGlobalMemorySurfaceStateIsPatchedWithCpuAddress) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup global memory
    SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization AllocateStatelessGlobalMemorySurfaceWithInitialization;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.SurfaceStateHeapOffset = 0;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = &AllocateStatelessGlobalMemorySurfaceWithInitialization;

    char buffer[16];
    GraphicsAllocation gfxAlloc(buffer, sizeof(buffer));
    void *bufferAddress = gfxAlloc.getUnderlyingBuffer();

    MockContext context;
    MockProgram program(&context);
    program.setGlobalSurface(&gfxAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // setup surface state heap
    char surfaceStateHeap[0x80];
    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

    // define stateful path
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization->SurfaceStateHeapOffset));
    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(bufferAddress, surfaceAddress);

    program.setGlobalSurface(nullptr);
    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelGlobalSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenGlobalMemorySurfaceStateIsNotPatched) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup global memory
    char buffer[16];
    GraphicsAllocation gfxAlloc(buffer, sizeof(buffer));

    MockProgram program;
    program.setGlobalSurface(&gfxAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());

    program.setGlobalSurface(nullptr);
    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelConstantSurfaceTest, givenBuiltInKernelWhenKernelIsCreatedThenConstantSurfaceIsPatchedWithCpuAddress) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    // setup constant memory
    SPatchAllocateStatelessConstantMemorySurfaceWithInitialization AllocateStatelessConstantMemorySurfaceWithInitialization;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = &AllocateStatelessConstantMemorySurfaceWithInitialization;

    SPatchDataParameterStream tempSPatchDataParameterStream;
    tempSPatchDataParameterStream.DataParameterStreamSize = 16;
    pKernelInfo->patchInfo.dataParameterStream = &tempSPatchDataParameterStream;

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    char buffer[16];

    GraphicsAllocation gfxAlloc((void *)buffer, (uint64_t)buffer - 8u, 8);
    uint64_t bufferAddress = (uint64_t)gfxAlloc.getUnderlyingBuffer();

    // create kernel
    MockProgram program;
    program.setConstantSurface(&gfxAlloc);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    pKernel->isBuiltIn = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)pKernel->getCrossThreadData());

    program.setConstantSurface(nullptr);
    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelConstantSurfaceTest, givenNDRangeKernelWhenKernelIsCreatedThenConstantSurfaceIsPatchedWithBaseAddressOffset) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    // setup constant memory
    SPatchAllocateStatelessConstantMemorySurfaceWithInitialization AllocateStatelessConstantMemorySurfaceWithInitialization;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = &AllocateStatelessConstantMemorySurfaceWithInitialization;

    SPatchDataParameterStream tempSPatchDataParameterStream;
    tempSPatchDataParameterStream.DataParameterStreamSize = 16;
    pKernelInfo->patchInfo.dataParameterStream = &tempSPatchDataParameterStream;

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    char buffer[16];

    GraphicsAllocation gfxAlloc((void *)buffer, (uint64_t)buffer - 8u, 8);
    uint64_t bufferAddress = gfxAlloc.getGpuAddress();

    // create kernel
    MockProgram program;
    program.setConstantSurface(&gfxAlloc);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)pKernel->getCrossThreadData());

    program.setConstantSurface(nullptr);

    delete pKernel;
    delete pKernelInfo;
}

HWTEST_F(KernelConstantSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenConstantMemorySurfaceStateIsPatchedWithCpuAddress) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup constant memory
    SPatchAllocateStatelessConstantMemorySurfaceWithInitialization AllocateStatelessConstantMemorySurfaceWithInitialization;
    AllocateStatelessConstantMemorySurfaceWithInitialization.SurfaceStateHeapOffset = 0;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = &AllocateStatelessConstantMemorySurfaceWithInitialization;

    char buffer[16];
    GraphicsAllocation gfxAlloc(buffer, sizeof(buffer));
    void *bufferAddress = gfxAlloc.getUnderlyingBuffer();

    MockContext context;
    MockProgram program(&context);
    program.setConstantSurface(&gfxAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // setup surface state heap
    char surfaceStateHeap[0x80];
    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

    // define stateful path
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization->SurfaceStateHeapOffset));
    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(bufferAddress, surfaceAddress);

    program.setConstantSurface(nullptr);
    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelConstantSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenConstantMemorySurfaceStateIsNotPatched) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup global memory
    char buffer[16];
    GraphicsAllocation gfxAlloc(buffer, sizeof(buffer));

    MockProgram program;
    program.setConstantSurface(&gfxAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());

    program.setConstantSurface(nullptr);
    delete pKernel;
    delete pKernelInfo;
}

HWTEST_F(KernelEventPoolSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenEventPoolSurfaceStateIsPatchedWithNullSurface) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup event pool surface
    SPatchAllocateStatelessEventPoolSurface AllocateStatelessEventPoolSurface;
    AllocateStatelessEventPoolSurface.SurfaceStateHeapOffset = 0;
    AllocateStatelessEventPoolSurface.DataParamOffset = 0;
    AllocateStatelessEventPoolSurface.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface = &AllocateStatelessEventPoolSurface;

    // create kernel
    MockProgram program(&context);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // setup surface state heap
    char surfaceStateHeap[0x80];
    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

    // define stateful path
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface->SurfaceStateHeapOffset));
    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(nullptr, surfaceAddress);
    auto surfaceType = surfaceState->getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, surfaceType);

    delete pKernel;
    delete pKernelInfo;
}

HWTEST_F(KernelEventPoolSurfaceTest, givenStatefulKernelWhenEventPoolIsPatchedThenEventPoolSurfaceStateIsProgrammed) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup event pool surface
    SPatchAllocateStatelessEventPoolSurface AllocateStatelessEventPoolSurface;
    AllocateStatelessEventPoolSurface.SurfaceStateHeapOffset = 0;
    AllocateStatelessEventPoolSurface.DataParamOffset = 0;
    AllocateStatelessEventPoolSurface.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface = &AllocateStatelessEventPoolSurface;

    // create kernel
    MockProgram program(&context);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // setup surface state heap
    char surfaceStateHeap[0x80];
    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

    // define stateful path
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    pKernel->patchEventPool(pDevQueue);

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface->SurfaceStateHeapOffset));
    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(pDevQueue->getEventPoolBuffer()->getGpuAddress(), (uint64_t)surfaceAddress);
    auto surfaceType = surfaceState->getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, surfaceType);

    delete pKernel;
    delete pKernelInfo;
}

HWTEST_F(KernelEventPoolSurfaceTest, givenKernelWithNullEventPoolInKernelInfoWhenEventPoolIsPatchedThenAddressIsNotPatched) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface = nullptr;

    // create kernel
    MockProgram program;
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    uint64_t crossThreadData = 123;

    pKernel->setCrossThreadData(&crossThreadData, sizeof(uint64_t));

    pKernel->patchEventPool(pDevQueue);

    EXPECT_EQ(123u, *(uint64_t *)pKernel->getCrossThreadData());

    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelEventPoolSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenEventPoolSurfaceStateIsNotPatched) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup event pool surface
    SPatchAllocateStatelessEventPoolSurface AllocateStatelessEventPoolSurface;
    AllocateStatelessEventPoolSurface.SurfaceStateHeapOffset = 0;
    AllocateStatelessEventPoolSurface.DataParamOffset = 0;
    AllocateStatelessEventPoolSurface.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface = &AllocateStatelessEventPoolSurface;

    // create kernel
    MockProgram program;
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    if (pDevice->getSupportedClVersion() < 20) {
        EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());
    } else {
    }

    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelEventPoolSurfaceTest, givenStatelessKernelWhenEventPoolIsPatchedThenCrossThreadDataIsPatched) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup event pool surface
    SPatchAllocateStatelessEventPoolSurface AllocateStatelessEventPoolSurface;
    AllocateStatelessEventPoolSurface.SurfaceStateHeapOffset = 0;
    AllocateStatelessEventPoolSurface.DataParamOffset = 0;
    AllocateStatelessEventPoolSurface.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface = &AllocateStatelessEventPoolSurface;

    // create kernel
    MockProgram program;
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    uint64_t crossThreadData = 0;

    pKernel->setCrossThreadData(&crossThreadData, sizeof(uint64_t));

    pKernel->patchEventPool(pDevQueue);

    EXPECT_EQ(pDevQueue->getEventPoolBuffer()->getGpuAddress(), *(uint64_t *)pKernel->getCrossThreadData());

    delete pKernel;
    delete pKernelInfo;
}

HWTEST_F(KernelDefaultDeviceQueueSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenDefaultDeviceQueueSurfaceStateIsPatchedWithNullSurface) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup default device queue surface
    SPatchAllocateStatelessDefaultDeviceQueueSurface AllocateStatelessDefaultDeviceQueueSurface;
    AllocateStatelessDefaultDeviceQueueSurface.SurfaceStateHeapOffset = 0;
    AllocateStatelessDefaultDeviceQueueSurface.DataParamOffset = 0;
    AllocateStatelessDefaultDeviceQueueSurface.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = &AllocateStatelessDefaultDeviceQueueSurface;

    // create kernel
    MockProgram program(&context);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // setup surface state heap
    char surfaceStateHeap[0x80];
    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

    // define stateful path
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->SurfaceStateHeapOffset));
    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(nullptr, surfaceAddress);
    auto surfaceType = surfaceState->getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, surfaceType);

    delete pKernel;
    delete pKernelInfo;
}

HWTEST_F(KernelDefaultDeviceQueueSurfaceTest, givenStatefulKernelWhenDefaultDeviceQueueIsPatchedThenSurfaceStateIsCorrectlyProgrammed) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup default device queue surface
    SPatchAllocateStatelessDefaultDeviceQueueSurface AllocateStatelessDefaultDeviceQueueSurface;
    AllocateStatelessDefaultDeviceQueueSurface.SurfaceStateHeapOffset = 0;
    AllocateStatelessDefaultDeviceQueueSurface.DataParamOffset = 0;
    AllocateStatelessDefaultDeviceQueueSurface.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = &AllocateStatelessDefaultDeviceQueueSurface;

    // create kernel
    MockProgram program(&context);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // setup surface state heap
    char surfaceStateHeap[0x80];
    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

    // define stateful path
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    pKernel->patchDefaultDeviceQueue(pDevQueue);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->SurfaceStateHeapOffset));
    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(pDevQueue->getQueueBuffer()->getGpuAddress(), (uint64_t)surfaceAddress);
    auto surfaceType = surfaceState->getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, surfaceType);

    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelDefaultDeviceQueueSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenDefaultDeviceQueueSurfaceStateIsNotPatched) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup default device queue surface
    SPatchAllocateStatelessDefaultDeviceQueueSurface AllocateStatelessDefaultDeviceQueueSurface;
    AllocateStatelessDefaultDeviceQueueSurface.SurfaceStateHeapOffset = 0;
    AllocateStatelessDefaultDeviceQueueSurface.DataParamOffset = 0;
    AllocateStatelessDefaultDeviceQueueSurface.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = &AllocateStatelessDefaultDeviceQueueSurface;

    // create kernel
    MockProgram program;
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());

    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelDefaultDeviceQueueSurfaceTest, givenKernelWithNullDeviceQueueKernelInfoWhenDefaultDeviceQueueIsPatchedThenAddressIsNotPatched) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = nullptr;

    // create kernel
    MockProgram program;
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    uint64_t crossThreadData = 123;

    pKernel->setCrossThreadData(&crossThreadData, sizeof(uint64_t));

    pKernel->patchDefaultDeviceQueue(pDevQueue);

    EXPECT_EQ(123u, *(uint64_t *)pKernel->getCrossThreadData());

    delete pKernel;
    delete pKernelInfo;
}

TEST_F(KernelDefaultDeviceQueueSurfaceTest, givenStatelessKernelWhenDefaultDeviceQueueIsPatchedThenCrossThreadDataIsPatched) {

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup default device queue surface
    SPatchAllocateStatelessDefaultDeviceQueueSurface AllocateStatelessDefaultDeviceQueueSurface;
    AllocateStatelessDefaultDeviceQueueSurface.SurfaceStateHeapOffset = 0;
    AllocateStatelessDefaultDeviceQueueSurface.DataParamOffset = 0;
    AllocateStatelessDefaultDeviceQueueSurface.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = &AllocateStatelessDefaultDeviceQueueSurface;

    // create kernel
    MockProgram program;
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    uint64_t crossThreadData = 0;

    pKernel->setCrossThreadData(&crossThreadData, sizeof(uint64_t));

    pKernel->patchDefaultDeviceQueue(pDevQueue);

    EXPECT_EQ(pDevQueue->getQueueBuffer()->getGpuAddress(), *(uint64_t *)pKernel->getCrossThreadData());

    delete pKernel;
    delete pKernelInfo;
}

typedef Test<DeviceFixture> KernelResidencyTest;

TEST_F(KernelResidencyTest, test_MakeArgsResident) {
    ASSERT_NE(nullptr, pDevice);
    char pCrossThreadData[64];

    // define kernel info
    KernelInfo *pKernelInfo = KernelInfo::create();

    // setup kernel arg offsets
    KernelArgPatchInfo kernelArgPatchInfo;

    pKernelInfo->kernelArgInfo.resize(3);
    pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

    pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset = 0x10;
    pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
    pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;

    MockProgram program;
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

    // Currently just a sandbox for testing
    pKernel->makeResident(pDevice->getCommandStreamReceiver());

    delete pKernel;
    delete pKernelInfo;
}

struct KernelExecutionEnvironmentTest : public Test<DeviceFixture> {
    void SetUp() override {
        DeviceFixture::SetUp();
        pKernelInfo = KernelInfo::create();
        pKernelInfo->patchInfo.executionEnvironment = &executionEnvironment;

        pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    }

    void TearDown() override {
        delete pKernelInfo;
        delete pKernel;
        DeviceFixture::TearDown();
    }

    MockKernel *pKernel;
    MockProgram program;
    KernelInfo *pKernelInfo;
    SPatchExecutionEnvironment executionEnvironment;
};

TEST_F(KernelExecutionEnvironmentTest, getMaxSimdReturnsMaxOfAll32) {

    executionEnvironment.CompiledSIMD32 = true;
    executionEnvironment.CompiledSIMD16 = true;
    executionEnvironment.CompiledSIMD8 = true;

    EXPECT_EQ(32u, this->pKernelInfo->getMaxSimdSize());
}

TEST_F(KernelExecutionEnvironmentTest, getMaxSimdReturnsMaxOfAll16) {

    executionEnvironment.CompiledSIMD32 = false;
    executionEnvironment.CompiledSIMD16 = true;
    executionEnvironment.CompiledSIMD8 = true;

    EXPECT_EQ(16u, this->pKernelInfo->getMaxSimdSize());
}

TEST_F(KernelExecutionEnvironmentTest, getMaxSimdReturnsMaxOfAll8) {

    executionEnvironment.CompiledSIMD32 = false;
    executionEnvironment.CompiledSIMD16 = false;
    executionEnvironment.CompiledSIMD8 = true;

    EXPECT_EQ(8u, this->pKernelInfo->getMaxSimdSize());
}

TEST_F(KernelExecutionEnvironmentTest, getMaxSimdReturns8ByDefault) {

    executionEnvironment.CompiledSIMD32 = false;
    executionEnvironment.CompiledSIMD16 = false;
    executionEnvironment.CompiledSIMD8 = false;

    EXPECT_EQ(8u, this->pKernelInfo->getMaxSimdSize());
}

TEST_F(KernelExecutionEnvironmentTest, getMaxSimdReturns1WhenExecutionEnvironmentNotAvailable) {

    executionEnvironment.CompiledSIMD32 = false;
    executionEnvironment.CompiledSIMD16 = false;
    executionEnvironment.CompiledSIMD8 = false;

    auto oldExcEnv = this->pKernelInfo->patchInfo.executionEnvironment;

    this->pKernelInfo->patchInfo.executionEnvironment = nullptr;
    EXPECT_EQ(1U, this->pKernelInfo->getMaxSimdSize());
    this->pKernelInfo->patchInfo.executionEnvironment = oldExcEnv;
}

TEST_F(KernelExecutionEnvironmentTest, getMaxRequiredWorkGroupSizeWhenCompiledWorkGroupSizeIsZero) {
    auto maxWorkGroupSize = pDevice->getDeviceInfo().maxWorkGroupSize;
    auto oldRequiredWorkGroupSizeX = this->pKernelInfo->patchInfo.executionEnvironment->RequiredWorkGroupSizeX;
    auto oldRequiredWorkGroupSizeY = this->pKernelInfo->patchInfo.executionEnvironment->RequiredWorkGroupSizeY;
    auto oldRequiredWorkGroupSizeZ = this->pKernelInfo->patchInfo.executionEnvironment->RequiredWorkGroupSizeZ;

    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeX = 0;
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeY = 0;
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeZ = 0;

    EXPECT_EQ(maxWorkGroupSize, this->pKernelInfo->getMaxRequiredWorkGroupSize(maxWorkGroupSize));

    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeX = oldRequiredWorkGroupSizeX;
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeY = oldRequiredWorkGroupSizeY;
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeZ = oldRequiredWorkGroupSizeZ;
}

TEST_F(KernelExecutionEnvironmentTest, getMaxRequiredWorkGroupSizeWhenCompiledWorkGroupSizeIsLowerThanMaxWorkGroupSize) {
    auto maxWorkGroupSize = pDevice->getDeviceInfo().maxWorkGroupSize;
    auto oldRequiredWorkGroupSizeX = this->pKernelInfo->patchInfo.executionEnvironment->RequiredWorkGroupSizeX;
    auto oldRequiredWorkGroupSizeY = this->pKernelInfo->patchInfo.executionEnvironment->RequiredWorkGroupSizeY;
    auto oldRequiredWorkGroupSizeZ = this->pKernelInfo->patchInfo.executionEnvironment->RequiredWorkGroupSizeZ;

    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeX = static_cast<uint32_t>(maxWorkGroupSize / 2);
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeY = 1;
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeZ = 1;

    EXPECT_EQ(maxWorkGroupSize / 2, this->pKernelInfo->getMaxRequiredWorkGroupSize(maxWorkGroupSize));

    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeX = oldRequiredWorkGroupSizeX;
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeY = oldRequiredWorkGroupSizeY;
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeZ = oldRequiredWorkGroupSizeZ;
}

TEST_F(KernelExecutionEnvironmentTest, getMaxRequiredWorkGroupSizeWhenCompiledWorkGroupSizeIsGreaterThanMaxWorkGroupSize) {
    auto maxWorkGroupSize = pDevice->getDeviceInfo().maxWorkGroupSize;
    auto oldRequiredWorkGroupSizeX = this->pKernelInfo->patchInfo.executionEnvironment->RequiredWorkGroupSizeX;
    auto oldRequiredWorkGroupSizeY = this->pKernelInfo->patchInfo.executionEnvironment->RequiredWorkGroupSizeY;
    auto oldRequiredWorkGroupSizeZ = this->pKernelInfo->patchInfo.executionEnvironment->RequiredWorkGroupSizeZ;

    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeX = static_cast<uint32_t>(maxWorkGroupSize);
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeY = static_cast<uint32_t>(maxWorkGroupSize);
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeZ = static_cast<uint32_t>(maxWorkGroupSize);

    EXPECT_EQ(maxWorkGroupSize, this->pKernelInfo->getMaxRequiredWorkGroupSize(maxWorkGroupSize));

    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeX = oldRequiredWorkGroupSizeX;
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeY = oldRequiredWorkGroupSizeY;
    const_cast<SPatchExecutionEnvironment *>(this->pKernelInfo->patchInfo.executionEnvironment)->RequiredWorkGroupSizeZ = oldRequiredWorkGroupSizeZ;
}

struct KernelCrossThreadTests : Test<DeviceFixture> {
    KernelCrossThreadTests() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        patchDataParameterStream.DataParameterStreamSize = 64 * sizeof(uint8_t);

        pKernelInfo = KernelInfo::create();
        ASSERT_NE(nullptr, pKernelInfo);
        pKernelInfo->patchInfo.dataParameterStream = &patchDataParameterStream;
        pKernelInfo->patchInfo.executionEnvironment = &executionEnvironment;
    }

    void TearDown() override {
        delete pKernelInfo;
        DeviceFixture::TearDown();
    }

    MockProgram program;
    KernelInfo *pKernelInfo = nullptr;
    SPatchDataParameterStream patchDataParameterStream;
    SPatchExecutionEnvironment executionEnvironment;
};

TEST_F(KernelCrossThreadTests, globalWorkOffset) {

    pKernelInfo->workloadInfo.globalWorkOffsetOffsets[1] = 4;

    MockKernel kernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.globalWorkOffsetX);
    EXPECT_NE(nullptr, kernel.globalWorkOffsetY);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.globalWorkOffsetY);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.globalWorkOffsetZ);
}

TEST_F(KernelCrossThreadTests, localWorkSize) {

    pKernelInfo->workloadInfo.localWorkSizeOffsets[0] = 0xc;

    MockKernel kernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.localWorkSizeX);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.localWorkSizeX);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.localWorkSizeY);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.localWorkSizeZ);
}

TEST_F(KernelCrossThreadTests, localWorkSize2) {

    pKernelInfo->workloadInfo.localWorkSizeOffsets2[1] = 0xd;

    MockKernel kernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.localWorkSizeX2);
    EXPECT_NE(nullptr, kernel.localWorkSizeY2);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.localWorkSizeY2);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.localWorkSizeZ2);
}

TEST_F(KernelCrossThreadTests, globalWorkSize) {

    pKernelInfo->workloadInfo.globalWorkSizeOffsets[2] = 8;

    MockKernel kernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.globalWorkSizeX);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.globalWorkSizeY);
    EXPECT_NE(nullptr, kernel.globalWorkSizeZ);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.globalWorkSizeZ);
}

TEST_F(KernelCrossThreadTests, workDim) {

    pKernelInfo->workloadInfo.workDimOffset = 12;

    MockKernel kernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.workDim);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.workDim);
}

TEST_F(KernelCrossThreadTests, numWorkGroups) {

    pKernelInfo->workloadInfo.numWorkGroupsOffset[0] = 0 * sizeof(uint32_t);
    pKernelInfo->workloadInfo.numWorkGroupsOffset[1] = 1 * sizeof(uint32_t);
    pKernelInfo->workloadInfo.numWorkGroupsOffset[2] = 2 * sizeof(uint32_t);

    MockKernel kernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.numWorkGroupsX);
    EXPECT_NE(nullptr, kernel.numWorkGroupsY);
    EXPECT_NE(nullptr, kernel.numWorkGroupsZ);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.numWorkGroupsX);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.numWorkGroupsY);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.numWorkGroupsZ);
}

TEST_F(KernelCrossThreadTests, enqueuedLocalWorkSize) {

    pKernelInfo->workloadInfo.enqueuedLocalWorkSizeOffsets[0] = 0;

    MockKernel kernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.enqueuedLocalWorkSizeX);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.enqueuedLocalWorkSizeX);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.enqueuedLocalWorkSizeY);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.enqueuedLocalWorkSizeZ);
}

TEST_F(KernelCrossThreadTests, maxWorkGroupSize) {

    pKernelInfo->workloadInfo.maxWorkGroupSizeOffset = 12;

    MockKernel kernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.maxWorkGroupSize);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.maxWorkGroupSize);
    EXPECT_EQ(static_cast<void *>(kernel.getCrossThreadData() + pKernelInfo->workloadInfo.maxWorkGroupSizeOffset), static_cast<void *>(kernel.maxWorkGroupSize));
    EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize, *kernel.maxWorkGroupSize);
}

TEST_F(KernelCrossThreadTests, dataParameterSimdSize) {

    pKernelInfo->workloadInfo.simdSizeOffset = 16;
    MockKernel kernel(&program, *pKernelInfo, *pDevice);
    executionEnvironment.CompiledSIMD32 = false;
    executionEnvironment.CompiledSIMD16 = false;
    executionEnvironment.CompiledSIMD8 = true;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.dataParameterSimdSize);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.dataParameterSimdSize);
    EXPECT_EQ(static_cast<void *>(kernel.getCrossThreadData() + pKernelInfo->workloadInfo.simdSizeOffset), static_cast<void *>(kernel.dataParameterSimdSize));
    EXPECT_EQ_VAL(pKernelInfo->getMaxSimdSize(), *kernel.dataParameterSimdSize);
}

TEST_F(KernelCrossThreadTests, GIVENparentEventOffsetWHENinitializeKernelTHENparentEventInitWithInvalid) {
    pKernelInfo->workloadInfo.parentEventOffset = 16;
    MockKernel kernel(&program, *pKernelInfo, *pDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.parentEventOffset);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.parentEventOffset);
    EXPECT_EQ(static_cast<void *>(kernel.getCrossThreadData() + pKernelInfo->workloadInfo.parentEventOffset), static_cast<void *>(kernel.parentEventOffset));
    EXPECT_EQ(WorkloadInfo::invalidParentEvent, *kernel.parentEventOffset);
}

TEST_F(KernelCrossThreadTests, kernelAddRefCountToProgram) {

    auto refCount = program.getReference();
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pDevice);
    auto refCount2 = program.getReference();
    EXPECT_EQ(refCount2, refCount + 1);

    delete kernel;
    auto refCount3 = program.getReference();
    EXPECT_EQ(refCount, refCount3);
}

TEST_F(KernelCrossThreadTests, kernelSetsTotalSLMSize) {

    pKernelInfo->workloadInfo.slmStaticSize = 1024;

    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    EXPECT_EQ(1024u, kernel->slmTotalSize);

    delete kernel;
}
TEST_F(KernelCrossThreadTests, givenKernelWithPrivateMemoryWhenItIsCreatedThenCurbeIsPatchedProperly) {

    SPatchAllocateStatelessPrivateSurface allocatePrivate;
    allocatePrivate.DataParamSize = 8;
    allocatePrivate.DataParamOffset = 0;
    allocatePrivate.PerThreadPrivateMemorySize = 1;
    pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = &allocatePrivate;

    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    kernel->initialize();

    auto privateSurface = kernel->getPrivateSurface();

    auto constantBuffer = kernel->getCrossThreadData();
    auto privateAddress = (uintptr_t)privateSurface->getGpuAddressToPatch();
    auto ptrCurbe = (uint64_t *)constantBuffer;
    auto privateAddressFromCurbe = (uintptr_t)*ptrCurbe;

    EXPECT_EQ(privateAddressFromCurbe, privateAddress);

    delete kernel;
}

TEST_F(KernelCrossThreadTests, givenKernelWithPrefferedWkgMultipleWhenItIsCreatedThenCurbeIsPatchedProperly) {

    pKernelInfo->workloadInfo.prefferedWkgMultipleOffset = 8;
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    kernel->initialize();

    auto *crossThread = kernel->getCrossThreadData();

    uint32_t *prefferedWkgMultipleOffset = (uint32_t *)ptrOffset(crossThread, 8);

    EXPECT_EQ(pKernelInfo->getMaxSimdSize(), *prefferedWkgMultipleOffset);

    delete kernel;
}

TEST_F(KernelCrossThreadTests, patchBlocksSimdSize) {
    MockKernelWithInternals *kernel = new MockKernelWithInternals(*pDevice);

    // store offset to child's simd size in kernel info
    uint32_t crossThreadOffset = 0; //offset of simd size
    kernel->kernelInfo.childrenKernelsIdOffset.push_back({0, crossThreadOffset});

    // add a new block kernel to program
    KernelInfo *infoBlock = new KernelInfo();
    kernel->executionEnvironmentBlock.CompiledSIMD8 = 0;
    kernel->executionEnvironmentBlock.CompiledSIMD16 = 1;
    kernel->executionEnvironmentBlock.CompiledSIMD32 = 0;
    infoBlock->patchInfo.executionEnvironment = &kernel->executionEnvironmentBlock;
    kernel->mockProgram->addBlockKernel(infoBlock);

    // patch block's simd size
    kernel->mockKernel->patchBlocksSimdSize();

    // obtain block's simd size from cross thread data
    void *blockSimdSize = ptrOffset(kernel->mockKernel->getCrossThreadData(), kernel->kernelInfo.childrenKernelsIdOffset[0].second);
    uint32_t *simdSize = reinterpret_cast<uint32_t *>(blockSimdSize);

    // check of block's simd size has been patched correctly
    EXPECT_EQ(kernel->mockProgram->getBlockKernelInfo(0)->getMaxSimdSize(), *simdSize);

    delete kernel;
}

TEST(KernelInfoTest, borderColorOffset) {
    KernelInfo info;
    SPatchSamplerStateArray samplerState;
    samplerState.BorderColorOffset = 3;

    info.patchInfo.samplerStateArray = nullptr;

    EXPECT_EQ(0u, info.getBorderColorOffset());

    info.patchInfo.samplerStateArray = &samplerState;

    EXPECT_EQ(3u, info.getBorderColorOffset());
}

TEST(KernelInfoTest, getArgNumByName) {
    KernelInfo info;
    EXPECT_EQ(-1, info.getArgNumByName(""));

    KernelArgInfo kai;
    kai.name = "arg1";
    info.kernelArgInfo.push_back(kai);

    EXPECT_EQ(-1, info.getArgNumByName(""));
    EXPECT_EQ(-1, info.getArgNumByName("arg2"));

    EXPECT_EQ(0, info.getArgNumByName("arg1"));

    kai.name = "arg2";
    info.kernelArgInfo.push_back(kai);

    EXPECT_EQ(0, info.getArgNumByName("arg1"));
    EXPECT_EQ(1, info.getArgNumByName("arg2"));
}

TEST(KernelTest, getInstructionHeapSizeForExecutionModelReturnsZeroForNormalKernel) {
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    MockKernelWithInternals kernel(*device);

    EXPECT_EQ(0u, kernel.mockKernel->getInstructionHeapSizeForExecutionModel());
}

TEST(KernelTest, setKernelArgUsesBuiltinDispatchInfoBuilderIfAvailable) {
    struct MockBuiltinDispatchBuilder : BuiltinDispatchInfoBuilder {
        MockBuiltinDispatchBuilder(BuiltIns &builtins)
            : BuiltinDispatchInfoBuilder(builtins) {
        }

        bool setExplicitArg(uint32_t argIndex, size_t argSize, const void *argVal, cl_int &err) const override {
            receivedArgs.push_back(std::make_tuple(argIndex, argSize, argVal));
            err = errToReturn;
            return valueToReturn;
        }

        bool valueToReturn = false;
        cl_int errToReturn = CL_SUCCESS;
        mutable std::vector<std::tuple<uint32_t, size_t, const void *>> receivedArgs;
    };

    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    kernel.kernelInfo.resizeKernelArgInfoAndRegisterParameter(1);
    kernel.mockKernel->initialize();

    MockBuiltinDispatchBuilder mockBuilder(BuiltIns::getInstance());
    kernel.kernelInfo.builtinDispatchBuilder = &mockBuilder;

    mockBuilder.valueToReturn = false;
    mockBuilder.errToReturn = CL_SUCCESS;
    EXPECT_EQ(0u, kernel.mockKernel->getPatchedArgumentsNum());
    auto ret = kernel.mockKernel->setArg(1, 3, reinterpret_cast<const void *>(5));
    EXPECT_EQ(CL_SUCCESS, ret);
    EXPECT_EQ(1u, kernel.mockKernel->getPatchedArgumentsNum());

    mockBuilder.valueToReturn = false;
    mockBuilder.errToReturn = CL_INVALID_ARG_SIZE;
    ret = kernel.mockKernel->setArg(7, 11, reinterpret_cast<const void *>(13));
    EXPECT_EQ(CL_INVALID_ARG_SIZE, ret);
    EXPECT_EQ(1u, kernel.mockKernel->getPatchedArgumentsNum());

    mockBuilder.valueToReturn = true;
    mockBuilder.errToReturn = CL_SUCCESS;
    ret = kernel.mockKernel->setArg(17, 19, reinterpret_cast<const void *>(23));
    EXPECT_EQ(CL_INVALID_ARG_INDEX, ret);
    EXPECT_EQ(1u, kernel.mockKernel->getPatchedArgumentsNum());

    mockBuilder.valueToReturn = true;
    mockBuilder.errToReturn = CL_INVALID_ARG_SIZE;
    ret = kernel.mockKernel->setArg(29, 31, reinterpret_cast<const void *>(37));
    EXPECT_EQ(CL_INVALID_ARG_INDEX, ret);
    EXPECT_EQ(1u, kernel.mockKernel->getPatchedArgumentsNum());

    ASSERT_EQ(4U, mockBuilder.receivedArgs.size());

    EXPECT_EQ(1U, std::get<0>(mockBuilder.receivedArgs[0]));
    EXPECT_EQ(3U, std::get<1>(mockBuilder.receivedArgs[0]));
    EXPECT_EQ(reinterpret_cast<const void *>(5), std::get<2>(mockBuilder.receivedArgs[0]));

    EXPECT_EQ(7U, std::get<0>(mockBuilder.receivedArgs[1]));
    EXPECT_EQ(11U, std::get<1>(mockBuilder.receivedArgs[1]));
    EXPECT_EQ(reinterpret_cast<const void *>(13), std::get<2>(mockBuilder.receivedArgs[1]));

    EXPECT_EQ(17U, std::get<0>(mockBuilder.receivedArgs[2]));
    EXPECT_EQ(19U, std::get<1>(mockBuilder.receivedArgs[2]));
    EXPECT_EQ(reinterpret_cast<const void *>(23), std::get<2>(mockBuilder.receivedArgs[2]));

    EXPECT_EQ(29U, std::get<0>(mockBuilder.receivedArgs[3]));
    EXPECT_EQ(31U, std::get<1>(mockBuilder.receivedArgs[3]));
    EXPECT_EQ(reinterpret_cast<const void *>(37), std::get<2>(mockBuilder.receivedArgs[3]));

    BuiltIns::shutDown();
}
TEST(KernelTest, givenKernelWhenDebugFlagToUseMaxSimdForCalculationsIsUsedThenMaxWorkgroupSizeIsSimdSizeDependant) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.set(true);

    GT_SYSTEM_INFO mySysInfo = *platformDevices[0]->pSysInfo;
    FeatureTable mySkuTable = *platformDevices[0]->pSkuTable;
    HardwareInfo myHwInfo = {platformDevices[0]->pPlatform, &mySkuTable, platformDevices[0]->pWaTable,
                             &mySysInfo, platformDevices[0]->capabilityTable};

    mySysInfo.EUCount = 24;
    mySysInfo.SubSliceCount = 3;
    mySysInfo.ThreadCount = 24 * 7;
    auto device = std::unique_ptr<Device>(DeviceHelper<>::create(&myHwInfo));

    MockKernelWithInternals kernel(*device);
    kernel.executionEnvironment.LargestCompiledSIMDSize = 32;

    size_t maxKernelWkgSize;
    kernel.mockKernel->getWorkGroupInfo(device.get(), CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &maxKernelWkgSize, nullptr);
    EXPECT_EQ(1024u, maxKernelWkgSize);
    kernel.executionEnvironment.LargestCompiledSIMDSize = 16;
    kernel.mockKernel->getWorkGroupInfo(device.get(), CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &maxKernelWkgSize, nullptr);
    EXPECT_EQ(512u, maxKernelWkgSize);
    kernel.executionEnvironment.LargestCompiledSIMDSize = 8;
    kernel.mockKernel->getWorkGroupInfo(device.get(), CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &maxKernelWkgSize, nullptr);
    EXPECT_EQ(256u, maxKernelWkgSize);
}
