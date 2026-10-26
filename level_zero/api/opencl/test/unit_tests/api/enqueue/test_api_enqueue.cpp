/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/kernel/kernel_execution_type.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/event/leo_event.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/context/context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct EnqueueKernelExecutionTypeFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        device = platform->getDevices()[0].get();
        cl_device_id clDevice = device;
        context = std::make_unique<Context>(nullptr, nullptr, 1, &clDevice, true);
        commandQueue = std::make_unique<CommandQueue>(context.get(), device, nullptr, nullptr);
        l0Kernel = std::make_unique<L0::ult::Mock<L0::KernelImp>>();
        program = std::make_unique<Program>(context.get());
        std::map<uint32_t, ze_kernel_handle_t> kernelHandles{{0u, l0Kernel->toHandle()}};
        kernel = std::make_unique<Kernel>(std::move(kernelHandles), program.get());
    }

    void TearDown() override {
        kernel.reset();
        program.reset();
        l0Kernel.release();
        commandQueue.reset();
        context.reset();
        Test<OclFixture>::TearDown();
    }

    void setConcurrent() {
        cl_execution_info_kernel_type_intel type = CL_KERNEL_EXEC_INFO_CONCURRENT_TYPE_INTEL;
        auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_KERNEL_TYPE_INTEL, sizeof(type), &type);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    void setUsesSyncBuffer() {
        l0Kernel->getDescriptor().kernelAttributes.flags.usesSyncBuffer = true;
    }

    ClDevice *device = nullptr;
    std::unique_ptr<Context> context;
    std::unique_ptr<CommandQueue> commandQueue;
    std::unique_ptr<L0::ult::Mock<L0::KernelImp>> l0Kernel;
    std::unique_ptr<Program> program;
    std::unique_ptr<Kernel> kernel;
};

TEST_F(EnqueueKernelExecutionTypeFixture, givenConcurrentKernelWhenEnqueueNDRangeKernelThenReturnsCLInvalidKernel) {
    setConcurrent();

    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    auto retVal = clEnqueueNDRangeKernel(commandQueue.get(), kernel.get(), 1, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(EnqueueKernelExecutionTypeFixture, givenKernelUsingSyncBufferWhenEnqueueNDRangeKernelThenReturnsCLInvalidKernel) {
    setUsesSyncBuffer();

    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    auto retVal = clEnqueueNDRangeKernel(commandQueue.get(), kernel.get(), 1, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

TEST_F(EnqueueKernelExecutionTypeFixture, givenKernelUsingSyncBufferAndNotConcurrentWhenEnqueueNDCountKernelThenReturnsCLInvalidKernel) {
    setUsesSyncBuffer();

    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t workgroupCount[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    auto retVal = clEnqueueNDCountKernelINTEL(commandQueue.get(), kernel.get(), 1, globalWorkOffset, workgroupCount, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_INVALID_KERNEL, retVal);
}

struct EnqueueYuvImageFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        device = platform->getDevices()[0].get();
        if (!device->getHardwareInfo().capabilityTable.supportsImages) {
            GTEST_SKIP() << "Product does not support images";
        }
        cl_device_id clDevice = device;
        context = std::make_unique<Context>(nullptr, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDevice, true);
        ASSERT_EQ(CL_SUCCESS, context->initialize());
        commandQueue = std::make_unique<CommandQueue>(context.get(), device, nullptr, nullptr);

        packedYuvImage = createImage({CL_YUYV_INTEL, CL_UNORM_INT8});
        ASSERT_NE(nullptr, packedYuvImage);
        rgbaImage = createImage({CL_RGBA, CL_UNORM_INT8});
        ASSERT_NE(nullptr, rgbaImage);
        buffer = clCreateBuffer(context.get(), CL_MEM_READ_WRITE, width * height * 4, nullptr, nullptr);
        ASSERT_NE(nullptr, buffer);
    }

    void TearDown() override {
        if (buffer) {
            clReleaseMemObject(buffer);
        }
        if (rgbaImage) {
            clReleaseMemObject(rgbaImage);
        }
        if (packedYuvImage) {
            clReleaseMemObject(packedYuvImage);
        }
        commandQueue.reset();
        context.reset();
        Test<OclFixture>::TearDown();
    }

    cl_mem createImage(cl_image_format format) {
        cl_image_desc desc{};
        desc.image_type = CL_MEM_OBJECT_IMAGE2D;
        desc.image_width = width;
        desc.image_height = height;
        cl_int err = CL_INVALID_VALUE;
        auto image = clCreateImage(context.get(), CL_MEM_READ_ONLY, &format, &desc, nullptr, &err);
        EXPECT_EQ(CL_SUCCESS, err);
        return image;
    }

    static constexpr size_t width = 16;
    static constexpr size_t height = 16;
    size_t oddOrigin[3] = {1, 0, 0};
    size_t evenOrigin[3] = {0, 0, 0};
    size_t oddRegion[3] = {3, 2, 1};
    ClDevice *device = nullptr;
    std::unique_ptr<Context> context;
    std::unique_ptr<CommandQueue> commandQueue;
    cl_mem packedYuvImage = nullptr;
    cl_mem rgbaImage = nullptr;
    cl_mem buffer = nullptr;
    char hostBuffer[width * height * 4] = {};
};

TEST_F(EnqueueYuvImageFixture, givenPackedYuvImageWithOddOriginWhenEnqueueReadImageThenReturnsCLInvalidValue) {
    size_t region[3] = {2, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueReadImage(commandQueue.get(), packedYuvImage, CL_TRUE, oddOrigin, region,
                                                   0, 0, hostBuffer, 0, nullptr, nullptr));
}

TEST_F(EnqueueYuvImageFixture, givenPackedYuvImageWithOddRegionWhenEnqueueWriteImageThenReturnsCLInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueWriteImage(commandQueue.get(), packedYuvImage, CL_TRUE, evenOrigin, oddRegion,
                                                    0, 0, hostBuffer, 0, nullptr, nullptr));
}

TEST_F(EnqueueYuvImageFixture, givenPackedYuvImageWithOddOriginWhenEnqueueCopyImageToBufferThenReturnsCLInvalidValue) {
    size_t region[3] = {2, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueCopyImageToBuffer(commandQueue.get(), packedYuvImage, buffer, oddOrigin,
                                                           region, 0, 0, nullptr, nullptr));
}

TEST_F(EnqueueYuvImageFixture, givenPackedYuvImageWithOddOriginWhenEnqueueCopyBufferToImageThenReturnsCLInvalidValue) {
    size_t region[3] = {2, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueCopyBufferToImage(commandQueue.get(), buffer, packedYuvImage, 0, oddOrigin,
                                                           region, 0, nullptr, nullptr));
}

TEST_F(EnqueueYuvImageFixture, givenPackedYuvImageWithOddOriginWhenEnqueueMapImageThenInvalidValueReportedThroughErrcode) {
    size_t region[3] = {2, 2, 1};
    cl_int retVal = CL_SUCCESS;
    EXPECT_EQ(nullptr, clEnqueueMapImage(commandQueue.get(), packedYuvImage, CL_TRUE, CL_MAP_READ, oddOrigin, region,
                                         nullptr, nullptr, 0, nullptr, nullptr, &retVal));
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(EnqueueYuvImageFixture, givenMismatchedFormatsWhenEnqueueCopyImageThenReturnsCLImageFormatMismatch) {
    size_t region[3] = {2, 2, 1};
    EXPECT_EQ(CL_IMAGE_FORMAT_MISMATCH, clEnqueueCopyImage(commandQueue.get(), packedYuvImage, rgbaImage, evenOrigin,
                                                           evenOrigin, region, 0, nullptr, nullptr));
}

TEST_F(EnqueueYuvImageFixture, givenPackedYuvImageWithOddSrcOriginWhenEnqueueCopyImageThenReturnsCLInvalidValue) {
    size_t region[3] = {2, 2, 1};
    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueCopyImage(commandQueue.get(), packedYuvImage, packedYuvImage, oddOrigin,
                                                   evenOrigin, region, 0, nullptr, nullptr));
}

TEST_F(EnqueueKernelExecutionTypeFixture, givenGlobalWorkSizeAboveUint32MaxNotDivisibleByLocalWorkSizeWhenEnqueueNDRangeKernelThenReturnsCLInvalidWorkGroupSize) {
    constexpr uint32_t groupSize = 3u; // does not divide 2^32, so a truncated range would look divisible
    // pre-set the group size so that zeKernelSetGroupSize takes its no-change early return
    l0Kernel->privateState.groupSize[0] = groupSize;
    l0Kernel->privateState.groupSize[1] = 1u;
    l0Kernel->privateState.groupSize[2] = 1u;

    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1u, 1, 1};
    size_t localWorkSize[3] = {groupSize, 1, 1};

    auto retVal = clEnqueueNDRangeKernel(commandQueue.get(), kernel.get(), 1, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, retVal);
}

struct EnqueueKernelEventFixture : public Test<LeoCaptureFixture> {
    void SetUp() override {
        Test<LeoCaptureFixture>::SetUp();
        l0Kernel = std::make_unique<L0::ult::Mock<L0::KernelImp>>();
        program = std::make_unique<Program>(context);
        std::map<uint32_t, ze_kernel_handle_t> kernelHandles{{0u, l0Kernel->toHandle()}};
        kernel = std::make_unique<Kernel>(std::move(kernelHandles), program.get());
    }

    void TearDown() override {
        kernel.reset();
        program.reset();
        l0Kernel.release();
        Test<LeoCaptureFixture>::TearDown();
    }

    std::unique_ptr<L0::ult::Mock<L0::KernelImp>> l0Kernel;
    std::unique_ptr<Program> program;
    std::unique_ptr<Kernel> kernel;
};

TEST_F(EnqueueKernelEventFixture, givenGroupCountAboveUint32WhenEnqueueNDRangeKernelThenOutOfResourcesIsReturnedAndOutputEventIsUntouched) {
    constexpr uint32_t groupSize = 2u;
    l0Kernel->privateState.groupSize[0] = groupSize;
    l0Kernel->privateState.groupSize[1] = 1u;
    l0Kernel->privateState.groupSize[2] = 1u;

    cl_int errcode = CL_SUCCESS;
    cl_event waitEvent = clCreateUserEvent(clContext, &errcode);
    ASSERT_EQ(CL_SUCCESS, errcode);
    const auto waitEventRefCount = castToObject<Event>(waitEvent)->getRefInternalCount();

    std::array<uint8_t, 64> eventStorage{};
    const auto notAnEvent = reinterpret_cast<cl_event>(eventStorage.data());
    cl_event event = notAnEvent;

    size_t globalWorkSize[3] = {groupSize * (static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 1u), 1, 1};
    size_t localWorkSize[3] = {groupSize, 1, 1};

    EXPECT_EQ(CL_OUT_OF_RESOURCES, clEnqueueNDRangeKernel(getCommandQueue(), kernel.get(), 1, nullptr, globalWorkSize, localWorkSize, 1, &waitEvent, &event));
    EXPECT_EQ(notAnEvent, event);
    EXPECT_EQ(waitEventRefCount, castToObject<Event>(waitEvent)->getRefInternalCount());
    EXPECT_EQ(0u, capturingCmdList.totalCalls());

    EXPECT_EQ(CL_SUCCESS, clReleaseEvent(waitEvent));
}

struct WhiteBoxNonUniformProgram : public Program {
    using Program::allowNonUniform;
    using Program::Program;
};

template <int32_t leoEnableNonUniformWorkGroups>
struct EnqueueNonUniformKernelFixture : public Test<LeoCaptureFixture> {
    void SetUp() override {
        debugManager.flags.LeoEnableNonUniformWorkGroups.set(leoEnableNonUniformWorkGroups);
        Test<LeoCaptureFixture>::SetUp();
        l0Kernel = std::make_unique<L0::ult::Mock<L0::KernelImp>>();
        l0Kernel->privateState.groupSize[0] = enqueuedGroupSize[0];
        l0Kernel->privateState.groupSize[1] = enqueuedGroupSize[1];
        l0Kernel->privateState.groupSize[2] = enqueuedGroupSize[2];
        program = std::make_unique<WhiteBoxNonUniformProgram>(context);
        program->allowNonUniform = true;
        std::map<uint32_t, ze_kernel_handle_t> kernelHandles{{0u, l0Kernel->toHandle()}};
        kernel = std::make_unique<Kernel>(std::move(kernelHandles), program.get());
    }

    void TearDown() override {
        kernel.reset();
        program.reset();
        l0Kernel.release();
        Test<LeoCaptureFixture>::TearDown();
    }

    cl_int enqueue(const size_t *globalWorkSize) {
        return clEnqueueNDRangeKernel(getCommandQueue(), kernel.get(), 2, nullptr, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    }

    static constexpr uint32_t enqueuedGroupSize[3] = {4u, 3u, 1u};
    const size_t localWorkSize[3] = {enqueuedGroupSize[0], enqueuedGroupSize[1], enqueuedGroupSize[2]};
    const size_t nonUniformGlobalWorkSize[3] = {10u, 7u, 1u};
    const size_t uniformGlobalWorkSize[3] = {8u, 6u, 1u};

    DebugManagerStateRestore restorer;
    std::unique_ptr<L0::ult::Mock<L0::KernelImp>> l0Kernel;
    std::unique_ptr<WhiteBoxNonUniformProgram> program;
    std::unique_ptr<Kernel> kernel;
};

using EnqueueNonUniformKernelDefaultFixture = EnqueueNonUniformKernelFixture<-1>;
using EnqueueNonUniformKernelEnabledFixture = EnqueueNonUniformKernelFixture<1>;

TEST_F(EnqueueNonUniformKernelDefaultFixture, givenDefaultLeoEnableNonUniformWorkGroupsWhenEnqueueNDRangeKernelWithNonUniformRangeThenInvalidWorkGroupSizeIsReturned) {
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, enqueue(nonUniformGlobalWorkSize));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueNonUniformKernelEnabledFixture, givenProgramNotAllowingNonUniformWhenEnqueueNDRangeKernelWithNonUniformRangeThenInvalidWorkGroupSizeIsReturned) {
    program->allowNonUniform = false;
    EXPECT_EQ(CL_INVALID_WORK_GROUP_SIZE, enqueue(nonUniformGlobalWorkSize));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueNonUniformKernelEnabledFixture, givenKernelWithRequiredWorkGroupSizeWhenEnqueueNDRangeKernelWithNonUniformRangeThenNonUniformLaunchWithRequiredGroupSizeIsAppended) {
    l0Kernel->getDescriptor().kernelAttributes.requiredWorkgroupSize[0] = static_cast<uint16_t>(enqueuedGroupSize[0]);
    l0Kernel->getDescriptor().kernelAttributes.requiredWorkgroupSize[1] = static_cast<uint16_t>(enqueuedGroupSize[1]);
    l0Kernel->getDescriptor().kernelAttributes.requiredWorkgroupSize[2] = static_cast<uint16_t>(enqueuedGroupSize[2]);
    EXPECT_EQ(CL_SUCCESS, clEnqueueNDRangeKernel(getCommandQueue(), kernel.get(), 2, nullptr, nonUniformGlobalWorkSize, nullptr, 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendLaunchKernelNonUniformArgs.count());
    const auto &captured = capturingCmdList.appendLaunchKernelNonUniformArgs[0];
    for (uint32_t dim = 0u; dim < 3u; dim++) {
        EXPECT_EQ(enqueuedGroupSize[dim], captured.launchArgs.enqueuedGroupSize[dim]);
    }
}

TEST_F(EnqueueNonUniformKernelEnabledFixture, givenGlobalWorkSizeAboveUint32WhenEnqueueNDRangeKernelWithNonUniformRangeThenInvalidGlobalWorkSizeIsReturned) {
    const size_t globalWorkSize[3] = {static_cast<size_t>(std::numeric_limits<uint32_t>::max()) + 2u, 7u, 1u};
    EXPECT_EQ(CL_INVALID_GLOBAL_WORK_SIZE, enqueue(globalWorkSize));
    EXPECT_EQ(0u, capturingCmdList.totalCalls());
}

TEST_F(EnqueueNonUniformKernelEnabledFixture, givenLeoEnableNonUniformWorkGroupsWhenEnqueueNDRangeKernelWithNonUniformRangeThenNonUniformLaunchIsAppended) {
    EXPECT_EQ(CL_SUCCESS, enqueue(nonUniformGlobalWorkSize));

    EXPECT_FALSE(capturingCmdList.appendLaunchKernelArgs.wasCalled());
    ASSERT_EQ(1u, capturingCmdList.appendLaunchKernelNonUniformArgs.count());
    const auto &captured = capturingCmdList.appendLaunchKernelNonUniformArgs[0];
    EXPECT_EQ(l0Kernel->toHandle(), captured.kernel);
    EXPECT_EQ(2u, captured.launchArgs.workDim);
    for (uint32_t dim = 0u; dim < 3u; dim++) {
        EXPECT_EQ(nonUniformGlobalWorkSize[dim], captured.launchArgs.globalWorkSize[dim]);
        EXPECT_EQ(enqueuedGroupSize[dim], captured.launchArgs.enqueuedGroupSize[dim]);
    }
}

TEST_F(EnqueueNonUniformKernelEnabledFixture, givenLeoEnableNonUniformWorkGroupsWhenEnqueueNDRangeKernelWithUniformRangeThenRegularLaunchIsAppended) {
    EXPECT_EQ(CL_SUCCESS, enqueue(uniformGlobalWorkSize));

    EXPECT_FALSE(capturingCmdList.appendLaunchKernelNonUniformArgs.wasCalled());
    ASSERT_EQ(1u, capturingCmdList.appendLaunchKernelArgs.count());
    EXPECT_EQ(2u, capturingCmdList.appendLaunchKernelArgs[0].groupCount.groupCountX);
    EXPECT_EQ(2u, capturingCmdList.appendLaunchKernelArgs[0].groupCount.groupCountY);
}

struct EnqueueSvmFixture : public Test<LeoCaptureFixture> {
    cl_command_type queryCommandType(cl_event event) {
        cl_command_type commandType = 0u;
        EXPECT_EQ(CL_SUCCESS, clGetEventInfo(event, CL_EVENT_COMMAND_TYPE, sizeof(commandType), &commandType, nullptr));
        return commandType;
    }

    // Stands in for a caller variable that the API must leave untouched when the enqueue fails.
    cl_event notAnEvent() { return reinterpret_cast<cl_event>(eventStorage.data()); }

    std::array<uint8_t, 64> eventStorage{};
    std::array<uint8_t, 32> dstStorage{};
    std::array<uint8_t, 32> srcStorage{};
    uint32_t pattern = 0u;
};

TEST_F(EnqueueSvmFixture, givenUserProvidedEventWhenClEnqueueSVMMemcpyThenCommandTypeIsSvmMemcpy) {
    cl_event event = nullptr;

    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMMemcpy(getCommandQueue(), CL_FALSE, dstStorage.data(), srcStorage.data(), dstStorage.size(), 0, nullptr, &event));
    ASSERT_NE(nullptr, event);

    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMCPY), queryCommandType(event));

    EXPECT_EQ(CL_SUCCESS, clReleaseEvent(event));
}

TEST_F(EnqueueSvmFixture, givenInvalidDstPtrWhenClEnqueueSVMMemcpyThenUserProvidedEventIsNotDereferenced) {
    cl_event event = notAnEvent();

    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueSVMMemcpy(getCommandQueue(), CL_FALSE, nullptr, srcStorage.data(), dstStorage.size(), 0, nullptr, &event));

    EXPECT_EQ(notAnEvent(), event);
    EXPECT_FALSE(capturingCmdList.appendMemoryCopyArgs.wasCalled());
}

TEST_F(EnqueueSvmFixture, givenOverlappingRegionsWhenClEnqueueSVMMemcpyThenCopyOverlapIsReturnedAndNothingIsEnqueued) {
    constexpr size_t size = 16u;
    std::array<uint8_t, 2 * size> storage{};
    auto base = storage.data();
    const std::array<std::pair<uint8_t *, uint8_t *>, 3> dstSrcPairs = {{{base + size / 2, base},
                                                                         {base, base + size / 2},
                                                                         {base, base}}};

    for (const auto &[dst, src] : dstSrcPairs) {
        cl_event event = notAnEvent();

        EXPECT_EQ(CL_MEM_COPY_OVERLAP, clEnqueueSVMMemcpy(getCommandQueue(), CL_FALSE, dst, src, size, 0, nullptr, &event));

        EXPECT_EQ(notAnEvent(), event);
    }
    EXPECT_FALSE(capturingCmdList.appendMemoryCopyArgs.wasCalled());
}

TEST_F(EnqueueSvmFixture, givenAdjacentRegionsWhenClEnqueueSVMMemcpyThenCopyIsEnqueued) {
    constexpr size_t size = 16u;
    std::array<uint8_t, 2 * size> storage{};

    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMMemcpy(getCommandQueue(), CL_FALSE, storage.data() + size, storage.data(), size, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMMemcpy(getCommandQueue(), CL_FALSE, storage.data(), storage.data() + size, size, 0, nullptr, nullptr));

    EXPECT_EQ(2u, capturingCmdList.appendMemoryCopyArgs.count());
}

TEST_F(EnqueueSvmFixture, givenUserProvidedEventWhenClEnqueueSVMMemFillThenCommandTypeIsSvmMemFill) {
    cl_event event = nullptr;

    EXPECT_EQ(CL_SUCCESS, clEnqueueSVMMemFill(getCommandQueue(), dstStorage.data(), &pattern, sizeof(pattern), dstStorage.size(), 0, nullptr, &event));
    ASSERT_NE(nullptr, event);

    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMFILL), queryCommandType(event));

    EXPECT_EQ(CL_SUCCESS, clReleaseEvent(event));
}

TEST_F(EnqueueSvmFixture, givenInvalidSvmPtrWhenClEnqueueSVMMemFillThenUserProvidedEventIsNotDereferenced) {
    cl_event event = notAnEvent();

    EXPECT_EQ(CL_INVALID_VALUE, clEnqueueSVMMemFill(getCommandQueue(), nullptr, &pattern, sizeof(pattern), dstStorage.size(), 0, nullptr, &event));

    EXPECT_EQ(notAnEvent(), event);
    EXPECT_FALSE(capturingCmdList.appendMemoryFillArgs.wasCalled());
}

struct EnqueueMapImageFixture : public Test<LeoCaptureFixture> {
    void SetUp() override {
        Test<LeoCaptureFixture>::SetUp();
        if (!clDevice->getHardwareInfo().capabilityTable.supportsImages) {
            GTEST_SKIP() << "Product does not support images";
        }
        capturingCmdList.setCmdListContext(context->getL0ContextHandle());

        cl_image_format format{CL_RGBA, CL_FLOAT};
        cl_image_desc desc{};
        desc.image_type = CL_MEM_OBJECT_IMAGE2D;
        desc.image_width = width;
        desc.image_height = height;

        cl_int errcode = CL_INVALID_VALUE;
        image = clCreateImage(clContext, CL_MEM_READ_WRITE, &format, &desc, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, image);

        leoImage = static_cast<Image *>(castToObject<MemObj>(image));
        ASSERT_NE(nullptr, leoImage);
    }

    void TearDown() override {
        if (image != nullptr) {
            EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(image));
        }
        Test<LeoCaptureFixture>::TearDown();
    }

    static constexpr size_t width = 16;
    static constexpr size_t height = 16;
    static constexpr size_t elementSize = 4 * sizeof(float);
    cl_mem image = nullptr;
    Image *leoImage = nullptr;
};

TEST_F(EnqueueMapImageFixture, givenHostPtrSizeAboveMaxMemAllocSizeWhenClEnqueueMapImageThenMappingSucceeds) {
    const auto hostPtrSize = leoImage->getHostptrSize();
    ASSERT_LT(1u, hostPtrSize);
    neoDevice->deviceInfo.maxMemAllocSize = hostPtrSize - 1u;

    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {width, height, 1};
    cl_int retVal = CL_INVALID_VALUE;

    auto mappedPtr = clEnqueueMapImage(getCommandQueue(), image, CL_FALSE, CL_MAP_WRITE, origin, region,
                                       nullptr, nullptr, 0, nullptr, nullptr, &retVal);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, mappedPtr);
    EXPECT_EQ(leoImage->getCpuPtr(), mappedPtr);
}

TEST_F(EnqueueMapImageFixture, givenRegionReachingImageEndWhenClEnqueueMapImageThenMappedRangeFitsInHostAllocation) {
    size_t origin[3] = {width / 2, height / 2, 0};
    size_t region[3] = {width - origin[0], height - origin[1], 1};
    cl_int retVal = CL_INVALID_VALUE;

    auto mappedPtr = clEnqueueMapImage(getCommandQueue(), image, CL_FALSE, CL_MAP_WRITE, origin, region,
                                       nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, mappedPtr);

    const auto mappedOffset = ptrDiff(mappedPtr, leoImage->getCpuPtr());
    const auto mappedEnd = mappedOffset + (region[1] - 1) * width * elementSize + region[0] * elementSize;

    EXPECT_EQ(leoImage->getHostptrSize(), mappedEnd);
}

struct EnqueueMapMipmappedImageFixture : public EnqueueMapImageFixture {
    void SetUp() override {
        EnqueueMapImageFixture::SetUp();
        if (IsSkipped()) {
            return;
        }
        cl_image_format format{CL_RGBA, CL_FLOAT};
        cl_image_desc desc{};
        desc.image_type = CL_MEM_OBJECT_IMAGE2D;
        desc.image_width = width;
        desc.image_height = height;
        desc.num_mip_levels = 3;

        cl_int errcode = CL_INVALID_VALUE;
        mipImage = clCreateImage(clContext, CL_MEM_READ_WRITE, &format, &desc, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, mipImage);
    }

    void TearDown() override {
        if (mipImage != nullptr) {
            EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(mipImage));
        }
        EnqueueMapImageFixture::TearDown();
    }

    void *mapMipLevel(size_t mipLevel, size_t *rowPitch, cl_int *retVal) {
        size_t origin[3] = {0, 0, mipLevel};
        size_t region[3] = {width >> mipLevel, height >> mipLevel, 1};
        return clEnqueueMapImage(getCommandQueue(), mipImage, CL_FALSE, CL_MAP_WRITE, origin, region,
                                 rowPitch, nullptr, 0, nullptr, nullptr, retVal);
    }

    static constexpr size_t mipLevel1Offset = width * height * elementSize;
    static constexpr size_t mipLevel2Offset = mipLevel1Offset + (width / 2) * (height / 2) * elementSize;
    cl_mem mipImage = nullptr;
};

TEST_F(EnqueueMapMipmappedImageFixture, givenMipmappedImageWhenClEnqueueMapImageOnTwoMipLevelsThenEachMipLevelGetsOwnHostRangeAndPitch) {
    size_t rowPitch1 = 0;
    size_t rowPitch2 = 0;
    cl_int retVal1 = CL_INVALID_VALUE;
    cl_int retVal2 = CL_INVALID_VALUE;

    auto mappedPtr1 = mapMipLevel(1u, &rowPitch1, &retVal1);
    auto mappedPtr2 = mapMipLevel(2u, &rowPitch2, &retVal2);
    ASSERT_EQ(CL_SUCCESS, retVal1);
    ASSERT_EQ(CL_SUCCESS, retVal2);

    auto cpuPtr = castToObject<MemObj>(mipImage)->getCpuPtr();
    EXPECT_EQ(ptrOffset(cpuPtr, mipLevel1Offset), mappedPtr1);
    EXPECT_EQ(ptrOffset(cpuPtr, mipLevel2Offset), mappedPtr2);
    EXPECT_EQ((width / 2) * elementSize, rowPitch1);
    EXPECT_EQ((width / 4) * elementSize, rowPitch2);

    EXPECT_EQ(CL_SUCCESS, clEnqueueUnmapMemObject(getCommandQueue(), mipImage, mappedPtr2, 0, nullptr, nullptr));
    EXPECT_EQ(CL_SUCCESS, clEnqueueUnmapMemObject(getCommandQueue(), mipImage, mappedPtr1, 0, nullptr, nullptr));
}

TEST_F(EnqueueMapMipmappedImageFixture, givenMipLevelNotInImageWhenClEnqueueMapImageThenInvalidMipLevelIsReturned) {
    cl_int retVal = CL_SUCCESS;

    auto mappedPtr = mapMipLevel(3u, nullptr, &retVal);

    EXPECT_EQ(CL_INVALID_MIP_LEVEL, retVal);
    EXPECT_EQ(nullptr, mappedPtr);
}

TEST_F(EnqueueMapMipmappedImageFixture, givenMappedMipLevelWhenClEnqueueUnmapMemObjectThenWriteBackUsesMipLevelRowPitch) {
    cl_int retVal = CL_INVALID_VALUE;
    auto mappedPtr = mapMipLevel(2u, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    capturingCmdList.clearCaptures();

    EXPECT_EQ(CL_SUCCESS, clEnqueueUnmapMemObject(getCommandQueue(), mipImage, mappedPtr, 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendImageCopyFromMemoryExtArgs.count());
    const auto &writeBack = capturingCmdList.appendImageCopyFromMemoryExtArgs[0];
    EXPECT_EQ(mappedPtr, writeBack.srcptr);
    EXPECT_EQ((width / 4) * elementSize, writeBack.srcRowPitch);
}

struct EnqueueMapHostBackedImageFixture : public EnqueueMapImageFixture {
    void SetUp() override {
        EnqueueMapImageFixture::SetUp();
        if (IsSkipped()) {
            return;
        }
        hostStorage.resize(unalignedWidth * elementSize * height);

        cl_image_format format{CL_RGBA, CL_FLOAT};
        cl_image_desc desc{};
        desc.image_type = CL_MEM_OBJECT_IMAGE2D;
        desc.image_width = unalignedWidth;
        desc.image_height = height;

        cl_int errcode = CL_INVALID_VALUE;
        hostPtrImage = clCreateImage(clContext, CL_MEM_READ_WRITE, &format, &desc, nullptr, &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, hostPtrImage);
        castToObject<MemObj>(hostPtrImage)->setCpuPtr(hostStorage.data());
    }

    void TearDown() override {
        if (hostPtrImage != nullptr) {
            castToObject<MemObj>(hostPtrImage)->setCpuPtr(nullptr);
            EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(hostPtrImage));
        }
        EnqueueMapImageFixture::TearDown();
    }

    static constexpr size_t unalignedWidth = 329;
    std::vector<uint8_t> hostStorage{};
    cl_mem hostPtrImage = nullptr;
};

TEST_F(EnqueueMapHostBackedImageFixture, givenHostBackedImageWithUnalignedWidthWhenClEnqueueMapImageThenMappedPtrAndPitchesFollowHostPtrLayout) {
    size_t origin[3] = {3, height - 1, 0};
    size_t region[3] = {unalignedWidth - origin[0], 1, 1};
    size_t rowPitch = 0;
    size_t slicePitch = std::numeric_limits<size_t>::max();
    cl_int retVal = CL_INVALID_VALUE;

    auto mappedPtr = clEnqueueMapImage(getCommandQueue(), hostPtrImage, CL_FALSE, CL_MAP_READ, origin, region,
                                       &rowPitch, &slicePitch, 0, nullptr, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(ptrOffset(hostStorage.data(), origin[1] * unalignedWidth * elementSize + origin[0] * elementSize), mappedPtr);
    EXPECT_EQ(unalignedWidth * elementSize, rowPitch);
    EXPECT_EQ(0u, slicePitch);

    EXPECT_EQ(CL_SUCCESS, clEnqueueUnmapMemObject(getCommandQueue(), hostPtrImage, mappedPtr, 0, nullptr, nullptr));
}

TEST_F(EnqueueMapHostBackedImageFixture, givenMappedHostBackedImageWithUnalignedWidthWhenClEnqueueUnmapMemObjectThenWriteBackUsesHostRowPitch) {
    size_t origin[3] = {0, 0, 0};
    size_t region[3] = {unalignedWidth, height, 1};
    cl_int retVal = CL_INVALID_VALUE;

    auto mappedPtr = clEnqueueMapImage(getCommandQueue(), hostPtrImage, CL_FALSE, CL_MAP_WRITE, origin, region,
                                       nullptr, nullptr, 0, nullptr, nullptr, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    capturingCmdList.clearCaptures();

    EXPECT_EQ(CL_SUCCESS, clEnqueueUnmapMemObject(getCommandQueue(), hostPtrImage, mappedPtr, 0, nullptr, nullptr));

    ASSERT_EQ(1u, capturingCmdList.appendImageCopyFromMemoryExtArgs.count());
    const auto &writeBack = capturingCmdList.appendImageCopyFromMemoryExtArgs[0];
    EXPECT_EQ(hostStorage.data(), writeBack.srcptr);
    EXPECT_EQ(unalignedWidth * elementSize, writeBack.srcRowPitch);
}

struct WhiteBoxMapImageContext : public Context {
    using Context::Context;
    using Context::internalCopyCmdLists;
};

struct EnqueueMapUseHostPtrImageFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        if (!clDevice->getHardwareInfo().capabilityTable.supportsImages) {
            GTEST_SKIP() << "Product does not support images";
        }

        cl_device_id clDeviceId = clDevice;
        whiteBoxContext = std::make_unique<WhiteBoxMapImageContext>(nullptr, context->toHandle(), 1, &clDeviceId, true);
        capturingCmdList.setCmdListContext(whiteBoxContext->getL0ContextHandle());
        whiteBoxContext->internalCopyCmdLists[clDevice->getRootDeviceIndex()] = capturingCmdList.toHandle();
        commandQueue = new CommandQueue(whiteBoxContext.get(), clDevice, nullptr, capturingCmdList.toHandle());
    }

    void TearDown() override {
        if (image != nullptr) {
            EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(image));
        }
        if (commandQueue != nullptr) {
            commandQueue->decRefApi();
        }
        if (whiteBoxContext != nullptr) {
            whiteBoxContext->internalCopyCmdLists.clear();
            whiteBoxContext.reset();
        }
        Test<OclFixture>::TearDown();
    }

    CommandQueue *getCommandQueue() const { return commandQueue; }

    void createUseHostPtrImage(const cl_image_desc &desc) {
        cl_image_format format{CL_RGBA, CL_FLOAT};
        cl_int errcode = CL_INVALID_VALUE;
        image = clCreateImage(whiteBoxContext.get(), CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, &format, &desc, hostMemory.data(), &errcode);
        ASSERT_EQ(CL_SUCCESS, errcode);
        ASSERT_NE(nullptr, image);
    }

    void mapAndUnmapWithExpectedHostLayout(const size_t *origin, const size_t *region, size_t expectedRowPitch, size_t expectedSlicePitch, size_t expectedOffset) {
        size_t rowPitch = 0;
        size_t slicePitch = std::numeric_limits<size_t>::max();
        cl_int retVal = CL_INVALID_VALUE;

        auto mappedPtr = clEnqueueMapImage(getCommandQueue(), image, CL_FALSE, CL_MAP_READ | CL_MAP_WRITE, origin, region,
                                           &rowPitch, &slicePitch, 0, nullptr, nullptr, &retVal);
        ASSERT_EQ(CL_SUCCESS, retVal);

        EXPECT_EQ(expectedRowPitch, rowPitch);
        EXPECT_EQ(expectedSlicePitch, slicePitch);
        EXPECT_EQ(ptrOffset(static_cast<void *>(hostMemory.data()), expectedOffset), mappedPtr);

        const auto &readArgs = capturingCmdList.appendImageCopyToMemoryExtArgs;
        ASSERT_EQ(1u, readArgs.count());
        EXPECT_EQ(mappedPtr, readArgs[0].dstptr);
        EXPECT_EQ(expectedRowPitch, readArgs[0].destRowPitch);
        EXPECT_EQ(expectedSlicePitch, readArgs[0].destSlicePitch);

        EXPECT_EQ(CL_SUCCESS, clEnqueueUnmapMemObject(getCommandQueue(), image, mappedPtr, 0, nullptr, nullptr));

        const auto &copyArgs = capturingCmdList.appendImageCopyFromMemoryExtArgs;
        ASSERT_TRUE(copyArgs.wasCalled());
        const auto &unmapWrite = copyArgs[copyArgs.count() - 1];
        EXPECT_EQ(static_cast<const void *>(mappedPtr), unmapWrite.srcptr);
        EXPECT_EQ(expectedRowPitch, unmapWrite.srcRowPitch);
        EXPECT_EQ(expectedSlicePitch, unmapWrite.srcSlicePitch);
    }

    static constexpr size_t width = 21;
    static constexpr size_t height = 8;
    static constexpr size_t depth = 4;
    static constexpr size_t elementSize = 4 * sizeof(float);
    static constexpr size_t hostRowPitch = width * elementSize;
    std::vector<uint8_t> hostMemory = std::vector<uint8_t>(width * height * depth * elementSize, 0);
    ClDevice *clDevice = nullptr;
    std::unique_ptr<WhiteBoxMapImageContext> whiteBoxContext;
    CapturingCommandList capturingCmdList{};
    CommandQueue *commandQueue = nullptr;
    cl_mem image = nullptr;
};

TEST_F(EnqueueMapUseHostPtrImageFixture, given3dImageWithUseHostPtrWhenClEnqueueMapImageThenHostPtrPackedLayoutIsUsed) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_depth = depth;
    createUseHostPtrImage(desc);

    const size_t hostSlicePitch = hostRowPitch * height;
    const size_t origin[3] = {1, 2, 3};
    const size_t region[3] = {width - 1, height - 2, 1};
    const size_t expectedOffset = origin[2] * hostSlicePitch + origin[1] * hostRowPitch + origin[0] * elementSize;

    mapAndUnmapWithExpectedHostLayout(origin, region, hostRowPitch, hostSlicePitch, expectedOffset);
}

TEST_F(EnqueueMapUseHostPtrImageFixture, given2dImageWithUseHostPtrWhenClEnqueueMapImageThenHostPtrPackedLayoutWithZeroSlicePitchIsUsed) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = width;
    desc.image_height = height;
    createUseHostPtrImage(desc);

    const size_t origin[3] = {3, 5, 0};
    const size_t region[3] = {width - 3, height - 5, 1};
    const size_t expectedOffset = origin[1] * hostRowPitch + origin[0] * elementSize;

    mapAndUnmapWithExpectedHostLayout(origin, region, hostRowPitch, 0u, expectedOffset);
}

TEST_F(EnqueueMapUseHostPtrImageFixture, given1dImageArrayWithUseHostPtrWhenClEnqueueMapImageThenSlicePitchEqualsHostRowPitch) {
    cl_image_desc desc{};
    desc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    desc.image_width = width;
    desc.image_array_size = height;
    createUseHostPtrImage(desc);

    const size_t origin[3] = {2, 6, 0};
    const size_t region[3] = {width - 2, height - 6, 1};
    const size_t expectedOffset = origin[1] * hostRowPitch + origin[0] * elementSize;

    mapAndUnmapWithExpectedHostLayout(origin, region, hostRowPitch, hostRowPitch, expectedOffset);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
