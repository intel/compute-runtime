/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/source/kernel/kernel_execution_type.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_base_object.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/test/common/fixtures/leo_capture_fixture.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <array>
#include <limits>

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

    const auto &imgInfo = leoImage->getL0Object()->getImageInfo();
    const auto mappedOffset = ptrDiff(mappedPtr, leoImage->getCpuPtr());
    const auto mappedEnd = mappedOffset + (region[1] - 1) * imgInfo.rowPitch + region[0] * imgInfo.surfaceFormat->imageElementSizeInBytes;

    EXPECT_EQ(leoImage->getHostptrSize(), mappedEnd);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
