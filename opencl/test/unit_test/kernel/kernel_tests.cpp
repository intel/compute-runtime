/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/page_fault_manager/mock_cpu_page_fault_manager.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/memory_properties_flags_helpers.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/memory_manager/os_agnostic_memory_manager.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/device_host_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/execution_model_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_from_binary.h"
#include "opencl/test/unit_test/program/program_tests.h"
#include "test.h"

#include <memory>

using namespace NEO;
using namespace DeviceHostQueue;

class KernelTest : public ProgramFromBinaryTest {
  public:
    ~KernelTest() override = default;

  protected:
    void SetUp() override {
        ProgramFromBinaryTest::SetUp();
        ASSERT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        cl_device_id device = pClDevice;
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
        knownSource.reset();
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

TEST_P(KernelTest, givenBinaryWhenItIsQueriedForGpuAddressThenAbsoluteAddressIsReturned) {
    cl_kernel_info paramName = CL_KERNEL_BINARY_GPU_ADDRESS_INTEL;
    uint64_t paramValue = 0llu;
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    retVal = pKernel->getInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    auto expectedGpuAddress = GmmHelper::decanonize(pKernel->getKernelInfo().kernelAllocation->getGpuAddress());
    EXPECT_EQ(expectedGpuAddress, paramValue);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
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

    auto kernelMaxWorkGroupSize = pDevice->getDeviceInfo().maxWorkGroupSize - 1;
    pKernel->maxKernelWorkGroupSize = static_cast<uint32_t>(kernelMaxWorkGroupSize);

    retVal = pKernel->getWorkGroupInfo(
        pClDevice,
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
    EXPECT_EQ(kernelMaxWorkGroupSize, paramValue);
}

TEST_P(KernelTest, GetWorkGroupInfo_CompileWorkgroupSize) {
    cl_kernel_info paramName = CL_KERNEL_COMPILE_WORK_GROUP_SIZE;
    size_t paramValue[3];
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    retVal = pKernel->getWorkGroupInfo(
        pClDevice,
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

class KernelFromBinaryTest : public ProgramSimpleFixture {
  public:
    void SetUp() override {
        ProgramSimpleFixture::SetUp();
    }
    void TearDown() override {
        ProgramSimpleFixture::TearDown();
    }
};
typedef Test<KernelFromBinaryTest> KernelFromBinaryTests;

TEST_F(KernelFromBinaryTests, getInfo_NumArgs) {
    cl_device_id device = pClDevice;

    CreateProgramFromBinary(pContext, &device, "kernel_num_args");

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
    cl_device_id device = pClDevice;

    CreateProgramFromBinary(pContext, &device, "simple_kernels");

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

TEST_F(KernelFromBinaryTests, givenArgumentDeclaredAsConstantWhenKernelIsCreatedThenArgumentIsMarkedAsReadOnly) {
    cl_device_id device = pClDevice;

    CreateProgramFromBinary(pContext, &device, "simple_kernels");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        1,
        &device,
        nullptr,
        nullptr,
        nullptr,
        false);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("simple_kernel_6");
    EXPECT_TRUE(pKernelInfo->kernelArgInfo[1].isReadOnly);
    pKernelInfo = pProgram->getKernelInfo("simple_kernel_1");
    EXPECT_TRUE(pKernelInfo->kernelArgInfo[0].isReadOnly);
}

TEST(PatchInfo, Constructor) {
    PatchInfo patchInfo;
    EXPECT_EQ(nullptr, patchInfo.interfaceDescriptorDataLoad);
    EXPECT_EQ(nullptr, patchInfo.localsurface);
    EXPECT_EQ(nullptr, patchInfo.mediavfestate);
    EXPECT_EQ(nullptr, patchInfo.mediaVfeStateSlot1);
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
        DeviceQueueFixture::SetUp(&context, pClDevice);
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
    using CommandStreamReceiver::executionEnvironment;

    using BaseClass::CommandStreamReceiver;

    bool isMultiOsContextCapable() const override { return false; }

    CommandStreamReceiverMock() : BaseClass(*(new ExecutionEnvironment), 0) {
        this->mockExecutionEnvironment.reset(&this->executionEnvironment);
        executionEnvironment.setHwInfo(platformDevices[0]);
        executionEnvironment.prepareRootDeviceEnvironments(1);
        executionEnvironment.initializeMemoryManager();
    }

    void makeResident(GraphicsAllocation &graphicsAllocation) override {
        residency[graphicsAllocation.getUnderlyingBuffer()] = graphicsAllocation.getUnderlyingBufferSize();
        if (passResidencyCallToBaseClass) {
            CommandStreamReceiver::makeResident(graphicsAllocation);
        }
    }

    void makeNonResident(GraphicsAllocation &graphicsAllocation) override {
        residency.erase(graphicsAllocation.getUnderlyingBuffer());
        if (passResidencyCallToBaseClass) {
            CommandStreamReceiver::makeNonResident(graphicsAllocation);
        }
    }

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        return true;
    }

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, bool forcePowerSavingMode) override {
    }
    uint32_t blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking) override { return taskCount; };

    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap &dsh,
        const IndirectHeap &ioh,
        const IndirectHeap &ssh,
        uint32_t taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override {
        CompletionStamp cs = {};
        return cs;
    }

    bool flushBatchedSubmissions() override { return true; }

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_HW;
    }

    std::map<const void *, size_t> residency;
    bool passResidencyCallToBaseClass = true;
    std::unique_ptr<ExecutionEnvironment> mockExecutionEnvironment;
};

TEST_F(KernelPrivateSurfaceTest, testPrivateSurface) {
    ASSERT_NE(nullptr, pDevice);

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

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

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // create kernel
    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    // Test it
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    std::unique_ptr<CommandStreamReceiverMock> csr(new CommandStreamReceiverMock(*executionEnvironment, 0));
    csr->setupContext(*pDevice->getDefaultEngine().osContext);
    csr->residency.clear();
    EXPECT_EQ(0u, csr->residency.size());

    pKernel->makeResident(*csr.get());
    EXPECT_EQ(1u, csr->residency.size());

    csr->makeSurfacePackNonResident(csr->getResidencyAllocations());
    EXPECT_EQ(0u, csr->residency.size());

    delete pKernel;
}

TEST_F(KernelPrivateSurfaceTest, givenKernelWithPrivateSurfaceThatIsInUseByGpuWhenKernelIsBeingDestroyedThenAllocationIsAddedToDefferedFreeList) {
    auto pKernelInfo = std::make_unique<KernelInfo>();
    SPatchAllocateStatelessPrivateSurface tokenSPS;
    tokenSPS.SurfaceStateHeapOffset = 64;
    tokenSPS.DataParamOffset = 40;
    tokenSPS.DataParamSize = 8;
    tokenSPS.PerThreadPrivateMemorySize = 112;
    pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = &tokenSPS;

    SPatchDataParameterStream tokenDPS;
    tokenDPS.DataParameterStreamSize = 64;
    pKernelInfo->patchInfo.dataParameterStream = &tokenDPS;

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    std::unique_ptr<MockKernel> pKernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    pKernel->initialize();

    auto &csr = pDevice->getGpgpuCommandStreamReceiver();

    auto privateSurface = pKernel->getPrivateSurface();
    auto tagAddress = csr.getTagAddress();

    privateSurface->updateTaskCount(*tagAddress + 1, csr.getOsContext().getContextId());

    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    pKernel.reset(nullptr);

    EXPECT_FALSE(csr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_EQ(csr.getTemporaryAllocations().peekHead(), privateSurface);
}

TEST_F(KernelPrivateSurfaceTest, testPrivateSurfaceAllocationFailure) {
    ASSERT_NE(nullptr, pDevice);

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

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

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // create kernel
    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    MemoryManagementFixture::InjectedFunction method = [&](size_t failureIndex) {
        MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
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
}

TEST_F(KernelPrivateSurfaceTest, given32BitDeviceWhenKernelIsCreatedThenPrivateSurfaceIs32BitAllocation) {
    if (is64bit) {
        pDevice->getMemoryManager()->setForce32BitAllocations(true);

        // define kernel info
        auto pKernelInfo = std::make_unique<KernelInfo>();

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

        SPatchExecutionEnvironment tokenEE = {};
        tokenEE.CompiledSIMD8 = false;
        tokenEE.CompiledSIMD16 = false;
        tokenEE.CompiledSIMD32 = true;
        pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

        // create kernel
        MockContext context;
        MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
        MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

        EXPECT_TRUE(pKernel->getPrivateSurface()->is32BitAllocation());

        delete pKernel;
    }
}

HWTEST_F(KernelPrivateSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenPrivateMemorySurfaceStateIsPatchedWithCpuAddress) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

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
}

TEST_F(KernelPrivateSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenPrivateMemorySurfaceStateIsNotPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup global memory
    char buffer[16];
    MockGraphicsAllocation gfxAlloc(buffer, sizeof(buffer));

    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    program.setConstantSurface(&gfxAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());
    EXPECT_EQ(nullptr, pKernel->getSurfaceStateHeap());

    program.setConstantSurface(nullptr);
    delete pKernel;
}

TEST_F(KernelPrivateSurfaceTest, givenNullDataParameterStreamGetConstantBufferSizeReturnsZero) {
    auto pKernelInfo = std::make_unique<KernelInfo>();

    EXPECT_EQ(0u, pKernelInfo->getConstantBufferSize());
}

TEST_F(KernelPrivateSurfaceTest, givenNonNullDataParameterStreamGetConstantBufferSizeReturnsCorrectSize) {
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchDataParameterStream tokenDPS;
    tokenDPS.DataParameterStreamSize = 64;
    pKernelInfo->patchInfo.dataParameterStream = &tokenDPS;

    EXPECT_EQ(64u, pKernelInfo->getConstantBufferSize());
}

TEST_F(KernelPrivateSurfaceTest, GivenKernelWhenPrivateSurfaceTooBigAndGpuPointerSize4ThenReturnOutOfResources) {
    auto pAllocateStatelessPrivateSurface = std::unique_ptr<SPatchAllocateStatelessPrivateSurface>(new SPatchAllocateStatelessPrivateSurface());
    pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize = std::numeric_limits<uint32_t>::max();
    auto executionEnvironment = std::unique_ptr<SPatchExecutionEnvironment>(new SPatchExecutionEnvironment());
    *executionEnvironment = {};
    executionEnvironment->CompiledSIMD32 = 32;
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = pAllocateStatelessPrivateSurface.get();
    pKernelInfo->patchInfo.executionEnvironment = executionEnvironment.get();
    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    std::unique_ptr<MockKernel> pKernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    pKernelInfo->gpuPointerSize = 4;
    pDevice->getMemoryManager()->setForce32BitAllocations(false);
    if (pDevice->getDeviceInfo().computeUnitsUsedForScratch == 0)
        pDevice->deviceInfo.computeUnitsUsedForScratch = 120;
    EXPECT_EQ(CL_OUT_OF_RESOURCES, pKernel->initialize());
}

TEST_F(KernelPrivateSurfaceTest, GivenKernelWhenPrivateSurfaceTooBigAndGpuPointerSize4And32BitAllocationsThenReturnOutOfResources) {
    auto pAllocateStatelessPrivateSurface = std::unique_ptr<SPatchAllocateStatelessPrivateSurface>(new SPatchAllocateStatelessPrivateSurface());
    pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize = std::numeric_limits<uint32_t>::max();
    auto executionEnvironment = std::unique_ptr<SPatchExecutionEnvironment>(new SPatchExecutionEnvironment());
    *executionEnvironment = {};
    executionEnvironment->CompiledSIMD32 = 32;
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = pAllocateStatelessPrivateSurface.get();
    pKernelInfo->patchInfo.executionEnvironment = executionEnvironment.get();
    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    std::unique_ptr<MockKernel> pKernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    pKernelInfo->gpuPointerSize = 4;
    pDevice->getMemoryManager()->setForce32BitAllocations(true);
    if (pDevice->getDeviceInfo().computeUnitsUsedForScratch == 0)
        pDevice->deviceInfo.computeUnitsUsedForScratch = 120;
    EXPECT_EQ(CL_OUT_OF_RESOURCES, pKernel->initialize());
}

TEST_F(KernelPrivateSurfaceTest, GivenKernelWhenPrivateSurfaceTooBigAndGpuPointerSize8And32BitAllocationsThenReturnOutOfResources) {
    auto pAllocateStatelessPrivateSurface = std::unique_ptr<SPatchAllocateStatelessPrivateSurface>(new SPatchAllocateStatelessPrivateSurface());
    pAllocateStatelessPrivateSurface->PerThreadPrivateMemorySize = std::numeric_limits<uint32_t>::max();
    auto executionEnvironment = std::unique_ptr<SPatchExecutionEnvironment>(new SPatchExecutionEnvironment());
    *executionEnvironment = {};
    executionEnvironment->CompiledSIMD32 = 32;
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = pAllocateStatelessPrivateSurface.get();
    pKernelInfo->patchInfo.executionEnvironment = executionEnvironment.get();
    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    std::unique_ptr<MockKernel> pKernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    pKernelInfo->gpuPointerSize = 8;
    pDevice->getMemoryManager()->setForce32BitAllocations(true);
    if (pDevice->getDeviceInfo().computeUnitsUsedForScratch == 0)
        pDevice->deviceInfo.computeUnitsUsedForScratch = 120;
    EXPECT_EQ(CL_OUT_OF_RESOURCES, pKernel->initialize());
}

TEST_F(KernelGlobalSurfaceTest, givenBuiltInKernelWhenKernelIsCreatedThenGlobalSurfaceIsPatchedWithCpuAddress) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    // setup global memory
    SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization AllocateStatelessGlobalMemorySurfaceWithInitialization;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = &AllocateStatelessGlobalMemorySurfaceWithInitialization;

    SPatchDataParameterStream tempSPatchDataParameterStream;
    tempSPatchDataParameterStream.DataParameterStreamSize = 16;
    pKernelInfo->patchInfo.dataParameterStream = &tempSPatchDataParameterStream;

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    char buffer[16];

    GraphicsAllocation gfxAlloc(0, GraphicsAllocation::AllocationType::UNKNOWN, buffer, (uint64_t)buffer - 8u, 8, (osHandle)1u, MemoryPool::MemoryNull);
    uint64_t bufferAddress = (uint64_t)gfxAlloc.getUnderlyingBuffer();

    // create kernel
    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    program.setGlobalSurface(&gfxAlloc);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    pKernel->isBuiltIn = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)pKernel->getCrossThreadData());

    program.setGlobalSurface(nullptr);
    delete pKernel;
}

TEST_F(KernelGlobalSurfaceTest, givenNDRangeKernelWhenKernelIsCreatedThenGlobalSurfaceIsPatchedWithBaseAddressOffset) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    // setup global memory
    SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization AllocateStatelessGlobalMemorySurfaceWithInitialization;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = &AllocateStatelessGlobalMemorySurfaceWithInitialization;

    SPatchDataParameterStream tempSPatchDataParameterStream;
    tempSPatchDataParameterStream.DataParameterStreamSize = 16;
    pKernelInfo->patchInfo.dataParameterStream = &tempSPatchDataParameterStream;

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    char buffer[16];

    GraphicsAllocation gfxAlloc(0, GraphicsAllocation::AllocationType::UNKNOWN, buffer, (uint64_t)buffer - 8u, 8, MemoryPool::MemoryNull);
    uint64_t bufferAddress = gfxAlloc.getGpuAddress();

    // create kernel
    MockProgram program(*pDevice->getExecutionEnvironment());
    program.setGlobalSurface(&gfxAlloc);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)pKernel->getCrossThreadData());

    program.setGlobalSurface(nullptr);

    delete pKernel;
}

HWTEST_F(KernelGlobalSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenGlobalMemorySurfaceStateIsPatchedWithCpuAddress) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockGraphicsAllocation gfxAlloc(buffer, sizeof(buffer));
    auto bufferAddress = gfxAlloc.getGpuAddress();

    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    program.setGlobalSurface(&gfxAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

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
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(bufferAddress, surfaceAddress);

    program.setGlobalSurface(nullptr);
    delete pKernel;
}

TEST_F(KernelGlobalSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenGlobalMemorySurfaceStateIsNotPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup global memory
    char buffer[16];
    MockGraphicsAllocation gfxAlloc(buffer, sizeof(buffer));

    MockProgram program(*pDevice->getExecutionEnvironment());
    program.setGlobalSurface(&gfxAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());
    EXPECT_EQ(nullptr, pKernel->getSurfaceStateHeap());

    program.setGlobalSurface(nullptr);
    delete pKernel;
}

TEST_F(KernelConstantSurfaceTest, givenBuiltInKernelWhenKernelIsCreatedThenConstantSurfaceIsPatchedWithCpuAddress) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    // setup constant memory
    SPatchAllocateStatelessConstantMemorySurfaceWithInitialization AllocateStatelessConstantMemorySurfaceWithInitialization;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = &AllocateStatelessConstantMemorySurfaceWithInitialization;

    SPatchDataParameterStream tempSPatchDataParameterStream;
    tempSPatchDataParameterStream.DataParameterStreamSize = 16;
    pKernelInfo->patchInfo.dataParameterStream = &tempSPatchDataParameterStream;

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    char buffer[16];

    GraphicsAllocation gfxAlloc(0, GraphicsAllocation::AllocationType::UNKNOWN, buffer, (uint64_t)buffer - 8u, 8, (osHandle)1u, MemoryPool::MemoryNull);
    uint64_t bufferAddress = (uint64_t)gfxAlloc.getUnderlyingBuffer();

    // create kernel
    MockProgram program(*pDevice->getExecutionEnvironment());
    program.setConstantSurface(&gfxAlloc);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    pKernel->isBuiltIn = true;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)pKernel->getCrossThreadData());

    program.setConstantSurface(nullptr);
    delete pKernel;
}

TEST_F(KernelConstantSurfaceTest, givenNDRangeKernelWhenKernelIsCreatedThenConstantSurfaceIsPatchedWithBaseAddressOffset) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    // setup constant memory
    SPatchAllocateStatelessConstantMemorySurfaceWithInitialization AllocateStatelessConstantMemorySurfaceWithInitialization;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = &AllocateStatelessConstantMemorySurfaceWithInitialization;

    SPatchDataParameterStream tempSPatchDataParameterStream;
    tempSPatchDataParameterStream.DataParameterStreamSize = 16;
    pKernelInfo->patchInfo.dataParameterStream = &tempSPatchDataParameterStream;

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    char buffer[16];

    GraphicsAllocation gfxAlloc(0, GraphicsAllocation::AllocationType::UNKNOWN, buffer, (uint64_t)buffer - 8u, 8, MemoryPool::MemoryNull);
    uint64_t bufferAddress = gfxAlloc.getGpuAddress();

    // create kernel
    MockProgram program(*pDevice->getExecutionEnvironment());
    program.setConstantSurface(&gfxAlloc);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)pKernel->getCrossThreadData());

    program.setConstantSurface(nullptr);

    delete pKernel;
}

HWTEST_F(KernelConstantSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenConstantMemorySurfaceStateIsPatchedWithCpuAddress) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockGraphicsAllocation gfxAlloc(buffer, sizeof(buffer));
    auto bufferAddress = gfxAlloc.getGpuAddress();

    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    program.setConstantSurface(&gfxAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

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
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(bufferAddress, surfaceAddress);

    program.setConstantSurface(nullptr);
    delete pKernel;
}

TEST_F(KernelConstantSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenConstantMemorySurfaceStateIsNotPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // setup global memory
    char buffer[16];
    MockGraphicsAllocation gfxAlloc(buffer, sizeof(buffer));

    MockProgram program(*pDevice->getExecutionEnvironment());
    program.setConstantSurface(&gfxAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());
    EXPECT_EQ(nullptr, pKernel->getSurfaceStateHeap());

    program.setConstantSurface(nullptr);
    delete pKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelEventPoolSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenEventPoolSurfaceStateIsPatchedWithNullSurface) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

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
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(0u, surfaceAddress);
    auto surfaceType = surfaceState->getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, surfaceType);

    delete pKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelEventPoolSurfaceTest, givenStatefulKernelWhenEventPoolIsPatchedThenEventPoolSurfaceStateIsProgrammed) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

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
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(pDevQueue->getEventPoolBuffer()->getGpuAddress(), surfaceAddress);
    auto surfaceType = surfaceState->getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, surfaceType);

    delete pKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelEventPoolSurfaceTest, givenKernelWithNullEventPoolInKernelInfoWhenEventPoolIsPatchedThenAddressIsNotPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface = nullptr;

    // create kernel
    MockProgram program(*pDevice->getExecutionEnvironment());
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    uint64_t crossThreadData = 123;

    pKernel->setCrossThreadData(&crossThreadData, sizeof(uint64_t));

    pKernel->patchEventPool(pDevQueue);

    EXPECT_EQ(123u, *(uint64_t *)pKernel->getCrossThreadData());

    delete pKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelEventPoolSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenEventPoolSurfaceStateIsNotPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockProgram program(*pDevice->getExecutionEnvironment());
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    if (pDevice->getSupportedClVersion() < 20) {
        EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());
    } else {
    }

    delete pKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelEventPoolSurfaceTest, givenStatelessKernelWhenEventPoolIsPatchedThenCrossThreadDataIsPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockProgram program(*pDevice->getExecutionEnvironment());
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    uint64_t crossThreadData = 0;

    pKernel->setCrossThreadData(&crossThreadData, sizeof(uint64_t));

    pKernel->patchEventPool(pDevQueue);

    EXPECT_EQ(pDevQueue->getEventPoolBuffer()->getGpuAddressToPatch(), *(uint64_t *)pKernel->getCrossThreadData());

    delete pKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelDefaultDeviceQueueSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenDefaultDeviceQueueSurfaceStateIsPatchedWithNullSurface) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

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
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(0u, surfaceAddress);
    auto surfaceType = surfaceState->getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL, surfaceType);

    delete pKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelDefaultDeviceQueueSurfaceTest, givenStatefulKernelWhenDefaultDeviceQueueIsPatchedThenSurfaceStateIsCorrectlyProgrammed) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false, pDevice);
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

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
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(pDevQueue->getQueueBuffer()->getGpuAddress(), surfaceAddress);
    auto surfaceType = surfaceState->getSurfaceType();
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, surfaceType);

    delete pKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelDefaultDeviceQueueSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenDefaultDeviceQueueSurfaceStateIsNotPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockProgram program(*pDevice->getExecutionEnvironment());
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(0u, pKernel->getSurfaceStateHeapSize());

    delete pKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelDefaultDeviceQueueSurfaceTest, givenKernelWithNullDeviceQueueKernelInfoWhenDefaultDeviceQueueIsPatchedThenAddressIsNotPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = nullptr;

    // create kernel
    MockProgram program(*pDevice->getExecutionEnvironment());
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    uint64_t crossThreadData = 123;

    pKernel->setCrossThreadData(&crossThreadData, sizeof(uint64_t));

    pKernel->patchDefaultDeviceQueue(pDevQueue);

    EXPECT_EQ(123u, *(uint64_t *)pKernel->getCrossThreadData());

    delete pKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelDefaultDeviceQueueSurfaceTest, givenStatelessKernelWhenDefaultDeviceQueueIsPatchedThenCrossThreadDataIsPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE = {};
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
    MockProgram program(*pDevice->getExecutionEnvironment());
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // define stateful path
    pKernelInfo->usesSsh = false;
    pKernelInfo->requiresSshForBuffers = false;

    uint64_t crossThreadData = 0;

    pKernel->setCrossThreadData(&crossThreadData, sizeof(uint64_t));

    pKernel->patchDefaultDeviceQueue(pDevQueue);

    EXPECT_EQ(pDevQueue->getQueueBuffer()->getGpuAddressToPatch(), *(uint64_t *)pKernel->getCrossThreadData());

    delete pKernel;
}

typedef Test<DeviceFixture> KernelResidencyTest;

HWTEST_F(KernelResidencyTest, givenKernelWhenMakeResidentIsCalledThenKernelIsaIsMadeResident) {
    ASSERT_NE(nullptr, pDevice);
    char pCrossThreadData[64];

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    // setup kernel arg offsets
    KernelArgPatchInfo kernelArgPatchInfo;

    pKernelInfo->kernelArgInfo.resize(3);
    pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);
    pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector.push_back(kernelArgPatchInfo);

    pKernelInfo->kernelArgInfo[2].kernelArgPatchInfoVector[0].crossthreadOffset = 0x10;
    pKernelInfo->kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset = 0x20;
    pKernelInfo->kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0x30;

    MockProgram program(*pDevice->getExecutionEnvironment());
    MockContext ctx;
    program.setContext(&ctx);
    std::unique_ptr<MockKernel> pKernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

    EXPECT_EQ(0u, commandStreamReceiver.makeResidentAllocations.size());
    pKernel->makeResident(pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(1u, commandStreamReceiver.makeResidentAllocations.size());
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(pKernel->getKernelInfo().getGraphicsAllocation()));

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenMakeResidentIsCalledThenExportedFunctionsIsaAllocationIsMadeResident) {
    auto pKernelInfo = std::make_unique<KernelInfo>();
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    MockProgram program(*pDevice->getExecutionEnvironment());
    auto exportedFunctionsSurface = std::make_unique<MockGraphicsAllocation>();
    program.exportedFunctionsSurface = exportedFunctionsSurface.get();
    MockContext ctx;
    program.setContext(&ctx);
    std::unique_ptr<MockKernel> pKernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(0u, commandStreamReceiver.makeResidentAllocations.size());
    pKernel->makeResident(pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(program.exportedFunctionsSurface));

    // check getResidency as well
    std::vector<NEO::Surface *> residencySurfaces;
    pKernel->getResidency(residencySurfaces);
    std::unique_ptr<NEO::ExecutionEnvironment> mockCsrExecEnv;
    {
        CommandStreamReceiverMock csrMock;
        csrMock.passResidencyCallToBaseClass = false;
        for (const auto &s : residencySurfaces) {
            s->makeResident(csrMock);
            delete s;
        }
        EXPECT_EQ(1U, csrMock.residency.count(exportedFunctionsSurface->getUnderlyingBuffer()));
        mockCsrExecEnv = std::move(csrMock.mockExecutionEnvironment);
    }

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenMakeResidentIsCalledThenGlobalBufferIsMadeResident) {
    auto pKernelInfo = std::make_unique<KernelInfo>();
    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

    MockProgram program(*pDevice->getExecutionEnvironment());
    MockContext ctx;
    program.setContext(&ctx);
    program.globalSurface = new MockGraphicsAllocation();
    std::unique_ptr<MockKernel> pKernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    EXPECT_EQ(0u, commandStreamReceiver.makeResidentAllocations.size());
    pKernel->makeResident(pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(program.globalSurface));

    std::vector<NEO::Surface *> residencySurfaces;
    pKernel->getResidency(residencySurfaces);
    std::unique_ptr<NEO::ExecutionEnvironment> mockCsrExecEnv;
    {
        CommandStreamReceiverMock csrMock;
        csrMock.passResidencyCallToBaseClass = false;
        for (const auto &s : residencySurfaces) {
            s->makeResident(csrMock);
            delete s;
        }
        EXPECT_EQ(1U, csrMock.residency.count(program.globalSurface->getUnderlyingBuffer()));
        mockCsrExecEnv = std::move(csrMock.mockExecutionEnvironment);
    }

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenItUsesIndirectUnifiedMemoryDeviceAllocationThenTheyAreMadeResident) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto unifiedMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY));

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());

    EXPECT_EQ(0u, commandStreamReceiver.getResidencyAllocations().size());

    mockKernel.mockKernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, true);

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());

    EXPECT_EQ(1u, commandStreamReceiver.getResidencyAllocations().size());

    EXPECT_EQ(commandStreamReceiver.getResidencyAllocations()[0]->getGpuAddress(), castToUint64(unifiedMemoryAllocation));

    mockKernel.mockKernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_SVM_PTRS, true);

    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelUsingIndirectHostMemoryWhenMakeResidentIsCalledThenOnlyHostAllocationsAreMadeResident) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto unifiedDeviceMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY));
    auto unifiedHostMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY));

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(0u, commandStreamReceiver.getResidencyAllocations().size());
    mockKernel.mockKernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, true);

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(1u, commandStreamReceiver.getResidencyAllocations().size());
    EXPECT_EQ(commandStreamReceiver.getResidencyAllocations()[0]->getGpuAddress(), castToUint64(unifiedHostMemoryAllocation));

    svmAllocationsManager->freeSVMAlloc(unifiedDeviceMemoryAllocation);
    svmAllocationsManager->freeSVMAlloc(unifiedHostMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelUsingIndirectSharedMemoryWhenMakeResidentIsCalledThenOnlySharedAllocationsAreMadeResident) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto unifiedSharedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY), mockKernel.mockContext->getSpecialQueue());
    auto unifiedHostMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::HOST_UNIFIED_MEMORY));

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(0u, commandStreamReceiver.getResidencyAllocations().size());
    mockKernel.mockKernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, true);

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(1u, commandStreamReceiver.getResidencyAllocations().size());
    EXPECT_EQ(commandStreamReceiver.getResidencyAllocations()[0]->getGpuAddress(), castToUint64(unifiedSharedMemoryAllocation));

    svmAllocationsManager->freeSVMAlloc(unifiedSharedMemoryAllocation);
    svmAllocationsManager->freeSVMAlloc(unifiedHostMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenDeviceUnifiedMemoryAndPageFaultManagerWhenMakeResidentIsCalledThenAllocationIsNotDecommited) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto unifiedMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY));
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);

    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    mockKernel.mockKernel->setUnifiedMemoryExecInfo(unifiedMemoryGraphicsAllocation->gpuAllocation);
    EXPECT_EQ(1u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());

    mockKernel.mockKernel->makeResident(commandStreamReceiver);

    EXPECT_EQ(mockPageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(mockPageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 0);

    mockKernel.mockKernel->clearUnifiedMemoryExecInfo();
    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenSharedUnifiedMemoryAndPageFaultManagerWhenMakeResidentIsCalledThenAllocationIsDecommited) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY), mockKernel.mockContext->getSpecialQueue());
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    mockPageFaultManager->insertAllocation(unifiedMemoryAllocation, 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue());

    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 1);

    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    mockKernel.mockKernel->setUnifiedMemoryExecInfo(unifiedMemoryGraphicsAllocation->gpuAllocation);
    EXPECT_EQ(1u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());

    mockKernel.mockKernel->makeResident(commandStreamReceiver);

    EXPECT_EQ(mockPageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(mockPageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 1);
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 1);

    EXPECT_EQ(mockPageFaultManager->protectedMemoryAccessAddress, unifiedMemoryAllocation);
    EXPECT_EQ(mockPageFaultManager->protectedSize, 4096u);
    EXPECT_EQ(mockPageFaultManager->transferToGpuAddress, unifiedMemoryAllocation);

    mockKernel.mockKernel->clearUnifiedMemoryExecInfo();
    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenSharedUnifiedMemoryAndNotRequiredMemSyncWhenMakeResidentIsCalledThenAllocationIsNotDecommited) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice, nullptr, true);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY), mockKernel.mockContext->getSpecialQueue());
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    mockPageFaultManager->insertAllocation(unifiedMemoryAllocation, 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue());

    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 1);
    mockKernel.mockKernel->kernelArguments[0] = {Kernel::kernelArgType::SVM_ALLOC_OBJ, unifiedMemoryGraphicsAllocation->gpuAllocation, unifiedMemoryAllocation, 4096u, unifiedMemoryGraphicsAllocation->gpuAllocation, sizeof(uintptr_t)};
    mockKernel.mockKernel->setUnifiedMemorySyncRequirement(false);

    mockKernel.mockKernel->makeResident(commandStreamReceiver);

    EXPECT_EQ(mockPageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(mockPageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 1);
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 0);

    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenSharedUnifiedMemoryRequiredMemSyncWhenMakeResidentIsCalledThenAllocationIsDecommited) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice, nullptr, true);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY), mockKernel.mockContext->getSpecialQueue());
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    mockPageFaultManager->insertAllocation(unifiedMemoryAllocation, 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue());

    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 1);
    mockKernel.mockKernel->kernelArguments[0] = {Kernel::kernelArgType::SVM_ALLOC_OBJ, unifiedMemoryGraphicsAllocation->gpuAllocation, unifiedMemoryAllocation, 4096u, unifiedMemoryGraphicsAllocation->gpuAllocation, sizeof(uintptr_t)};
    mockKernel.mockKernel->setUnifiedMemorySyncRequirement(true);

    mockKernel.mockKernel->makeResident(commandStreamReceiver);

    EXPECT_EQ(mockPageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(mockPageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 1);
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 1);

    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenSharedUnifiedMemoryAllocPageFaultManagerAndIndirectAllocsAllowedWhenMakeResidentIsCalledThenAllocationIsDecommited) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY), mockKernel.mockContext->getSpecialQueue());
    mockPageFaultManager->insertAllocation(unifiedMemoryAllocation, 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue());

    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 1);
    mockKernel.mockKernel->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;

    mockKernel.mockKernel->makeResident(commandStreamReceiver);

    EXPECT_EQ(mockPageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(mockPageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 1);
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 1);

    EXPECT_EQ(mockPageFaultManager->protectedMemoryAccessAddress, unifiedMemoryAllocation);
    EXPECT_EQ(mockPageFaultManager->protectedSize, 4096u);
    EXPECT_EQ(mockPageFaultManager->transferToGpuAddress, unifiedMemoryAllocation);

    mockKernel.mockKernel->clearUnifiedMemoryExecInfo();
    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenSetKernelExecInfoWithUnifiedMemoryIsCalledThenAllocationIsStoredWithinKernel) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto unifiedMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY));
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);

    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());

    mockKernel.mockKernel->setUnifiedMemoryExecInfo(unifiedMemoryGraphicsAllocation->gpuAllocation);

    EXPECT_EQ(1u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    EXPECT_EQ(mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations[0]->getGpuAddress(), castToUint64(unifiedMemoryAllocation));

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(1u, commandStreamReceiver.getResidencyAllocations().size());
    EXPECT_EQ(commandStreamReceiver.getResidencyAllocations()[0]->getGpuAddress(), castToUint64(unifiedMemoryAllocation));

    mockKernel.mockKernel->clearUnifiedMemoryExecInfo();
    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryIsCalledThenAllocationIsStoredWithinKernel) {
    MockKernelWithInternals mockKernel(*this->pClDevice);

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto unifiedMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY));

    auto unifiedMemoryAllocation2 = svmAllocationsManager->createUnifiedMemoryAllocation(pDevice->getRootDeviceIndex(), 4096u, SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::DEVICE_UNIFIED_MEMORY));

    auto status = clSetKernelExecInfo(mockKernel.mockKernel, CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL, sizeof(unifiedMemoryAllocation), &unifiedMemoryAllocation);
    EXPECT_EQ(CL_SUCCESS, status);

    EXPECT_EQ(1u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    EXPECT_EQ(mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations[0]->getGpuAddress(), castToUint64(unifiedMemoryAllocation));

    status = clSetKernelExecInfo(mockKernel.mockKernel, CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL, sizeof(unifiedMemoryAllocation), &unifiedMemoryAllocation2);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    EXPECT_EQ(mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations[0]->getGpuAddress(), castToUint64(unifiedMemoryAllocation2));

    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation2);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryDevicePropertyIsCalledThenKernelControlIsChanged) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectDeviceAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockKernel, CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectDeviceAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockKernel.mockKernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    enableIndirectDeviceAccess = CL_FALSE;
    status = clSetKernelExecInfo(mockKernel.mockKernel, CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectDeviceAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryHostPropertyIsCalledThenKernelControlIsChanged) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectHostAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockKernel, CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectHostAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockKernel.mockKernel->unifiedMemoryControls.indirectHostAllocationsAllowed);
    enableIndirectHostAccess = CL_FALSE;
    status = clSetKernelExecInfo(mockKernel.mockKernel, CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectHostAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectHostAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemorySharedPropertyIsCalledThenKernelControlIsChanged) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectSharedAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockKernel, CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectSharedAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockKernel.mockKernel->unifiedMemoryControls.indirectSharedAllocationsAllowed);
    enableIndirectSharedAccess = CL_FALSE;
    status = clSetKernelExecInfo(mockKernel.mockKernel, CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectSharedAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

TEST(KernelImageDetectionTests, givenKernelWithImagesOnlyWhenItIsAskedIfItHasImagesOnlyThenTrueIsReturned) {
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelArgInfo.resize(3);
    pKernelInfo->kernelArgInfo[2].isImage = true;
    pKernelInfo->kernelArgInfo[1].isMediaBlockImage = true;
    pKernelInfo->kernelArgInfo[0].isMediaImage = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice()));
    auto kernel = clUniquePtr(new MockKernel(program.get(), *pKernelInfo, *device));
    EXPECT_FALSE(kernel->usesOnlyImages());
    kernel->initialize();
    EXPECT_TRUE(kernel->usesOnlyImages());
}

TEST(KernelImageDetectionTests, givenKernelWithImagesAndBuffersWhenItIsAskedIfItHasImagesOnlyThenFalseIsReturned) {
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelArgInfo.resize(3);
    pKernelInfo->kernelArgInfo[2].isImage = true;
    pKernelInfo->kernelArgInfo[1].isBuffer = true;
    pKernelInfo->kernelArgInfo[0].isMediaImage = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice()));
    auto kernel = clUniquePtr(new MockKernel(program.get(), *pKernelInfo, *device));
    EXPECT_FALSE(kernel->usesOnlyImages());
    kernel->initialize();
    EXPECT_FALSE(kernel->usesOnlyImages());
}

TEST(KernelImageDetectionTests, givenKernelWithNoImagesWhenItIsAskedIfItHasImagesOnlyThenFalseIsReturned) {
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelArgInfo.resize(1);
    pKernelInfo->kernelArgInfo[0].isBuffer = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice()));
    auto kernel = clUniquePtr(new MockKernel(program.get(), *pKernelInfo, *device));
    EXPECT_FALSE(kernel->usesOnlyImages());
    kernel->initialize();
    EXPECT_FALSE(kernel->usesOnlyImages());
}

HWTEST_F(KernelResidencyTest, test_MakeArgsResidentCheckImageFromImage) {
    ASSERT_NE(nullptr, pDevice);

    //create NV12 image
    cl_mem_flags flags = CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS;
    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_NV12_INTEL;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, pClDevice->getHardwareInfo().capabilityTable.clVersionSupport);

    cl_image_desc imageDesc = {};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 16;
    imageDesc.image_height = 16;
    imageDesc.image_depth = 1;

    cl_int retVal;
    MockContext context;
    std::unique_ptr<NEO::Image> imageNV12(Image::create(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0),
                                                        flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(imageNV12->getMediaPlaneType(), 0u);

    //create Y plane
    imageFormat.image_channel_order = CL_R;
    flags = CL_MEM_READ_ONLY;
    surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.clVersionSupport);

    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.mem_object = imageNV12.get();

    std::unique_ptr<NEO::Image> imageY(Image::create(&context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0),
                                                     flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(imageY->getMediaPlaneType(), 0u);

    auto pKernelInfo = std::make_unique<KernelInfo>();
    KernelArgInfo kernelArgInfo;
    kernelArgInfo.isImage = true;

    pKernelInfo->kernelArgInfo.push_back(std::move(kernelArgInfo));

    auto program = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment());
    program->setContext(&context);
    std::unique_ptr<MockKernel> pKernel(new MockKernel(program.get(), *pKernelInfo, *pClDevice));

    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    pKernel->storeKernelArg(0, Kernel::IMAGE_OBJ, (cl_mem)imageY.get(), NULL, 0);
    pKernel->makeResident(pDevice->getGpgpuCommandStreamReceiver());

    EXPECT_FALSE(imageNV12->isImageFromImage());
    EXPECT_TRUE(imageY->isImageFromImage());

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore, commandStreamReceiver.samplerCacheFlushRequired);
}

struct KernelExecutionEnvironmentTest : public Test<DeviceFixture> {
    void SetUp() override {
        DeviceFixture::SetUp();

        program = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment());
        pKernelInfo = std::make_unique<KernelInfo>();
        executionEnvironment.CompiledSIMD32 = 1;
        pKernelInfo->patchInfo.executionEnvironment = &executionEnvironment;

        pKernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    }

    void TearDown() override {
        delete pKernel;

        DeviceFixture::TearDown();
    }

    MockKernel *pKernel;
    std::unique_ptr<MockProgram> program;
    std::unique_ptr<KernelInfo> pKernelInfo;
    SPatchExecutionEnvironment executionEnvironment = {};
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

TEST_F(KernelExecutionEnvironmentTest, getMaxSimdReturns1WhenLargestCompilledSimdSizeEqualOne) {

    executionEnvironment.LargestCompiledSIMDSize = 1;

    auto oldExcEnv = this->pKernelInfo->patchInfo.executionEnvironment;

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
        program = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment());
        patchDataParameterStream.DataParameterStreamSize = 64 * sizeof(uint8_t);

        pKernelInfo = std::make_unique<KernelInfo>();
        ASSERT_NE(nullptr, pKernelInfo);
        pKernelInfo->patchInfo.dataParameterStream = &patchDataParameterStream;
        executionEnvironment.CompiledSIMD32 = 1;
        pKernelInfo->patchInfo.executionEnvironment = &executionEnvironment;
    }

    void TearDown() override {

        DeviceFixture::TearDown();
    }

    std::unique_ptr<MockProgram> program;
    std::unique_ptr<KernelInfo> pKernelInfo;
    SPatchDataParameterStream patchDataParameterStream;
    SPatchExecutionEnvironment executionEnvironment = {};
};

TEST_F(KernelCrossThreadTests, globalWorkOffset) {

    pKernelInfo->workloadInfo.globalWorkOffsetOffsets[1] = 4;

    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.globalWorkOffsetX);
    EXPECT_NE(nullptr, kernel.globalWorkOffsetY);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.globalWorkOffsetY);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.globalWorkOffsetZ);
}

TEST_F(KernelCrossThreadTests, localWorkSize) {

    pKernelInfo->workloadInfo.localWorkSizeOffsets[0] = 0xc;

    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.localWorkSizeX);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.localWorkSizeX);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.localWorkSizeY);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.localWorkSizeZ);
}

TEST_F(KernelCrossThreadTests, localWorkSize2) {

    pKernelInfo->workloadInfo.localWorkSizeOffsets2[1] = 0xd;

    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.localWorkSizeX2);
    EXPECT_NE(nullptr, kernel.localWorkSizeY2);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.localWorkSizeY2);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.localWorkSizeZ2);
}

TEST_F(KernelCrossThreadTests, globalWorkSize) {

    pKernelInfo->workloadInfo.globalWorkSizeOffsets[2] = 8;

    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.globalWorkSizeX);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.globalWorkSizeY);
    EXPECT_NE(nullptr, kernel.globalWorkSizeZ);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.globalWorkSizeZ);
}

TEST_F(KernelCrossThreadTests, workDim) {

    pKernelInfo->workloadInfo.workDimOffset = 12;

    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.workDim);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.workDim);
}

TEST_F(KernelCrossThreadTests, numWorkGroups) {

    pKernelInfo->workloadInfo.numWorkGroupsOffset[0] = 0 * sizeof(uint32_t);
    pKernelInfo->workloadInfo.numWorkGroupsOffset[1] = 1 * sizeof(uint32_t);
    pKernelInfo->workloadInfo.numWorkGroupsOffset[2] = 2 * sizeof(uint32_t);

    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
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

    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.enqueuedLocalWorkSizeX);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.enqueuedLocalWorkSizeX);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.enqueuedLocalWorkSizeY);
    EXPECT_EQ(&Kernel::dummyPatchLocation, kernel.enqueuedLocalWorkSizeZ);
}

TEST_F(KernelCrossThreadTests, maxWorkGroupSize) {

    pKernelInfo->workloadInfo.maxWorkGroupSizeOffset = 12;

    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.maxWorkGroupSizeForCrossThreadData);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.maxWorkGroupSizeForCrossThreadData);
    EXPECT_EQ(static_cast<void *>(kernel.getCrossThreadData() + pKernelInfo->workloadInfo.maxWorkGroupSizeOffset), static_cast<void *>(kernel.maxWorkGroupSizeForCrossThreadData));
    EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize, *kernel.maxWorkGroupSizeForCrossThreadData);
    EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize, kernel.maxKernelWorkGroupSize);
}

TEST_F(KernelCrossThreadTests, dataParameterSimdSize) {

    pKernelInfo->workloadInfo.simdSizeOffset = 16;
    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    executionEnvironment.CompiledSIMD32 = false;
    executionEnvironment.CompiledSIMD16 = true;
    executionEnvironment.CompiledSIMD8 = true;
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.dataParameterSimdSize);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.dataParameterSimdSize);
    EXPECT_EQ(static_cast<void *>(kernel.getCrossThreadData() + pKernelInfo->workloadInfo.simdSizeOffset), static_cast<void *>(kernel.dataParameterSimdSize));
    EXPECT_EQ_VAL(pKernelInfo->getMaxSimdSize(), *kernel.dataParameterSimdSize);
}

TEST_F(KernelCrossThreadTests, GIVENparentEventOffsetWHENinitializeKernelTHENparentEventInitWithInvalid) {
    pKernelInfo->workloadInfo.parentEventOffset = 16;
    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.parentEventOffset);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.parentEventOffset);
    EXPECT_EQ(static_cast<void *>(kernel.getCrossThreadData() + pKernelInfo->workloadInfo.parentEventOffset), static_cast<void *>(kernel.parentEventOffset));
    EXPECT_EQ(WorkloadInfo::invalidParentEvent, *kernel.parentEventOffset);
}

TEST_F(KernelCrossThreadTests, kernelAddRefCountToProgram) {

    auto refCount = program->getReference();
    MockKernel *kernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);
    auto refCount2 = program->getReference();
    EXPECT_EQ(refCount2, refCount + 1);

    delete kernel;
    auto refCount3 = program->getReference();
    EXPECT_EQ(refCount, refCount3);
}

TEST_F(KernelCrossThreadTests, kernelSetsTotalSLMSize) {

    pKernelInfo->workloadInfo.slmStaticSize = 1024;

    MockKernel *kernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);

    EXPECT_EQ(1024u, kernel->slmTotalSize);

    delete kernel;
}
TEST_F(KernelCrossThreadTests, givenKernelWithPrivateMemoryWhenItIsCreatedThenCurbeIsPatchedProperly) {

    SPatchAllocateStatelessPrivateSurface allocatePrivate;
    allocatePrivate.DataParamSize = 8;
    allocatePrivate.DataParamOffset = 0;
    allocatePrivate.PerThreadPrivateMemorySize = 1;
    pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = &allocatePrivate;

    MockKernel *kernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);

    kernel->initialize();

    auto privateSurface = kernel->getPrivateSurface();

    auto constantBuffer = kernel->getCrossThreadData();
    auto privateAddress = (uintptr_t)privateSurface->getGpuAddressToPatch();
    auto ptrCurbe = (uint64_t *)constantBuffer;
    auto privateAddressFromCurbe = (uintptr_t)*ptrCurbe;

    EXPECT_EQ(privateAddressFromCurbe, privateAddress);

    delete kernel;
}

TEST_F(KernelCrossThreadTests, givenKernelWithPreferredWkgMultipleWhenItIsCreatedThenCurbeIsPatchedProperly) {

    pKernelInfo->workloadInfo.preferredWkgMultipleOffset = 8;
    MockKernel *kernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);

    kernel->initialize();

    auto *crossThread = kernel->getCrossThreadData();

    uint32_t *preferredWkgMultipleOffset = (uint32_t *)ptrOffset(crossThread, 8);

    EXPECT_EQ(pKernelInfo->getMaxSimdSize(), *preferredWkgMultipleOffset);

    delete kernel;
}

TEST_F(KernelCrossThreadTests, patchBlocksSimdSize) {
    MockKernelWithInternals *kernel = new MockKernelWithInternals(*pClDevice);

    // store offset to child's simd size in kernel info
    uint32_t crossThreadOffset = 0; //offset of simd size
    kernel->kernelInfo.childrenKernelsIdOffset.push_back({0, crossThreadOffset});

    // add a new block kernel to program
    auto infoBlock = new KernelInfo();
    kernel->executionEnvironmentBlock.CompiledSIMD8 = 0;
    kernel->executionEnvironmentBlock.CompiledSIMD16 = 1;
    kernel->executionEnvironmentBlock.CompiledSIMD32 = 0;
    infoBlock->patchInfo.executionEnvironment = &kernel->executionEnvironmentBlock;
    kernel->mockProgram->blockKernelManager->addBlockKernelInfo(infoBlock);

    // patch block's simd size
    kernel->mockKernel->patchBlocksSimdSize();

    // obtain block's simd size from cross thread data
    void *blockSimdSize = ptrOffset(kernel->mockKernel->getCrossThreadData(), kernel->kernelInfo.childrenKernelsIdOffset[0].second);
    uint32_t *simdSize = reinterpret_cast<uint32_t *>(blockSimdSize);

    // check of block's simd size has been patched correctly
    EXPECT_EQ(kernel->mockProgram->blockKernelManager->getBlockKernelInfo(0)->getMaxSimdSize(), *simdSize);

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
    kai.metadataExtended = std::make_unique<ArgTypeMetadataExtended>();
    kai.metadataExtended->argName = "arg1";
    info.kernelArgInfo.push_back(std::move(kai));

    EXPECT_EQ(-1, info.getArgNumByName(""));
    EXPECT_EQ(-1, info.getArgNumByName("arg2"));

    EXPECT_EQ(0, info.getArgNumByName("arg1"));

    kai = {};
    kai.metadataExtended = std::make_unique<ArgTypeMetadataExtended>();
    kai.metadataExtended->argName = "arg2";
    info.kernelArgInfo.push_back(std::move(kai));

    EXPECT_EQ(0, info.getArgNumByName("arg1"));
    EXPECT_EQ(1, info.getArgNumByName("arg2"));

    info.kernelArgInfo[0].metadataExtended.reset();
    EXPECT_EQ(-1, info.getArgNumByName("arg1"));
}

TEST(KernelTest, getInstructionHeapSizeForExecutionModelReturnsZeroForNormalKernel) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
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

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    kernel.kernelInfo.resizeKernelArgInfoAndRegisterParameter(1);
    kernel.mockKernel->initialize();

    MockBuiltinDispatchBuilder mockBuilder(*device->getBuiltIns());
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
}
TEST(KernelTest, givenKernelWhenDebugFlagToUseMaxSimdForCalculationsIsUsedThenMaxWorkgroupSizeIsSimdSizeDependant) {
    DebugManagerStateRestore dbgStateRestore;
    DebugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.set(true);

    HardwareInfo myHwInfo = *platformDevices[0];
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;

    mySysInfo.EUCount = 24;
    mySysInfo.SubSliceCount = 3;
    mySysInfo.ThreadCount = 24 * 7;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

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

TEST(KernelTest, givenKernelWithKernelInfoWith32bitPointerSizeThenReport32bit) {
    KernelInfo info;
    info.gpuPointerSize = 4;

    MockContext context;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment(), &context, false, &device->getDevice());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, info, *device.get()));

    EXPECT_TRUE(kernel->is32Bit());
}

TEST(KernelTest, givenKernelWithKernelInfoWith64bitPointerSizeThenReport64bit) {
    KernelInfo info;
    info.gpuPointerSize = 8;

    MockContext context;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockProgram program(*device->getExecutionEnvironment(), &context, false, &device->getDevice());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, info, *device.get()));

    EXPECT_FALSE(kernel->is32Bit());
}

TEST(KernelTest, givenFtrRenderCompressedBuffersWhenInitializingArgsWithNonStatefulAccessThenMarkKernelForAuxTranslation) {
    DebugManagerStateRestore restore;
    DebugManager.flags.DisableAuxTranslation.set(false);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto hwInfo = device->getExecutionEnvironment()->getMutableHardwareInfo();
    auto &capabilityTable = hwInfo->capabilityTable;
    auto context = clUniquePtr(new MockContext(device.get()));
    context->contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    MockKernelWithInternals kernel(*device, context.get());
    kernel.kernelInfo.kernelArgInfo.resize(1);
    kernel.kernelInfo.kernelArgInfo[0].metadataExtended = std::make_unique<NEO::ArgTypeMetadataExtended>();
    kernel.kernelInfo.kernelArgInfo[0].metadataExtended->type = "char *";
    kernel.kernelInfo.kernelArgInfo[0].isBuffer = true;

    capabilityTable.ftrRenderCompressedBuffers = false;
    kernel.kernelInfo.kernelArgInfo[0].pureStatefulBufferAccess = true;
    kernel.mockKernel->initialize();
    EXPECT_FALSE(kernel.mockKernel->isAuxTranslationRequired());

    kernel.kernelInfo.kernelArgInfo[0].pureStatefulBufferAccess = false;
    kernel.mockKernel->initialize();
    EXPECT_FALSE(kernel.mockKernel->isAuxTranslationRequired());

    capabilityTable.ftrRenderCompressedBuffers = true;
    kernel.mockKernel->initialize();

    if (HwHelper::get(hwInfo->platform.eRenderCoreFamily).requiresAuxResolves()) {
        EXPECT_TRUE(kernel.mockKernel->isAuxTranslationRequired());
    } else {
        EXPECT_FALSE(kernel.mockKernel->isAuxTranslationRequired());
    }

    DebugManager.flags.DisableAuxTranslation.set(true);
    kernel.mockKernel->initialize();
    EXPECT_FALSE(kernel.mockKernel->isAuxTranslationRequired());
}

TEST(KernelTest, givenDebugVariableSetWhenKernelHasStatefulBufferAccessThenMarkKernelForAuxTranslation) {
    DebugManagerStateRestore restore;
    DebugManager.flags.RenderCompressedBuffersEnabled.set(1);

    HardwareInfo localHwInfo = *platformDevices[0];

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    auto context = clUniquePtr(new MockContext(device.get()));
    MockKernelWithInternals kernel(*device, context.get());
    kernel.kernelInfo.kernelArgInfo.resize(1);
    kernel.kernelInfo.kernelArgInfo[0].metadataExtended = std::make_unique<NEO::ArgTypeMetadataExtended>();
    kernel.kernelInfo.kernelArgInfo[0].metadataExtended->type = "char *";
    kernel.kernelInfo.kernelArgInfo[0].isBuffer = true;

    kernel.kernelInfo.kernelArgInfo[0].pureStatefulBufferAccess = false;
    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;

    kernel.mockKernel->initialize();

    if (HwHelper::get(localHwInfo.platform.eRenderCoreFamily).requiresAuxResolves()) {
        EXPECT_TRUE(kernel.mockKernel->isAuxTranslationRequired());
    } else {
        EXPECT_FALSE(kernel.mockKernel->isAuxTranslationRequired());
    }
}

TEST(KernelTest, givenKernelWithPairArgumentWhenItIsInitializedThenPatchImmediateIsUsedAsArgHandler) {
    HardwareInfo localHwInfo = *platformDevices[0];

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    auto context = clUniquePtr(new MockContext(device.get()));
    MockKernelWithInternals kernel(*device, context.get());
    kernel.kernelInfo.kernelArgInfo.resize(1);
    kernel.kernelInfo.kernelArgInfo[0].metadataExtended = std::make_unique<NEO::ArgTypeMetadataExtended>();
    kernel.kernelInfo.kernelArgInfo[0].metadataExtended->type = "pair<char*, int>";

    kernel.mockKernel->initialize();
    EXPECT_EQ(&Kernel::setArgImmediate, kernel.mockKernel->kernelArgHandlers[0]);
}

TEST(KernelTest, whenNullAllocationThenAssignNullPointerToCacheFlushVector) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    kernel.mockKernel->kernelArgRequiresCacheFlush.resize(1);
    kernel.mockKernel->kernelArgRequiresCacheFlush[0] = reinterpret_cast<GraphicsAllocation *>(0x1);

    kernel.mockKernel->addAllocationToCacheFlushVector(0, nullptr);
    EXPECT_EQ(nullptr, kernel.mockKernel->kernelArgRequiresCacheFlush[0]);
}

TEST(KernelTest, givenKernelCompiledWithSimdSizeLowerThanExpectedWhenInitializingThenReturnError) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    auto minSimd = HwHelper::get(device->getHardwareInfo().platform.eRenderCoreFamily).getMinimalSIMDSize();
    MockKernelWithInternals kernel(*device);
    kernel.executionEnvironment.CompiledSIMD32 = 0;
    kernel.executionEnvironment.CompiledSIMD16 = 0;
    kernel.executionEnvironment.CompiledSIMD8 = 1;

    cl_int retVal = kernel.mockKernel->initialize();

    if (minSimd > 8) {
        EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    } else {
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST(KernelTest, givenKernelCompiledWithSimdOneWhenInitializingThenReturnError) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    kernel.executionEnvironment.CompiledSIMD32 = 0;
    kernel.executionEnvironment.CompiledSIMD16 = 0;
    kernel.executionEnvironment.CompiledSIMD8 = 0;
    kernel.executionEnvironment.LargestCompiledSIMDSize = 1;

    cl_int retVal = kernel.mockKernel->initialize();

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(KernelTest, whenAllocationRequiringCacheFlushThenAssignAllocationPointerToCacheFlushVector) {
    MockGraphicsAllocation mockAllocation;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    kernel.mockKernel->kernelArgRequiresCacheFlush.resize(1);

    mockAllocation.setMemObjectsAllocationWithWritableFlags(false);
    mockAllocation.setFlushL3Required(true);

    kernel.mockKernel->addAllocationToCacheFlushVector(0, &mockAllocation);
    EXPECT_EQ(&mockAllocation, kernel.mockKernel->kernelArgRequiresCacheFlush[0]);
}

TEST(KernelTest, whenKernelRequireCacheFlushAfterWalkerThenRequireCacheFlushAfterWalker) {
    MockGraphicsAllocation mockAllocation;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    kernel.mockKernel->svmAllocationsRequireCacheFlush = true;

    MockCommandQueue queue;

    DebugManagerStateRestore debugRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(true);

    queue.requiresCacheFlushAfterWalker = true;
    EXPECT_TRUE(kernel.mockKernel->requiresCacheFlushCommand(queue));

    queue.requiresCacheFlushAfterWalker = false;
    EXPECT_TRUE(kernel.mockKernel->requiresCacheFlushCommand(queue));
}

TEST(KernelTest, whenAllocationWriteableThenDoNotAssignAllocationPointerToCacheFlushVector) {
    MockGraphicsAllocation mockAllocation;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    kernel.mockKernel->kernelArgRequiresCacheFlush.resize(1);

    mockAllocation.setMemObjectsAllocationWithWritableFlags(true);
    mockAllocation.setFlushL3Required(false);

    kernel.mockKernel->addAllocationToCacheFlushVector(0, &mockAllocation);
    EXPECT_EQ(nullptr, kernel.mockKernel->kernelArgRequiresCacheFlush[0]);
}

TEST(KernelTest, whenAllocationReadOnlyNonFlushRequiredThenAssignNullPointerToCacheFlushVector) {
    MockGraphicsAllocation mockAllocation;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    kernel.mockKernel->kernelArgRequiresCacheFlush.resize(1);
    kernel.mockKernel->kernelArgRequiresCacheFlush[0] = reinterpret_cast<GraphicsAllocation *>(0x1);

    mockAllocation.setMemObjectsAllocationWithWritableFlags(false);
    mockAllocation.setFlushL3Required(false);

    kernel.mockKernel->addAllocationToCacheFlushVector(0, &mockAllocation);
    EXPECT_EQ(nullptr, kernel.mockKernel->kernelArgRequiresCacheFlush[0]);
}

TEST(KernelTest, givenKernelUsesPrivateMemoryWhenDeviceReleasedBeforeKernelThenKernelUsesMemoryManagerFromEnvironment) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    auto executionEnvironment = device->getExecutionEnvironment();

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    GraphicsAllocation *privateSurface = device->getExecutionEnvironment()->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    mockKernel->mockKernel->setPrivateSurface(privateSurface, 10);

    executionEnvironment->incRefInternal();
    device.reset(nullptr);
    mockKernel.reset(nullptr);
    executionEnvironment->decRefInternal();
}

TEST(KernelTest, givenAllArgumentsAreStatefulBuffersWhenInitializingThenAllBufferArgsStatefulIsTrue) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    std::vector<KernelArgInfo> kernelArgInfo(2);
    kernelArgInfo[0].isBuffer = true;
    kernelArgInfo[1].isBuffer = true;
    kernelArgInfo[0].pureStatefulBufferAccess = true;
    kernelArgInfo[1].pureStatefulBufferAccess = true;

    MockKernelWithInternals kernel{*device};
    kernel.kernelInfo.kernelArgInfo.swap(kernelArgInfo);

    kernel.mockKernel->initialize();
    EXPECT_TRUE(kernel.mockKernel->allBufferArgsStateful);
}

TEST(KernelTest, givenAllArgumentsAreBuffersButNotAllAreStatefulWhenInitializingThenAllBufferArgsStatefulIsFalse) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    std::vector<KernelArgInfo> kernelArgInfo(2);
    kernelArgInfo[0].isBuffer = true;
    kernelArgInfo[1].isBuffer = true;
    kernelArgInfo[0].pureStatefulBufferAccess = true;
    kernelArgInfo[1].pureStatefulBufferAccess = false;

    MockKernelWithInternals kernel{*device};
    kernel.kernelInfo.kernelArgInfo.swap(kernelArgInfo);

    kernel.mockKernel->initialize();
    EXPECT_FALSE(kernel.mockKernel->allBufferArgsStateful);
}

TEST(KernelTest, givenNotAllArgumentsAreBuffersButAllBuffersAreStatefulWhenInitializingThenAllBufferArgsStatefulIsTrue) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    std::vector<KernelArgInfo> kernelArgInfo(2);
    kernelArgInfo[0].isBuffer = true;
    kernelArgInfo[1].isBuffer = false;
    kernelArgInfo[0].pureStatefulBufferAccess = true;
    kernelArgInfo[1].pureStatefulBufferAccess = false;

    MockKernelWithInternals kernel{*device};
    kernel.kernelInfo.kernelArgInfo.swap(kernelArgInfo);

    kernel.mockKernel->initialize();
    EXPECT_TRUE(kernel.mockKernel->allBufferArgsStateful);
}

TEST(KernelTest, givenKernelRequiringPrivateScratchSpaceWhenGettingSizeForPrivateScratchSpaceThenCorrectSizeIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    MockKernelWithInternals mockKernel(*device);
    SPatchMediaVFEState mediaVFEstate;
    SPatchMediaVFEState mediaVFEstateSlot1;
    mediaVFEstateSlot1.PerThreadScratchSpace = 1024u;
    mediaVFEstate.PerThreadScratchSpace = 512u;
    mockKernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;
    mockKernel.kernelInfo.patchInfo.mediaVfeStateSlot1 = &mediaVFEstateSlot1;

    EXPECT_EQ(1024u, mockKernel.mockKernel->getPrivateScratchSize());
}

TEST(KernelTest, givenKernelWithoutMediaVfeStateSlot1WhenGettingSizeForPrivateScratchSpaceThenCorrectSizeIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    MockKernelWithInternals mockKernel(*device);
    mockKernel.kernelInfo.patchInfo.mediaVfeStateSlot1 = nullptr;

    EXPECT_EQ(0u, mockKernel.mockKernel->getPrivateScratchSize());
}

TEST(KernelTest, givenKernelWithPatchInfoCollectionEnabledWhenPatchWithImplicitSurfaceCalledThenPatchInfoDataIsCollected) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    MockGraphicsAllocation mockAllocation;
    SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization patchToken{};
    uint64_t crossThreadData = 0;
    EXPECT_EQ(0u, kernel.mockKernel->getPatchInfoDataList().size());
    kernel.mockKernel->patchWithImplicitSurface(&crossThreadData, mockAllocation, patchToken);
    EXPECT_EQ(1u, kernel.mockKernel->getPatchInfoDataList().size());
}

TEST(KernelTest, givenKernelWithPatchInfoCollectionDisabledWhenPatchWithImplicitSurfaceCalledThenPatchInfoDataIsNotCollected) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    MockGraphicsAllocation mockAllocation;
    SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization patchToken{};
    uint64_t crossThreadData = 0;
    EXPECT_EQ(0u, kernel.mockKernel->getPatchInfoDataList().size());
    kernel.mockKernel->patchWithImplicitSurface(&crossThreadData, mockAllocation, patchToken);
    EXPECT_EQ(0u, kernel.mockKernel->getPatchInfoDataList().size());
}

TEST(KernelTest, givenDefaultKernelWhenItIsCreatedThenItReportsStatelessWrites) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    EXPECT_TRUE(kernel.mockKernel->areStatelessWritesUsed());
}

TEST(KernelTest, givenPolicyWhensetKernelThreadArbitrationPolicyThenExpectedClValueIsReturned) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);
    EXPECT_EQ(CL_SUCCESS, kernel.mockKernel->setKernelThreadArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL));
    EXPECT_EQ(CL_SUCCESS, kernel.mockKernel->setKernelThreadArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL));
    EXPECT_EQ(CL_SUCCESS, kernel.mockKernel->setKernelThreadArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL));
    uint32_t notExistPolicy = 0;
    EXPECT_EQ(CL_INVALID_VALUE, kernel.mockKernel->setKernelThreadArbitrationPolicy(notExistPolicy));
}

TEST(KernelTest, GivenDifferentValuesWhenSetKernelExecutionTypeIsCalledThenCorrectValueIsSet) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals mockKernelWithInternals(*device);
    auto &kernel = *mockKernelWithInternals.mockKernel;
    cl_int retVal;

    EXPECT_EQ(KernelExecutionType::Default, kernel.executionType);

    retVal = kernel.setKernelExecutionType(-1);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(KernelExecutionType::Default, kernel.executionType);

    retVal = kernel.setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(KernelExecutionType::Concurrent, kernel.executionType);

    retVal = kernel.setKernelExecutionType(-1);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(KernelExecutionType::Concurrent, kernel.executionType);

    retVal = kernel.setKernelExecutionType(CL_KERNEL_EXEC_INFO_DEFAULT_TYPE_INTEL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(KernelExecutionType::Default, kernel.executionType);
}

TEST(KernelTest, givenKernelLocalIdGenerationByRuntimeFalseWhenGettingStartOffsetThenOffsetToSkipPerThreadDataLoadIsAdded) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    MockKernelWithInternals mockKernel(*device);
    SPatchThreadPayload threadPayload = {};

    threadPayload.OffsetToSkipPerThreadDataLoad = 128u;
    mockKernel.kernelInfo.patchInfo.threadPayload = &threadPayload;

    mockKernel.kernelInfo.createKernelAllocation(device->getRootDeviceIndex(), device->getMemoryManager());
    auto allocationOffset = mockKernel.kernelInfo.getGraphicsAllocation()->getGpuAddressToPatch();

    mockKernel.mockKernel->setStartOffset(128);
    auto offset = mockKernel.mockKernel->getKernelStartOffset(false, true, false);
    EXPECT_EQ(allocationOffset + 256u, offset);
    device->getMemoryManager()->freeGraphicsMemory(mockKernel.kernelInfo.getGraphicsAllocation());
}

TEST(KernelTest, givenKernelLocalIdGenerationByRuntimeTrueAndLocalIdsUsedWhenGettingStartOffsetThenOffsetToSkipPerThreadDataLoadIsNotAdded) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    MockKernelWithInternals mockKernel(*device);
    SPatchThreadPayload threadPayload = {};

    threadPayload.OffsetToSkipPerThreadDataLoad = 128u;
    mockKernel.kernelInfo.patchInfo.threadPayload = &threadPayload;

    mockKernel.kernelInfo.createKernelAllocation(device->getRootDeviceIndex(), device->getMemoryManager());
    auto allocationOffset = mockKernel.kernelInfo.getGraphicsAllocation()->getGpuAddressToPatch();

    mockKernel.mockKernel->setStartOffset(128);
    auto offset = mockKernel.mockKernel->getKernelStartOffset(true, true, false);
    EXPECT_EQ(allocationOffset + 128u, offset);
    device->getMemoryManager()->freeGraphicsMemory(mockKernel.kernelInfo.getGraphicsAllocation());
}

TEST(KernelTest, givenKernelLocalIdGenerationByRuntimeFalseAndLocalIdsNotUsedWhenGettingStartOffsetThenOffsetToSkipPerThreadDataLoadIsNotAdded) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));

    MockKernelWithInternals mockKernel(*device);
    SPatchThreadPayload threadPayload = {};

    threadPayload.OffsetToSkipPerThreadDataLoad = 128u;
    mockKernel.kernelInfo.patchInfo.threadPayload = &threadPayload;

    mockKernel.kernelInfo.createKernelAllocation(device->getRootDeviceIndex(), device->getMemoryManager());
    auto allocationOffset = mockKernel.kernelInfo.getGraphicsAllocation()->getGpuAddressToPatch();

    mockKernel.mockKernel->setStartOffset(128);
    auto offset = mockKernel.mockKernel->getKernelStartOffset(false, false, false);
    EXPECT_EQ(allocationOffset + 128u, offset);
    device->getMemoryManager()->freeGraphicsMemory(mockKernel.kernelInfo.getGraphicsAllocation());
}

TEST(KernelTest, givenKernelWhenForcePerDssBackedBufferProgrammingIsSetThenKernelRequiresPerDssBackedBuffer) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForcePerDssBackedBufferProgramming.set(true);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);

    EXPECT_TRUE(kernel.mockKernel->requiresPerDssBackedBuffer());
}

TEST(KernelTest, givenKernelWhenForcePerDssBackedBufferProgrammingIsNotSetThenKernelDoesntRequirePerDssBackedBuffer) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(platformDevices[0]));
    MockKernelWithInternals kernel(*device);

    EXPECT_FALSE(kernel.mockKernel->requiresPerDssBackedBuffer());
}

namespace NEO {

template <typename GfxFamily>
class DeviceQueueHwMock : public DeviceQueueHw<GfxFamily> {
    using BaseClass = DeviceQueueHw<GfxFamily>;

  public:
    using BaseClass::buildSlbDummyCommands;
    using BaseClass::getCSPrefetchSize;
    using BaseClass::getExecutionModelCleanupSectionSize;
    using BaseClass::getMediaStateClearCmdsSize;
    using BaseClass::getMinimumSlbSize;
    using BaseClass::getProfilingEndCmdsSize;
    using BaseClass::getSlbCS;
    using BaseClass::getWaCommandsSize;
    using BaseClass::offsetDsh;

    DeviceQueueHwMock(Context *context, ClDevice *device, cl_queue_properties &properties) : BaseClass(context, device, properties) {
        auto slb = this->getSlbBuffer();
        LinearStream *slbCS = getSlbCS();
        slbCS->replaceBuffer(slb->getUnderlyingBuffer(), slb->getUnderlyingBufferSize()); // reset
    };
};
} // namespace NEO

HWCMDTEST_F(IGFX_GEN8_CORE, DeviceQueueHwTest, whenSlbEndOffsetGreaterThanZeroThenOverwriteOneEnqueue) {
    std::unique_ptr<DeviceQueueHwMock<FamilyType>> mockDeviceQueueHw(new DeviceQueueHwMock<FamilyType>(pContext, device, deviceQueueProperties::minimumProperties[0]));

    auto slb = mockDeviceQueueHw->getSlbBuffer();
    auto commandsSize = mockDeviceQueueHw->getMinimumSlbSize() + mockDeviceQueueHw->getWaCommandsSize();
    auto slbCopy = malloc(slb->getUnderlyingBufferSize());
    memset(slb->getUnderlyingBuffer(), 0xFE, slb->getUnderlyingBufferSize());
    memcpy(slbCopy, slb->getUnderlyingBuffer(), slb->getUnderlyingBufferSize());

    auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(mockDeviceQueueHw->getQueueBuffer()->getUnderlyingBuffer());

    // slbEndOffset < commandsSize * 128
    // always fill only 1 enqueue (after offset)
    auto offset = static_cast<int>(commandsSize) * 50;
    igilCmdQueue->m_controls.m_SLBENDoffsetInBytes = offset;
    mockDeviceQueueHw->resetDeviceQueue();
    EXPECT_EQ(0, memcmp(slb->getUnderlyingBuffer(), slbCopy, offset)); // dont touch memory before offset
    EXPECT_NE(0, memcmp(ptrOffset(slb->getUnderlyingBuffer(), offset),
                        slbCopy, commandsSize)); // change 1 enqueue
    EXPECT_EQ(0, memcmp(ptrOffset(slb->getUnderlyingBuffer(), offset + commandsSize),
                        slbCopy, offset)); // dont touch memory after (offset + 1 enqueue)

    // slbEndOffset == commandsSize * 128
    // dont fill commands
    memset(slb->getUnderlyingBuffer(), 0xFEFEFEFE, slb->getUnderlyingBufferSize());
    offset = static_cast<int>(commandsSize) * 128;
    igilCmdQueue->m_controls.m_SLBENDoffsetInBytes = static_cast<int>(commandsSize);
    mockDeviceQueueHw->resetDeviceQueue();
    EXPECT_EQ(0, memcmp(slb->getUnderlyingBuffer(), slbCopy, commandsSize * 128)); // dont touch memory for enqueues

    free(slbCopy);
}

using KernelMultiRootDeviceTest = MultiRootDeviceFixture;

TEST_F(KernelMultiRootDeviceTest, privateSurfaceHasCorrectRootDeviceIndex) {
    auto kernelInfo = std::make_unique<KernelInfo>();

    // setup private memory
    SPatchAllocateStatelessPrivateSurface tokenSPS;
    tokenSPS.SurfaceStateHeapOffset = 64;
    tokenSPS.DataParamOffset = 40;
    tokenSPS.DataParamSize = 8;
    tokenSPS.PerThreadPrivateMemorySize = 112;
    kernelInfo->patchInfo.pAllocateStatelessPrivateSurface = &tokenSPS;

    MockProgram program(*device->getExecutionEnvironment(), context.get(), false, &device->getDevice());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *kernelInfo, *device.get()));
    kernel->initialize();

    auto privateSurface = kernel->getPrivateSurface();
    ASSERT_NE(nullptr, privateSurface);
    EXPECT_EQ(expectedRootDeviceIndex, privateSurface->getRootDeviceIndex());
}

TEST(KernelCreateTest, whenInitFailedThenReturnNull) {
    struct MockProgram {
        Device &getDevice() { return mDevice.getDevice(); }
        void getSource(std::string &) {}
        MockClDevice mDevice{new MockDevice};
    } mockProgram;
    struct MockKernel {
        MockKernel(MockProgram *, const KernelInfo &, ClDevice &) {}
        int initialize() { return -1; };
    };

    KernelInfo info;
    info.gpuPointerSize = 8;

    auto ret = Kernel::create<MockKernel>(&mockProgram, info, nullptr);
    EXPECT_EQ(nullptr, ret);
}

TEST(ArgTypeTraits, GivenDefaultInitializedArgTypeMetadataThenAddressSpaceIsGlobal) {
    ArgTypeTraits metadata;
    EXPECT_EQ(NEO::KernelArgMetadata::AddrGlobal, metadata.addressQualifier);
}
