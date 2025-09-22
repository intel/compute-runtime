/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/multi_root_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_from_binary.h"
#include "opencl/test/unit_test/program/program_tests.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include <memory>

using namespace NEO;

using KernelTest = ::testing::Test;

class KernelTests : public ProgramFromBinaryFixture {
  public:
    ~KernelTests() override = default;

  protected:
    void SetUp() override {
        ProgramFromBinaryFixture::setUp();
        ASSERT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = pProgram->build(
            pProgram->getDevices(),
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // create a kernel
        kernel = Kernel::create<MockKernel>(
            pProgram,
            pProgram->getKernelInfoForKernel(kernelName),
            *pClDevice,
            retVal);

        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, kernel);
    }

    void TearDown() override {
        delete kernel;
        kernel = nullptr;
        knownSource.reset();
        ProgramFromBinaryFixture::TearDown();
    }

    MockKernel *kernel = nullptr;
    cl_int retVal = CL_SUCCESS;
};

TEST(KernelTest, WhenKernelIsCreatedThenCorrectMembersAreMemObjects) {
    EXPECT_TRUE(Kernel::isMemObj(Kernel::BUFFER_OBJ));
    EXPECT_TRUE(Kernel::isMemObj(Kernel::IMAGE_OBJ));
    EXPECT_TRUE(Kernel::isMemObj(Kernel::PIPE_OBJ));

    EXPECT_FALSE(Kernel::isMemObj(Kernel::SAMPLER_OBJ));
    EXPECT_FALSE(Kernel::isMemObj(Kernel::ACCELERATOR_OBJ));
    EXPECT_FALSE(Kernel::isMemObj(Kernel::NONE_OBJ));
    EXPECT_FALSE(Kernel::isMemObj(Kernel::SVM_ALLOC_OBJ));
}

TEST_F(KernelTests, WhenKernelIsCreatedThenKernelHeapIsCorrect) {
    EXPECT_EQ(kernel->getKernelInfo().heapInfo.pKernelHeap, kernel->getKernelHeap());
    EXPECT_EQ(kernel->getKernelInfo().heapInfo.kernelHeapSize, kernel->getKernelHeapSize());
}

TEST_F(KernelTests, GivenInvalidParamNameWhenGettingInfoThenInvalidValueErrorIsReturned) {
    size_t paramValueSizeRet = 0;

    // get size
    retVal = kernel->getInfo(
        0,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(KernelTests, GivenInvalidParametersWhenGettingInfoThenValueSizeRetIsNotUpdated) {
    size_t paramValueSizeRet = 0x1234;

    // get size
    retVal = kernel->getInfo(
        0,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, paramValueSizeRet);
}

TEST_F(KernelTests, GivenKernelFunctionNameWhenGettingInfoThenKernelFunctionNameIsReturned) {
    cl_kernel_info paramName = CL_KERNEL_FUNCTION_NAME;
    size_t paramValueSize = 0;
    char *paramValue = nullptr;
    size_t paramValueSizeRet = 0;

    // get size
    retVal = kernel->getInfo(
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

    retVal = kernel->getInfo(
        paramName,
        paramValueSize,
        paramValue,
        nullptr);

    EXPECT_NE(nullptr, paramValue);
    EXPECT_EQ(0, strcmp(paramValue, kernelName));
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] paramValue;
}

TEST_F(KernelTests, GivenKernelBinaryProgramIntelWhenGettingInfoThenKernelBinaryIsReturned) {
    cl_kernel_info paramName = CL_KERNEL_BINARY_PROGRAM_INTEL;
    size_t paramValueSize = 0;
    char *paramValue = nullptr;
    size_t paramValueSizeRet = 0;
    const char *pKernelData = reinterpret_cast<const char *>(kernel->getKernelHeap());
    EXPECT_NE(nullptr, pKernelData);

    // get size of kernel binary
    retVal = kernel->getInfo(
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
    retVal = kernel->getInfo(
        paramName,
        paramValueSize,
        paramValue,
        nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, paramValue);
    EXPECT_EQ(0, memcmp(paramValue, pKernelData, paramValueSize));

    delete[] paramValue;
}

TEST_F(KernelTests, givenBinaryWhenItIsQueriedForGpuAddressThenAbsoluteAddressIsReturned) {
    cl_kernel_info paramName = CL_KERNEL_BINARY_GPU_ADDRESS_INTEL;
    uint64_t paramValue = 0llu;
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    retVal = kernel->getInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto gmmHelper = pDevice->getGmmHelper();
    auto expectedGpuAddress = gmmHelper->decanonize(kernel->getKernelInfo().kernelAllocation->getGpuAddress());
    EXPECT_EQ(expectedGpuAddress, paramValue);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
}

TEST_F(KernelTests, GivenKernelNumArgsWhenGettingInfoThenNumberOfKernelArgsIsReturned) {
    cl_kernel_info paramName = CL_KERNEL_NUM_ARGS;
    size_t paramValueSize = sizeof(cl_uint);
    cl_uint paramValue = 0;
    size_t paramValueSizeRet = 0;

    // get size
    retVal = kernel->getInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(sizeof(cl_uint), paramValueSizeRet);
    EXPECT_EQ(2u, paramValue);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(KernelTests, GivenKernelProgramWhenGettingInfoThenProgramIsReturned) {
    cl_kernel_info paramName = CL_KERNEL_PROGRAM;
    size_t paramValueSize = sizeof(cl_program);
    cl_program paramValue = 0;
    size_t paramValueSizeRet = 0;
    cl_program prog = pProgram;

    // get size
    retVal = kernel->getInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_program), paramValueSizeRet);
    EXPECT_EQ(prog, paramValue);
}

TEST_F(KernelTests, GivenKernelContextWhenGettingInfoThenKernelContextIsReturned) {
    cl_kernel_info paramName = CL_KERNEL_CONTEXT;
    cl_context paramValue = 0;
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;
    cl_context context = pContext;

    // get size
    retVal = kernel->getInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
    EXPECT_EQ(context, paramValue);
}

TEST_F(KernelTests, GivenKernelWorkGroupSizeWhenGettingWorkGroupInfoThenWorkGroupSizeIsReturned) {
    cl_kernel_info paramName = CL_KERNEL_WORK_GROUP_SIZE;
    size_t paramValue = 0;
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    auto kernelMaxWorkGroupSize = pDevice->getDeviceInfo().maxWorkGroupSize - 1;
    kernel->maxKernelWorkGroupSize = static_cast<uint32_t>(kernelMaxWorkGroupSize);

    retVal = kernel->getWorkGroupInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
    EXPECT_EQ(kernelMaxWorkGroupSize, paramValue);
}

TEST_F(KernelTests, GivenKernelCompileWorkGroupSizeWhenGettingWorkGroupInfoThenCompileWorkGroupSizeIsReturned) {
    cl_kernel_info paramName = CL_KERNEL_COMPILE_WORK_GROUP_SIZE;
    size_t paramValue[3];
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    retVal = kernel->getWorkGroupInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);
}

TEST_F(KernelTests, GivenRequiredDisabledEUFusionFlagWhenGettingPreferredWorkGroupSizeMultipleThenCorectValueIsReturned) {
    KernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresDisabledEUFusion = true;
    MockKernel kernel(pProgram, kernelInfo, *pClDevice);

    auto &gfxCoreHelper = pClDevice->getGfxCoreHelper();
    bool fusedDispatchEnabled = gfxCoreHelper.isFusedEuDispatchEnabled(*defaultHwInfo, true);
    auto expectedValue = kernelInfo.getMaxSimdSize() * (fusedDispatchEnabled ? 2 : 1);

    cl_kernel_info paramName = CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE;
    size_t paramValue;
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    retVal = kernel.getWorkGroupInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);

    EXPECT_EQ(expectedValue, paramValue);
}

TEST_F(KernelTests, GivenSlmInlineSizeAndSlmOffsetWhenGettingWorkGroupInfoThenCorrectValueIsReturned) {
    MockKernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelAttributes.slmInlineSize = 100u;

    kernelInfo.addArgLocal(0, 0x10, 0x1);
    kernelInfo.addArgBuffer(1, 0x20, sizeof(void *));
    kernelInfo.addArgBuffer(2, 0x20, sizeof(void *));
    kernelInfo.addArgLocal(3, 0x30, 0x10);

    MockKernel kernel(pProgram, kernelInfo, *pClDevice);
    kernel.kernelArguments.resize(4);
    kernel.slmSizes.resize(4);

    uint32_t crossThreadData[0x40]{};
    crossThreadData[0x20 / sizeof(uint32_t)] = 0x12344321;
    kernel.setCrossThreadData(crossThreadData, sizeof(crossThreadData));

    kernel.setArgLocal(0, 4096, nullptr);
    kernel.setArgLocal(3, 0, nullptr);

    cl_kernel_info paramName = CL_KERNEL_LOCAL_MEM_SIZE;
    cl_ulong paramValue;
    size_t paramValueSizeRet = 0;
    cl_ulong expectedValue = 4096 + 0 + 100;

    retVal = kernel.getWorkGroupInfo(
        paramName,
        sizeof(cl_ulong),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_ulong), paramValueSizeRet);
    EXPECT_EQ(expectedValue, paramValue);
}

TEST_F(KernelTests, GivenCFEFusedEUDispatchEnabledAndRequiredDisabledUEFusionWhenGettingPreferredWorkGroupSizeMultipleThenCorectValueIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.CFEFusedEUDispatch.set(0);

    KernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresDisabledEUFusion = true;
    MockKernel kernel(pProgram, kernelInfo, *pClDevice);

    auto &gfxCoreHelper = pClDevice->getGfxCoreHelper();
    bool fusedDispatchEnabled = gfxCoreHelper.isFusedEuDispatchEnabled(*defaultHwInfo, true);
    auto expectedValue = kernelInfo.getMaxSimdSize() * (fusedDispatchEnabled ? 2 : 1);

    cl_kernel_info paramName = CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE;
    size_t paramValue;
    size_t paramValueSize = sizeof(paramValue);
    size_t paramValueSizeRet = 0;

    retVal = kernel.getWorkGroupInfo(
        paramName,
        paramValueSize,
        &paramValue,
        &paramValueSizeRet);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(paramValueSize, paramValueSizeRet);

    EXPECT_EQ(expectedValue, paramValue);
}

TEST_F(KernelTests, GivenInvalidParamNameWhenGettingWorkGroupInfoThenInvalidValueErrorIsReturned) {
    size_t paramValueSizeRet = 0x1234u;

    retVal = kernel->getWorkGroupInfo(
        0,
        0,
        nullptr,
        &paramValueSizeRet);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, paramValueSizeRet);
}

TEST_F(KernelTests, WhenIsSingleSubdevicePreferredIsCalledThenCorrectValuesAreReturned) {
    auto &helper = pClDevice->getGfxCoreHelper();

    std::unique_ptr<MockKernel> kernel{MockKernel::create<MockKernel>(pClDevice->getDevice(), pProgram)};
    for (auto usesSyncBuffer : ::testing::Bool()) {
        kernel->getAllocatedKernelInfo()->kernelDescriptor.kernelAttributes.flags.usesSyncBuffer = usesSyncBuffer;

        EXPECT_EQ(usesSyncBuffer, kernel->usesSyncBuffer());
        EXPECT_EQ(helper.singleTileExecImplicitScalingRequired(usesSyncBuffer), kernel->isSingleSubdevicePreferred());
    }
}

using BindlessKernelTests = KernelTests;

TEST_F(BindlessKernelTests, GivenBindlessAddressingKernelWhenInitializeThenSurfaceStateIsCreatedWithCorrectSize) {
    KernelInfo kernelInfo = {};
    kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
    kernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Bindless;
    kernelInfo.kernelDescriptor.kernelAttributes.numArgsStateful = 3;

    MockKernel kernel(pProgram, kernelInfo, *pClDevice);

    auto retVal = kernel.initialize();
    EXPECT_EQ(CL_SUCCESS, retVal);

    const auto &gfxCoreHelper = pClDevice->getGfxCoreHelper();
    const auto surfaceStateSize = static_cast<uint32_t>(gfxCoreHelper.getRenderSurfaceStateSize());
    const auto expectedSsHeapSize = kernelInfo.kernelDescriptor.kernelAttributes.numArgsStateful * surfaceStateSize;

    const auto ssHeap = kernel.getSurfaceStateHeap();
    const auto ssHeapSize = kernel.getSurfaceStateHeapSize();

    EXPECT_EQ(expectedSsHeapSize, ssHeapSize);
    EXPECT_NE(nullptr, ssHeap);
}

TEST_F(BindlessKernelTests, givenBindlessKernelWhenPatchingCrossThreadDataThenCorrectBindlessOffsetsAreWritten) {
    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;

    auto argDescriptorImg = NEO::ArgDescriptor(NEO::ArgDescriptor::argTImage);
    argDescriptorImg.as<NEO::ArgDescImage>() = NEO::ArgDescImage();
    argDescriptorImg.as<NEO::ArgDescImage>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptorImg.as<NEO::ArgDescImage>().bindless = sizeof(uint64_t);

    auto argDescriptor2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor2.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor2.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor2.as<NEO::ArgDescPointer>().stateless = 2 * sizeof(uint64_t);

    KernelInfo kernelInfo = {};
    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorImg);
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor2);

    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless = 3 * sizeof(uint64_t);
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless = 4 * sizeof(uint64_t);

    MockKernel mockKernel(pProgram, pProgram->mockKernelInfo, *pClDevice);

    pProgram->mockKernelInfo.kernelDescriptor.initBindlessOffsetToSurfaceState();

    mockKernel.crossThreadData = new char[5 * sizeof(uint64_t)];
    mockKernel.crossThreadDataSize = 5 * sizeof(uint64_t);
    memset(mockKernel.crossThreadData, 0x00, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x1000;
    auto &gfxCoreHelper = pClDevice->getGfxCoreHelper();
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    auto patchValue1 = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(baseAddress));
    auto patchValue2 = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(baseAddress + 1 * surfaceStateSize));
    auto patchValue3 = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(baseAddress + 2 * surfaceStateSize));
    auto patchValue4 = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(baseAddress + 3 * surfaceStateSize));

    mockKernel.patchBindlessSurfaceStatesInCrossThreadData<false>(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData, mockKernel.crossThreadDataSize);

    EXPECT_EQ(patchValue1, crossThreadData[0]);
    EXPECT_EQ(patchValue2, crossThreadData[1]);
    EXPECT_EQ(0u, crossThreadData[2]);
    EXPECT_EQ(patchValue3, crossThreadData[3]);
    EXPECT_EQ(patchValue4, crossThreadData[4]);
}

TEST_F(BindlessKernelTests, givenBindlessKernelWhenPatchBindlessSurfaceStatesInCrossThreadDataThenCorrectAddressesAreWritten) {
    auto argDescriptorImg1 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTImage);
    argDescriptorImg1.as<NEO::ArgDescImage>() = NEO::ArgDescImage();
    argDescriptorImg1.as<NEO::ArgDescImage>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptorImg1.as<NEO::ArgDescImage>().bindless = 0x0;

    auto argDescriptorImg2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTImage);
    argDescriptorImg2.as<NEO::ArgDescImage>() = NEO::ArgDescImage();
    argDescriptorImg2.as<NEO::ArgDescImage>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptorImg2.as<NEO::ArgDescImage>().bindless = sizeof(uint64_t);

    auto argDescriptorPtr = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptorPtr.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptorPtr.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptorPtr.as<NEO::ArgDescPointer>().stateless = 2 * sizeof(uint64_t);

    KernelInfo kernelInfo = {};
    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::Stateless;
    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorImg1);
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorImg2);
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorPtr);

    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless = 3 * sizeof(uint64_t);
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless = 4 * sizeof(uint64_t);

    MockKernel mockKernel(pProgram, pProgram->mockKernelInfo, *pClDevice);

    pProgram->mockKernelInfo.kernelDescriptor.initBindlessOffsetToSurfaceState();

    mockKernel.crossThreadData = new char[5 * sizeof(uint64_t)];
    mockKernel.crossThreadDataSize = 5 * sizeof(uint64_t);
    memset(mockKernel.crossThreadData, 0x00, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x00ff'ffff'0000'1000;
    ASSERT_TRUE(baseAddress > std::numeric_limits<uint32_t>::max());

    auto &gfxCoreHelper = pClDevice->getGfxCoreHelper();
    auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    auto bindlessSufaceState1Address = baseAddress;
    auto bindlessSufaceState2Address = baseAddress + 1 * surfaceStateSize;
    auto globalVariablesSurfaceAddress = baseAddress + 2 * surfaceStateSize;
    auto globalConstantsSurfaceAddress = baseAddress + 3 * surfaceStateSize;

    constexpr bool heaplessEnabled = true;
    mockKernel.patchBindlessSurfaceStatesInCrossThreadData<heaplessEnabled>(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData, mockKernel.crossThreadDataSize);

    EXPECT_EQ(bindlessSufaceState1Address, crossThreadData[0]);
    EXPECT_EQ(bindlessSufaceState2Address, crossThreadData[1]);
    EXPECT_EQ(0u, crossThreadData[2]);
    EXPECT_EQ(globalVariablesSurfaceAddress, crossThreadData[3]);
    EXPECT_EQ(globalConstantsSurfaceAddress, crossThreadData[4]);
}

HWTEST_F(BindlessKernelTests, givenBindlessKernelAndSamplersWhenPatchBindlessSamplerStatesInCrossThreadDataThenCorrectAddressesAreWritten) {
    auto argDescriptorSampler1 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTSampler);
    argDescriptorSampler1.as<NEO::ArgDescSampler>() = NEO::ArgDescSampler();
    argDescriptorSampler1.as<NEO::ArgDescSampler>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptorSampler1.as<NEO::ArgDescSampler>().bindless = 0x0;
    argDescriptorSampler1.as<NEO::ArgDescSampler>().size = 8;
    argDescriptorSampler1.as<NEO::ArgDescSampler>().index = 0;

    auto argDescriptorSampler2 = NEO::ArgDescriptor(NEO::ArgDescriptor::argTSampler);
    argDescriptorSampler2.as<NEO::ArgDescSampler>() = NEO::ArgDescSampler();
    argDescriptorSampler2.as<NEO::ArgDescSampler>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptorSampler2.as<NEO::ArgDescSampler>().bindless = sizeof(uint64_t);
    argDescriptorSampler2.as<NEO::ArgDescSampler>().size = 8;
    argDescriptorSampler2.as<NEO::ArgDescSampler>().index = 1;

    KernelInfo kernelInfo = {};
    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.samplerAddressingMode = NEO::KernelDescriptor::Bindless;
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorSampler1);
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorSampler2);

    auto &inlineSampler = pProgram->mockKernelInfo.kernelDescriptor.inlineSamplers.emplace_back();
    inlineSampler.addrMode = NEO::KernelDescriptor::InlineSampler::AddrMode::repeat;
    inlineSampler.filterMode = NEO::KernelDescriptor::InlineSampler::FilterMode::nearest;
    inlineSampler.isNormalized = false;
    inlineSampler.bindless = 2 * sizeof(uint64_t);
    inlineSampler.samplerIndex = 2;
    inlineSampler.size = 8;

    const uint32_t borderColorSize = inlineSampler.borderColorStateSize;
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.samplerTable.tableOffset = borderColorSize;

    MockKernel mockKernel(pProgram, pProgram->mockKernelInfo, *pClDevice);
    using SamplerState = typename FamilyType::SAMPLER_STATE;
    std::array<uint8_t, 64 + 3 * sizeof(SamplerState)> dsh = {0};
    pProgram->mockKernelInfo.heapInfo.pDsh = dsh.data();
    pProgram->mockKernelInfo.heapInfo.dynamicStateHeapSize = static_cast<uint32_t>(dsh.size());
    mockKernel.crossThreadData = new char[3 * sizeof(uint64_t)];
    mockKernel.crossThreadDataSize = 3 * sizeof(uint64_t);
    memset(mockKernel.crossThreadData, 0x00, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = reinterpret_cast<uint64_t>(dsh.data()) + borderColorSize;

    auto &gfxCoreHelper = pClDevice->getGfxCoreHelper();
    auto samplerStateSize = gfxCoreHelper.getSamplerStateSize();

    auto bindlessSamplerState1Address = baseAddress;
    auto bindlessSamplerState2Address = baseAddress + 1 * samplerStateSize;
    auto bindlessInlineSamplerStateAddress = baseAddress + inlineSampler.samplerIndex * samplerStateSize;

    mockKernel.setInlineSamplers();
    mockKernel.patchBindlessSamplerStatesInCrossThreadData(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData, mockKernel.crossThreadDataSize);

    EXPECT_EQ(bindlessSamplerState1Address, crossThreadData[0]);
    EXPECT_EQ(bindlessSamplerState2Address, crossThreadData[1]);
    EXPECT_EQ(bindlessInlineSamplerStateAddress, crossThreadData[2]);
}

TEST_F(BindlessKernelTests, givenNoEntryInBindlessOffsetsMapWhenPatchingCrossThreadDataThenMemoryIsNotPatched) {
    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
    argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
    argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
    argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x0;
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless = sizeof(uint64_t);

    MockKernel mockKernel(pProgram, pProgram->mockKernelInfo, *pClDevice);

    mockKernel.crossThreadData = new char[4 * sizeof(uint64_t)];
    mockKernel.crossThreadDataSize = 4 * sizeof(uint64_t);
    memset(mockKernel.crossThreadData, 0, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x1000;
    mockKernel.patchBindlessSurfaceStatesInCrossThreadData<false>(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData, mockKernel.crossThreadDataSize);

    EXPECT_EQ(0u, crossThreadData[0]);
}

TEST_F(BindlessKernelTests, givenNoStatefulArgsWhenPatchingBindlessOffsetsInCrossThreadDataThenMemoryIsNotPatched) {
    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    pProgram->mockKernelInfo.kernelDescriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;

    auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTValue);
    argDescriptor.as<NEO::ArgDescValue>() = NEO::ArgDescValue();
    argDescriptor.as<NEO::ArgDescValue>().elements.push_back(NEO::ArgDescValue::Element{0, 8, 0, false});
    pProgram->mockKernelInfo.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

    MockKernel mockKernel(pProgram, pProgram->mockKernelInfo, *pClDevice);

    mockKernel.crossThreadData = new char[sizeof(uint64_t)];
    mockKernel.crossThreadDataSize = sizeof(uint64_t);
    memset(mockKernel.crossThreadData, 0, mockKernel.crossThreadDataSize);

    const uint64_t baseAddress = 0x1000;
    mockKernel.patchBindlessSurfaceStatesInCrossThreadData<false>(baseAddress);

    auto crossThreadData = std::make_unique<uint64_t[]>(mockKernel.crossThreadDataSize / sizeof(uint64_t));
    memcpy(crossThreadData.get(), mockKernel.crossThreadData, mockKernel.crossThreadDataSize);

    EXPECT_EQ(0u, crossThreadData[0]);
}

class KernelFromBinaryTest : public ProgramSimpleFixture {
  public:
    void setUp() {
        ProgramSimpleFixture::setUp();
    }
    void tearDown() {
        ProgramSimpleFixture::tearDown();
    }
};
typedef Test<KernelFromBinaryTest> KernelFromBinaryTests;

TEST_F(KernelFromBinaryTests, GivenKernelNumArgsWhenGettingInfoThenNumberOfKernelArgsIsReturned) {
    createProgramFromBinary(pContext, pContext->getDevices(), "simple_kernels");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto &kernelInfo = pProgram->getKernelInfoForKernel("simple_kernel_0");

    // create a kernel
    auto kernel = Kernel::create(
        pProgram,
        kernelInfo,
        *pClDevice,
        retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);

    cl_uint paramValue = 0;
    size_t paramValueSizeRet = 0;

    // get size
    retVal = kernel->getInfo(
        CL_KERNEL_NUM_ARGS,
        sizeof(cl_uint),
        &paramValue,
        &paramValueSizeRet);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint), paramValueSizeRet);
    EXPECT_EQ(3u, paramValue);

    delete kernel;
}

TEST_F(KernelFromBinaryTests, WhenRegularKernelIsCreatedThenItIsNotBuiltIn) {
    createProgramFromBinary(pContext, pContext->getDevices(), "simple_kernels");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto &kernelInfo = pProgram->getKernelInfoForKernel("simple_kernel_0");

    // create a kernel
    auto kernel = Kernel::create(
        pProgram,
        kernelInfo,
        *pClDevice,
        retVal);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, kernel);

    // get builtIn property
    bool isBuiltIn = kernel->isBuiltInKernel();

    EXPECT_FALSE(isBuiltIn);

    delete kernel;
}

HWTEST_F(KernelFromBinaryTests, givenArgumentDeclaredAsConstantWhenKernelIsCreatedThenArgumentIsMarkedAsReadOnly) {
    createProgramFromBinary(pContext, pContext->getDevices(), "simple_kernels");

    ASSERT_NE(nullptr, pProgram);
    retVal = pProgram->build(
        pProgram->getDevices(),
        nullptr);

    ASSERT_EQ(CL_SUCCESS, retVal);

    auto pKernelInfo = pProgram->getKernelInfo("simple_kernel_6", rootDeviceIndex);
    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(1).isReadOnly());
    pKernelInfo = pProgram->getKernelInfo("simple_kernel_1", rootDeviceIndex);
    EXPECT_TRUE(pKernelInfo->getArgDescriptorAt(0).isReadOnly());
}

typedef Test<ClDeviceFixture> KernelPrivateSurfaceTest;
typedef Test<ClDeviceFixture> KernelGlobalSurfaceTest;
typedef Test<ClDeviceFixture> KernelConstantSurfaceTest;

class CommandStreamReceiverMock : public CommandStreamReceiver {
    typedef CommandStreamReceiver BaseClass;

  public:
    using CommandStreamReceiver::executionEnvironment;

    using BaseClass::CommandStreamReceiver;

    TagAllocatorBase *getTimestampPacketAllocator() override { return nullptr; }
    std::unique_ptr<TagAllocatorBase> createMultiRootDeviceTimestampPacketAllocator(const RootDeviceIndicesContainer &rootDeviceIndices) override { return std::unique_ptr<TagAllocatorBase>(nullptr); }

    SubmissionStatus flushTagUpdate() override { return SubmissionStatus::success; };
    void updateTagFromWait() override{};
    bool isUpdateTagFromWaitEnabled() override { return false; };

    bool isMultiOsContextCapable() const override { return false; }
    bool submitDependencyUpdate(TagNodeBase *tag) override { return true; }

    MemoryCompressionState getMemoryCompressionState(bool auxTranslationRequired) const override {
        return MemoryCompressionState::notApplicable;
    }

    CommandStreamReceiverMock() : BaseClass(*(new ExecutionEnvironment), 0, 1) {
        this->mockExecutionEnvironment.reset(&this->executionEnvironment);
        executionEnvironment.prepareRootDeviceEnvironments(1);
        executionEnvironment.rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
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

    NEO::SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        return NEO::SubmissionStatus::success;
    }

    WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, QueueThrottle throttle) override {
        return WaitStatus::ready;
    }
    TaskCountType flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, Device &device) override { return taskCount; };

    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap *dsh,
        const IndirectHeap *ioh,
        const IndirectHeap *ssh,
        TaskCountType taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override {
        if (getHeaplessStateInitEnabled()) {
            return flushTaskHeapless(commandStream, commandStreamStart, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
        } else {
            return flushTaskHeapful(commandStream, commandStreamStart, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
        }
    }

    CompletionStamp flushTaskHeapful(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap *dsh,
        const IndirectHeap *ioh,
        const IndirectHeap *ssh,
        TaskCountType taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override {
        CompletionStamp cs = {};
        return cs;
    }

    CompletionStamp flushTaskHeapless(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap *dsh,
        const IndirectHeap *ioh,
        const IndirectHeap *ssh,
        TaskCountType taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override {
        CompletionStamp cs = {};
        return cs;
    }

    CompletionStamp flushImmediateTask(
        LinearStream &immediateCommandStream,
        size_t immediateCommandStreamStart,
        ImmediateDispatchFlags &dispatchFlags,
        Device &device) override {
        CompletionStamp cs = {};
        return cs;
    }

    CompletionStamp flushImmediateTaskStateless(
        LinearStream &immediateCommandStream,
        size_t immediateCommandStreamStart,
        ImmediateDispatchFlags &dispatchFlags,
        Device &device) override {
        CompletionStamp cs = {};
        return cs;
    }

    CompletionStamp flushBcsTask(LinearStream &commandStreamTask, size_t commandStreamTaskStart,
                                 const DispatchBcsFlags &dispatchBcsFlags, const HardwareInfo &hwInfo) override {
        CompletionStamp cs = {};
        return cs;
    }

    SubmissionStatus sendRenderStateCacheFlush() override {
        return SubmissionStatus::success;
    }

    bool flushBatchedSubmissions() override { return true; }

    CommandStreamReceiverType getType() const override {
        return CommandStreamReceiverType::hardware;
    }

    void programHardwareContext(LinearStream &cmdStream) override {}
    size_t getCmdsSizeForHardwareContext() const override {
        return 0;
    }
    void programComputeBarrierCommand(LinearStream &cmdStream) override {
    }
    size_t getCmdsSizeForComputeBarrierCommand() const override {
        return 0;
    }
    void programStallingCommandsForBarrier(LinearStream &cmdStream, TimestampPacketContainer *barrierTimestampPacketNodes, const bool isDcFlushRequired) override {
    }
    GraphicsAllocation *getClearColorAllocation() override { return nullptr; }

    bool createPreemptionAllocation() override {
        return createPreemptionAllocationReturn;
    }

    void postInitFlagsSetup() override {}
    SubmissionStatus initializeDeviceWithFirstSubmission(Device &device) override { return SubmissionStatus::success; }

    QueueThrottle getLastDirectSubmissionThrottle() override {
        return QueueThrottle::MEDIUM;
    }

    std::map<const void *, size_t> residency;
    std::unique_ptr<ExecutionEnvironment> mockExecutionEnvironment;
    bool passResidencyCallToBaseClass = true;
    bool createPreemptionAllocationReturn = true;
};

TEST_F(KernelPrivateSurfaceTest, WhenChangingResidencyThenCsrResidencySizeIsUpdated) {
    ASSERT_NE(nullptr, pDevice);

    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->setPrivateMemory(112, false, 8, 40, 64);
    pKernelInfo->setCrossThreadDataSize(64);

    // create kernel
    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    // Test it
    auto executionEnvironment = pDevice->getExecutionEnvironment();
    std::unique_ptr<CommandStreamReceiverMock> csr(new CommandStreamReceiverMock(*executionEnvironment, 0, 1));
    csr->setupContext(*pDevice->getDefaultEngine().osContext);
    csr->residency.clear();
    EXPECT_EQ(0u, csr->residency.size());

    kernel->makeResident(*csr.get());
    EXPECT_EQ(1u, csr->residency.size());

    csr->makeSurfacePackNonResident(csr->getResidencyAllocations(), true);
    EXPECT_EQ(0u, csr->residency.size());

    delete kernel;
}

TEST_F(KernelPrivateSurfaceTest, givenKernelWithPrivateSurfaceThatIsInUseByGpuWhenKernelIsBeingDestroyedThenAllocationIsAddedToDeferredFreeList) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->setPrivateMemory(112, false, 8, 40, 64);
    pKernelInfo->setCrossThreadDataSize(64);

    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    kernel->initialize();

    auto &csr = pDevice->getGpgpuCommandStreamReceiver();

    auto privateSurface = kernel->privateSurface;
    auto tagAddress = csr.getTagAddress();

    privateSurface->updateTaskCount(*tagAddress + 1, csr.getOsContext().getContextId());

    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_TRUE(csr.getDeferredAllocations().peekIsEmpty());
    kernel.reset(nullptr);

    EXPECT_TRUE(csr.getTemporaryAllocations().peekIsEmpty());
    EXPECT_FALSE(csr.getDeferredAllocations().peekIsEmpty());
    EXPECT_EQ(csr.getDeferredAllocations().peekHead(), privateSurface);
}

TEST_F(KernelPrivateSurfaceTest, WhenPrivateSurfaceAllocationFailsThenOutOfResourcesErrorIsReturned) {
    ASSERT_NE(nullptr, pDevice);

    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->setPrivateMemory(112, false, 8, 40, 64);
    pKernelInfo->setCrossThreadDataSize(64);
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

    // create kernel
    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    MemoryManagementFixture::InjectedFunction method = [&](size_t failureIndex) {
        MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

        if (MemoryManagement::nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, kernel->initialize());
        } else {
            EXPECT_EQ(CL_OUT_OF_RESOURCES, kernel->initialize());
        }
        delete kernel;
    };
    auto f = new MemoryManagementFixture();
    f->setUp();
    f->injectFailures(method);
    f->tearDown();
    delete f;
}

TEST_F(KernelPrivateSurfaceTest, given32BitDeviceWhenKernelIsCreatedThenPrivateSurfaceIs32BitAllocation) {
    if constexpr (is64bit) {
        pDevice->getMemoryManager()->setForce32BitAllocations(true);

        auto pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->setPrivateMemory(112, false, 8, 40, 64);
        pKernelInfo->setCrossThreadDataSize(64);
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

        // create kernel
        MockContext context;
        MockProgram program(&context, false, toClDeviceVector(*pClDevice));
        MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

        ASSERT_EQ(CL_SUCCESS, kernel->initialize());

        EXPECT_TRUE(kernel->privateSurface->is32BitAllocation());

        delete kernel;
    }
}

HWTEST_F(KernelPrivateSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenPrivateMemorySurfaceStateIsPatchedWithCpuAddress) {

    // define kernel info
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->setPrivateMemory(16, false, 8, 0, 0);

    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));

    // create kernel
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // setup surface state heap
    char surfaceStateHeap[0x80];
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_NE(0u, kernel->getSurfaceStateHeapSize());

    auto bufferAddress = kernel->privateSurface->getGpuAddress();

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(kernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.privateMemoryAddress.bindful));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(bufferAddress, surfaceAddress);

    delete kernel;
}

TEST_F(KernelPrivateSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenPrivateMemorySurfaceStateIsNotPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;

    // setup global memory
    char buffer[16];
    MockGraphicsAllocation gfxAlloc(buffer, sizeof(buffer));

    MockContext context(pClDevice);
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    program.setConstantSurface(&gfxAlloc);

    // create kernel
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_EQ(0u, kernel->getSurfaceStateHeapSize());
    EXPECT_EQ(nullptr, kernel->getSurfaceStateHeap());

    program.setConstantSurface(nullptr);
    delete kernel;
}

TEST_F(KernelPrivateSurfaceTest, givenNullDataParameterStreamWhenGettingConstantBufferSizeThenZeroIsReturned) {
    auto pKernelInfo = std::make_unique<KernelInfo>();

    EXPECT_EQ(0u, pKernelInfo->getConstantBufferSize());
}

TEST_F(KernelPrivateSurfaceTest, givenNonNullDataParameterStreamWhenGettingConstantBufferSizeThenCorrectSizeIsReturned) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->setCrossThreadDataSize(64);

    EXPECT_EQ(64u, pKernelInfo->getConstantBufferSize());
}

TEST_F(KernelPrivateSurfaceTest, GivenKernelWhenScratchSizeIsGreaterThanMaxScratchSizeThenReturnInvalidKernel) {
    auto &gfxCoreHelper = pDevice->getGfxCoreHelper();
    auto &productHelper = pDevice->getProductHelper();
    uint32_t maxScratchSize = gfxCoreHelper.getMaxScratchSize(productHelper);

    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->setPrivateMemory(0x100, false, 0, 0, 0);
    pKernelInfo->kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = maxScratchSize + 100;

    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    EXPECT_EQ(CL_INVALID_KERNEL, kernel->initialize());
}

TEST_F(KernelPrivateSurfaceTest, GivenKernelWhenPrivateSurfaceTooBigAndGpuPointerSize4And32BitAllocationsThenReturnOutOfResources) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->setPrivateMemory(std::numeric_limits<uint32_t>::max(), false, 0, 0, 0);

    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    pKernelInfo->kernelDescriptor.kernelAttributes.gpuPointerSize = 4;
    pDevice->getMemoryManager()->setForce32BitAllocations(true);
    if (pDevice->getDeviceInfo().computeUnitsUsedForScratch == 0)
        pDevice->deviceInfo.computeUnitsUsedForScratch = 120;
    EXPECT_EQ(CL_OUT_OF_RESOURCES, kernel->initialize());
}

TEST_F(KernelPrivateSurfaceTest, GivenKernelWhenPrivateSurfaceTooBigAndGpuPointerSize8And32BitAllocationsThenReturnOutOfResources) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->setPrivateMemory(std::numeric_limits<uint32_t>::max(), false, 0, 0, 0);

    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    pKernelInfo->kernelDescriptor.kernelAttributes.gpuPointerSize = 8;
    pDevice->getMemoryManager()->setForce32BitAllocations(true);
    if (pDevice->getDeviceInfo().computeUnitsUsedForScratch == 0)
        pDevice->deviceInfo.computeUnitsUsedForScratch = 120;
    EXPECT_EQ(CL_OUT_OF_RESOURCES, kernel->initialize());
}

TEST_F(KernelGlobalSurfaceTest, givenBuiltInKernelWhenKernelIsCreatedThenGlobalSurfaceIsPatchedWithCpuAddress) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->setGlobalVariablesSurface(8, 0);
    pKernelInfo->setCrossThreadDataSize(16);
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

    char buffer[16];

    GraphicsAllocation gfxAlloc(0, 1u /*num gmms*/, AllocationType::unknown, buffer, (uint64_t)buffer - 8u, 8, static_cast<osHandle>(1u), MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    uint64_t bufferAddress = (uint64_t)gfxAlloc.getUnderlyingBuffer();

    // create kernel
    MockContext context;
    MockProgram program(&context, true, toClDeviceVector(*pClDevice));
    program.setGlobalSurface(&gfxAlloc);
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    EXPECT_TRUE(kernel->isBuiltInKernel());

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)kernel->getCrossThreadData());

    program.setGlobalSurface(nullptr);
    delete kernel;
}

TEST_F(KernelGlobalSurfaceTest, givenNDRangeKernelWhenKernelIsCreatedThenGlobalSurfaceIsPatchedWithBaseAddressOffset) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->setGlobalVariablesSurface(8, 0);
    pKernelInfo->setCrossThreadDataSize(16);
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

    char buffer[16];
    auto gmmHelper = pDevice->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(buffer));
    GraphicsAllocation gfxAlloc(0, 1u /*num gmms*/, AllocationType::unknown, buffer, reinterpret_cast<size_t>(buffer) - 8u, 8, MemoryPool::memoryNull, 0u, canonizedGpuAddress);
    uint64_t bufferAddress = gfxAlloc.getGpuAddress();

    // create kernel
    MockProgram program(toClDeviceVector(*pClDevice));
    program.setGlobalSurface(&gfxAlloc);
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)kernel->getCrossThreadData());

    program.setGlobalSurface(nullptr);

    delete kernel;
}

HWTEST_F(KernelGlobalSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenGlobalMemorySurfaceStateIsPatchedWithCpuAddress) {

    // define kernel info
    auto pKernelInfo = std::make_unique<MockKernelInfo>();

    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

    // setup global memory
    pKernelInfo->setGlobalVariablesSurface(8, 0, 0);

    char buffer[16];
    MockGraphicsAllocation gfxAlloc(buffer, sizeof(buffer));
    auto bufferAddress = gfxAlloc.getGpuAddress();

    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    program.setGlobalSurface(&gfxAlloc);

    // create kernel
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // setup surface state heap
    char surfaceStateHeap[0x80];
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_NE(0u, kernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(kernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindful));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(bufferAddress, surfaceAddress);

    program.setGlobalSurface(nullptr);
    delete kernel;
}

TEST_F(KernelGlobalSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenGlobalMemorySurfaceStateIsNotPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;

    // setup global memory
    char buffer[16];
    MockGraphicsAllocation gfxAlloc(buffer, sizeof(buffer));

    MockProgram program(toClDeviceVector(*pClDevice));
    program.setGlobalSurface(&gfxAlloc);

    // create kernel
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_EQ(0u, kernel->getSurfaceStateHeapSize());
    EXPECT_EQ(nullptr, kernel->getSurfaceStateHeap());

    program.setGlobalSurface(nullptr);
    delete kernel;
}

TEST_F(KernelConstantSurfaceTest, givenBuiltInKernelWhenKernelIsCreatedThenConstantSurfaceIsPatchedWithCpuAddress) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->setGlobalConstantsSurface(8, 0);
    pKernelInfo->setCrossThreadDataSize(16);
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

    char buffer[16];

    GraphicsAllocation gfxAlloc(0, 1u /*num gmms*/, AllocationType::unknown, buffer, (uint64_t)buffer - 8u, 8, static_cast<osHandle>(1u), MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    uint64_t bufferAddress = (uint64_t)gfxAlloc.getUnderlyingBuffer();

    // create kernel
    MockContext context;
    MockProgram program(&context, true, toClDeviceVector(*pClDevice));
    program.setConstantSurface(&gfxAlloc);
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    EXPECT_TRUE(kernel->isBuiltInKernel());
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)kernel->getCrossThreadData());

    program.setConstantSurface(nullptr);
    delete kernel;
}

TEST_F(KernelConstantSurfaceTest, givenNDRangeKernelWhenKernelIsCreatedThenConstantSurfaceIsPatchedWithBaseAddressOffset) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->setGlobalConstantsSurface(8, 0);
    pKernelInfo->setCrossThreadDataSize(16);
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

    char buffer[16];
    auto gmmHelper = pDevice->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(buffer));
    GraphicsAllocation gfxAlloc(0, 1u /*num gmms*/, AllocationType::unknown, buffer, reinterpret_cast<size_t>(buffer) - 8u, 8, MemoryPool::memoryNull, 0u, canonizedGpuAddress);
    uint64_t bufferAddress = gfxAlloc.getGpuAddress();

    // create kernel
    MockProgram program(toClDeviceVector(*pClDevice));
    program.setConstantSurface(&gfxAlloc);
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_EQ(bufferAddress, *(uint64_t *)kernel->getCrossThreadData());

    program.setConstantSurface(nullptr);

    delete kernel;
}

HWTEST_F(KernelConstantSurfaceTest, givenStatefulKernelWhenKernelIsCreatedThenConstantMemorySurfaceStateIsPatchedWithCpuAddress) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

    // setup constant memory
    pKernelInfo->setGlobalConstantsSurface(8, 0, 0);

    char buffer[16];
    MockGraphicsAllocation gfxAlloc(buffer, sizeof(buffer));
    auto bufferAddress = gfxAlloc.getGpuAddress();

    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    program.setConstantSurface(&gfxAlloc);

    // create kernel
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // setup surface state heap
    char surfaceStateHeap[0x80];
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_NE(0u, kernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(kernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindful));
    auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    EXPECT_EQ(bufferAddress, surfaceAddress);

    program.setConstantSurface(nullptr);
    delete kernel;
}

TEST_F(KernelConstantSurfaceTest, givenStatelessKernelWhenKernelIsCreatedThenConstantMemorySurfaceStateIsNotPatched) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;

    // setup global memory
    char buffer[16];
    MockGraphicsAllocation gfxAlloc(buffer, sizeof(buffer));

    MockProgram program(toClDeviceVector(*pClDevice));
    program.setConstantSurface(&gfxAlloc);

    // create kernel
    MockKernel *kernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_EQ(0u, kernel->getSurfaceStateHeapSize());
    EXPECT_EQ(nullptr, kernel->getSurfaceStateHeap());

    program.setConstantSurface(nullptr);
    delete kernel;
}

typedef Test<ClDeviceFixture> KernelResidencyTest;

HWTEST_F(KernelResidencyTest, givenKernelWhenMakeResidentIsCalledThenKernelIsaIsMadeResident) {
    ASSERT_NE(nullptr, pDevice);
    char pCrossThreadData[64];

    // define kernel info
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    // setup kernel arg offsets
    pKernelInfo->addArgBuffer(0, 0x10);
    pKernelInfo->addArgBuffer(1, 0x20);
    pKernelInfo->addArgBuffer(2, 0x30);

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());
    kernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

    EXPECT_EQ(0u, commandStreamReceiver.makeResidentAllocations.size());
    kernel->makeResident(pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(1u, commandStreamReceiver.makeResidentAllocations.size());
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(kernel->getKernelInfo().getGraphicsAllocation()));

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenMakeResidentIsCalledThenExportedFunctionsIsaAllocationIsMadeResident) {
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    auto exportedFunctionsSurface = std::make_unique<MockGraphicsAllocation>();
    program.buildInfos[pDevice->getRootDeviceIndex()].exportedFunctionsSurface = exportedFunctionsSurface.get();
    MockContext ctx;
    program.setContext(&ctx);
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_EQ(0u, commandStreamReceiver.makeResidentAllocations.size());
    kernel->makeResident(pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(program.buildInfos[pDevice->getRootDeviceIndex()].exportedFunctionsSurface));

    // check getResidency as well
    std::vector<NEO::Surface *> residencySurfaces;
    kernel->getResidency(residencySurfaces);
    std::unique_ptr<NEO::ExecutionEnvironment> mockCsrExecEnv = std::make_unique<ExecutionEnvironment>();
    mockCsrExecEnv->prepareRootDeviceEnvironments(1);
    mockCsrExecEnv->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    mockCsrExecEnv->initializeMemoryManager();
    {
        CommandStreamReceiverMock csrMock(*mockCsrExecEnv.get(), 0, 1);
        csrMock.passResidencyCallToBaseClass = false;
        for (const auto &s : residencySurfaces) {
            s->makeResident(csrMock);
            delete s;
        }
        EXPECT_EQ(1U, csrMock.residency.count(exportedFunctionsSurface->getUnderlyingBuffer()));
    }

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenMakeResidentIsCalledThenGlobalBufferIsMadeResident) {
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_EQ(0u, commandStreamReceiver.makeResidentAllocations.size());
    kernel->makeResident(pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface->getGraphicsAllocation()));

    std::vector<NEO::Surface *> residencySurfaces;
    kernel->getResidency(residencySurfaces);
    std::unique_ptr<NEO::ExecutionEnvironment> mockCsrExecEnv = std::make_unique<ExecutionEnvironment>();
    mockCsrExecEnv->prepareRootDeviceEnvironments(1);
    mockCsrExecEnv->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    mockCsrExecEnv->initializeMemoryManager();
    {
        CommandStreamReceiverMock csrMock(*mockCsrExecEnv.get(), 0, 1);
        csrMock.passResidencyCallToBaseClass = false;
        for (const auto &s : residencySurfaces) {
            s->makeResident(csrMock);
            delete s;
        }
        EXPECT_EQ(1U, csrMock.residency.count(program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface->getGraphicsAllocation()->getUnderlyingBuffer()));
    }

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenBindlessHeapsHelperAndGlobalAndConstantBuffersWhenMakeResidentIsCalledThenGlobalAndConstantBufferHeapAllocationsAreMadeResident) {
    auto bindlessHeapHelper = new MockBindlesHeapsHelper(pDevice, false);
    pDevice->getExecutionEnvironment()->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapHelper);

    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    program.buildInfos[pDevice->getRootDeviceIndex()].constantSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    EXPECT_TRUE(memoryManager->allocateBindlessSlot(program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface->getGraphicsAllocation()));
    EXPECT_TRUE(memoryManager->allocateBindlessSlot(program.buildInfos[pDevice->getRootDeviceIndex()].constantSurface->getGraphicsAllocation()));

    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_EQ(0u, commandStreamReceiver.makeResidentAllocations.size());
    kernel->makeResident(pDevice->getGpgpuCommandStreamReceiver());

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface->getGraphicsAllocation()));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(program.getGlobalSurfaceGA(rootDeviceIndex)->getBindlessInfo().heapAllocation));

    EXPECT_TRUE(commandStreamReceiver.isMadeResident(program.buildInfos[pDevice->getRootDeviceIndex()].constantSurface->getGraphicsAllocation()));
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(program.getConstantSurfaceGA(rootDeviceIndex)->getBindlessInfo().heapAllocation));

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenItUsesIndirectUnifiedMemoryDeviceAllocationThenTheyAreMadeResident) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto properties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    properties.device = pDevice;
    auto unifiedMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, properties);

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());

    EXPECT_EQ(0u, commandStreamReceiver.getResidencyAllocations().size());

    mockKernel.mockKernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, true);

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());

    EXPECT_EQ(1u, commandStreamReceiver.getResidencyAllocations().size());

    EXPECT_EQ(commandStreamReceiver.getResidencyAllocations()[0]->getGpuAddress(), castToUint64(unifiedMemoryAllocation));

    mockKernel.mockKernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_SVM_PTRS, true);

    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelUsingIndirectHostMemoryWhenMakeResidentIsCalledThenAllAllocationsAreMadeResident) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto deviceProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    deviceProperties.device = pDevice;
    auto hostProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedDeviceMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, deviceProperties);
    auto unifiedHostMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, hostProperties);

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(0u, commandStreamReceiver.getResidencyAllocations().size());
    mockKernel.mockKernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, true);

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(2u, commandStreamReceiver.getResidencyAllocations().size());

    svmAllocationsManager->freeSVMAlloc(unifiedDeviceMemoryAllocation);
    svmAllocationsManager->freeSVMAlloc(unifiedHostMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelUsingIndirectSharedMemoryWhenMakeResidentIsCalledThenAllSharedAllocationsAreMadeResident) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto sharedProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto hostProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedSharedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(4096u, sharedProperties, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()));
    auto unifiedHostMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, hostProperties);

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(0u, commandStreamReceiver.getResidencyAllocations().size());
    mockKernel.mockKernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, true);

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(2u, commandStreamReceiver.getResidencyAllocations().size());

    svmAllocationsManager->freeSVMAlloc(unifiedSharedMemoryAllocation);
    svmAllocationsManager->freeSVMAlloc(unifiedHostMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelUsingIndirectSharedMemoryButNotHasIndirectAccessWhenMakeResidentIsCalledThenOnlySharedAllocationsAreNotMadeResident) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto sharedProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto hostProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedSharedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(4096u, sharedProperties, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()));
    auto unifiedHostMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, hostProperties);
    mockKernel.mockKernel->kernelHasIndirectAccess = false;

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(0u, commandStreamReceiver.getResidencyAllocations().size());
    mockKernel.mockKernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, true);

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(0u, commandStreamReceiver.getResidencyAllocations().size());

    svmAllocationsManager->freeSVMAlloc(unifiedSharedMemoryAllocation);
    svmAllocationsManager->freeSVMAlloc(unifiedHostMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenDeviceUnifiedMemoryAndPageFaultManagerWhenMakeResidentIsCalledThenAllocationIsNotDecommitted) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto deviceProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    deviceProperties.device = pDevice;
    auto unifiedMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, deviceProperties);
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);

    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    mockKernel.mockKernel->setUnifiedMemoryExecInfo(unifiedMemoryGraphicsAllocation->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex()));
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

HWTEST_F(KernelResidencyTest, givenSharedUnifiedMemoryAndPageFaultManagerWhenMakeResidentIsCalledThenAllocationIsDecommitted) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto sharedProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(4096u, sharedProperties, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()));
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    mockPageFaultManager->insertAllocation(unifiedMemoryAllocation, 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 0);

    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    mockKernel.mockKernel->setUnifiedMemoryExecInfo(unifiedMemoryGraphicsAllocation->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex()));
    EXPECT_EQ(1u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());

    mockKernel.mockKernel->makeResident(commandStreamReceiver);

    EXPECT_EQ(mockPageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(mockPageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 1);

    EXPECT_EQ(mockPageFaultManager->protectedMemoryAccessAddress, unifiedMemoryAllocation);
    EXPECT_EQ(mockPageFaultManager->protectedSize, 4096u);
    EXPECT_EQ(mockPageFaultManager->transferToGpuAddress, unifiedMemoryAllocation);

    mockKernel.mockKernel->clearUnifiedMemoryExecInfo();
    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenSharedUnifiedMemoryAndNotRequiredMemSyncWhenMakeResidentIsCalledThenAllocationIsNotDecommitted) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice, nullptr, true);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto sharedProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(4096u, sharedProperties, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()));
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    mockPageFaultManager->insertAllocation(unifiedMemoryAllocation, 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 0);
    auto gpuAllocation = unifiedMemoryGraphicsAllocation->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
    mockKernel.mockKernel->kernelArguments[0] = {
        sizeof(uintptr_t),
        gpuAllocation,
        unifiedMemoryAllocation,
        4096u,
        gpuAllocation,
        Kernel::KernelArgType::SVM_ALLOC_OBJ};
    mockKernel.mockKernel->setUnifiedMemorySyncRequirement(false);

    mockKernel.mockKernel->makeResident(commandStreamReceiver);

    EXPECT_EQ(mockPageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(mockPageFaultManager->protectMemoryCalled, 0);
    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 0);

    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

class MockGeneralSurface : public GeneralSurface {
  public:
    using GeneralSurface::needsMigration;
};

HWTEST_F(KernelResidencyTest, givenSvmArgWhenKernelDoesNotRequireUnifiedMemorySyncThenSurfaceDoesNotNeedMigration) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice, nullptr, true);

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto sharedProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(4096u, sharedProperties, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()));
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    mockPageFaultManager->insertAllocation(unifiedMemoryAllocation, 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    auto gpuAllocation = unifiedMemoryGraphicsAllocation->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
    mockKernel.mockKernel->kernelArguments[0] = {
        sizeof(uintptr_t),
        gpuAllocation,
        unifiedMemoryAllocation,
        4096u,
        gpuAllocation,
        Kernel::KernelArgType::SVM_ALLOC_OBJ};
    mockKernel.mockKernel->setUnifiedMemorySyncRequirement(false);
    std::vector<NEO::Surface *> residencySurfaces;
    mockKernel.mockKernel->getResidency(residencySurfaces);
    EXPECT_FALSE(reinterpret_cast<MockGeneralSurface *>(residencySurfaces[0])->needsMigration);
    for (auto surface : residencySurfaces) {
        delete surface;
    }
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenSvmArgWhenKernelRequireUnifiedMemorySyncThenSurfaceNeedMigration) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice, nullptr, true);

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto sharedProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(4096u, sharedProperties, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()));
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    mockPageFaultManager->insertAllocation(unifiedMemoryAllocation, 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    auto gpuAllocation = unifiedMemoryGraphicsAllocation->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
    mockKernel.mockKernel->kernelArguments[0] = {
        sizeof(uintptr_t),
        gpuAllocation,
        unifiedMemoryAllocation,
        4096u,
        gpuAllocation,
        Kernel::KernelArgType::SVM_ALLOC_OBJ};
    mockKernel.mockKernel->setUnifiedMemorySyncRequirement(true);
    std::vector<NEO::Surface *> residencySurfaces;
    mockKernel.mockKernel->getResidency(residencySurfaces);
    EXPECT_TRUE(reinterpret_cast<MockGeneralSurface *>(residencySurfaces[0])->needsMigration);
    for (auto surface : residencySurfaces) {
        delete surface;
    }
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenSharedUnifiedMemoryRequiredMemSyncWhenMakeResidentIsCalledThenAllocationIsDecommitted) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice, nullptr, true);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto sharedProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(4096u, sharedProperties, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()));
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    mockPageFaultManager->insertAllocation(unifiedMemoryAllocation, 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    auto gpuAllocation = unifiedMemoryGraphicsAllocation->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 0);
    mockKernel.mockKernel->kernelArguments[0] = {
        sizeof(uintptr_t),
        gpuAllocation,
        unifiedMemoryAllocation,
        4096u,
        gpuAllocation,
        Kernel::KernelArgType::SVM_ALLOC_OBJ};
    mockKernel.mockKernel->setUnifiedMemorySyncRequirement(true);

    mockKernel.mockKernel->makeResident(commandStreamReceiver);

    EXPECT_EQ(mockPageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(mockPageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 0);
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 1);

    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenSharedUnifiedMemoryAllocPageFaultManagerAndIndirectAllocsAllowedWhenMakeResidentIsCalledThenAllocationIsDecommitted) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice);
    auto &commandStreamReceiver = this->pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto sharedProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(4096u, sharedProperties, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()));
    mockPageFaultManager->insertAllocation(unifiedMemoryAllocation, 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 0);
    mockKernel.mockKernel->unifiedMemoryControls.indirectSharedAllocationsAllowed = true;

    mockKernel.mockKernel->makeResident(commandStreamReceiver);

    EXPECT_EQ(mockPageFaultManager->allowMemoryAccessCalled, 0);
    EXPECT_EQ(mockPageFaultManager->protectMemoryCalled, 1);
    EXPECT_EQ(mockPageFaultManager->transferToCpuCalled, 0);
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
    auto deviceProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    deviceProperties.device = pDevice;
    auto unifiedMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, deviceProperties);
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);

    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());

    mockKernel.mockKernel->setUnifiedMemoryExecInfo(unifiedMemoryGraphicsAllocation->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex()));

    EXPECT_EQ(1u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    EXPECT_EQ(mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations[0]->getGpuAddress(), castToUint64(unifiedMemoryAllocation));

    mockKernel.mockKernel->makeResident(this->pDevice->getGpgpuCommandStreamReceiver());
    EXPECT_EQ(1u, commandStreamReceiver.getResidencyAllocations().size());
    EXPECT_EQ(commandStreamReceiver.getResidencyAllocations()[0]->getGpuAddress(), castToUint64(unifiedMemoryAllocation));

    mockKernel.mockKernel->clearUnifiedMemoryExecInfo();
    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithAndWithoutUnifiedMemoryIsCalledThenAllocationIsStoredAndDeletedWithinKernel) {
    MockKernelWithInternals mockKernel(*this->pClDevice);

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto deviceProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    deviceProperties.device = pDevice;
    auto unifiedMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, deviceProperties);

    auto unifiedMemoryAllocation2 = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, deviceProperties);

    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL, sizeof(unifiedMemoryAllocation), &unifiedMemoryAllocation);
    EXPECT_EQ(CL_SUCCESS, status);

    EXPECT_EQ(1u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    EXPECT_EQ(mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations[0]->getGpuAddress(), castToUint64(unifiedMemoryAllocation));

    status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL, sizeof(unifiedMemoryAllocation), &unifiedMemoryAllocation2);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(1u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());
    EXPECT_EQ(mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations[0]->getGpuAddress(), castToUint64(unifiedMemoryAllocation2));

    status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_USM_PTRS_INTEL, 0, nullptr);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_EQ(0u, mockKernel.mockKernel->kernelUnifiedMemoryGfxAllocations.size());

    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation2);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryDevicePropertyAndDisableIndirectAccessNotSetThenKernelControlIsChanged) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableIndirectAccess.set(0);

    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectDeviceAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectDeviceAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockKernel.mockKernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    enableIndirectDeviceAccess = CL_FALSE;
    status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectDeviceAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryDevicePropertyAndDisableIndirectAccessSetThenKernelControlIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableIndirectAccess.set(1);

    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectDeviceAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectDeviceAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryDevicePropertyAndDisableIndirectAccessNotSetAndNoIndirectAccessInKernelThenKernelControlIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableIndirectAccess.set(0);

    MockKernelWithInternals mockKernel(*this->pClDevice);
    mockKernel.mockKernel->kernelHasIndirectAccess = false;
    cl_bool enableIndirectDeviceAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectDeviceAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryDevicePropertyIsCalledThenKernelControlIsChanged) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectDeviceAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectDeviceAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockKernel.mockKernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed);
    enableIndirectDeviceAccess = CL_FALSE;
    status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectDeviceAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectDeviceAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryHostPropertyAndDisableIndirectAccessNotSetThenKernelControlIsChanged) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableIndirectAccess.set(0);

    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectHostAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectHostAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockKernel.mockKernel->unifiedMemoryControls.indirectHostAllocationsAllowed);
    enableIndirectHostAccess = CL_FALSE;
    status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectHostAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectHostAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryHostPropertyAndDisableIndirectAccessSetThenKernelControlIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableIndirectAccess.set(1);

    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectHostAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectHostAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectHostAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryHostPropertyAndDisableIndirectAccessNotSetAndNoIndirectAccessInKernelThenKernelControlIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableIndirectAccess.set(0);

    MockKernelWithInternals mockKernel(*this->pClDevice);
    mockKernel.mockKernel->kernelHasIndirectAccess = false;
    cl_bool enableIndirectHostAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectHostAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectHostAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemoryHostPropertyIsCalledThenKernelControlIsChanged) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectHostAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectHostAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockKernel.mockKernel->unifiedMemoryControls.indirectHostAllocationsAllowed);
    enableIndirectHostAccess = CL_FALSE;
    status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_HOST_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectHostAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectHostAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemorySharedPropertyAndDisableIndirectAccessNotSetThenKernelControlIsChanged) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableIndirectAccess.set(0);

    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectSharedAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectSharedAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockKernel.mockKernel->unifiedMemoryControls.indirectSharedAllocationsAllowed);
    enableIndirectSharedAccess = CL_FALSE;
    status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectSharedAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemorySharedPropertyAndDisableIndirectAccessSetThenKernelControlIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableIndirectAccess.set(1);

    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectSharedAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectSharedAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemorySharedPropertyAndDisableIndirectAccessNotSetAndNoIndirectAccessInKernelThenKernelControlIsNotSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DisableIndirectAccess.set(0);

    MockKernelWithInternals mockKernel(*this->pClDevice);
    mockKernel.mockKernel->kernelHasIndirectAccess = false;
    cl_bool enableIndirectSharedAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectSharedAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenclSetKernelExecInfoWithUnifiedMemorySharedPropertyIsCalledThenKernelControlIsChanged) {
    MockKernelWithInternals mockKernel(*this->pClDevice);
    cl_bool enableIndirectSharedAccess = CL_TRUE;
    auto status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectSharedAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_TRUE(mockKernel.mockKernel->unifiedMemoryControls.indirectSharedAllocationsAllowed);
    enableIndirectSharedAccess = CL_FALSE;
    status = clSetKernelExecInfo(mockKernel.mockMultiDeviceKernel, CL_KERNEL_EXEC_INFO_INDIRECT_SHARED_ACCESS_INTEL, sizeof(cl_bool), &enableIndirectSharedAccess);
    EXPECT_EQ(CL_SUCCESS, status);
    EXPECT_FALSE(mockKernel.mockKernel->unifiedMemoryControls.indirectSharedAllocationsAllowed);
}

HWTEST_F(KernelResidencyTest, givenKernelWithNoKernelArgLoadNorKernelArgStoreNorKernelArgAtomicAndHasIndirectStatelessAccessAndDetectIndirectAccessInKernelEnabledThenKernelHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DetectIndirectAccessInKernel.set(1);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgStore = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = true;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_TRUE(kernel->getHasIndirectAccess());

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWithNoKernelArgLoadNorKernelArgStoreNorKernelArgAtomicNorIndirectStatelessAccessAndDetectIndirectAccessInKernelEnabledThenKernelHasIndirectAccessIsSetToFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DetectIndirectAccessInKernel.set(1);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgStore = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = false;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_FALSE(kernel->getHasIndirectAccess());

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWithStackCallsAndDetectIndirectAccessInKernelEnabledThenKernelHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DetectIndirectAccessInKernel.set(1);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgStore = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.useStackCalls = true;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_TRUE(kernel->getHasIndirectAccess());

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWithoutStackCallsAndDetectIndirectAccessInKernelEnabledThenKernelHasIndirectAccessIsSetToFalse) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DetectIndirectAccessInKernel.set(1);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgStore = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.useStackCalls = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectAccessInImplicitArg = false;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_FALSE(kernel->getHasIndirectAccess());

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWithPtrByValueArgumentAndDetectIndirectAccessInKernelEnabledThenKernelHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DetectIndirectAccessInKernel.set(1);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgStore = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectAccessInImplicitArg = false;

    auto ptrByValueArg = ArgDescriptor(ArgDescriptor::argTValue);
    ArgDescValue::Element element;
    element.isPtr = true;
    ptrByValueArg.as<ArgDescValue>().elements.push_back(element);
    pKernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(ptrByValueArg);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_TRUE(kernel->getHasIndirectAccess());

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWithNoKernelArgLoadNorKernelArgStoreNorKernelArgAtomicNorHasIndirectStatelessAccessAndDetectIndirectAccessInKernelDisabledThenKernelHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DetectIndirectAccessInKernel.set(0);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgStore = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectAccessInImplicitArg = false;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_TRUE(kernel->getHasIndirectAccess());

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWithNoKernelArgLoadAndDetectIndirectAccessInKernelEnabledThenKernelHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DetectIndirectAccessInKernel.set(1);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = true;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgStore = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectAccessInImplicitArg = false;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_TRUE(kernel->getHasIndirectAccess());

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWithNoKernelArgStoreAndDetectIndirectAccessInKernelEnabledThenKernelHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DetectIndirectAccessInKernel.set(1);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgStore = true;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectAccessInImplicitArg = false;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_TRUE(kernel->getHasIndirectAccess());

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWithNoKernelArgAtomicAndDetectIndirectAccessInKernelEnabledThenKernelHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DetectIndirectAccessInKernel.set(1);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgStore = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = true;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectAccessInImplicitArg = false;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_TRUE(kernel->getHasIndirectAccess());

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWithNoKernelArgAtomicAndImplicitArgsHasIndirectAccessAndDetectIndirectAccessInKernelEnabledThenKernelHasIndirectAccessIsSetToTrue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.DetectIndirectAccessInKernel.set(1);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgStore = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = false;
    pKernelInfo->kernelDescriptor.kernelAttributes.hasIndirectAccessInImplicitArg = true;

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;

    auto memoryManager = commandStreamReceiver.getMemoryManager();
    pKernelInfo->kernelAllocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    program.buildInfos[pDevice->getRootDeviceIndex()].globalSurface = std::make_unique<SharedPoolAllocation>(new MockGraphicsAllocation());
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    EXPECT_TRUE(kernel->getHasIndirectAccess());

    memoryManager->freeGraphicsMemory(pKernelInfo->kernelAllocation);
}

HWTEST_F(KernelResidencyTest, givenSimpleKernelWhenExecEnvDoesNotHavePageFaultManagerThenPageFaultDoesNotMoveAllocation) {
    auto mockPageFaultManager = std::make_unique<MockPageFaultManager>();
    MockKernelWithInternals mockKernel(*this->pClDevice);

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto sharedProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(4096u, sharedProperties, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()));
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    mockPageFaultManager->insertAllocation(reinterpret_cast<void *>(unifiedMemoryGraphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()), 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    Kernel::SimpleKernelArgInfo kernelArgInfo;
    kernelArgInfo.object = unifiedMemoryGraphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation();
    kernelArgInfo.type = Kernel::KernelArgType::SVM_ALLOC_OBJ;

    std::vector<Kernel::SimpleKernelArgInfo> kernelArguments;
    kernelArguments.resize(1);
    kernelArguments[0] = kernelArgInfo;
    mockKernel.kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    mockKernel.kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>(true).accessedUsingStatelessAddressingMode = true;
    mockKernel.mockKernel->setKernelArguments(kernelArguments);
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 0);
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset();
}

HWTEST_F(KernelResidencyTest, givenSimpleKernelWhenIsUnifiedMemorySyncRequiredIsFalseThenPageFaultDoesNotMoveAllocation) {
    auto mockPageFaultManager = new MockPageFaultManager();
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset(mockPageFaultManager);
    MockKernelWithInternals mockKernel(*this->pClDevice);

    auto svmAllocationsManager = mockKernel.mockContext->getSVMAllocsManager();
    auto sharedProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::sharedUnifiedMemory, 1, mockKernel.mockContext->getRootDeviceIndices(), mockKernel.mockContext->getDeviceBitfields());
    auto unifiedMemoryAllocation = svmAllocationsManager->createSharedUnifiedMemoryAllocation(4096u, sharedProperties, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()));
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    mockPageFaultManager->insertAllocation(reinterpret_cast<void *>(unifiedMemoryGraphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation()->getGpuAddress()), 4096u, svmAllocationsManager, mockKernel.mockContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    Kernel::SimpleKernelArgInfo kernelArgInfo;
    kernelArgInfo.object = unifiedMemoryGraphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation();
    kernelArgInfo.type = Kernel::KernelArgType::SVM_ALLOC_OBJ;

    std::vector<Kernel::SimpleKernelArgInfo> kernelArguments;
    kernelArguments.resize(1);
    kernelArguments[0] = kernelArgInfo;
    mockKernel.kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);
    mockKernel.kernelInfo.kernelDescriptor.payloadMappings.explicitArgs[0].as<ArgDescPointer>(true).accessedUsingStatelessAddressingMode = true;
    mockKernel.mockKernel->setKernelArguments(kernelArguments);
    mockKernel.mockKernel->isUnifiedMemorySyncRequired = false;
    EXPECT_EQ(mockPageFaultManager->transferToGpuCalled, 0);
    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
    static_cast<MockMemoryManager *>(this->pDevice->getExecutionEnvironment()->memoryManager.get())->pageFaultManager.reset();
}

TEST(KernelImageDetectionTests, givenKernelWithImagesOnlyWhenItIsAskedIfItHasImagesOnlyThenTrueIsReturned) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    pKernelInfo->addArgImage(0);
    pKernelInfo->addArgImage(1);
    pKernelInfo->addArgImage(2);

    const auto rootDeviceIndex = 0u;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), rootDeviceIndex));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));
    auto kernel = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *device);
    EXPECT_FALSE(kernel->usesOnlyImages());
    kernel->initialize();
    EXPECT_TRUE(kernel->usesOnlyImages());
    EXPECT_TRUE(kernel->usesImages());
}

TEST(KernelImageDetectionTests, givenKernelWithImagesAndBuffersWhenItIsAskedIfItHasImagesOnlyThenFalseIsReturned) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    pKernelInfo->addArgImage(0);
    pKernelInfo->addArgBuffer(1);
    pKernelInfo->addArgImage(2);

    const auto rootDeviceIndex = 0u;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), rootDeviceIndex));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));
    auto kernel = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *device);
    EXPECT_FALSE(kernel->usesOnlyImages());
    kernel->initialize();
    EXPECT_FALSE(kernel->usesOnlyImages());
    EXPECT_TRUE(kernel->usesImages());
}

TEST(KernelImageDetectionTests, givenKernelWithNoImagesWhenItIsAskedIfItHasImagesOnlyThenFalseIsReturned) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    pKernelInfo->addArgBuffer(0);

    const auto rootDeviceIndex = 0u;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), rootDeviceIndex));
    auto context = clUniquePtr(new MockContext(device.get()));
    auto program = clUniquePtr(new MockProgram(context.get(), false, toClDeviceVector(*device)));
    auto kernel = std::make_unique<MockKernel>(program.get(), *pKernelInfo, *device);
    EXPECT_FALSE(kernel->usesOnlyImages());
    kernel->initialize();
    EXPECT_FALSE(kernel->usesOnlyImages());
    EXPECT_FALSE(kernel->usesImages());
}

HWTEST_F(KernelResidencyTest, WhenMakingArgsResidentThenImageFromImageCheckIsCorrect) {
    ASSERT_NE(nullptr, pDevice);

    // create NV12 image
    cl_mem_flags flags = CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS;
    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_NV12_INTEL;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, pClDevice->getHardwareInfo().capabilityTable.supportsOcl21Features);

    cl_image_desc imageDesc = {};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 16;
    imageDesc.image_height = 16;
    imageDesc.image_depth = 1;

    cl_int retVal;
    MockContext context;
    std::unique_ptr<NEO::Image> imageNV12(
        Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(imageNV12->getMediaPlaneType(), 0u);

    // create Y plane
    imageFormat.image_channel_order = CL_R;
    flags = CL_MEM_READ_ONLY;
    surfaceFormat = Image::getSurfaceFormatFromTable(
        flags, &imageFormat, context.getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    imageDesc.image_width = 0;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.mem_object = imageNV12.get();

    std::unique_ptr<NEO::Image> imageY(
        Image::create(&context, ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context.getDevice(0)->getDevice()),
                      flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    EXPECT_EQ(imageY->getMediaPlaneType(), 0u);

    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    pKernelInfo->addArgImage(0);

    auto program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
    program->setContext(&context);
    std::unique_ptr<MockKernel> kernel(new MockKernel(program.get(), *pKernelInfo, *pClDevice));

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());
    kernel->storeKernelArg(0, Kernel::IMAGE_OBJ, (cl_mem)imageY.get(), NULL, 0);
    kernel->makeResident(pDevice->getGpgpuCommandStreamReceiver());

    EXPECT_FALSE(imageNV12->isImageFromImage());
    EXPECT_TRUE(imageY->isImageFromImage());

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    EXPECT_EQ(CommandStreamReceiver::SamplerCacheFlushState::samplerCacheFlushBefore, commandStreamReceiver.samplerCacheFlushRequired);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenMakeResidentIsCalledThenIndirectAllocationsArePacked) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());
    kernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, true);

    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    auto svmAllocationsManager = ctx.getSVMAllocsManager();
    auto deviceProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, ctx.getRootDeviceIndices(), ctx.getDeviceBitfields());
    deviceProperties.device = pDevice;
    auto unifiedMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, deviceProperties);
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    auto graphicsAllocation = unifiedMemoryGraphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation();

    // Verify that indirect allocation is always resident
    kernel->makeResident(csr);
    EXPECT_EQ(GraphicsAllocation::objectAlwaysResident, graphicsAllocation->getResidencyTaskCount(csr.getOsContext().getContextId()));

    // Force to non-resident
    graphicsAllocation->updateResidencyTaskCount(GraphicsAllocation::objectNotResident, csr.getOsContext().getContextId());

    // Verify that packed allocation is tracked and makeResident is called once
    kernel->makeResident(csr);
    EXPECT_EQ(GraphicsAllocation::objectNotResident, graphicsAllocation->getResidencyTaskCount(csr.getOsContext().getContextId()));

    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

HWTEST_F(KernelResidencyTest, givenKernelWhenMakeResidentIsCalledAndPackingIsDisabledThenIndirectAllocationsAreNotPacked) {
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.MakeIndirectAllocationsResidentAsPack.set(0);

    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    MockProgram program(toClDeviceVector(*pClDevice));
    MockContext ctx;
    program.setContext(&ctx);
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));
    ASSERT_EQ(CL_SUCCESS, kernel->initialize());
    kernel->setUnifiedMemoryProperty(CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, true);

    auto &csr = pDevice->getGpgpuCommandStreamReceiver();
    auto svmAllocationsManager = ctx.getSVMAllocsManager();
    auto deviceProperties = SVMAllocsManager::UnifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, ctx.getRootDeviceIndices(), ctx.getDeviceBitfields());
    deviceProperties.device = pDevice;
    auto unifiedMemoryAllocation = svmAllocationsManager->createUnifiedMemoryAllocation(4096u, deviceProperties);
    auto unifiedMemoryGraphicsAllocation = svmAllocationsManager->getSVMAlloc(unifiedMemoryAllocation);
    auto graphicsAllocation = unifiedMemoryGraphicsAllocation->gpuAllocations.getDefaultGraphicsAllocation();

    auto heaplessStateInit = reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(&csr)->heaplessStateInitialized;
    kernel->makeResident(csr);
    EXPECT_EQ(heaplessStateInit ? 2u : 1u, graphicsAllocation->getResidencyTaskCount(csr.getOsContext().getContextId()));

    // Force to non-resident
    graphicsAllocation->updateResidencyTaskCount(GraphicsAllocation::objectNotResident, csr.getOsContext().getContextId());

    // Verify that makeResident is always called when allocation is not packed
    kernel->makeResident(csr);
    EXPECT_EQ(heaplessStateInit ? 2u : 1u, graphicsAllocation->getResidencyTaskCount(csr.getOsContext().getContextId()));

    svmAllocationsManager->freeSVMAlloc(unifiedMemoryAllocation);
}

struct KernelExecutionEnvironmentTest : public Test<ClDeviceFixture> {
    void SetUp() override {
        ClDeviceFixture::setUp();

        program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
        pKernelInfo = std::make_unique<KernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

        kernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, kernel->initialize());
    }

    void TearDown() override {
        delete kernel;
        program.reset();
        ClDeviceFixture::tearDown();
    }

    MockKernel *kernel;
    std::unique_ptr<MockProgram> program;
    std::unique_ptr<KernelInfo> pKernelInfo;
    SPatchExecutionEnvironment executionEnvironment = {};
};

TEST_F(KernelExecutionEnvironmentTest, GivenCompiledWorkGroupSizeIsZeroWhenGettingMaxRequiredWorkGroupSizeThenMaxWorkGroupSizeIsCorrect) {
    auto maxWorkGroupSize = static_cast<size_t>(pDevice->getDeviceInfo().maxWorkGroupSize);
    auto oldRequiredWorkGroupSizeX = this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0];
    auto oldRequiredWorkGroupSizeY = this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1];
    auto oldRequiredWorkGroupSizeZ = this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2];

    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = 0;
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = 0;
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = 0;

    EXPECT_EQ(maxWorkGroupSize, this->pKernelInfo->getMaxRequiredWorkGroupSize(maxWorkGroupSize));

    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = oldRequiredWorkGroupSizeX;
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = oldRequiredWorkGroupSizeY;
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = oldRequiredWorkGroupSizeZ;
}

TEST_F(KernelExecutionEnvironmentTest, GivenCompiledWorkGroupSizeLowerThanMaxWorkGroupSizeWhenGettingMaxRequiredWorkGroupSizeThenMaxWorkGroupSizeIsCorrect) {
    auto maxWorkGroupSize = static_cast<size_t>(pDevice->getDeviceInfo().maxWorkGroupSize);
    auto oldRequiredWorkGroupSizeX = this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0];
    auto oldRequiredWorkGroupSizeY = this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1];
    auto oldRequiredWorkGroupSizeZ = this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2];

    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = static_cast<uint16_t>(maxWorkGroupSize / 2);
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = 1;
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = 1;

    EXPECT_EQ(maxWorkGroupSize / 2, this->pKernelInfo->getMaxRequiredWorkGroupSize(maxWorkGroupSize));

    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = oldRequiredWorkGroupSizeX;
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = oldRequiredWorkGroupSizeY;
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = oldRequiredWorkGroupSizeZ;
}

TEST_F(KernelExecutionEnvironmentTest, GivenCompiledWorkGroupSizeIsGreaterThanMaxWorkGroupSizeWhenGettingMaxRequiredWorkGroupSizeThenMaxWorkGroupSizeIsCorrect) {
    auto maxWorkGroupSize = static_cast<size_t>(pDevice->getDeviceInfo().maxWorkGroupSize);
    auto oldRequiredWorkGroupSizeX = this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0];
    auto oldRequiredWorkGroupSizeY = this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1];
    auto oldRequiredWorkGroupSizeZ = this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2];

    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = static_cast<uint16_t>(maxWorkGroupSize);
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = static_cast<uint16_t>(maxWorkGroupSize);
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = static_cast<uint16_t>(maxWorkGroupSize);

    EXPECT_EQ(maxWorkGroupSize, this->pKernelInfo->getMaxRequiredWorkGroupSize(maxWorkGroupSize));

    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = oldRequiredWorkGroupSizeX;
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = oldRequiredWorkGroupSizeY;
    this->pKernelInfo->kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = oldRequiredWorkGroupSizeZ;
}

struct KernelCrossThreadTests : Test<ClDeviceFixture> {
    KernelCrossThreadTests() {
    }

    void SetUp() override {
        ClDeviceFixture::setUp();
        program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));

        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->setCrossThreadDataSize(64);
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    }

    void TearDown() override {
        program.reset();
        ClDeviceFixture::tearDown();
    }

    std::unique_ptr<MockProgram> program;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    SPatchExecutionEnvironment executionEnvironment = {};
};

TEST_F(KernelCrossThreadTests, WhenLocalWorkSize2OffsetsAreValidThenIsLocalWorkSize2PatchableReturnsTrue) {
    auto &localWorkSize2 = pKernelInfo->kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize2;
    localWorkSize2[0] = 0;
    localWorkSize2[1] = 4;
    localWorkSize2[2] = 8;

    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    EXPECT_TRUE(kernel.isLocalWorkSize2Patchable());
}

TEST_F(KernelCrossThreadTests, WhenNotAllLocalWorkSize2OffsetsAreValidThenIsLocalWorkSize2PatchableReturnsTrue) {
    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    auto &localWorkSize2 = pKernelInfo->kernelDescriptor.payloadMappings.dispatchTraits.localWorkSize2;
    for (auto ele0 : {true, false}) {
        for (auto ele1 : {true, false}) {
            for (auto ele2 : {true, false}) {
                if (ele0 && ele1 && ele2) {
                    continue;
                } else {
                    localWorkSize2[0] = ele0 ? 0 : undefined<CrossThreadDataOffset>;
                    localWorkSize2[1] = ele1 ? 4 : undefined<CrossThreadDataOffset>;
                    localWorkSize2[2] = ele2 ? 8 : undefined<CrossThreadDataOffset>;
                    EXPECT_FALSE(kernel.isLocalWorkSize2Patchable());
                }
            }
        }
    }
}

HWTEST_F(KernelCrossThreadTests, WhenKernelIsInitializedThenEnqueuedMaxWorkGroupSizeIsCorrect) {
    pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.maxWorkGroupSize = 12;

    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.maxWorkGroupSizeForCrossThreadData);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.maxWorkGroupSizeForCrossThreadData);
    EXPECT_EQ(static_cast<void *>(kernel.getCrossThreadData() + pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.maxWorkGroupSize), static_cast<void *>(kernel.maxWorkGroupSizeForCrossThreadData));
    EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize, *kernel.maxWorkGroupSizeForCrossThreadData);
    EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize, kernel.maxKernelWorkGroupSize);
}

TEST_F(KernelCrossThreadTests, WhenKernelIsInitializedThenDataParameterSimdSizeIsCorrect) {
    pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.simdSize = pClDevice->getGfxCoreHelper().getMinimalSIMDSize();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = pClDevice->getGfxCoreHelper().getMinimalSIMDSize();
    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.dataParameterSimdSize);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.dataParameterSimdSize);
    EXPECT_EQ(static_cast<void *>(kernel.getCrossThreadData() + pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.simdSize), static_cast<void *>(kernel.dataParameterSimdSize));
    EXPECT_EQ_VAL(pKernelInfo->getMaxSimdSize(), *kernel.dataParameterSimdSize);
}

TEST_F(KernelCrossThreadTests, GivenParentEventOffsetWhenKernelIsInitializedThenParentEventIsInitiatedWithUndefined) {
    pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueParentEvent = 16;
    MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_NE(nullptr, kernel.parentEventOffset);
    EXPECT_NE(&Kernel::dummyPatchLocation, kernel.parentEventOffset);
    EXPECT_EQ(static_cast<void *>(kernel.getCrossThreadData() + pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.deviceSideEnqueueParentEvent), static_cast<void *>(kernel.parentEventOffset));
    EXPECT_EQ(undefined<uint32_t>, *kernel.parentEventOffset);
}

TEST_F(KernelCrossThreadTests, WhenAddingKernelThenProgramRefCountIsIncremented) {

    auto refCount = program->getReference();
    MockKernel *kernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);
    auto refCount2 = program->getReference();
    EXPECT_EQ(refCount2, refCount + 1);

    delete kernel;
    auto refCount3 = program->getReference();
    EXPECT_EQ(refCount, refCount3);
}

TEST_F(KernelCrossThreadTests, GivenSlmStatisSizeWhenCreatingKernelThenSlmTotalSizeIsSet) {

    pKernelInfo->kernelDescriptor.kernelAttributes.slmInlineSize = 1024;

    MockKernel *kernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);

    EXPECT_EQ(1024u, kernel->slmTotalSize);

    delete kernel;
}
TEST_F(KernelCrossThreadTests, givenKernelWithPrivateMemoryWhenItIsCreatedThenCurbeIsPatchedProperly) {
    pKernelInfo->setPrivateMemory(1, false, 8, 0);

    MockKernel *kernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);

    kernel->initialize();

    auto privateSurface = kernel->privateSurface;

    auto constantBuffer = kernel->getCrossThreadData();
    auto privateAddress = (uintptr_t)privateSurface->getGpuAddressToPatch();
    auto ptrCurbe = (uint64_t *)constantBuffer;
    auto privateAddressFromCurbe = (uintptr_t)*ptrCurbe;

    EXPECT_EQ(privateAddressFromCurbe, privateAddress);

    delete kernel;
}

TEST_F(KernelCrossThreadTests, givenKernelWithPreferredWkgMultipleWhenItIsCreatedThenCurbeIsPatchedProperly) {

    pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.preferredWkgMultiple = 8;
    MockKernel *kernel = new MockKernel(program.get(), *pKernelInfo, *pClDevice);

    kernel->initialize();

    auto *crossThread = kernel->getCrossThreadData();

    uint32_t *preferredWkgMultipleOffset = (uint32_t *)ptrOffset(crossThread, 8);

    EXPECT_EQ(pKernelInfo->getMaxSimdSize(), *preferredWkgMultipleOffset);

    delete kernel;
}

TEST(KernelInfoTest, WhenPatchingBorderColorOffsetThenPatchIsAppliedCorrectly) {
    MockKernelInfo info;
    EXPECT_EQ(0u, info.getBorderColorOffset());

    info.setSamplerTable(3, 1, 0);
    EXPECT_EQ(3u, info.getBorderColorOffset());
}

TEST(KernelInfoTest, GivenArgNameWhenGettingArgNumberByNameThenCorrectValueIsReturned) {
    MockKernelInfo info;
    EXPECT_EQ(-1, info.getArgNumByName(""));

    info.addExtendedMetadata(0, "arg1");
    EXPECT_EQ(-1, info.getArgNumByName(""));
    EXPECT_EQ(-1, info.getArgNumByName("arg2"));
    EXPECT_EQ(0, info.getArgNumByName("arg1"));

    info.addExtendedMetadata(1, "arg2");
    EXPECT_EQ(0, info.getArgNumByName("arg1"));
    EXPECT_EQ(1, info.getArgNumByName("arg2"));

    info.kernelDescriptor.explicitArgsExtendedMetadata.clear();
    EXPECT_EQ(-1, info.getArgNumByName("arg1"));
}

TEST(KernelInfoTest, givenGfxCoreHelperWhenCreatingKernelAllocationThenCorrectPaddingIsAdded) {

    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get(), mockRootDeviceIndex));
    auto context = std::make_unique<MockContext>(clDevice.get());

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*clDevice, context.get());
    uint32_t kernelHeap = 0;
    mockKernel->kernelInfo.heapInfo.kernelHeapSize = 1;
    mockKernel->kernelInfo.heapInfo.pKernelHeap = &kernelHeap;
    mockKernel->kernelInfo.createKernelAllocation(clDevice->getDevice(), false);

    auto graphicsAllocation = mockKernel->kernelInfo.getGraphicsAllocation();
    auto &helper = clDevice->getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
    size_t isaPadding = helper.getPaddingForISAAllocation();
    EXPECT_EQ(graphicsAllocation->getUnderlyingBufferSize(), mockKernel->kernelInfo.heapInfo.kernelHeapSize + isaPadding);
    clDevice->getMemoryManager()->freeGraphicsMemory(mockKernel->kernelInfo.getGraphicsAllocation());
}

TEST(KernelTest, WhenSettingKernelArgThenBuiltinDispatchInfoBuilderIsUsed) {
    struct MockBuiltinDispatchBuilder : BuiltinDispatchInfoBuilder {
        using BuiltinDispatchInfoBuilder::BuiltinDispatchInfoBuilder;

        bool setExplicitArg(uint32_t argIndex, size_t argSize, const void *argVal, cl_int &err) const override {
            receivedArgs.push_back(std::make_tuple(argIndex, argSize, argVal));
            err = errToReturn;
            return valueToReturn;
        }

        bool valueToReturn = false;
        cl_int errToReturn = CL_SUCCESS;
        mutable std::vector<std::tuple<uint32_t, size_t, const void *>> receivedArgs;
    };

    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel(*device);
    kernel.mockKernel->initialize();
    kernel.mockKernel->kernelArguments.resize(2);

    MockBuiltinDispatchBuilder mockBuilder(*device->getBuiltIns(), *device);
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

HWTEST_F(KernelTest, givenKernelWhenDebugFlagToUseMaxSimdForCalculationsIsUsedThenMaxWorkgroupSizeIsSimdSizeDependant) {
    DebugManagerStateRestore dbgStateRestore;
    debugManager.flags.UseMaxSimdSizeToDeduceMaxWorkgroupSize.set(true);

    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;

    mySysInfo.EUCount = 24;
    mySysInfo.SubSliceCount = 3;
    mySysInfo.DualSubSliceCount = 3;
    mySysInfo.NumThreadsPerEu = 7;
    mySysInfo.ThreadCount = 24 * 7;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    MockKernelWithInternals kernel(*device);

    size_t maxKernelWkgSize;

    kernel.kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
    kernel.mockKernel->getWorkGroupInfo(CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &maxKernelWkgSize, nullptr);
    EXPECT_EQ(1024u, maxKernelWkgSize);

    kernel.kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 16;
    kernel.mockKernel->getWorkGroupInfo(CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &maxKernelWkgSize, nullptr);
    EXPECT_EQ(512u, maxKernelWkgSize);

    kernel.kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 8;
    kernel.mockKernel->getWorkGroupInfo(CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &maxKernelWkgSize, nullptr);
    EXPECT_EQ(256u, maxKernelWkgSize);
}

TEST(KernelTest, givenKernelWithKernelInfoWith32bitPointerSizeThenReport32bit) {
    KernelInfo info;
    info.kernelDescriptor.kernelAttributes.gpuPointerSize = 4;

    const auto rootDeviceIndex = 0u;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, rootDeviceIndex));
    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*device));
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, info, *device));

    EXPECT_TRUE(kernel->is32Bit());
}

TEST(KernelTest, givenKernelWithKernelInfoWith64bitPointerSizeThenReport64bit) {
    KernelInfo info;
    info.kernelDescriptor.kernelAttributes.gpuPointerSize = 8;

    const auto rootDeviceIndex = 0u;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, rootDeviceIndex));
    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*device));
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, info, *device));

    EXPECT_FALSE(kernel->is32Bit());
}

TEST(KernelTest, givenBuiltInProgramWhenCallingInitializeThenAuxTranslationRequiredIsFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    KernelInfo info{};

    ArgDescriptor argDescriptorPointer(ArgDescriptor::ArgType::argTPointer);
    argDescriptorPointer.as<ArgDescPointer>().accessedUsingStatelessAddressingMode = true;
    info.kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptorPointer);

    const auto rootDeviceIndex = 0u;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr, rootDeviceIndex));
    MockContext context(device.get());
    MockProgram program(&context, true, toClDeviceVector(*device));
    MockKernel kernel(&program, info, *device);

    kernel.initialize();
    EXPECT_FALSE(kernel.auxTranslationRequired);
}

TEST(KernelTest, givenFtrRenderCompressedBuffersWhenInitializingArgsWithNonStatefulAccessThenMarkKernelForAuxTranslation) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceAuxTranslationEnabled.set(1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &capabilityTable = hwInfo->capabilityTable;
    auto context = clUniquePtr(new MockContext(device.get()));
    context->contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    MockKernelWithInternals kernel(*device, context.get());
    kernel.kernelInfo.kernelDescriptor.kernelAttributes.crossThreadDataSize = 0;

    kernel.kernelInfo.addArgBuffer(0);
    kernel.kernelInfo.addExtendedMetadata(0, "", "char *");

    capabilityTable.ftrRenderCompressedBuffers = false;
    kernel.kernelInfo.setBufferStateful(0);
    kernel.mockKernel->initialize();
    EXPECT_FALSE(kernel.mockKernel->isAuxTranslationRequired());

    kernel.kernelInfo.setBufferStateful(0, false);
    kernel.mockKernel->initialize();
    EXPECT_FALSE(kernel.mockKernel->isAuxTranslationRequired());

    capabilityTable.ftrRenderCompressedBuffers = true;
    kernel.mockKernel->initialize();

    auto &rootDeviceEnvironment = device->getRootDeviceEnvironment();
    auto &clGfxCoreHelper = rootDeviceEnvironment.getHelper<ClGfxCoreHelper>();
    EXPECT_EQ(clGfxCoreHelper.requiresAuxResolves(kernel.kernelInfo), kernel.mockKernel->isAuxTranslationRequired());

    debugManager.flags.ForceAuxTranslationEnabled.set(-1);
    kernel.mockKernel->initialize();
    EXPECT_EQ(clGfxCoreHelper.requiresAuxResolves(kernel.kernelInfo), kernel.mockKernel->isAuxTranslationRequired());

    debugManager.flags.ForceAuxTranslationEnabled.set(0);
    kernel.mockKernel->initialize();
    EXPECT_FALSE(kernel.mockKernel->isAuxTranslationRequired());
}

TEST(KernelTest, WhenAuxTranslationIsRequiredThenKernelSetsRequiredResolvesInContext) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceAuxTranslationEnabled.set(1);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;

    auto context = clUniquePtr(new MockContext(device.get()));
    context->contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    MockKernelWithInternals kernel(*device, context.get());

    kernel.kernelInfo.addArgBuffer(0);
    kernel.kernelInfo.addExtendedMetadata(0, "", "char *");

    kernel.mockKernel->initialize();

    auto &rootDeviceEnvironment = device->getRootDeviceEnvironment();
    auto &clGfxCoreHelper = rootDeviceEnvironment.getHelper<ClGfxCoreHelper>();

    if (clGfxCoreHelper.requiresAuxResolves(kernel.kernelInfo)) {
        EXPECT_TRUE(context->getResolvesRequiredInKernels());
    } else {
        EXPECT_FALSE(context->getResolvesRequiredInKernels());
    }
}

TEST(KernelTest, WhenAuxTranslationIsNotRequiredThenKernelDoesNotSetRequiredResolvesInContext) {
    DebugManagerStateRestore restore;
    debugManager.flags.ForceAuxTranslationEnabled.set(0);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->capabilityTable.ftrRenderCompressedBuffers = true;

    auto context = clUniquePtr(new MockContext(device.get()));
    context->contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    MockKernelWithInternals kernel(*device, context.get());

    kernel.kernelInfo.addArgBuffer(0);
    kernel.kernelInfo.addExtendedMetadata(0, "", "char *");
    kernel.kernelInfo.setBufferStateful(0);

    kernel.mockKernel->initialize();
    EXPECT_FALSE(context->getResolvesRequiredInKernels());
}

TEST(KernelTest, givenDebugVariableSetWhenKernelHasStatefulBufferAccessThenMarkKernelForAuxTranslation) {
    DebugManagerStateRestore restore;
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);

    HardwareInfo localHwInfo = *defaultHwInfo;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    auto context = clUniquePtr(new MockContext(device.get()));
    MockKernelWithInternals kernel(*device, context.get());

    kernel.kernelInfo.addArgBuffer(0);
    kernel.kernelInfo.addExtendedMetadata(0, "", "char *");

    localHwInfo.capabilityTable.ftrRenderCompressedBuffers = false;

    kernel.mockKernel->initialize();

    auto &rootDeviceEnvironment = device->getRootDeviceEnvironment();
    auto &clGfxCoreHelper = rootDeviceEnvironment.getHelper<ClGfxCoreHelper>();

    if (clGfxCoreHelper.requiresAuxResolves(kernel.kernelInfo)) {
        EXPECT_TRUE(kernel.mockKernel->isAuxTranslationRequired());
    } else {
        EXPECT_FALSE(kernel.mockKernel->isAuxTranslationRequired());
    }
}

TEST(KernelTest, givenKernelWithPairArgumentWhenItIsInitializedThenPatchImmediateIsUsedAsArgHandler) {
    HardwareInfo localHwInfo = *defaultHwInfo;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
    auto context = clUniquePtr(new MockContext(device.get()));
    MockKernelWithInternals kernel(*device, context.get());

    kernel.kernelInfo.addExtendedMetadata(0, "", "pair<char*, int>");
    kernel.kernelInfo.kernelDescriptor.payloadMappings.explicitArgs.resize(1);

    kernel.mockKernel->initialize();
    EXPECT_EQ(&Kernel::setArgImmediate, kernel.mockKernel->kernelArgHandlers[0]);
}

TEST(KernelTest, givenKernelCompiledWithSimdSizeLowerThanExpectedWhenInitializingThenReturnError) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto minSimd = gfxCoreHelper.getMinimalSIMDSize();
    MockKernelWithInternals kernel(*device);
    kernel.kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 8;

    cl_int retVal = kernel.mockKernel->initialize();

    if (minSimd > 8) {
        EXPECT_EQ(CL_INVALID_KERNEL, retVal);
    } else {
        EXPECT_EQ(CL_SUCCESS, retVal);
    }
}

TEST(KernelTest, givenKernelCompiledWithSimdOneWhenInitializingThenReturnError) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel(*device);
    kernel.kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 1;

    cl_int retVal = kernel.mockKernel->initialize();

    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(KernelTest, givenKernelUsesPrivateMemoryWhenDeviceReleasedBeforeKernelThenKernelUsesMemoryManagerFromEnvironment) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    auto executionEnvironment = device->getExecutionEnvironment();

    auto mockKernel = std::make_unique<MockKernelWithInternals>(*device);
    GraphicsAllocation *privateSurface = device->getExecutionEnvironment()->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{device->getRootDeviceIndex(), MemoryConstants::pageSize});
    mockKernel->mockKernel->setPrivateSurface(privateSurface, 10);

    executionEnvironment->incRefInternal();
    mockKernel.reset(nullptr);
    executionEnvironment->decRefInternal();
}

TEST(KernelTest, givenAllArgumentsAreStatefulBuffersWhenInitializingThenAllBufferArgsStatefulIsTrue) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel{*device};

    kernel.kernelInfo.addArgBuffer(0);
    kernel.kernelInfo.setBufferStateful(0);
    kernel.kernelInfo.addArgBuffer(1);
    kernel.kernelInfo.setBufferStateful(1);

    kernel.mockKernel->initialize();
    EXPECT_TRUE(kernel.mockKernel->allBufferArgsStateful);
}

TEST(KernelTest, givenAllArgumentsAreBuffersButNotAllAreStatefulWhenInitializingThenAllBufferArgsStatefulIsFalse) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel{*device};

    kernel.kernelInfo.addArgBuffer(0);
    kernel.kernelInfo.setBufferStateful(0);
    kernel.kernelInfo.addArgBuffer(1);

    kernel.mockKernel->initialize();
    EXPECT_FALSE(kernel.mockKernel->allBufferArgsStateful);
}

TEST(KernelTest, givenNotAllArgumentsAreBuffersButAllBuffersAreStatefulWhenInitializingThenAllBufferArgsStatefulIsTrue) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel{*device};

    kernel.kernelInfo.addArgImage(0);
    kernel.kernelInfo.addArgBuffer(1);
    kernel.kernelInfo.setBufferStateful(1);

    kernel.mockKernel->initialize();
    EXPECT_TRUE(kernel.mockKernel->allBufferArgsStateful);
}

TEST(KernelTest, givenKernelRequiringTwoSlotScratchSpaceWhenGettingSizeForScratchSpaceThenCorrectSizeIsReturned) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));

    MockKernelWithInternals mockKernel(*device);
    EXPECT_EQ(0u, mockKernel.mockKernel->getScratchSize(0u));
    EXPECT_EQ(0u, mockKernel.mockKernel->getScratchSize(1u));
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[0] = 512u;
    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.perThreadScratchSize[1] = 1024u;
    EXPECT_EQ(512u, mockKernel.mockKernel->getScratchSize(0u));
    EXPECT_EQ(1024u, mockKernel.mockKernel->getScratchSize(1u));
}

TEST(KernelTest, givenKernelWithPatchInfoCollectionEnabledWhenPatchWithImplicitSurfaceCalledThenPatchInfoDataIsCollected) {
    DebugManagerStateRestore restore;
    debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel(*device);
    MockGraphicsAllocation mockAllocation;
    kernel.kernelInfo.addArgBuffer(0, 0, sizeof(void *));
    uint64_t crossThreadData = 0;
    EXPECT_EQ(0u, kernel.mockKernel->getPatchInfoDataList().size());
    kernel.mockKernel->patchWithImplicitSurface(castToUint64(&crossThreadData), mockAllocation, kernel.kernelInfo.argAsPtr(0));
    EXPECT_EQ(1u, kernel.mockKernel->getPatchInfoDataList().size());
}

TEST(KernelTest, givenKernelWithPatchInfoCollecitonEnabledAndArgumentWithInvalidCrossThreadDataOffsetWhenPatchWithImplicitSurfaceCalledThenPatchInfoDataIsNotCollected) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel(*device);
    MockGraphicsAllocation mockAllocation;
    kernel.kernelInfo.addArgBuffer(0, undefined<CrossThreadDataOffset>, sizeof(void *));
    uint64_t crossThreadData = 0;
    kernel.mockKernel->patchWithImplicitSurface(castToUint64(&crossThreadData), mockAllocation, kernel.kernelInfo.argAsPtr(0));
    EXPECT_EQ(0u, kernel.mockKernel->getPatchInfoDataList().size());
}

TEST(KernelTest, givenKernelWithPatchInfoCollectionEnabledAndValidArgumentWhenPatchWithImplicitSurfaceCalledThenPatchInfoDataIsCollected) {
    DebugManagerStateRestore restore;
    debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel(*device);
    MockGraphicsAllocation mockAllocation;
    kernel.kernelInfo.addArgBuffer(0, 0, sizeof(void *));
    uint64_t crossThreadData = 0;
    EXPECT_EQ(0u, kernel.mockKernel->getPatchInfoDataList().size());
    kernel.mockKernel->patchWithImplicitSurface(castToUint64(&crossThreadData), mockAllocation, kernel.kernelInfo.argAsPtr(0));
    EXPECT_EQ(1u, kernel.mockKernel->getPatchInfoDataList().size());
}

TEST(KernelTest, givenKernelWithPatchInfoCollectionDisabledWhenPatchWithImplicitSurfaceCalledThenPatchInfoDataIsNotCollected) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel(*device);
    MockGraphicsAllocation mockAllocation;
    kernel.kernelInfo.addArgBuffer(0, 0, sizeof(void *));
    uint64_t crossThreadData = 0;
    EXPECT_EQ(0u, kernel.mockKernel->getPatchInfoDataList().size());
    kernel.mockKernel->patchWithImplicitSurface(castToUint64(&crossThreadData), mockAllocation, kernel.kernelInfo.argAsPtr(0));
    EXPECT_EQ(0u, kernel.mockKernel->getPatchInfoDataList().size());
}

HWTEST_F(KernelTest, givenBindlessArgBufferWhenPatchWithImplicitSurfaceThenSurfaceStateIsEncodedAtProperOffset) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel(*device);

    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    MockGraphicsAllocation mockAllocation(const_cast<void *>(cpuPtr), gpuAddress, allocSize);

    kernel.kernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;

    const CrossThreadDataOffset bindlessOffset = 0x10;
    kernel.kernelInfo.addArgBuffer(0, 0, sizeof(void *), undefined<CrossThreadDataOffset>, bindlessOffset);

    kernel.kernelInfo.kernelDescriptor.initBindlessOffsetToSurfaceState();

    uint64_t crossThreadData = 0;
    kernel.mockKernel->patchWithImplicitSurface(castToUint64(&crossThreadData), mockAllocation, kernel.kernelInfo.argAsPtr(0));

    const auto &gfxCoreHelper = device->getGfxCoreHelper();
    const auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    const auto ssIndex = kernel.kernelInfo.kernelDescriptor.bindlessArgsMap.find(bindlessOffset)->second;
    const auto ssOffset = ssIndex * surfaceStateSize;

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    const auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ptrOffset(kernel.mockKernel->getSurfaceStateHeap(), ssOffset));
    const auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    const auto bufferAddress = mockAllocation.getGpuAddressToPatch();
    EXPECT_EQ(bufferAddress, surfaceAddress);
}

HWTEST_F(KernelTest, givenBindlessArgBufferAndNotInitializedBindlessOffsetToSurfaceStateWhenPatchWithImplicitSurfaceThenSurfaceStateIsNotEncoded) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel(*device);

    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    MockGraphicsAllocation mockAllocation(const_cast<void *>(cpuPtr), gpuAddress, allocSize);

    kernel.kernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;

    const CrossThreadDataOffset bindlessOffset = 0x10;
    kernel.kernelInfo.addArgBuffer(0, 0, sizeof(void *), undefined<CrossThreadDataOffset>, bindlessOffset);

    const auto surfaceStateHeap = kernel.mockKernel->getSurfaceStateHeap();
    const auto surfaceStateHeapSize = kernel.mockKernel->getSurfaceStateHeapSize();

    auto ssHeapDataInitial = std::make_unique<char[]>(surfaceStateHeapSize);
    std::memcpy(ssHeapDataInitial.get(), surfaceStateHeap, surfaceStateHeapSize);

    kernel.kernelInfo.kernelDescriptor.bindlessArgsMap.clear();

    uint64_t crossThreadData = 0;
    kernel.mockKernel->patchWithImplicitSurface(castToUint64(&crossThreadData), mockAllocation, kernel.kernelInfo.argAsPtr(0));

    EXPECT_EQ(0, std::memcmp(ssHeapDataInitial.get(), surfaceStateHeap, surfaceStateHeapSize));
}

HWTEST_F(KernelTest, givenBindlessHeapsHelperAndBindlessArgBufferWhenPatchWithImplicitSurfaceThenCrossThreadDataIsPatchedAndSurfaceStateIsEncoded) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    auto &neoDevice = device->getDevice();

    auto bindlessHeapHelper = new MockBindlesHeapsHelper(&neoDevice, false);
    neoDevice.getExecutionEnvironment()->rootDeviceEnvironments[neoDevice.getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapHelper);

    MockKernelWithInternals kernel(*device);

    uint64_t gpuAddress = 0x1200;
    const void *cpuPtr = reinterpret_cast<const void *>(gpuAddress);
    size_t allocSize = 0x1000;
    MockGraphicsAllocation mockAllocation(const_cast<void *>(cpuPtr), gpuAddress, allocSize);

    kernel.kernelInfo.kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::BindlessAndStateless;

    EXPECT_TRUE(device->getMemoryManager()->allocateBindlessSlot(&mockAllocation));

    const CrossThreadDataOffset bindlessOffset = 0x10;
    kernel.kernelInfo.addArgBuffer(0, 0, sizeof(void *), undefined<CrossThreadDataOffset>, bindlessOffset);

    kernel.kernelInfo.kernelDescriptor.initBindlessOffsetToSurfaceState();

    uint64_t crossThreadData = 0;
    kernel.mockKernel->patchWithImplicitSurface(castToUint64(&crossThreadData), mockAllocation, kernel.kernelInfo.argAsPtr(0));

    auto &ssInHeapInfo = mockAllocation.getBindlessInfo();

    auto patchLocation = reinterpret_cast<uint32_t *>(ptrOffset(kernel.mockKernel->crossThreadData, bindlessOffset));
    auto patchValue = device->getGfxCoreHelper().getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(ssInHeapInfo.surfaceStateOffset));

    EXPECT_EQ(patchValue, *patchLocation);

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    const auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(ssInHeapInfo.ssPtr);
    const auto surfaceAddress = surfaceState->getSurfaceBaseAddress();

    const auto bufferAddress = mockAllocation.getGpuAddressToPatch();
    EXPECT_EQ(bufferAddress, surfaceAddress);
}

TEST(KernelTest, givenDefaultKernelWhenItIsCreatedThenItReportsStatelessWrites) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel(*device);
    EXPECT_TRUE(kernel.mockKernel->areStatelessWritesUsed());
}

TEST(KernelTest, givenPolicyWhensetKernelThreadArbitrationPolicyThenExpectedClValueIsReturned) {

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    auto &clGfxCoreHelper = device->getRootDeviceEnvironment().getHelper<ClGfxCoreHelper>();
    if (!clGfxCoreHelper.isSupportedKernelThreadArbitrationPolicy()) {
        GTEST_SKIP();
    }

    MockKernelWithInternals kernel(*device);
    EXPECT_EQ(CL_SUCCESS, kernel.mockKernel->setKernelThreadArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL));
    EXPECT_EQ(CL_SUCCESS, kernel.mockKernel->setKernelThreadArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_OLDEST_FIRST_INTEL));
    EXPECT_EQ(CL_SUCCESS, kernel.mockKernel->setKernelThreadArbitrationPolicy(CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_AFTER_DEPENDENCY_ROUND_ROBIN_INTEL));
    uint32_t notExistPolicy = 0;
    EXPECT_EQ(CL_INVALID_VALUE, kernel.mockKernel->setKernelThreadArbitrationPolicy(notExistPolicy));
}

TEST(KernelTest, GivenDifferentValuesWhenSetKernelExecutionTypeIsCalledThenCorrectValueIsSet) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals mockKernelWithInternals(*device);
    auto &kernel = *mockKernelWithInternals.mockKernel;
    cl_int retVal;

    EXPECT_EQ(KernelExecutionType::defaultType, kernel.executionType);

    retVal = kernel.setKernelExecutionType(-1);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(KernelExecutionType::defaultType, kernel.executionType);

    retVal = kernel.setKernelExecutionType(CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(KernelExecutionType::concurrent, kernel.executionType);

    retVal = kernel.setKernelExecutionType(-1);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(KernelExecutionType::concurrent, kernel.executionType);

    retVal = kernel.setKernelExecutionType(CL_KERNEL_EXEC_INFO_DEFAULT_TYPE_INTEL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(KernelExecutionType::defaultType, kernel.executionType);
}

TEST(KernelTest, givenKernelLocalIdGenerationByRuntimeFalseWhenGettingStartOffsetThenOffsetToSkipPerThreadDataLoadIsAdded) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));

    MockKernelWithInternals mockKernel(*device);
    mockKernel.kernelInfo.setLocalIds({0, 0, 0});
    mockKernel.kernelInfo.kernelDescriptor.entryPoints.skipPerThreadDataLoad = 128;

    mockKernel.kernelInfo.createKernelAllocation(device->getDevice(), false);
    auto allocationOffset = mockKernel.kernelInfo.getGraphicsAllocation()->getGpuAddressToPatch();

    mockKernel.mockKernel->setStartOffset(128);
    auto offset = mockKernel.mockKernel->getKernelStartAddress(false, true, false, false);
    EXPECT_EQ(allocationOffset + 256u, offset);
    device->getMemoryManager()->freeGraphicsMemory(mockKernel.kernelInfo.getGraphicsAllocation());
}

TEST(KernelTest, givenFullAddressRequestWhenAskingForKernelStartAddressThenReturnFullAddress) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));

    MockKernelWithInternals mockKernel(*device);

    mockKernel.kernelInfo.createKernelAllocation(device->getDevice(), false);

    auto address = mockKernel.mockKernel->getKernelStartAddress(false, true, false, true);
    EXPECT_EQ(mockKernel.kernelInfo.getGraphicsAllocation()->getGpuAddress(), address);

    device->getMemoryManager()->freeGraphicsMemory(mockKernel.kernelInfo.getGraphicsAllocation());
}

TEST(KernelTest, givenKernelLocalIdGenerationByRuntimeTrueAndLocalIdsUsedWhenGettingStartOffsetThenOffsetToSkipPerThreadDataLoadIsNotAdded) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));

    MockKernelWithInternals mockKernel(*device);
    mockKernel.kernelInfo.setLocalIds({0, 0, 0});
    mockKernel.kernelInfo.kernelDescriptor.entryPoints.skipPerThreadDataLoad = 128;

    mockKernel.kernelInfo.createKernelAllocation(device->getDevice(), false);
    auto allocationOffset = mockKernel.kernelInfo.getGraphicsAllocation()->getGpuAddressToPatch();

    mockKernel.mockKernel->setStartOffset(128);
    auto offset = mockKernel.mockKernel->getKernelStartAddress(true, true, false, false);
    EXPECT_EQ(allocationOffset + 128u, offset);
    device->getMemoryManager()->freeGraphicsMemory(mockKernel.kernelInfo.getGraphicsAllocation());
}

TEST(KernelTest, givenKernelLocalIdGenerationByRuntimeFalseAndLocalIdsNotUsedWhenGettingStartOffsetThenOffsetToSkipPerThreadDataLoadIsNotAdded) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));

    MockKernelWithInternals mockKernel(*device);
    mockKernel.kernelInfo.setLocalIds({0, 0, 0});
    mockKernel.kernelInfo.kernelDescriptor.entryPoints.skipPerThreadDataLoad = 128;

    mockKernel.kernelInfo.createKernelAllocation(device->getDevice(), false);
    auto allocationOffset = mockKernel.kernelInfo.getGraphicsAllocation()->getGpuAddressToPatch();

    mockKernel.mockKernel->setStartOffset(128);
    auto offset = mockKernel.mockKernel->getKernelStartAddress(false, false, false, false);
    EXPECT_EQ(allocationOffset + 128u, offset);
    device->getMemoryManager()->freeGraphicsMemory(mockKernel.kernelInfo.getGraphicsAllocation());
}

TEST(KernelTest, whenKernelIsInitializedThenThreadArbitrationPolicyIsSetToDefaultValue) {
    UltClDeviceFactory deviceFactory{1, 0};

    SPatchExecutionEnvironment sPatchExecEnv = {};
    MockKernelWithInternals mockKernelWithInternals{*deviceFactory.rootDevices[0], sPatchExecEnv};

    auto &mockKernel = *mockKernelWithInternals.mockKernel;
    auto &gfxCoreHelper = deviceFactory.rootDevices[0]->getGfxCoreHelper();
    EXPECT_EQ(gfxCoreHelper.getDefaultThreadArbitrationPolicy(), mockKernel.getDescriptor().kernelAttributes.threadArbitrationPolicy);
}

using ThreadArbitrationPolicyKernelTest = ::testing::TestWithParam<ThreadArbitrationPolicy>;
TEST_P(ThreadArbitrationPolicyKernelTest, givenThreadArbitrationPolicyAndIFPRequiredWhenKernelIsInitializedThenThreadArbitrationPolicyIsSetToRoundRobinPolicy) {
    struct MockKernelWithTAP : public MockKernelWithInternals {
        MockKernelWithTAP(ClDevice &deviceArg, SPatchExecutionEnvironment execEnv) : MockKernelWithInternals(deviceArg, nullptr, false, execEnv) {
            kernelInfo.kernelDescriptor.kernelAttributes.threadArbitrationPolicy = GetParam();
            mockKernel->initialize();
        }
    };
    UltClDeviceFactory deviceFactory{1, 0};

    SPatchExecutionEnvironment sPatchExecEnv = {};
    sPatchExecEnv.SubgroupIndependentForwardProgressRequired = true;
    MockKernelWithTAP mockKernelWithTAP{*deviceFactory.rootDevices[0], sPatchExecEnv};

    auto &mockKernel = *mockKernelWithTAP.mockKernel;
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobin, mockKernel.getDescriptor().kernelAttributes.threadArbitrationPolicy);
}

TEST_P(ThreadArbitrationPolicyKernelTest, givenThreadArbitrationPolicyAndIFPNotRequiredWhenKernelIsInitializedThenThreadArbitrationPolicyIsSetCorrectly) {
    struct MockKernelWithTAP : public MockKernelWithInternals {
        MockKernelWithTAP(ClDevice &deviceArg, SPatchExecutionEnvironment execEnv) : MockKernelWithInternals(deviceArg, nullptr, false, execEnv) {
            kernelInfo.kernelDescriptor.kernelAttributes.threadArbitrationPolicy = GetParam();
            mockKernel->initialize();
        }
    };
    UltClDeviceFactory deviceFactory{1, 0};

    SPatchExecutionEnvironment sPatchExecEnv = {};
    sPatchExecEnv.SubgroupIndependentForwardProgressRequired = false;
    MockKernelWithTAP mockKernelWithTAP{*deviceFactory.rootDevices[0], sPatchExecEnv};

    auto &mockKernel = *mockKernelWithTAP.mockKernel;
    EXPECT_EQ(GetParam(), mockKernel.getDescriptor().kernelAttributes.threadArbitrationPolicy);
}

static ThreadArbitrationPolicy threadArbitrationPolicies[] = {
    ThreadArbitrationPolicy::AgeBased,
    ThreadArbitrationPolicy::RoundRobin,
    ThreadArbitrationPolicy::RoundRobinAfterDependency};

INSTANTIATE_TEST_SUITE_P(ThreadArbitrationPolicyKernelInitializationTests,
                         ThreadArbitrationPolicyKernelTest,
                         testing::ValuesIn(threadArbitrationPolicies));

TEST(KernelTest, givenKernelWhenSettingAdditionalKernelExecInfoThenCorrectValueIsSet) {
    UltClDeviceFactory deviceFactory{1, 0};
    MockKernelWithInternals mockKernelWithInternals{*deviceFactory.rootDevices[0]};
    mockKernelWithInternals.kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress = true;
    EXPECT_TRUE(mockKernelWithInternals.kernelInfo.requiresSubgroupIndependentForwardProgress());

    auto &mockKernel = *mockKernelWithInternals.mockKernel;

    mockKernel.setAdditionalKernelExecInfo(123u);
    EXPECT_EQ(123u, mockKernel.getAdditionalKernelExecInfo());
    mockKernel.setAdditionalKernelExecInfo(AdditionalKernelExecInfo::notApplicable);
    EXPECT_EQ(AdditionalKernelExecInfo::notApplicable, mockKernel.getAdditionalKernelExecInfo());
}

using KernelMultiRootDeviceTest = MultiRootDeviceFixture;

TEST_F(KernelMultiRootDeviceTest, givenKernelWithPrivateSurfaceWhenInitializeThenPrivateSurfacesHaveCorrectRootDeviceIndex) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->setPrivateMemory(112, false, 8, 40, 64);

    KernelInfoContainer kernelInfos;
    kernelInfos.resize(deviceFactory->rootDevices.size());
    for (auto &rootDeviceIndex : context->getRootDeviceIndices()) {
        kernelInfos[rootDeviceIndex] = pKernelInfo.get();
    }

    MockProgram program(context.get(), false, context->getDevices());

    int32_t retVal = CL_INVALID_VALUE;
    auto pMultiDeviceKernel = std::unique_ptr<MultiDeviceKernel>(MultiDeviceKernel::create<MockKernel>(&program, kernelInfos, retVal));

    EXPECT_EQ(CL_SUCCESS, retVal);

    for (auto &rootDeviceIndex : context->getRootDeviceIndices()) {
        auto kernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
        auto privateSurface = kernel->privateSurface;
        ASSERT_NE(nullptr, privateSurface);
        EXPECT_EQ(rootDeviceIndex, privateSurface->getRootDeviceIndex());
    }
}

class KernelCreateTest : public ::testing::Test {
  protected:
    struct MockProgram {
        ClDeviceVector getDevices() {
            ClDeviceVector deviceVector;
            deviceVector.push_back(&mDevice);
            return deviceVector;
        }
        void getSource(std::string &) {}
        MockClDevice mDevice{new MockDevice};
    };

    struct MockKernel {
        MockKernel(MockProgram *, const KernelInfo &, ClDevice &) {}
        int initialize() { return -1; };
        uint32_t getSlmTotalSize() const { return 0u; };
    };

    MockProgram mockProgram{};
};

TEST_F(KernelCreateTest, whenInitFailedThenReturnNull) {
    KernelInfo info{};
    info.kernelDescriptor.kernelAttributes.gpuPointerSize = 8;

    cl_int retVal{CL_SUCCESS};
    auto ret = Kernel::create<MockKernel>(&mockProgram, info, mockProgram.mDevice, retVal);
    EXPECT_EQ(nullptr, ret);
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST(KernelInitializationTest, givenSlmSizeExceedingLocalMemorySizeWhenInitializingKernelThenDebugMsgErrIsPrintedAndOutOfResourcesIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.PrintDebugMessages.set(true);

    StreamCapture capture;
    capture.captureStderr();

    MockContext context;
    MockProgram mockProgram(&context, false, context.getDevices());
    auto clDevice = context.getDevice(0);
    auto deviceLocalMemSize = static_cast<uint32_t>(clDevice->getDevice().getDeviceInfo().localMemSize);

    MockKernelInfo mockKernelInfoExceedsSLM{};
    mockKernelInfoExceedsSLM.kernelDescriptor.kernelAttributes.simdSize = 1u;

    auto slmTotalSize = deviceLocalMemSize + 10u;
    mockKernelInfoExceedsSLM.kernelDescriptor.kernelAttributes.slmInlineSize = slmTotalSize;
    auto localMemSize = static_cast<uint32_t>(clDevice->getDevice().getDeviceInfo().localMemSize);

    std::string output = capture.getCapturedStderr();

    cl_int retVal{};
    capture.captureStderr();
    std::unique_ptr<MockKernel> kernelPtr(Kernel::create<MockKernel>(&mockProgram, mockKernelInfoExceedsSLM, *clDevice, retVal));
    EXPECT_EQ(nullptr, kernelPtr.get());
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);

    output = capture.getCapturedStderr();
    std::string expectedOutput = "Size of SLM (" + std::to_string(slmTotalSize) + ") larger than available (" + std::to_string(localMemSize) + ")\n";
    EXPECT_EQ(expectedOutput, output);
}

TEST(MultiDeviceKernelCreateTest, whenInitFailedThenReturnNullAndPropagateErrorCode) {
    MockContext context;
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 0;

    KernelInfoContainer kernelInfos;
    kernelInfos.push_back(pKernelInfo.get());

    MockProgram program(&context, false, context.getDevices());

    int32_t retVal = CL_SUCCESS;
    auto pMultiDeviceKernel = MultiDeviceKernel::create<MockKernel>(&program, kernelInfos, retVal);

    EXPECT_EQ(nullptr, pMultiDeviceKernel);
    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST(ArgTypeTraits, GivenDefaultInitializedArgTypeMetadataThenAddressSpaceIsGlobal) {
    ArgTypeTraits metadata;
    EXPECT_EQ(NEO::KernelArgMetadata::AddrGlobal, metadata.addressQualifier);
}
TEST_F(KernelTests, givenKernelWithSimdGreaterThan1WhenKernelCreatedThenMaxWorgGroupSizeEqualDeviceProperty) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    std::unique_ptr<MockKernel> kernel(new MockKernel(pProgram, *pKernelInfo, *pClDevice));

    auto kernelMaxWorkGroupSize = pDevice->getDeviceInfo().maxWorkGroupSize;
    EXPECT_EQ(kernel->getMaxKernelWorkGroupSize(), kernelMaxWorkGroupSize);
}

TEST_F(KernelTests, givenKernelWithSimdEqual1WhenKernelCreatedThenMaxWorgGroupSizeExualMaxHwThreadsPerWG) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    std::unique_ptr<MockKernel> kernel(new MockKernel(pProgram, *pKernelInfo, *pClDevice));

    auto deviceMaxWorkGroupSize = pDevice->getDeviceInfo().maxWorkGroupSize;
    auto deviceInfo = pClDevice->getDevice().getDeviceInfo();

    auto &productHelper = pClDevice->getProductHelper();
    auto maxThreadsPerWG = productHelper.getMaxThreadsForWorkgroupInDSSOrSS(kernel->getHardwareInfo(), static_cast<uint32_t>(deviceInfo.maxNumEUsPerSubSlice), static_cast<uint32_t>(deviceInfo.maxNumEUsPerDualSubSlice));

    EXPECT_LT(kernel->getMaxKernelWorkGroupSize(), deviceMaxWorkGroupSize);
    EXPECT_EQ(kernel->getMaxKernelWorkGroupSize(), maxThreadsPerWG);
}

struct KernelLargeGrfTests : Test<ClDeviceFixture> {
    void SetUp() override {
        ClDeviceFixture::setUp();
        program = std::make_unique<MockProgram>(toClDeviceVector(*pClDevice));
        pKernelInfo = std::make_unique<KernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize = 64;
    }

    void TearDown() override {
        pKernelInfo.reset();
        program.reset();
        ClDeviceFixture::tearDown();
    }

    std::unique_ptr<MockProgram> program;
    std::unique_ptr<KernelInfo> pKernelInfo;
    SPatchExecutionEnvironment executionEnvironment = {};
};

HWTEST2_F(KernelLargeGrfTests, GivenLargeGrfAndSimdSizeWhenGettingMaxWorkGroupSizeThenCorrectValueReturned, IsAtLeastXeCore) {
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = pClDevice->getGfxCoreHelper().getMinimalSIMDSize();
    pKernelInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize = 4;
    pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.maxWorkGroupSize = 0;
    {
        MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);

        pKernelInfo->kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
        EXPECT_EQ(CL_SUCCESS, kernel.initialize());
        EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize, *kernel.maxWorkGroupSizeForCrossThreadData);
        EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize, kernel.maxKernelWorkGroupSize);
    }

    {
        MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);

        pKernelInfo->kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;
        EXPECT_EQ(CL_SUCCESS, kernel.initialize());
        if (pKernelInfo->kernelDescriptor.kernelAttributes.simdSize != 32) {

            EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize >> 1, *kernel.maxWorkGroupSizeForCrossThreadData);
            EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize >> 1, kernel.maxKernelWorkGroupSize);
        }
    }

    {
        MockKernel kernel(program.get(), *pKernelInfo, *pClDevice);
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
        pKernelInfo->kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::largeGrfNumber;
        EXPECT_EQ(CL_SUCCESS, kernel.initialize());
        EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize, *kernel.maxWorkGroupSizeForCrossThreadData);
        EXPECT_EQ(pDevice->getDeviceInfo().maxWorkGroupSize, kernel.maxKernelWorkGroupSize);
    }
}

HWTEST2_F(KernelConstantSurfaceTest, givenKernelWithConstantSurfaceWhenKernelIsCreatedThenConstantMemorySurfaceStateIsPatchedWithMocs, IsAtLeastXeCore) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

    pKernelInfo->setGlobalConstantsSurface(8, 0, 0);

    char buffer[MemoryConstants::pageSize64k];
    auto gmmHelper = pDevice->getGmmHelper();
    auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(buffer));
    GraphicsAllocation gfxAlloc(0, 1u /*num gmms*/, AllocationType::constantSurface, buffer, MemoryConstants::pageSize64k,
                                static_cast<osHandle>(8), MemoryPool::memoryNull, MemoryManager::maxOsContextCount, canonizedGpuAddress);

    MockContext context(pClDevice);
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    program.setConstantSurface(&gfxAlloc);

    // create kernel
    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *pKernelInfo, *pClDevice));

    // setup surface state heap
    char surfaceStateHeap[0x80];
    pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;

    ASSERT_EQ(CL_SUCCESS, kernel->initialize());

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(kernel->getSurfaceStateHeap(),
                  pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindful));
    auto actualMocs = surfaceState->getMemoryObjectControlState();
    const auto expectedMocs = context.getDevice(0)->getGmmHelper()->getL1EnabledMOCS();

    EXPECT_EQ(expectedMocs, actualMocs);

    program.setConstantSurface(nullptr);
}

using KernelImplicitArgsTest = Test<ClDeviceFixture>;
TEST_F(KernelImplicitArgsTest, WhenKernelRequiresImplicitArgsThenImplicitArgsStructIsCreatedAndProperlyInitialized) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = false;

    MockContext context(pClDevice);
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    {
        MockKernel kernel(&program, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, kernel.initialize());
        EXPECT_EQ(nullptr, kernel.getImplicitArgs());
    }
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    {
        MockKernel kernel(&program, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, kernel.initialize());
        auto pImplicitArgs = kernel.getImplicitArgs();

        ASSERT_NE(nullptr, pImplicitArgs);

        ImplicitArgs expectedImplicitArgs = {};
        if (pClDevice->getGfxCoreHelper().getImplicitArgsVersion() == 0) {
            expectedImplicitArgs.v0.header.structVersion = 0;
            expectedImplicitArgs.v0.header.structSize = ImplicitArgsV0::getSize();
        } else if (pClDevice->getGfxCoreHelper().getImplicitArgsVersion() == 1) {
            expectedImplicitArgs.v1.header.structVersion = 1;
            expectedImplicitArgs.v1.header.structSize = ImplicitArgsV1::getSize();
        } else if (pClDevice->getGfxCoreHelper().getImplicitArgsVersion() == 2) {
            expectedImplicitArgs.v2.header.structVersion = 2;
            expectedImplicitArgs.v2.header.structSize = ImplicitArgsV2::getSize();
        }
        expectedImplicitArgs.setSimdWidth(32);

        EXPECT_EQ(0, memcmp(&expectedImplicitArgs, pImplicitArgs, pImplicitArgs->getSize()));
    }
}

TEST_F(KernelImplicitArgsTest, GivenProgramWithImplicitAccessBufferVersionWhenKernelCreatedThenCorrectVersionIsSet) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;

    MockContext context(pClDevice);
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    program.indirectAccessBufferMajorVersion = 5;
    {
        MockKernel kernel(&program, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, kernel.initialize());
        EXPECT_EQ(5u, kernel.implicitArgsVersion);
    }
}

TEST_F(KernelImplicitArgsTest, GivenKernelWithSyncBufferWhenInitializingKernelThenImplicitArgsSyncBufferPtrIsSet) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;

    MockContext context(pClDevice);
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    program.indirectAccessBufferMajorVersion = 1;

    pKernelInfo->kernelDescriptor.kernelMetadata.kernelName = "test";
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.usesSyncBuffer = true;
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;
    pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.stateless = 0;
    pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.syncBufferAddress.pointerSize = sizeof(uintptr_t);

    MockKernel kernel(&program, *pKernelInfo, *pClDevice);
    kernel.initialize();
    kernel.setCrossThreadData(nullptr, 64);

    NEO::MockGraphicsAllocation alloc;
    alloc.setGpuPtr(0xffff800300060000);
    alloc.allocationOffset = 0x0;

    size_t bufferOffset = 0u;

    kernel.patchSyncBuffer(&alloc, bufferOffset);

    auto implicitArgs = kernel.getImplicitArgs();
    ASSERT_NE(nullptr, implicitArgs);

    auto syncBufferAddress = alloc.getGpuAddress();
    EXPECT_EQ(syncBufferAddress, implicitArgs->v1.syncBufferPtr);
}

TEST_F(KernelImplicitArgsTest, givenKernelWithImplicitArgsWhenSettingKernelParamsThenImplicitArgsAreProperlySet) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;

    MockContext context(pClDevice);
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));

    MockKernel kernel(&program, *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());
    auto pImplicitArgs = kernel.getImplicitArgs();

    ASSERT_NE(nullptr, pImplicitArgs);

    ImplicitArgs expectedImplicitArgs = {};
    if (pClDevice->getGfxCoreHelper().getImplicitArgsVersion() == 0) {
        expectedImplicitArgs.v0.header.structVersion = 0;
        expectedImplicitArgs.v0.header.structSize = ImplicitArgsV0::getSize();
    } else if (pClDevice->getGfxCoreHelper().getImplicitArgsVersion() == 1) {
        expectedImplicitArgs.v1.header.structVersion = 1;
        expectedImplicitArgs.v1.header.structSize = ImplicitArgsV1::getSize();
    } else if (pClDevice->getGfxCoreHelper().getImplicitArgsVersion() == 2) {
        expectedImplicitArgs.v2.header.structVersion = 2;
        expectedImplicitArgs.v2.header.structSize = ImplicitArgsV2::getSize();
    }

    expectedImplicitArgs.setNumWorkDim(3);
    expectedImplicitArgs.setSimdWidth(32);
    expectedImplicitArgs.setLocalSize(4, 5, 6);
    expectedImplicitArgs.setGlobalSize(7, 8, 9);
    expectedImplicitArgs.setGlobalOffset(1, 2, 3);
    expectedImplicitArgs.setGroupCount(3, 2, 1);

    kernel.setWorkDim(3);
    kernel.setLocalWorkSizeValues(4, 5, 6);
    kernel.setGlobalWorkSizeValues(7, 8, 9);
    kernel.setGlobalWorkOffsetValues(1, 2, 3);
    kernel.setNumWorkGroupsValues(3, 2, 1);

    EXPECT_EQ(0, memcmp(&expectedImplicitArgs, pImplicitArgs, ImplicitArgsV0::getSize()));
}

HWTEST_F(KernelImplicitArgsTest, givenGfxCoreRequiringImplicitArgsV1WhenSettingKernelParamsThenImplicitArgsAreProperlySet) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;

    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        uint32_t getImplicitArgsVersion() const override {
            return 1;
        }
    };

    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*pClDevice->getDevice().getExecutionEnvironment()->rootDeviceEnvironments[0]);

    MockContext context(pClDevice);
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));

    MockKernel kernel(&program, *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());
    auto pImplicitArgs = kernel.getImplicitArgs();

    ASSERT_NE(nullptr, pImplicitArgs);

    ImplicitArgsV1 expectedImplicitArgs = {{ImplicitArgsV1::getSize(), 1}};
    expectedImplicitArgs.simdWidth = kernel.getDescriptor().kernelAttributes.simdSize;
    expectedImplicitArgs.numWorkDim = 3;
    expectedImplicitArgs.localSizeX = 4;
    expectedImplicitArgs.localSizeY = 5;
    expectedImplicitArgs.localSizeZ = 6;
    expectedImplicitArgs.globalSizeX = 7;
    expectedImplicitArgs.globalSizeY = 8;
    expectedImplicitArgs.globalSizeZ = 9;
    expectedImplicitArgs.globalOffsetX = 1;
    expectedImplicitArgs.globalOffsetY = 2;
    expectedImplicitArgs.globalOffsetZ = 3;
    expectedImplicitArgs.groupCountX = 3;
    expectedImplicitArgs.groupCountY = 2;
    expectedImplicitArgs.groupCountZ = 1;

    kernel.setWorkDim(3);
    kernel.setLocalWorkSizeValues(4, 5, 6);
    kernel.setGlobalWorkSizeValues(7, 8, 9);
    kernel.setGlobalWorkOffsetValues(1, 2, 3);
    kernel.setNumWorkGroupsValues(3, 2, 1);

    EXPECT_EQ(0, memcmp(&expectedImplicitArgs, pImplicitArgs, ImplicitArgsV1::getSize()));
}

TEST_F(KernelImplicitArgsTest, givenKernelWithImplicitArgsWhenCloneKernelThenImplicitArgsAreCopied) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = true;

    MockContext context(pClDevice);
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));

    MockKernel kernel(&program, *pKernelInfo, *pClDevice);
    MockKernel kernel2(&program, *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());
    ASSERT_EQ(CL_SUCCESS, kernel2.initialize());

    ImplicitArgs expectedImplicitArgs = {};
    if (pClDevice->getGfxCoreHelper().getImplicitArgsVersion() == 0) {
        expectedImplicitArgs.v0.header.structVersion = 0;
        expectedImplicitArgs.v0.header.structSize = ImplicitArgsV0::getSize();
    } else if (pClDevice->getGfxCoreHelper().getImplicitArgsVersion() == 1) {
        expectedImplicitArgs.v1.header.structVersion = 1;
        expectedImplicitArgs.v1.header.structSize = ImplicitArgsV1::getSize();
    } else if (pClDevice->getGfxCoreHelper().getImplicitArgsVersion() == 2) {
        expectedImplicitArgs.v2.header.structVersion = 2;
        expectedImplicitArgs.v2.header.structSize = ImplicitArgsV2::getSize();
    }

    expectedImplicitArgs.setNumWorkDim(3);
    expectedImplicitArgs.setSimdWidth(32);
    expectedImplicitArgs.setLocalSize(4, 5, 6);
    expectedImplicitArgs.setGlobalSize(7, 8, 9);
    expectedImplicitArgs.setGlobalOffset(1, 2, 3);
    expectedImplicitArgs.setGroupCount(3, 2, 1);

    kernel.setWorkDim(3);
    kernel.setLocalWorkSizeValues(4, 5, 6);
    kernel.setGlobalWorkSizeValues(7, 8, 9);
    kernel.setGlobalWorkOffsetValues(1, 2, 3);
    kernel.setNumWorkGroupsValues(3, 2, 1);

    ASSERT_EQ(CL_SUCCESS, kernel2.cloneKernel(&kernel));

    auto pImplicitArgs = kernel2.getImplicitArgs();

    ASSERT_NE(nullptr, pImplicitArgs);

    EXPECT_EQ(0, memcmp(&expectedImplicitArgs, pImplicitArgs, pImplicitArgs->getSize()));
}

TEST_F(KernelImplicitArgsTest, givenKernelWithoutImplicitArgsWhenSettingKernelParamsThenImplicitArgsAreNotSet) {
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;
    pKernelInfo->kernelDescriptor.kernelAttributes.flags.requiresImplicitArgs = false;

    MockContext context(pClDevice);
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));

    MockKernel kernel(&program, *pKernelInfo, *pClDevice);
    ASSERT_EQ(CL_SUCCESS, kernel.initialize());

    EXPECT_EQ(nullptr, kernel.getImplicitArgs());

    kernel.setWorkDim(3);
    kernel.setLocalWorkSizeValues(4, 5, 6);
    kernel.setGlobalWorkSizeValues(7, 8, 9);
    kernel.setGlobalWorkOffsetValues(1, 2, 3);
    kernel.setNumWorkGroupsValues(3, 2, 1);

    EXPECT_EQ(nullptr, kernel.getImplicitArgs());
}

TEST_F(KernelTests, GivenCorrectAllocationTypeThenFunctionCheckingSystemMemoryReturnsTrue) {
    std::vector<NEO::AllocationType> systemMemoryAllocationType = {
        NEO::AllocationType::bufferHostMemory,
        NEO::AllocationType::externalHostPtr,
        NEO::AllocationType::svmCpu,
        NEO::AllocationType::svmZeroCopy};

    for (uint32_t allocationTypeIndex = static_cast<uint32_t>(NEO::AllocationType::unknown);
         allocationTypeIndex < static_cast<uint32_t>(NEO::AllocationType::count);
         allocationTypeIndex++) {
        auto currentAllocationType = static_cast<NEO::AllocationType>(allocationTypeIndex);
        bool ret = Kernel::graphicsAllocationTypeUseSystemMemory(currentAllocationType);
        if (std::find(systemMemoryAllocationType.begin(),
                      systemMemoryAllocationType.end(),
                      currentAllocationType) != systemMemoryAllocationType.end()) {
            EXPECT_TRUE(ret);
        } else {
            EXPECT_FALSE(ret);
        }
    }
}

TEST(KernelTest, givenKernelWithNumThreadsRequiredPatchTokenWhenQueryingEuThreadCountThenEuThreadCountIsReturned) {
    cl_int retVal = CL_SUCCESS;
    KernelInfo kernelInfo = {};

    kernelInfo.kernelDescriptor.kernelAttributes.numThreadsRequired = 123U;
    auto rootDeviceIndex = 0u;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(NEO::defaultHwInfo.get(), rootDeviceIndex));
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*device));
    MockKernel kernel(program.get(), kernelInfo, *device);

    cl_uint euThreadCount;
    size_t paramRetSize;
    retVal = kernel.getWorkGroupInfo(CL_KERNEL_EU_THREAD_COUNT_INTEL, sizeof(cl_uint), &euThreadCount, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint), paramRetSize);
    EXPECT_EQ(123U, euThreadCount);
}

TEST(KernelTest, givenKernelWithNumGRFRequiredPatchTokenWhenQueryingRegisterCountThenRegisterCountIsReturned) {
    cl_int retVal = CL_SUCCESS;
    KernelInfo kernelInfo = {};

    kernelInfo.kernelDescriptor.kernelAttributes.numGrfRequired = 213U;
    auto rootDeviceIndex = 0u;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(NEO::defaultHwInfo.get(), rootDeviceIndex));
    auto program = std::make_unique<MockProgram>(toClDeviceVector(*device));
    MockKernel kernel(program.get(), kernelInfo, *device);

    cl_uint regCount;
    size_t paramRetSize;
    retVal = kernel.getWorkGroupInfo(CL_KERNEL_REGISTER_COUNT_INTEL, sizeof(cl_uint), &regCount, &paramRetSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_uint), paramRetSize);
    EXPECT_EQ(213U, regCount);
}

HWTEST2_F(KernelTest, GivenInlineSamplersWhenSettingInlineSamplerThenDshIsPatched, SupportsSampler) {
    auto device = clUniquePtr(new MockClDevice(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())));
    MockKernelWithInternals kernel(*device);

    auto &inlineSampler = kernel.kernelInfo.kernelDescriptor.inlineSamplers.emplace_back();
    inlineSampler.addrMode = NEO::KernelDescriptor::InlineSampler::AddrMode::repeat;
    inlineSampler.filterMode = NEO::KernelDescriptor::InlineSampler::FilterMode::nearest;
    inlineSampler.isNormalized = false;

    using SamplerState = typename FamilyType::SAMPLER_STATE;
    std::array<uint8_t, 64 + sizeof(SamplerState)> dsh = {0};
    kernel.kernelInfo.heapInfo.pDsh = dsh.data();
    kernel.kernelInfo.heapInfo.dynamicStateHeapSize = static_cast<uint32_t>(dsh.size());
    kernel.mockKernel->setInlineSamplers();

    auto samplerState = reinterpret_cast<const SamplerState *>(dsh.data() + 64U);
    EXPECT_TRUE(samplerState->getNonNormalizedCoordinateEnable());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_WRAP, samplerState->getTcxAddressControlMode());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_WRAP, samplerState->getTcyAddressControlMode());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_WRAP, samplerState->getTczAddressControlMode());
    EXPECT_EQ(SamplerState::MIN_MODE_FILTER_NEAREST, samplerState->getMinModeFilter());
    EXPECT_EQ(SamplerState::MAG_MODE_FILTER_NEAREST, samplerState->getMagModeFilter());
}

TEST(KernelTest, whenCallingGetEnqueuedLocalWorkSizeValuesThenReturnProperValuesFromKernelDescriptor) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(NEO::defaultHwInfo.get()));
    MockKernelWithInternals kernel(*device);

    std::array<uint32_t, 3> expectedELWS = {8u, 2u, 2u};
    kernel.mockKernel->setCrossThreadData(expectedELWS.data(), static_cast<uint32_t>(sizeof(uint32_t) * expectedELWS.size()));

    auto &eLWSOffsets = kernel.kernelInfo.kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize;
    eLWSOffsets[0] = 0;
    eLWSOffsets[1] = sizeof(uint32_t);
    eLWSOffsets[2] = 2 * sizeof(uint32_t);

    const auto &enqueuedLocalWorkSize = kernel.mockKernel->getEnqueuedLocalWorkSizeValues();
    EXPECT_EQ(expectedELWS[0], *(enqueuedLocalWorkSize[0]));
    EXPECT_EQ(expectedELWS[1], *(enqueuedLocalWorkSize[1]));
    EXPECT_EQ(expectedELWS[2], *(enqueuedLocalWorkSize[2]));
}
