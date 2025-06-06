/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "driver_diagnostics_tests.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"

#include "opencl/source/command_queue/cl_local_work_size.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/helpers/dispatch_info.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"

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

cl_diagnostic_verbose_level_intel diagnosticsVerboseLevels[] = {
    CL_CONTEXT_DIAGNOSTICS_LEVEL_BAD_INTEL,
    CL_CONTEXT_DIAGNOSTICS_LEVEL_GOOD_INTEL,
    CL_CONTEXT_DIAGNOSTICS_LEVEL_NEUTRAL_INTEL};

TEST_P(VerboseLevelTest, GivenVerboseLevelWhenProvidedHintLevelIsSameOrAllThenCallbackFunctionTakesProvidedHint) {
    cl_device_id deviceID = devices[0];
    cl_diagnostic_verbose_level_intel diagnosticsLevel = GetParam();
    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, (cl_context_properties)diagnosticsLevel, 0};
    retVal = CL_SUCCESS;

    auto context = Context::create<Context>(validProperties, ClDeviceVector(&deviceID, 1), callbackFunction, (void *)userData, retVal);
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
    cl_diagnostic_verbose_level_intel providedHintLevel = GetParam();
    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    retVal = CL_SUCCESS;

    auto context = Context::create<Context>(validProperties, ClDeviceVector(&deviceID, 1), callbackFunction, (void *)userData, retVal);
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
        flags |= CL_MEM_FORCE_HOST_MEMORY_INTEL;
    }

    Buffer::AdditionalBufferCreateArgs bufferCreateArgs{};
    bufferCreateArgs.doNotProvidePerformanceHints = !providePerformanceHint;

    buffer = Buffer::create(
        context,
        flags,
        sizeForBuffer,
        (void *)addressForBuffer,
        bufferCreateArgs,
        retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS], addressForBuffer, sizeForBuffer, MemoryConstants::pageSize, MemoryConstants::pageSize);
    EXPECT_EQ(providePerformanceHint && !(alignedSize && alignedAddress), containsHint(expectedHint, userData));
    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_BUFFER_NEEDS_ALLOCATE_MEMORY], 0);
    EXPECT_EQ(providePerformanceHint && !(alignedSize && alignedAddress), containsHint(expectedHint, userData));
}

TEST_P(PerformanceHintCommandQueueTest, GivenProfilingFlagAndPreemptionFlagWhenCommandQueueIsCreatingThenContextProvidesProperHints) {
    cl_command_queue_properties properties = 0;
    if (profilingEnabled) {
        properties = CL_QUEUE_PROFILING_ENABLE;
    }
    cmdQ = clCreateCommandQueue(context, context->getDevice(0), properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[DRIVER_CALLS_INTERNAL_CL_FLUSH], 0);
    EXPECT_TRUE(containsHint(expectedHint, userData));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PROFILING_ENABLED], 0);
    EXPECT_EQ(profilingEnabled, containsHint(expectedHint, userData));
}

TEST_P(PerformanceHintCommandQueueTest, GivenEnabledProfilingFlagAndSupportedPreemptionFlagWhenCommandQueueIsCreatingWithPropertiesThenContextProvidesProperHints) {
    cl_command_queue_properties properties[3] = {0};
    if (profilingEnabled) {
        properties[0] = CL_QUEUE_PROPERTIES;
        properties[1] = CL_QUEUE_PROFILING_ENABLE;
    }
    cmdQ = clCreateCommandQueueWithProperties(context, context->getDevice(0), properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[DRIVER_CALLS_INTERNAL_CL_FLUSH], 0);
    EXPECT_TRUE(containsHint(expectedHint, userData));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PROFILING_ENABLED], 0);
    EXPECT_EQ(profilingEnabled, containsHint(expectedHint, userData));
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

    const ClDeviceInfo &devInfo = pPlatform->getClDevice(0)->getDeviceInfo();
    if (devInfo.svmCapabilities != 0) {
        cl_mem_flags flg = CL_MEM_READ_WRITE;

        size_t size = 4096;

        auto svmPtr = clSVMAlloc(context, flg, size, 128);
        EXPECT_NE(nullptr, svmPtr);

        snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_SVM_ALLOC_MEETS_ALIGNMENT_RESTRICTIONS], svmPtr, size);
        EXPECT_TRUE(containsHint(expectedHint, userData));

        clSVMFree(context, svmPtr);
    }
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeNDIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeNDIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = debugManager.flags.EnableComputeWorkSizeND.get();
    debugManager.flags.EnableComputeWorkSizeND.set(true);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
    debugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeNDIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = debugManager.flags.EnableComputeWorkSizeND.get();
    debugManager.flags.EnableComputeWorkSizeND.set(false);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
    debugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeSquaredIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeSquaredIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(true);
    debugManager.flags.EnableComputeWorkSizeND.set(false);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndEmptyDispatchinfoAndEnableComputeWorkSizeSquaredIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(false);
    debugManager.flags.EnableComputeWorkSizeND.set(false);
    DispatchInfo emptyDispatchInfo;
    provideLocalWorkGroupSizeHints(nullptr, emptyDispatchInfo);
}

TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeNDIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(pDevice, mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeNDIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = debugManager.flags.EnableComputeWorkSizeND.get();
    debugManager.flags.EnableComputeWorkSizeND.set(true);
    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(pDevice, mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
    debugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeNDIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    bool isWorkGroupSizeEnabled = debugManager.flags.EnableComputeWorkSizeND.get();
    debugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(pDevice, mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
    debugManager.flags.EnableComputeWorkSizeND.set(isWorkGroupSizeEnabled);
}

TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeSquaredIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(pDevice, mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeSquaredIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(true);
    debugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(pDevice, mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenNullContextAndInvalidDispatchinfoAndEnableComputeWorkSizeSquaredIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(false);
    debugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(pDevice, mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}

TEST_F(PerformanceHintTest, GivenContextAndDispatchinfoAndEnableComputeWorkSizeSquaredIsDefaultWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(pDevice, mockKernel, 100, {32, 32, 32}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenContextAndDispatchinfoAndEnableComputeWorkSizeSquaredIsTrueWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(true);
    debugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(pDevice, mockKernel, 2, {32, 32, 1}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}
TEST_F(PerformanceHintTest, GivenContextAndDispatchinfoAndEnableComputeWorkSizeSquaredIsFalseWhenProvideLocalWorkGroupSizeIsCalledThenItDoesntCrash) {

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableComputeWorkSizeSquared.set(false);
    debugManager.flags.EnableComputeWorkSizeND.set(false);
    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    DispatchInfo invalidDispatchInfo(pDevice, mockKernel, 2, {32, 32, 1}, {1, 1, 1}, {0, 0, 0});
    provideLocalWorkGroupSizeHints(context, invalidDispatchInfo);
}

TEST_F(PerformanceHintTest, GivenZeroCopyImageAndContextWhenCreateImageThenContextProvidesHintAboutAlignment) {

    std::unique_ptr<Image> image(ImageHelperUlt<Image1dDefaults>::create(context));
    EXPECT_TRUE(image->isMemObjZeroCopy());

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_IMAGE_MEETS_ALIGNMENT_RESTRICTIONS], static_cast<cl_mem>(image.get()));
    EXPECT_TRUE(containsHint(expectedHint, userData));
}

TEST_F(PerformanceHintTest, GivenNonZeroCopyImageAndContextWhenCreateImageThenContextDoesntProvidesHintAboutAlignment) {

    std::unique_ptr<Image> image(ImageHelperUlt<ImageUseHostPtr<Image1dDefaults>>::create(context));
    EXPECT_FALSE(image->isMemObjZeroCopy());

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[CL_IMAGE_MEETS_ALIGNMENT_RESTRICTIONS], static_cast<cl_mem>(image.get()));
    EXPECT_FALSE(containsHint(expectedHint, userData));
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticValueWhenContextIsCreatedThenItHasHintLevelSetToThatValue) {
    DebugManagerStateRestore dbgRestore;
    auto hintLevel = 1;
    debugManager.flags.PrintDriverDiagnostics.set(hintLevel);

    auto pDevice = castToObject<ClDevice>(devices[0]);
    cl_device_id clDevice = pDevice;

    auto context = Context::create<MockContext>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal);

    EXPECT_TRUE(!!context->isProvidingPerformanceHints());
    auto driverDiagnostics = context->driverDiagnostics;
    ASSERT_NE(nullptr, driverDiagnostics);
    EXPECT_TRUE(driverDiagnostics->validFlags(hintLevel));
    context->release();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeEnabledWhenHintIsCalledThenDriverProvidedOutputOnCout) {
    DebugManagerStateRestore dbgRestore;
    auto hintLevel = 255;
    debugManager.flags.PrintDriverDiagnostics.set(hintLevel);

    auto pDevice = castToObject<ClDevice>(devices[0]);
    cl_device_id clDevice = pDevice;

    auto context = Context::create<MockContext>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal);

    StreamCapture capture;
    capture.captureStdout();
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        context,
        CL_MEM_READ_ONLY,
        4096,
        nullptr,
        retVal));

    std::string output = capture.getCapturedStdout();
    EXPECT_NE(0u, output.size());
    EXPECT_EQ('\n', output[0]);

    context->release();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsAndBadHintLevelWhenActionForHintOccursThenNothingIsProvidedToCout) {
    DebugManagerStateRestore dbgRestore;
    auto hintLevel = 8;
    debugManager.flags.PrintDriverDiagnostics.set(hintLevel);

    auto pDevice = castToObject<ClDevice>(devices[0]);
    cl_device_id clDevice = pDevice;

    auto context = Context::create<MockContext>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal);

    StreamCapture capture;
    capture.captureStdout();
    auto buffer = Buffer::create(
        context,
        CL_MEM_READ_ONLY,
        4096,
        nullptr,
        retVal);

    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(0u, output.size());

    buffer->release();
    context->release();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeEnabledWhenContextIsBeingCreatedThenPropertiesPassedToContextAreOverwritten) {
    DebugManagerStateRestore dbgRestore;
    auto hintLevel = 1;
    debugManager.flags.PrintDriverDiagnostics.set(hintLevel);

    auto pDevice = castToObject<ClDevice>(devices[0]);
    cl_device_id clDevice = pDevice;
    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto retValue = CL_SUCCESS;
    auto context = Context::create<MockContext>(validProperties, ClDeviceVector(&clDevice, 1), callbackFunction, (void *)userData, retVal);
    EXPECT_EQ(CL_SUCCESS, retValue);
    auto driverDiagnostics = context->driverDiagnostics;
    ASSERT_NE(nullptr, driverDiagnostics);
    EXPECT_TRUE(driverDiagnostics->validFlags(hintLevel));
    EXPECT_FALSE(driverDiagnostics->validFlags(2));

    context->release();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeEnabledWhenCallFillWithKernelObjsForAuxTranslationOnMemObjectThenContextProvidesProperHint) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.PrintDriverDiagnostics.set(1);

    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    MockBuffer buffer;
    cl_mem clMem = &buffer;

    MockBuffer::setAllocationType(buffer.getGraphicsAllocation(0), pDevice->getRootDeviceEnvironment().getGmmHelper(), true);

    mockKernel.kernelInfo.addArgBuffer(0, 0, 0, 0);
    mockKernel.kernelInfo.addExtendedMetadata(0, "arg0");

    mockKernel.mockKernel->initialize();
    mockKernel.mockKernel->auxTranslationRequired = true;
    mockKernel.mockKernel->setArgBuffer(0, sizeof(cl_mem *), &clMem);

    StreamCapture capture;
    capture.captureStdout();
    auto kernelObjects = mockKernel.mockKernel->fillWithKernelObjsForAuxTranslation();

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[KERNEL_ARGUMENT_AUX_TRANSLATION],
             mockKernel.mockKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.c_str(), 0, mockKernel.mockKernel->getKernelInfo().getExtendedMetadata(0).argName.c_str());

    std::string output = capture.getCapturedStdout();
    EXPECT_NE(0u, output.size());
    EXPECT_TRUE(containsHint(expectedHint, userData));
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeEnabledWhenCallFillWithKernelObjsForAuxTranslationOnGfxAllocationThenContextProvidesProperHint) {
    auto device = castToObject<ClDevice>(devices[0]);
    const ClDeviceInfo &devInfo = device->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.PrintDriverDiagnostics.set(1);

    MockKernelWithInternals mockKernel(*device, context);
    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);

    MockBuffer::setAllocationType(&gfxAllocation, device->getRootDeviceEnvironment().getGmmHelper(), true);

    mockKernel.kernelInfo.addExtendedMetadata(0, "arg0");
    mockKernel.kernelInfo.addArgBuffer(0, 0, 0, 0);

    mockKernel.mockKernel->initialize();
    mockKernel.mockKernel->auxTranslationRequired = true;
    mockKernel.mockKernel->setArgSvmAlloc(0, ptr, &gfxAllocation, 0u);

    StreamCapture capture;
    capture.captureStdout();
    auto kernelObjects = mockKernel.mockKernel->fillWithKernelObjsForAuxTranslation();

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[KERNEL_ARGUMENT_AUX_TRANSLATION],
             mockKernel.mockKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.c_str(), 0, mockKernel.mockKernel->getKernelInfo().getExtendedMetadata(0).argName.c_str());

    std::string output = capture.getCapturedStdout();
    EXPECT_NE(0u, output.size());
    EXPECT_TRUE(containsHint(expectedHint, userData));

    delete gfxAllocation.getDefaultGmm();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeEnabledWhenCallFillWithKernelObjsForAuxTranslationOnUnifiedMemoryThenContextProvidesProperHint) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.PrintDriverDiagnostics.set(1);
    debugManager.flags.EnableStatelessCompression.set(1);

    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);

    MockBuffer::setAllocationType(&gfxAllocation, pDevice->getRootDeviceEnvironment().getGmmHelper(), true);

    mockKernel.mockKernel->initialize();
    mockKernel.mockKernel->setUnifiedMemoryExecInfo(&gfxAllocation);

    StreamCapture capture;
    capture.captureStdout();
    auto kernelObjects = mockKernel.mockKernel->fillWithKernelObjsForAuxTranslation();

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[KERNEL_ALLOCATION_AUX_TRANSLATION],
             mockKernel.mockKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.c_str(), ptr, 128);

    std::string output = capture.getCapturedStdout();
    EXPECT_NE(0u, output.size());
    EXPECT_TRUE(containsHint(expectedHint, userData));

    delete gfxAllocation.getDefaultGmm();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeEnabledWhenCallFillWithKernelObjsForAuxTranslationOnAllocationInSvmAllocsManagerThenContextProvidesProperHint) {
    if (context->getSVMAllocsManager() == nullptr) {
        return;
    }

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.PrintDriverDiagnostics.set(1);
    debugManager.flags.EnableStatelessCompression.set(1);

    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);

    MockBuffer::setAllocationType(&gfxAllocation, pDevice->getRootDeviceEnvironment().getGmmHelper(), true);

    mockKernel.mockKernel->initialize();

    SvmAllocationData allocData(0);
    allocData.gpuAllocations.addAllocation(&gfxAllocation);
    allocData.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocData.device = &pDevice->getDevice();
    context->getSVMAllocsManager()->insertSVMAlloc(allocData);

    StreamCapture capture;
    capture.captureStdout();
    auto kernelObjects = mockKernel.mockKernel->fillWithKernelObjsForAuxTranslation();

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[KERNEL_ALLOCATION_AUX_TRANSLATION],
             mockKernel.mockKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.c_str(), ptr, 128);

    std::string output = capture.getCapturedStdout();
    EXPECT_NE(0u, output.size());
    EXPECT_TRUE(containsHint(expectedHint, userData));

    delete gfxAllocation.getDefaultGmm();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeEnabledWhenKernelObjectWithGraphicsAllocationAccessedStatefullyOnlyThenDontReportAnyHint) {
    auto device = castToObject<ClDevice>(devices[0]);
    const ClDeviceInfo &devInfo = device->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.PrintDriverDiagnostics.set(1);

    MockKernelWithInternals mockKernel(*device, context);
    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);

    MockBuffer::setAllocationType(&gfxAllocation, device->getRootDeviceEnvironment().getGmmHelper(), true);

    mockKernel.kernelInfo.addExtendedMetadata(0, "arg0");
    mockKernel.kernelInfo.addArgBuffer(0, 0, 0, 0);
    mockKernel.kernelInfo.setBufferStateful(0);

    mockKernel.mockKernel->initialize();
    mockKernel.mockKernel->auxTranslationRequired = true;
    mockKernel.mockKernel->setArgSvmAlloc(0, ptr, &gfxAllocation, 0u);

    StreamCapture capture;
    capture.captureStdout();

    auto kernelObjects = mockKernel.mockKernel->fillWithKernelObjsForAuxTranslation();

    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(0u, output.size());

    delete gfxAllocation.getDefaultGmm();
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeDisabledWhenCallFillWithKernelObjsForAuxTranslationOnGfxAllocationThenDontReportAnyHint) {
    auto device = castToObject<ClDevice>(devices[0]);
    const ClDeviceInfo &devInfo = device->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    MockKernelWithInternals mockKernel(*device, context);
    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);

    MockBuffer::setAllocationType(&gfxAllocation, device->getRootDeviceEnvironment().getGmmHelper(), true);

    mockKernel.kernelInfo.addExtendedMetadata(0, "arg0");
    mockKernel.kernelInfo.addArgBuffer(0, 0, 0, 0);

    mockKernel.mockKernel->initialize();
    mockKernel.mockKernel->auxTranslationRequired = true;
    mockKernel.mockKernel->setArgSvmAlloc(0, ptr, &gfxAllocation, 0u);

    StreamCapture capture;
    capture.captureStdout();

    auto kernelObjects = mockKernel.mockKernel->fillWithKernelObjsForAuxTranslation();

    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(0u, output.size());

    delete gfxAllocation.getDefaultGmm();
}

TEST_F(PerformanceHintTest, whenCallingFillWithKernelObjsForAuxTranslationOnNullGfxAllocationThenDontReportAnyHint) {
    auto device = castToObject<ClDevice>(devices[0]);
    const ClDeviceInfo &devInfo = device->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    MockKernelWithInternals mockKernel(*device, context);

    mockKernel.kernelInfo.addExtendedMetadata(0, "arg0");
    mockKernel.kernelInfo.addArgBuffer(0, 0, 0, 0);

    mockKernel.mockKernel->initialize();
    mockKernel.mockKernel->setArgSvmAlloc(0, nullptr, nullptr, 0u);

    StreamCapture capture;
    capture.captureStdout();

    auto kernelObjects = mockKernel.mockKernel->fillWithKernelObjsForAuxTranslation();

    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(0u, output.size());
}

TEST_F(PerformanceHintTest, givenPrintDriverDiagnosticsDebugModeDisabledWhenCallFillWithKernelObjsForAuxTranslationOnUnifiedMemoryThenDontReportAnyHint) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableStatelessCompression.set(1);

    auto pDevice = castToObject<ClDevice>(devices[0]);
    MockKernelWithInternals mockKernel(*pDevice, context);
    char data[128];
    void *ptr = &data;
    MockGraphicsAllocation gfxAllocation(ptr, 128);

    MockBuffer::setAllocationType(&gfxAllocation, pDevice->getRootDeviceEnvironment().getGmmHelper(), true);

    mockKernel.mockKernel->initialize();
    mockKernel.mockKernel->setUnifiedMemoryExecInfo(&gfxAllocation);

    StreamCapture capture;
    capture.captureStdout();
    auto kernelObjects = mockKernel.mockKernel->fillWithKernelObjsForAuxTranslation();

    std::string output = capture.getCapturedStdout();
    EXPECT_EQ(0u, output.size());

    delete gfxAllocation.getDefaultGmm();
}

HWTEST2_F(PerformanceHintTest, given64bitCompressedBufferWhenItsCreatedThenProperPerformanceHintIsProvided, MatchAny) {
    cl_int retVal;
    HardwareInfo hwInfo = context->getDevice(0)->getHardwareInfo();
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    cl_device_id deviceId = device.get();
    size_t size = 8192u;

    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto context = std::unique_ptr<MockContext>(Context::create<NEO::MockContext>(validProperties, ClDeviceVector(&deviceId, 1), callbackFunction, static_cast<void *>(userData), retVal));
    auto buffer = std::unique_ptr<Buffer>(
        Buffer::create(context.get(), ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &context->getDevice(0)->getDevice()),
                       0, CL_MEM_COMPRESSED_HINT_INTEL, size, static_cast<void *>(NULL), retVal));
    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[BUFFER_IS_COMPRESSED], buffer.get());

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &productHelper = device->getProductHelper();
    auto compressionEnabled = gfxCoreHelper.isBufferSizeSuitableForCompression(size) &&
                              GfxCoreHelper::compressedBuffersSupported(hwInfo) && !productHelper.isCompressionForbidden(hwInfo);
    if (compressionEnabled) {
        EXPECT_TRUE(containsHint(expectedHint, userData));
    } else {
        EXPECT_FALSE(containsHint(expectedHint, userData));
    }
}

TEST_F(PerformanceHintTest, givenUncompressedBufferWhenItsCreatedThenProperPerformanceHintIsProvided) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ExperimentalSmallBufferPoolAllocator.set(0); // pool buffer will not provide performance hints
    cl_int retVal;
    HardwareInfo hwInfo = context->getDevice(0)->getHardwareInfo();
    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    cl_device_id deviceId = device.get();
    MemoryProperties memoryProperties =
        ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &context->getDevice(0)->getDevice());

    size_t size = 0u;

    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto context = std::unique_ptr<MockContext>(Context::create<NEO::MockContext>(validProperties, ClDeviceVector(&deviceId, 1), callbackFunction, static_cast<void *>(userData), retVal));
    std::unique_ptr<Buffer> buffer;
    bool isCompressed = true;

    auto &gfxCoreHelper = device->getGfxCoreHelper();

    if (context->getMemoryManager()) {
        isCompressed = MemObjHelper::isSuitableForCompression(
                           GfxCoreHelper::compressedBuffersSupported(hwInfo),
                           memoryProperties, *context,
                           gfxCoreHelper.isBufferSizeSuitableForCompression(size)) &&
                       !is32bit &&
                       (!memoryProperties.flags.useHostPtr || context->getMemoryManager()->isLocalMemorySupported(device->getRootDeviceIndex())) &&
                       !memoryProperties.flags.forceHostMemory;

        buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), memoryProperties, CL_MEM_READ_WRITE, 0, size, static_cast<void *>(NULL), retVal));
    }
    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[BUFFER_IS_NOT_COMPRESSED], buffer.get());

    if (isCompressed || is32bit) {
        Buffer::provideCompressionHint(false, context.get(), buffer.get());
    }
    EXPECT_TRUE(containsHint(expectedHint, userData));
}

HWTEST_F(PerformanceHintTest, givenCompressedImageWhenItsCreatedThenProperPerformanceHintIsProvided) {
    HardwareInfo hwInfo = context->getDevice(0)->getHardwareInfo();
    hwInfo.capabilityTable.ftrRenderCompressedImages = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    cl_device_id deviceId = device.get();

    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto context = std::unique_ptr<MockContext>(Context::create<NEO::MockContext>(validProperties, ClDeviceVector(&deviceId, 1), callbackFunction, static_cast<void *>(userData), retVal));

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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    auto gmm = new Gmm(device->getGmmHelper(), static_cast<const void *>(nullptr), t, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, info, gmmRequirements);
    gmm->setCompressionEnabled(true);

    auto graphicsAllocation = mockBuffer->getGraphicsAllocation(device->getRootDeviceIndex());

    graphicsAllocation->setDefaultGmm(gmm);
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    if (!gfxCoreHelper.checkResourceCompatibility(*graphicsAllocation)) {
        GTEST_SKIP();
    }

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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    auto image = std::unique_ptr<Image>(Image::create(
        context.get(),
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[IMAGE_IS_COMPRESSED], image.get());
    alignedFree(hostPtr);

    if (GfxCoreHelper::compressedImagesSupported(hwInfo)) {
        EXPECT_TRUE(containsHint(expectedHint, userData));
    } else {
        EXPECT_FALSE(containsHint(expectedHint, userData));
    }
}

TEST_F(PerformanceHintTest, givenUncompressedImageWhenItsCreatedThenProperPerformanceHintIsProvided) {
    HardwareInfo hwInfo = context->getDevice(0)->getHardwareInfo();
    hwInfo.capabilityTable.ftrRenderCompressedImages = true;

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo));
    cl_device_id deviceId = device.get();

    cl_context_properties validProperties[3] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, CL_CONTEXT_DIAGNOSTICS_LEVEL_ALL_INTEL, 0};
    auto context = std::unique_ptr<MockContext>(Context::create<NEO::MockContext>(validProperties, ClDeviceVector(&deviceId, 1), callbackFunction, static_cast<void *>(userData), retVal));

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
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = true;
    auto gmm = new Gmm(device->getGmmHelper(), (const void *)nullptr, t, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, info, gmmRequirements);
    gmm->setCompressionEnabled(false);

    mockBuffer->getGraphicsAllocation(device->getRootDeviceIndex())->setDefaultGmm(gmm);
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
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);

    auto image = std::unique_ptr<Image>(Image::create(
        context.get(),
        ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, &context->getDevice(0)->getDevice()),
        flags,
        0,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal));

    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[IMAGE_IS_NOT_COMPRESSED], image.get());
    alignedFree(hostPtr);

    if (GfxCoreHelper::compressedImagesSupported(hwInfo)) {
        EXPECT_TRUE(containsHint(expectedHint, userData));
    } else {
        EXPECT_FALSE(containsHint(expectedHint, userData));
    }
}

TEST_P(PerformanceHintKernelTest, GivenSpillFillWhenKernelIsInitializedThenContextProvidesProperHint) {

    auto spillSize = zeroSized ? 0 : 1024;
    MockKernelWithInternals mockKernel(context->getDevices(), context);

    mockKernel.kernelInfo.kernelDescriptor.kernelAttributes.spillFillScratchMemorySize = spillSize;

    uint32_t computeUnitsForScratch[] = {0x10, 0x20};
    auto pClDevice = &mockKernel.mockKernel->getDevice();
    auto &deviceInfo = const_cast<DeviceInfo &>(pClDevice->getSharedDeviceInfo());
    deviceInfo.computeUnitsUsedForScratch = computeUnitsForScratch[pClDevice->getRootDeviceIndex()];

    mockKernel.mockKernel->initialize();

    auto expectedSize = spillSize * pClDevice->getSharedDeviceInfo().computeUnitsUsedForScratch * mockKernel.mockKernel->getKernelInfo().getMaxSimdSize();
    snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[REGISTER_PRESSURE_TOO_HIGH],
             mockKernel.mockKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.c_str(), expectedSize);
    EXPECT_EQ(!zeroSized, containsHint(expectedHint, userData));
}

TEST_P(PerformanceHintKernelTest, GivenPrivateSurfaceWhenKernelIsInitializedThenContextProvidesProperHint) {
    auto pDevice = castToObject<ClDevice>(devices[1]);
    static_cast<OsAgnosticMemoryManager *>(pDevice->getMemoryManager())->turnOnFakingBigAllocations();

    for (auto isSimtThread : {false, true}) {
        auto size = zeroSized ? 0 : 1024;

        MockKernelWithInternals mockKernel(*pDevice, context);

        mockKernel.kernelInfo.setPrivateMemory(size, isSimtThread, 8, 16, 0);

        size *= pDevice->getSharedDeviceInfo().computeUnitsUsedForScratch;
        size *= isSimtThread ? mockKernel.mockKernel->getKernelInfo().getMaxSimdSize() : 1;

        mockKernel.mockKernel->initialize();

        snprintf(expectedHint, DriverDiagnostics::maxHintStringSize, DriverDiagnostics::hintFormat[PRIVATE_MEMORY_USAGE_TOO_HIGH],
                 mockKernel.mockKernel->getKernelInfo().kernelDescriptor.kernelMetadata.kernelName.c_str(), size);
        EXPECT_EQ(!zeroSized, containsHint(expectedHint, userData));
    }
}

INSTANTIATE_TEST_SUITE_P(
    DriverDiagnosticsTests,
    VerboseLevelTest,
    testing::ValuesIn(diagnosticsVerboseLevels));

INSTANTIATE_TEST_SUITE_P(
    DriverDiagnosticsTests,
    PerformanceHintBufferTest,
    testing::Combine(
        ::testing::Bool(),
        ::testing::Bool(),
        ::testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    DriverDiagnosticsTests,
    PerformanceHintCommandQueueTest,
    testing::Combine(
        ::testing::Bool(),
        ::testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    DriverDiagnosticsTests,
    PerformanceHintKernelTest,
    testing::Bool());

TEST(PerformanceHintsDebugVariables, givenDefaultDebugManagerWhenPrintDriverDiagnosticsIsCalledThenMinusOneIsReturned) {
    EXPECT_EQ(-1, debugManager.flags.PrintDriverDiagnostics.get());
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
    auto context = std::unique_ptr<MockContext>(Context::create<MockContext>(validProperties, ClDeviceVector(&deviceId, 1),
                                                                             callbackFunction, (void *)userData, retVal));

    auto buffer = std::unique_ptr<Buffer>(Buffer::create(context.get(), CL_MEM_READ_WRITE, 1, nullptr, retVal));
    auto address = reinterpret_cast<void *>(0x12345);
    EXPECT_THROW(context->providePerformanceHintForMemoryTransfer(CL_COMMAND_BARRIER, true, buffer.get(), address), std::exception);
}
