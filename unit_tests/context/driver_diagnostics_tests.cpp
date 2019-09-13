/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "driver_diagnostics_tests.h"

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/helpers/memory_properties_flags_helpers.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "unit_tests/mocks/mock_gmm.h"

#include <tuple>

using namespace NEO;

bool containsHint(const char *providedHint, char *userData) {
    for (auto i = 0; i < maxHintCounter; i++) {
        if (strcmp(providedHint, userData + i * DriverDiagnostics::maxHintStringSize) == 0) {
            return true;
        }
    }
    return false;
}

void CL_CALLBACK callbackFunction(const char *providedHint, const void *flags, size_t size, void *userData) {
    int offset = 0;
    while (((char *)userData + offset)[0] != 0) {
        offset += DriverDiagnostics::maxHintStringSize;
    }
    strcpy_s((char *)userData + offset, DriverDiagnostics::maxHintStringSize, providedHint);
}

cl_diagnostics_verbose_level diagnosticsVerboseLevels[] = {
    CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL,
    CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL,
    CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL};

TEST_P(VerboseLevelTest, GivenVerboseLevelWhenProvidedHintLevelIsSameOrAllThenCallbackFunctionTakesProvidedHint) {
    cl_device_id deviceID = devices[0];
    cl_diagnostics_verbose_level diagnosticsLevel = GetParam();
    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, (cl_context_properties)diagnosticsLevel, 0};
    retVal = CL_SUCCESS;

    auto context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), callbackFunction, (void *)userData, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    for (auto hintLevel : validLevels) {
        memset(userData, 0, maxHintCounter * DriverDiagnostics::maxHintStringSize);
        context->providePerformanceHint(hintLevel, hintId);
        if (hintLevel == diagnosticsLevel || hintLevel == CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL) {
            EXPECT_TRUE(containsHint(expectedHint, userData));
        } else {
            EXPECT_FALSE(containsHint(expectedHint, userData));
        }
    }
    delete context;
}

TEST_P(VerboseLevelTest, GivenVerboseLevelAllWhenAnyHintIsProvidedThenCallbackFunctionTakesProvidedHint) {
    cl_device_id deviceID = devices[0];
    cl_diagnostics_verbose_level providedHintLevel = GetParam();
    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    retVal = CL_SUCCESS;

    auto context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), callbackFunction, (void *)userData, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    context->providePerformanceHint(providedHintLevel, hintId);
    EXPECT_TRUE(containsHint(expectedHint, userData));
    delete context;
}

TEST_P(PerformanceHintBufferTest, GivenHostPtrAndSizeAlignmentsWhenBufferIsCreatingThenContextProvidesHintsAboutAlignmentsAndAllocatingMemory) {
    uintptr_t addressForBuffer = (uintptr_t)address;
    size_t sizeForBuffer = MemoryConstants::cacheLineSize;
    if (!alignedAddress) {
        addressForBuffer++;
    }
    if (!alignedSize) {
        sizeForBuffer--;
    }
    auto flags = CL_MEM_USE_HOST_PTR;

    if (alignedAddress && alignedSize) {
        flags |= CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL;
    }

    buffer = Buffer::create(
        context,
        flags,
        sizeForBuffer,
        (void *)addressForBuffer,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS], addressForBuffer, sizeForBuffer, MemoryConstants::pageSize, MemoryConstants::pageSize);
    EXPECT_EQ(!(alignedSize && alignedAddress), containsHint(expectedHint, userData));
    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_BUFFER_NEEDS_ALLOCATE_MEMORY], 0);
    EXPECT_EQ(!(alignedSize && alignedAddress), containsHint(expectedHint, userData));
}

TEST_P(PerformanceHintCommandQueueTest, GivenProfilingFlagAndPreemptionFlagWhenCommandQueueIsCreatingThenContextProvidesProperHints) {
    cl_command_queue_properties properties = 0;
    if (profilingEnabled) {
        properties = CL_QUEUE_PROFILING_ENABLE;
    }
    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[DRIVER_CALLS_INTERNAL_CL_FLUSH], 0);
    EXPECT_TRUE(containsHint(expectedHint, userData));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PROFILING_ENABLED], 0);
    EXPECT_EQ(profilingEnabled, containsHint(expectedHint, userData));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PROFILING_ENABLED_WITH_DISABLED_PREEMPTION], 0);
    if (device->getHardwareInfo().platform.eProductFamily < IGFX_SKYLAKE && preemptionSupported && profilingEnabled) {
        EXPECT_TRUE(containsHint(expectedHint, userData));
    } else {
        EXPECT_FALSE(containsHint(expectedHint, userData));
    }
}

TEST_P(PerformanceHintCommandQueueTest, GivenEnabledProfilingFlagAndSupportedPreemptionFlagWhenCommandQueueIsCreatingWithPropertiesThenContextProvidesProperHints) {
    cl_command_queue_properties properties[3] = {0};
    if (profilingEnabled) {
        properties[0] = CL_QUEUE_PROPERTIES;
        properties[1] = CL_QUEUE_PROFILING_ENABLE;
    }
    cmdQ = clCreateCommandQueueWithProperties(context, device, properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[DRIVER_CALLS_INTERNAL_CL_FLUSH], 0);
    EXPECT_TRUE(containsHint(expectedHint, userData));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PROFILING_ENABLED], 0);
    EXPECT_EQ(profilingEnabled, containsHint(expectedHint, userData));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PROFILING_ENABLED_WITH_DISABLED_PREEMPTION], 0);
    if (device->getHardwareInfo().platform.eProductFamily < IGFX_SKYLAKE && preemptionSupported && profilingEnabled) {
        EXPECT_TRUE(containsHint(expectedHint, userData));
    } else {
        EXPECT_FALSE(containsHint(expectedHint, userData));
    }
}

TEST_F(PerformanceHintTest, GivenAlignedHostPtrWhenSubbufferIsCreatingThenContextProvidesHintAboutSharingMemoryWithParentBuffer) {

    cl_mem_flags flg = CL_MEM_USE_HOST_PTR;
    cl_buffer_region region = {0, MemoryConstants::cacheLineSize - 1};
    void *address = alignedMalloc(MemoryConstants::cacheLineSize, MemoryConstants::cacheLineSize);

    auto buffer = clCreateBuffer(context, flg, MemoryConstants::cacheLineSize, address, &retVal);
    EXPECT_NE(nullptr, buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto subBuffer = clCreateSubBuffer(buffer, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION,
                                       &region, &retVal);
    EXPECT_NE(nullptr, subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[SUBBUFFER_SHARES_MEMORY], buffer);
    EXPECT_TRUE(containsHint(expectedHint, userData));

    retVal = clReleaseMemObject(subBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    alignedFree(address);
}

TEST_F(PerformanceHintTest, GivenContextWhenSVMAllocIsCreatingThenContextProvidesHintAboutAlignment) {

    const DeviceInfo &devInfo = pPlatform->getDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        cl_mem_flags flg = CL_MEM_READ_WRITE;

        size_t size = 4096;

        auto SVMPtr = clSVMAlloc(context, flg, size, 128);
        EXPECT_NE(nullptr, SVMPtr);

        snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_SVM_ALLOC_MEETS_ALIGNMENT_RESTRICTIONS], SVMPtr, size);
        EXPECT_TRUE(containsHint(expectedHint, userData));

        clSVMFree(context, SVMPtr);
    }
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeNDIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeNDIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeNDIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeSquaredIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeSquaredIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeSquaredIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
}

TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeNDIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeNDIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(true);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeNDIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = DebugManager.flags.EnableComputeWorkSizeND.get();
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
    DebugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeSquaredIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeSquaredIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeSquaredIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}

TEST_F(PerformanceHintTest, GivenContextAndDispatchinfoAndEnableComputeWorkSizeSquaredIsDefaultWhenProvideLocalWorkGroupSizeIsCalledReturnValue) {

    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenContextAndDispatchinfoAndEnableComputeWorkSizeSquaredIsTrueWhenProvideLocalWorkGroupSizeIsCalledReturnValue) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(true);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 2, {32, 32, 1}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenContextAndDispatchinfoAndEnableComputeWorkSizeSquaredIsFalseWhenProvideLocalWorkGroupSizeIsCalledReturnValue) {

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableComputeWorkSizeSquared.set(false);
    DebugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(mockKernel, 2, {32, 32, 1}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}

TEST_F(PerformanceHintTest, GivenZeroCopyImageAndContextWhenCreateImageThenContextProvidesHintAboutAlignment) {

    std::unique_ptr<Image> image(ImageHelper<Image1dDefaults>::create(context));
    EXPECT_TRUE(image->isMemObjZeroCopy());

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_IMAGE_MEETS_ALIGNMENT_RESTRICTIONS], static_cast<cl_mem>(image.get()));
    EXPECT_TRUE(containsHint(expectedHint, userData));
}

TEST_F(PerformanceHintTest, GivenNonZeroCopyImageAndContextWhenCreateImageThenContextDoesntProvidesHintAboutAlignment) {

    std::unique_ptr<Image> image(ImageHelper<ImageUseHostPtr<Image1dDefaults>>::create(context));
    EXPECT_FALSE(image->isMemObjZeroCopy());

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_IMAGE_MEETS_ALIGNMENT_RESTRICTIONS], static_cast<cl_mem>(image.get()));
    EXPECT_FALSE(containsHint(expectedHint, userData));
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticValueWhenContextIsCreatedThenItHasHintLevelSetToThatValue) {
    DebugManagerStateRestore dbgRestore;
    auto hintLevel = 1;
    DebugManager.flags.PrintDriverDiagnostics.set(hintLevel);

    auto pDevice = castToObject<Device>(devices[0]);
    cl_device_id clDevice = pDevice;

    auto context = Context::create<MockContext>(nullptr, DeviceVector(&clDevice, 1), nullptr, nullptr, retVal);

    EXPECT_TRUE(!!context->isProvidingPerformanceHints());
    auto driverDiagnostics = context->getDriverDiagnostics();
    ASSERT_NE(nullptr, driverDiagnostics);
    EXPECT_TRUE(driverDiagnostics->validFlags(hintLevel));
    context->release();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeEnabledWhenHintIsCalledThenDriverProvidedOutputOnCout) {
    DebugManagerStateRestore dbgRestore;
    auto hintLevel = 255;
    DebugManager.flags.PrintDriverDiagnostics.set(hintLevel);

    auto pDevice = castToObject<Device>(devices[0]);
    cl_device_id clDevice = pDevice;

    auto context = Context::create<MockContext>(nullptr, DeviceVector(&clDevice, 1), nullptr, nullptr, retVal);

    testing::internal::CaptureStdout();
    auto buffer = Buffer::create(
        context,
        CL_MEM_READ_ONLY,
        4096,
        nullptr,
        retVal);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(0u, output.size());
    EXPECT_EQ('\n', output[0]);

    buffer->release();
    context->release();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsAndBadHintLevelWhenActionForHintOccursThenNothingIsProvidedToCout) {
    DebugManagerStateRestore dbgRestore;
    auto hintLevel = 8;
    DebugManager.flags.PrintDriverDiagnostics.set(hintLevel);

    auto pDevice = castToObject<Device>(devices[0]);
    cl_device_id clDevice = pDevice;

    auto context = Context::create<MockContext>(nullptr, DeviceVector(&clDevice, 1), nullptr, nullptr, retVal);

    testing::internal::CaptureStdout();
    auto buffer = Buffer::create(
        context,
        CL_MEM_READ_ONLY,
        4096,
        nullptr,
        retVal);

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(0u, output.size());

    buffer->release();
    context->release();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeEnabledWhenContextIsBeingCreatedThenPropertiesPassedToContextAreOverwritten) {
    DebugManagerStateRestore dbgRestore;
    auto hintLevel = 1;
    DebugManager.flags.PrintDriverDiagnostics.set(hintLevel);

    auto pDevice = castToObject<Device>(devices[0]);
    cl_device_id clDevice = pDevice;
    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto retValue = CL_SUCCESS;
    auto context = Context::create<MockContext>(validProperties, DeviceVector(&clDevice, 1), callbackFunction, (void *)userData, retVal);
    EXPECT_EQ(CL_SUCCESS, retValue);
    auto driverDiagnostics = context->getDriverDiagnostics();
    ASSERT_NE(nullptr, driverDiagnostics);
    EXPECT_TRUE(driverDiagnostics->validFlags(hintLevel));
    EXPECT_FALSE(driverDiagnostics->validFlags(2));

    context->release();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeEnabledWhenCallFillWithBuffersForAuxTranslationThenContextProvidesProperHint) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.PrintDriverDiagnostics.set(1);

    auto pDevice = castToObject<Device>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    MockBuffer buffer;
    cl_mem clMem = &buffer;

    buffer.getGraphicsAllocation()->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
    mockKernel.kernelInfo.kernelArgInfo.resize(1);
    mockKernel.kernelInfo.kernelArgInfo.at(0).kernelArgPatchInfoVector.resize(1);
    mockKernel.kernelInfo.kernelArgInfo.at(0).pureStatefulBufferAccess = false;
    mockKernel.mockKernel->initialize();
    mockKernel.mockKernel->auxTranslationRequired = true;
    mockKernel.mockKernel->setArgBuffer(0, sizeof(cl_mem *), &clMem);

    testing::internal::CaptureStdout();
    MemObjsForAuxTranslation memObjects;
    mockKernel.mockKernel->fillWithBuffersForAuxTranslation(memObjects);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[KERNEL_ARGUMENT_AUX_TRANSLATION],
             mockKernel.mockKernel->getKernelInfo().name.c_str(), 0, mockKernel.mockKernel->getKernelInfo().kernelArgInfo.at(0).name.c_str());

    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(0u, output.size());
    EXPECT_TRUE(containsHint(expectedHint, userData));
}

TEST_F(PerformanceHintTest, given64bitCompressedBufferWhenItsCreatedThenProperPerformanceHintIsProvided) {
    cl_int retVal;
    HardwareInfo hwInfo = context->getDevice(0)->getHardwareInfo();
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    cl_device_id deviceId = static_cast<cl_device_id>(device.get());
    const MemoryProperties properties(1 << 21);
    size_t size = 8192u;

    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto context = std::unique_ptr<MockContext>(Context::create<NEO::MockContext>(validProperties, DeviceVector(&deviceId, 1), callbackFunction, static_cast<void *>(userData), retVal));
    context->isSharedContext = false;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), properties, size, static_cast<void *>(NULL), retVal));
    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[BUFFER_IS_COMPRESSED], buffer.get());
    if (!is32bit && HwHelper::renderCompressedBuffersSupported(hwInfo)) {
        EXPECT_TRUE(containsHint(expectedHint, userData));
    } else {
        EXPECT_FALSE(containsHint(expectedHint, userData));
    }
}

TEST_F(PerformanceHintTest, givenUncompressedBufferWhenItsCreatedThenProperPerformanceHintIsProvided) {
    cl_int retVal;
    HardwareInfo hwInfo = context->getDevice(0)->getHardwareInfo();
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    cl_device_id deviceId = static_cast<cl_device_id>(device.get());
    const MemoryProperties properties(CL_MEM_READ_WRITE);
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(properties);

    size_t size = 0u;

    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto context = std::unique_ptr<MockContext>(Context::create<NEO::MockContext>(validProperties, DeviceVector(&deviceId, 1), callbackFunction, static_cast<void *>(userData), retVal));
    std::unique_ptr<Buffer> buffer;
    bool isCompressed = true;
    if (context->getMemoryManager()) {
        isCompressed = MemObjHelper::isSuitableForRenderCompression(
                           HwHelper::renderCompressedBuffersSupported(hwInfo),
                           memoryProperties, context->peekContextType(),
                           HwHelper::get(hwInfo.platform.eRenderCoreFamily).obtainRenderBufferCompressionPreference(size)) &&
                       !is32bit && !context->isSharedContext &&
                       (!isValueSet(properties.flags, CL_MEM_USE_HOST_PTR) || context->getMemoryManager()->isLocalMemorySupported()) &&
                       !isValueSet(properties.flags, CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL);

        buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), properties, size, static_cast<void *>(NULL), retVal));
    }
    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[BUFFER_IS_NOT_COMPRESSED], buffer.get());

    if (isCompressed) {
        Buffer::provideCompressionHint(GraphicsAllocation::AllocationType::BUFFER, context.get(), buffer.get());
    }
    EXPECT_TRUE(containsHint(expectedHint, userData));
}

TEST_F(PerformanceHintTest, givenCompressedImageWhenItsCreatedThenProperPerformanceHintIsProvided) {
    HardwareInfo hwInfo = context->getDevice(0)->getHardwareInfo();
    hwInfo.capabilityTable.ftrRenderCompressedImages = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    cl_device_id deviceId = static_cast<cl_device_id>(device.get());

    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto context = std::unique_ptr<MockContext>(Context::create<NEO::MockContext>(validProperties, DeviceVector(&deviceId, 1), callbackFunction, static_cast<void *>(userData), retVal));

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    cl_int retVal = CL_SUCCESS;
    auto const elementSize = 4;
    char *hostPtr = static_cast<char *>(alignedMalloc(width * height * depth * elementSize * 2, 64));

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    auto mockBuffer = std::unique_ptr<MockBuffer>(new MockBuffer());
    StorageInfo info;
    size_t t = 4;
    auto gmm = std::unique_ptr<Gmm>(new Gmm(static_cast<const void *>(nullptr), t, false, true, true, info));
    gmm->isRenderCompressed = true;

    mockBuffer->getGraphicsAllocation()->setDefaultGmm(gmm.get());
    cl_mem mem = mockBuffer.get();
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = mem;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    imageDesc.image_width = width;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);

    auto image = std::unique_ptr<Image>(Image::create(
        context.get(),
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[IMAGE_IS_COMPRESSED], image.get());
    alignedFree(hostPtr);

    if (HwHelper::renderCompressedImagesSupported(hwInfo)) {
        EXPECT_TRUE(containsHint(expectedHint, userData));
    } else {
        EXPECT_FALSE(containsHint(expectedHint, userData));
    }
}

TEST_F(PerformanceHintTest, givenImageWithNoGmmWhenItsCreatedThenNoPerformanceHintIsProvided) {
    HardwareInfo hwInfo = context->getDevice(0)->getHardwareInfo();
    hwInfo.capabilityTable.ftrRenderCompressedImages = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    cl_device_id deviceId = static_cast<cl_device_id>(device.get());

    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto context = std::unique_ptr<MockContext>(Context::create<NEO::MockContext>(validProperties, DeviceVector(&deviceId, 1), callbackFunction, static_cast<void *>(userData), retVal));

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    cl_int retVal = CL_SUCCESS;
    auto const elementSize = 4;
    char *hostPtr = static_cast<char *>(alignedMalloc(width * height * depth * elementSize * 2, 64));

    cl_image_format imageFormat;
    cl_image_desc imageDesc;

    auto mockBuffer = std::unique_ptr<MockBuffer>(new MockBuffer());
    cl_mem mem = mockBuffer.get();

    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = mem;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    imageDesc.image_width = width;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);

    auto image = std::unique_ptr<Image>(Image::create(
        context.get(),
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[IMAGE_IS_COMPRESSED], image.get());
    EXPECT_FALSE(containsHint(expectedHint, userData));
    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[IMAGE_IS_NOT_COMPRESSED], image.get());
    EXPECT_FALSE(containsHint(expectedHint, userData));

    alignedFree(hostPtr);
}

TEST_F(PerformanceHintTest, givenUncompressedImageWhenItsCreatedThenProperPerformanceHintIsProvided) {
    HardwareInfo hwInfo = context->getDevice(0)->getHardwareInfo();
    hwInfo.capabilityTable.ftrRenderCompressedImages = true;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    cl_device_id deviceId = static_cast<cl_device_id>(device.get());

    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto context = std::unique_ptr<MockContext>(Context::create<NEO::MockContext>(validProperties, DeviceVector(&deviceId, 1), callbackFunction, static_cast<void *>(userData), retVal));

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    cl_int retVal = CL_SUCCESS;
    auto const elementSize = 4;
    char *hostPtr = static_cast<char *>(alignedMalloc(width * height * depth * elementSize * 2, 64));

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    auto mockBuffer = std::unique_ptr<MockBuffer>(new MockBuffer());
    StorageInfo info;
    size_t t = 4;
    auto gmm = std::unique_ptr<Gmm>(new Gmm((const void *)nullptr, t, false, true, true, info));
    gmm->isRenderCompressed = false;

    mockBuffer->getGraphicsAllocation()->setDefaultGmm(gmm.get());
    cl_mem mem = mockBuffer.get();
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_RGBA;
    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = mem;
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
    imageDesc.image_width = width;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);

    auto image = std::unique_ptr<Image>(Image::create(
        context.get(),
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[IMAGE_IS_NOT_COMPRESSED], image.get());
    alignedFree(hostPtr);

    if (HwHelper::renderCompressedImagesSupported(hwInfo)) {
        EXPECT_TRUE(containsHint(expectedHint, userData));
    } else {
        EXPECT_FALSE(containsHint(expectedHint, userData));
    }
}

TEST_P(PerformanceHintKernelTest, GivenSpillFillWhenKernelIsInitializedThenContextProvidesProperHint) {

    auto pDevice = castToObject<Device>(devices[0]);
    auto size = zeroSized ? 0 : 1024;
    MockKernelWithInternals mockKernel(*pDevice, context);
    SPatchMediaVFEState mediaVFEstate;

    mediaVFEstate.PerThreadScratchSpace = size;

    mockKernel.kernelInfo.patchInfo.mediavfestate = &mediaVFEstate;
    size *= pDevice->getDeviceInfo().computeUnitsUsedForScratch * mockKernel.mockKernel->getKernelInfo().getMaxSimdSize();

    mockKernel.mockKernel->initialize();

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[REGISTER_PRESSURE_TOO_HIGH],
             mockKernel.mockKernel->getKernelInfo().name.c_str(), size);
    EXPECT_EQ(!zeroSized, containsHint(expectedHint, userData));
}

TEST_P(PerformanceHintKernelTest, GivenPrivateSurfaceWhenKernelIsInitializedThenContextProvidesProperHint) {

    auto pDevice = castToObject<Device>(devices[0]);
    auto size = zeroSized ? 0 : 1024;
    MockKernelWithInternals mockKernel(*pDevice, context);
    SPatchAllocateStatelessPrivateSurface allocateStatelessPrivateMemorySurface;

    allocateStatelessPrivateMemorySurface.PerThreadPrivateMemorySize = size;
    allocateStatelessPrivateMemorySurface.SurfaceStateHeapOffset = 128;
    allocateStatelessPrivateMemorySurface.DataParamOffset = 16;
    allocateStatelessPrivateMemorySurface.DataParamSize = 8;

    mockKernel.kernelInfo.patchInfo.pAllocateStatelessPrivateSurface = &allocateStatelessPrivateMemorySurface;
    size *= pDevice->getDeviceInfo().computeUnitsUsedForScratch * mockKernel.mockKernel->getKernelInfo().getMaxSimdSize();

    mockKernel.mockKernel->initialize();

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PRIVATE_MEMORY_USAGE_TOO_HIGH],
             mockKernel.mockKernel->getKernelInfo().name.c_str(), size);
    EXPECT_EQ(!zeroSized, containsHint(expectedHint, userData));
}

INSTANTIATE_TEST_CASE_P(
    DriverDiagnosticsTests,
    VerboseLevelTest,
    testing::ValuesIn(diagnosticsVerboseLevels));

INSTANTIATE_TEST_CASE_P(
    DriverDiagnosticsTests,
    PerformanceHintBufferTest,
    testing::Combine(
        ::testing::Bool(),
        ::testing::Bool()));

INSTANTIATE_TEST_CASE_P(
    DriverDiagnosticsTests,
    PerformanceHintCommandQueueTest,
    testing::Combine(
        ::testing::Bool(),
        ::testing::Bool()));

INSTANTIATE_TEST_CASE_P(
    DriverDiagnosticsTests,
    PerformanceHintKernelTest,
    testing::Bool());

TEST(PerformanceHintsDebugVariables, givenDefaultDebugManagerWhenPrintDriverDiagnosticsIsCalledThenMinusOneIsReturned) {
    EXPECT_EQ(-1, DebugManager.flags.PrintDriverDiagnostics.get());
}

TEST(PerformanceHintsTransferTest, givenCommandTypeAndMemoryTransferRequiredWhenAskingForHintThenReturnCorrectValue) {
    DriverDiagnostics driverDiagnostics(0);
    const uint32_t numHints = 8;
    std::tuple<uint32_t, PerformanceHints, PerformanceHints> commandHints[numHints] = {
        // commandType, transfer required, transfer not required
        std::make_tuple(CL_COMMAND_MAP_BUFFER, CL_ENQUEUE_MAP_BUFFER_REQUIRES_COPY_DATA, CL_ENQUEUE_MAP_BUFFER_DOESNT_REQUIRE_COPY_DATA),
        std::make_tuple(CL_COMMAND_MAP_IMAGE, CL_ENQUEUE_MAP_IMAGE_REQUIRES_COPY_DATA, CL_ENQUEUE_MAP_IMAGE_DOESNT_REQUIRE_COPY_DATA),
        std::make_tuple(CL_COMMAND_UNMAP_MEM_OBJECT, CL_ENQUEUE_UNMAP_MEM_OBJ_REQUIRES_COPY_DATA, CL_ENQUEUE_UNMAP_MEM_OBJ_DOESNT_REQUIRE_COPY_DATA),
        std::make_tuple(CL_COMMAND_WRITE_BUFFER, CL_ENQUEUE_WRITE_BUFFER_REQUIRES_COPY_DATA, CL_ENQUEUE_WRITE_BUFFER_DOESNT_REQUIRE_COPY_DATA),
        std::make_tuple(CL_COMMAND_READ_BUFFER, CL_ENQUEUE_READ_BUFFER_REQUIRES_COPY_DATA, CL_ENQUEUE_READ_BUFFER_DOESNT_REQUIRE_COPY_DATA),
        std::make_tuple(CL_COMMAND_WRITE_BUFFER_RECT, CL_ENQUEUE_WRITE_BUFFER_RECT_REQUIRES_COPY_DATA, CL_ENQUEUE_WRITE_BUFFER_RECT_DOESNT_REQUIRE_COPY_DATA),
        std::make_tuple(CL_COMMAND_READ_BUFFER_RECT, CL_ENQUEUE_READ_BUFFER_RECT_REQUIRES_COPY_DATA, CL_ENQUEUE_READ_BUFFER_RECT_DOESNT_REQUIRES_COPY_DATA),
        std::make_tuple(CL_COMMAND_WRITE_IMAGE, CL_ENQUEUE_WRITE_IMAGE_REQUIRES_COPY_DATA, CL_ENQUEUE_WRITE_IMAGE_DOESNT_REQUIRES_COPY_DATA),
    };

    for (uint32_t i = 0; i < numHints; i++) {
        auto hintWithTransferRequired = driverDiagnostics.obtainHintForTransferOperation(std::get<0>(commandHints[i]), true);
        auto hintWithoutTransferRequired = driverDiagnostics.obtainHintForTransferOperation(std::get<0>(commandHints[i]), false);

        EXPECT_EQ(std::get<1>(commandHints[i]), hintWithTransferRequired);
        EXPECT_EQ(std::get<2>(commandHints[i]), hintWithoutTransferRequired);
    }

    EXPECT_THROW(driverDiagnostics.obtainHintForTransferOperation(CL_COMMAND_READ_IMAGE, true), std::exception); // no hint for this scenario
    EXPECT_EQ(CL_ENQUEUE_READ_IMAGE_DOESNT_REQUIRES_COPY_DATA,
              driverDiagnostics.obtainHintForTransferOperation(CL_COMMAND_READ_IMAGE, false));
}

TEST_F(DriverDiagnosticsTest, givenInvalidCommandTypeWhenAskingForZeroCopyOperatonThenAbort) {
    cl_device_id deviceId = devices[0];
    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto context = std::unique_ptr<MockContext>(Context::create<MockContext>(validProperties, DeviceVector(&deviceId, 1),
                                                                             callbackFunction, (void *)userData, retVal));

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto address = reinterpret_cast<void *>(0x12345);
    EXPECT_THROW(context->providePerformanceHintForMemoryTransfer(CL_COMMAND_BARRIER, true, buffer.get(), address), std::exception);
}
