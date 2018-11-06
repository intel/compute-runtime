/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/compiler_interface/compiler_interface.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/mipmap.h"
#include "runtime/mem_obj/image.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/memory_management.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "test.h"

using namespace OCLRT;

static const unsigned int testImageDimensions = 45;
auto channelType = CL_UNORM_INT8;
auto channelOrder = CL_RGBA;
auto const elementSize = 4; //sizeof CL_RGBA * CL_UNORM_INT8

class CreateImageTest : public DeviceFixture,
                        public testing::TestWithParam<uint64_t /*cl_mem_flags*/>,
                        public CommandQueueHwFixture {
    typedef CommandQueueHwFixture CommandQueueFixture;

  public:
    CreateImageTest() {
    }
    Image *createImageWithFlags(cl_mem_flags flags) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        return Image::create(context, flags, surfaceFormat, &imageDesc, nullptr, retVal);
    }

  protected:
    void SetUp() override {
        DeviceFixture::SetUp();
        memoryManager = new MockMemoryManager(*pDevice->getExecutionEnvironment());

        pDevice->injectMemoryManager(memoryManager);

        CommandQueueFixture::SetUp(pDevice, 0);
        flags = GetParam();

        // clang-format off
        imageFormat.image_channel_data_type = channelType;
        imageFormat.image_channel_order     = channelOrder;

        imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width       = testImageDimensions;
        imageDesc.image_height      = testImageDimensions;
        imageDesc.image_depth       = 0;
        imageDesc.image_array_size  = 1;
        imageDesc.image_row_pitch   = 0;
        imageDesc.image_slice_pitch = 0;
        imageDesc.num_mip_levels    = 0;
        imageDesc.num_samples       = 0;
        imageDesc.mem_object = NULL;
        // clang-format on
    }

    void TearDown() override {
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    MockMemoryManager *memoryManager;
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal = CL_SUCCESS;
    cl_mem_flags flags = 0;
    unsigned char pHostPtr[testImageDimensions * testImageDimensions * elementSize * 4];
};

typedef CreateImageTest CreateImageNoHostPtr;

TEST(TestSliceAndRowPitch, ForDifferentDescriptorsGetHostPtrSlicePitchAndRowPitchReturnsProperValues) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceLinearImages.set(true);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    char *hostPtr = (char *)alignedMalloc(width * height * depth * elementSize * 2, 64);

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 1D image with 0 row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = width;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    auto image = Image::create(
        &context,
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(width * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ(0u, image->getHostPtrSlicePitch());

    delete image;

    // 1D image with non-zero row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = width;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = 0;

    image = Image::create(
        &context,
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ(0u, image->getHostPtrSlicePitch());

    delete image;

    // 2D image with non-zero row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = 0;

    image = Image::create(
        &context,
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ(0u, image->getHostPtrSlicePitch());

    delete image;

    // 1D ARRAY image with non-zero row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    imageDesc.image_width = width;
    imageDesc.image_height = 0;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 2;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = 0;

    image = Image::create(
        &context,
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrSlicePitch());

    delete image;

    // 2D ARRAY image with non-zero row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 2;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = 0;

    image = Image::create(
        &context,
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ((width + 1) * elementSize * height, image->getHostPtrSlicePitch());

    delete image;

    // 2D ARRAY image with zero row_pitch and non-zero slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 2;
    imageDesc.image_row_pitch = 0;
    imageDesc.image_slice_pitch = (width + 1) * elementSize * height;

    image = Image::create(
        &context,
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ(width * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ((width + 1) * elementSize * height, image->getHostPtrSlicePitch());

    delete image;

    // 2D ARRAY image with non-zero row_pitch and non-zero slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 2;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = (width + 1) * elementSize * height;

    image = Image::create(
        &context,
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ((width + 1) * elementSize * height, image->getHostPtrSlicePitch());

    delete image;

    // 2D ARRAY image with non-zero row_pitch and non-zero slice_pitch > row_pitch * height
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 2;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = (width + 1) * elementSize * (height + 1);

    image = Image::create(
        &context,
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ((width + 1) * elementSize * (height + 1), image->getHostPtrSlicePitch());

    delete image;

    alignedFree(hostPtr);
}

TEST(TestCreateImage, UseSharedContextToCreateImage) {
    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    context.isSharedContext = true;

    const size_t width = 5;
    const size_t height = 3;
    const size_t depth = 2;
    char *hostPtr = (char *)alignedMalloc(width * height * depth * elementSize * 2, 64);

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 2D image with non-zero row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = (width + 1) * elementSize;
    imageDesc.image_slice_pitch = 0;

    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);

    auto image = Image::create(
        &context,
        flags,
        surfaceFormat,
        &imageDesc,
        hostPtr,
        retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_EQ((width + 1) * elementSize, image->getHostPtrRowPitch());
    EXPECT_EQ(0u, image->getHostPtrSlicePitch());
    EXPECT_TRUE(image->isMemObjZeroCopy());

    delete image;

    alignedFree(hostPtr);
}

TEST(TestCreateImageUseHostPtr, CheckMemoryAllocationForDifferenHostPtrAlignments) {
    KernelBinaryHelper kbHelper(KernelBinaryHelper::BUILT_INS);

    cl_image_format imageFormat;
    cl_image_desc imageDesc;
    cl_int retVal;
    MockContext context;

    const size_t width = 4;
    const size_t height = 32;

    imageFormat.image_channel_data_type = channelType;
    imageFormat.image_channel_order = channelOrder;

    imageDesc.num_mip_levels = 0;
    imageDesc.num_samples = 0;
    imageDesc.mem_object = NULL;

    // 2D image with 0 row_pitch and 0 slice_pitch
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = width;
    imageDesc.image_height = height;
    imageDesc.image_depth = 0;
    imageDesc.image_array_size = 0;
    imageDesc.image_row_pitch = alignUp(alignUp(width, 4) * 4, 0x80); //row pitch for tiled img
    imageDesc.image_slice_pitch = 0;

    void *pageAlignedPointer = alignedMalloc(imageDesc.image_row_pitch * height * 1 * 4 + 256, 4096);
    void *hostPtr[] = {reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(pageAlignedPointer) + 16),   // 16 - byte alignment
                       reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(pageAlignedPointer) + 32),   // 32 - byte alignment
                       reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(pageAlignedPointer) + 64),   // 64 - byte alignment
                       reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(pageAlignedPointer) + 128)}; // 128 - byte alignment

    bool result[] = {false,
                     false,
                     true,
                     true};

    cl_mem_flags flags = CL_MEM_HOST_NO_ACCESS | CL_MEM_USE_HOST_PTR;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    for (int i = 0; i < 4; i++) {
        auto image = Image::create(
            &context,
            flags,
            surfaceFormat,
            &imageDesc,
            hostPtr[i],
            retVal);
        ASSERT_NE(nullptr, image);

        auto address = image->getCpuAddress();
        if (result[i] && !image->allowTiling()) {
            EXPECT_EQ(hostPtr[i], address);
        } else {
            EXPECT_NE(hostPtr[i], address);
        }
        delete image;
    }
    alignedFree(pageAlignedPointer);
}

TEST_P(CreateImageNoHostPtr, validFlags) {
    auto image = createImageWithFlags(flags);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    delete image;
}

TEST_P(CreateImageNoHostPtr, getImageDesc) {
    auto image = createImageWithFlags(flags);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    const auto &imageDesc = image->getImageDesc();

    // Sometimes the user doesn't pass image_row/slice_pitch during a create.
    // Ensure the driver fills in the missing data.
    EXPECT_NE(0u, imageDesc.image_row_pitch);
    EXPECT_GE(imageDesc.image_slice_pitch, imageDesc.image_row_pitch);
    delete image;
}

TEST_P(CreateImageNoHostPtr, withImageGraphicsAllocationReportsImageType) {
    auto image = createImageWithFlags(flags);

    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, image);

    auto allocation = image->getGraphicsAllocation();
    EXPECT_TRUE(allocation->getAllocationType() == GraphicsAllocation::AllocationType::IMAGE);

    auto isImageWritable = !(flags & (CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS));
    EXPECT_EQ(isImageWritable, allocation->isMemObjectsAllocationWithWritableFlags());

    delete image;
}

// Parameterized test that tests image creation with all flags that should be
// valid with a nullptr host ptr
static cl_mem_flags NoHostPtrFlags[] = {
    CL_MEM_READ_WRITE,
    CL_MEM_WRITE_ONLY,
    CL_MEM_READ_ONLY,
    CL_MEM_HOST_READ_ONLY,
    CL_MEM_HOST_WRITE_ONLY,
    CL_MEM_HOST_NO_ACCESS};

INSTANTIATE_TEST_CASE_P(
    CreateImageTest_Create,
    CreateImageNoHostPtr,
    testing::ValuesIn(NoHostPtrFlags));

struct CreateImageHostPtr
    : public CreateImageTest,
      public MemoryManagementFixture {
    typedef CreateImageTest BaseClass;

    CreateImageHostPtr() {
    }

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        BaseClass::SetUp();
    }

    void TearDown() override {
        delete image;
        CompilerInterface::shutdown();
        BaseClass::TearDown();
        MemoryManagementFixture::TearDown();
    }

    Image *createImage(cl_int &retVal) {
        auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
        return Image::create(
            context,
            flags,
            surfaceFormat,
            &imageDesc,
            pHostPtr,
            retVal);
    }

    cl_int retVal = CL_INVALID_VALUE;
    Image *image = nullptr;
};

TEST_P(CreateImageHostPtr, isResidentDefaultsToFalseAfterCreate) {
    KernelBinaryHelper kbHelper(KernelBinaryHelper::BUILT_INS);

    image = createImage(retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_FALSE(image->getGraphicsAllocation()->isResident(0u));
}

TEST_P(CreateImageHostPtr, getAddress) {
    image = createImage(retVal);
    ASSERT_NE(nullptr, image);

    auto address = image->getBasePtrForMap();
    EXPECT_NE(nullptr, address);

    if (!(flags & CL_MEM_USE_HOST_PTR)) {
        EXPECT_EQ(nullptr, image->getHostPtr());
    }

    if (flags & CL_MEM_USE_HOST_PTR) {
        //if size fits within a page then zero copy can be applied, if not RT needs to do a copy of image
        auto computedSize = imageDesc.image_width * elementSize * alignUp(imageDesc.image_height, 4) * imageDesc.image_array_size;
        auto ptrSize = imageDesc.image_width * elementSize * imageDesc.image_height * imageDesc.image_array_size;
        auto alignedRequiredSize = alignSizeWholePage(static_cast<void *>(pHostPtr), computedSize);
        auto alignedPtrSize = alignSizeWholePage(static_cast<void *>(pHostPtr), ptrSize);

        size_t HalignReq = imageDesc.image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ? 64 : 1;
        auto rowPitch = imageDesc.image_width * elementSize;
        auto slicePitch = rowPitch * imageDesc.image_height;
        auto requiredRowPitch = alignUp(imageDesc.image_width, HalignReq) * elementSize;
        auto requiredSlicePitch = requiredRowPitch * alignUp(imageDesc.image_height, 4);

        bool copyRequired = (alignedRequiredSize > alignedPtrSize) | (requiredRowPitch != rowPitch) | (slicePitch != requiredSlicePitch);

        EXPECT_EQ(pHostPtr, address);
        EXPECT_EQ(pHostPtr, image->getHostPtr());

        if (copyRequired) {
            EXPECT_FALSE(image->isMemObjZeroCopy());
        }
    } else {
        EXPECT_NE(pHostPtr, address);
    }

    if (flags & CL_MEM_COPY_HOST_PTR && !image->allowTiling()) {
        // Buffer should contain a copy of host memory
        EXPECT_EQ(0, memcmp(pHostPtr, address, sizeof(testImageDimensions)));
    }
}

TEST_P(CreateImageHostPtr, getSize) {
    image = createImage(retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_NE(0u, image->getSize());
}

TEST_P(CreateImageHostPtr, graphicsAllocationPresent) {
    image = createImage(retVal);
    ASSERT_NE(nullptr, image);

    EXPECT_NE(nullptr, image->getGraphicsAllocation());
}

TEST_P(CreateImageHostPtr, getImageDesc) {
    image = createImage(retVal);
    ASSERT_NE(nullptr, image);

    const auto &imageDesc = image->getImageDesc();
    // clang-format off
    EXPECT_EQ(this->imageDesc.image_type,        imageDesc.image_type);
    EXPECT_EQ(this->imageDesc.image_width,       imageDesc.image_width);
    EXPECT_EQ(this->imageDesc.image_height,      imageDesc.image_height);
    EXPECT_EQ(this->imageDesc.image_depth,       imageDesc.image_depth);
    EXPECT_EQ(0u,                                imageDesc.image_array_size);
    EXPECT_NE(0u,                                imageDesc.image_row_pitch);
    EXPECT_GE(imageDesc.image_slice_pitch,       imageDesc.image_row_pitch);
    EXPECT_EQ(this->imageDesc.num_mip_levels,    imageDesc.num_mip_levels);
    EXPECT_EQ(this->imageDesc.num_samples,       imageDesc.num_samples);
    EXPECT_EQ(this->imageDesc.buffer,            imageDesc.buffer);
    EXPECT_EQ(this->imageDesc.mem_object,        imageDesc.mem_object);
    // clang-format on

    EXPECT_EQ(image->getHostPtrRowPitch(), static_cast<size_t>(imageDesc.image_width * elementSize));
    // Only 3D, and array images can have slice pitch
    int isArrayOr3DType = 0;
    if (this->imageDesc.image_type == CL_MEM_OBJECT_IMAGE3D ||
        this->imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY ||
        this->imageDesc.image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) {
        isArrayOr3DType = 1;
    }

    EXPECT_EQ(image->getHostPtrSlicePitch(), static_cast<size_t>(imageDesc.image_width * elementSize * imageDesc.image_height) * isArrayOr3DType);
    EXPECT_EQ(image->getImageCount(), 1u);
}

TEST_P(CreateImageHostPtr, failedAllocationInjection) {
    InjectedFunction method = [this](size_t failureIndex) {
        // System under test
        image = createImage(retVal);

        if (nonfailingAllocation == failureIndex) {
            EXPECT_EQ(CL_SUCCESS, retVal);
            EXPECT_NE(nullptr, image);
        } else {
            EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal) << "for allocation " << failureIndex;
            EXPECT_EQ(nullptr, image);
        }

        delete image;
        image = nullptr;
    };
    injectFailures(method, 4); // check only first 5 allocations - avoid checks on writeImg call allocations for tiled imgs
}

TEST_P(CreateImageHostPtr, checkWritingOutsideAllocatedMemoryWhileCreatingImage) {
    memoryManager->redundancyRatio = 2;
    memset(pHostPtr, 1, testImageDimensions * testImageDimensions * elementSize * 4);
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    imageDesc.image_height = 1;
    imageDesc.image_row_pitch = elementSize * imageDesc.image_width + 1;
    image = createImage(retVal);

    char *memory = (char *)image->getGraphicsAllocation()->getUnderlyingBuffer();
    auto memorySize = image->getGraphicsAllocation()->getUnderlyingBufferSize() / 2;
    for (size_t i = 0; i < image->getHostPtrSlicePitch(); ++i) {
        if (i < imageDesc.image_width * elementSize) {
            EXPECT_EQ(1, memory[i]);
        } else {
            EXPECT_EQ(0, memory[i]);
        }
    }
    for (size_t i = 0; i < memorySize; ++i) {
        EXPECT_EQ(0, memory[memorySize + i]);
    }

    memoryManager->redundancyRatio = 1;
}

struct ModifyableImage {
    enum { flags = 0 };
    static cl_image_format imageFormat;
    static cl_image_desc imageDesc;
    static void *hostPtr;
    static OCLRT::Context *context;
};

void *ModifyableImage::hostPtr = nullptr;
OCLRT::Context *ModifyableImage::context = nullptr;
cl_image_format ModifyableImage::imageFormat;
cl_image_desc ModifyableImage::imageDesc;

class ImageTransfer : public ::testing::Test {
  public:
    void SetUp() override {
        context = new MockContext();
        ASSERT_NE(context, nullptr);
        ModifyableImage::context = context;
        ModifyableImage::hostPtr = nullptr;
        ModifyableImage::imageFormat = {CL_R, CL_FLOAT};
        ModifyableImage::imageDesc = {CL_MEM_OBJECT_IMAGE1D, 512, 0, 0, 0, 0, 0, 0, 0, {nullptr}};
        hostPtr = nullptr;
        unalignedHostPtr = nullptr;
    }

    void TearDown() override {
        if (context)
            delete context;
        if (hostPtr)
            alignedFree(hostPtr);
    }

    void createHostPtrs(size_t imageSize) {
        hostPtr = alignedMalloc(imageSize + 100, 4096);
        unalignedHostPtr = (char *)hostPtr + 4;
        memset(hostPtr, 0, imageSize + 100);
        memset(unalignedHostPtr, 1, imageSize);
    }

    MockContext *context;
    void *hostPtr;
    void *unalignedHostPtr;
};

TEST_F(ImageTransfer, GivenNonZeroCopyImageWhenDataTransferedFromHostPtrToMemStorageThenNoOverflowOfHostPtr) {
    size_t imageSize = 512 * 4;
    createHostPtrs(imageSize);

    ModifyableImage::imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    ModifyableImage::imageDesc.image_width = 512;
    ModifyableImage::imageDesc.image_height = 0;
    ModifyableImage::imageDesc.image_row_pitch = 0;
    ModifyableImage::imageDesc.image_array_size = 0;
    ModifyableImage::imageFormat.image_channel_order = CL_R;
    ModifyableImage::imageFormat.image_channel_data_type = CL_FLOAT;

    ModifyableImage::hostPtr = unalignedHostPtr;
    Image *imageNonZeroCopy = ImageHelper<ImageUseHostPtr<ModifyableImage>>::create();

    ASSERT_NE(nullptr, imageNonZeroCopy);

    void *memoryStorage = imageNonZeroCopy->getCpuAddress();
    size_t memoryStorageSize = imageNonZeroCopy->getSize();

    ASSERT_NE(memoryStorage, unalignedHostPtr);

    int result = memcmp(memoryStorage, unalignedHostPtr, imageSize);
    EXPECT_EQ(0, result);

    memset(memoryStorage, 0, memoryStorageSize);
    memset((char *)unalignedHostPtr + imageSize, 2, 100 - 4);

    auto &imgDesc = imageNonZeroCopy->getImageDesc();
    MemObjOffsetArray copyOffset = {{0, 0, 0}};
    MemObjSizeArray copySize = {{imgDesc.image_width, imgDesc.image_height, imgDesc.image_depth}};
    imageNonZeroCopy->transferDataFromHostPtr(copySize, copyOffset);

    void *foundData = memchr(memoryStorage, 2, memoryStorageSize);
    EXPECT_EQ(0, foundData);
    delete imageNonZeroCopy;
}

TEST_F(ImageTransfer, GivenNonZeroCopyNonZeroRowPitchImageWhenDataIsTransferedFromHostPtrToMemStorageThenDestinationIsNotOverflowed) {
    ModifyableImage::imageDesc.image_width = 16;
    ModifyableImage::imageDesc.image_row_pitch = 65;
    ModifyableImage::imageFormat.image_channel_data_type = CL_UNORM_INT8;

    size_t imageSize = ModifyableImage::imageDesc.image_row_pitch;
    size_t imageWidth = ModifyableImage::imageDesc.image_width;

    createHostPtrs(imageSize);

    ModifyableImage::hostPtr = unalignedHostPtr;
    Image *imageNonZeroCopy = ImageHelper<ImageUseHostPtr<ModifyableImage>>::create();

    ASSERT_NE(nullptr, imageNonZeroCopy);

    void *memoryStorage = imageNonZeroCopy->getCpuAddress();
    size_t memoryStorageSize = imageNonZeroCopy->getSize();

    ASSERT_NE(memoryStorage, unalignedHostPtr);

    int result = memcmp(memoryStorage, unalignedHostPtr, imageWidth);
    EXPECT_EQ(0, result);

    memset(memoryStorage, 0, memoryStorageSize);
    memset((char *)unalignedHostPtr + imageSize, 2, 100 - 4);

    auto &imgDesc = imageNonZeroCopy->getImageDesc();
    MemObjOffsetArray copyOffset = {{0, 0, 0}};
    MemObjSizeArray copySize = {{imgDesc.image_width, imgDesc.image_height, imgDesc.image_depth}};
    imageNonZeroCopy->transferDataFromHostPtr(copySize, copyOffset);

    void *foundData = memchr(memoryStorage, 2, memoryStorageSize);
    EXPECT_EQ(0, foundData);
    delete imageNonZeroCopy;
}

TEST_F(ImageTransfer, GivenNonZeroCopyNonZeroRowPitchWithExtraBytes1DArrayImageWhenDataIsTransferedForthAndBackThenDataValidates) {
    ModifyableImage::imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    ModifyableImage::imageDesc.image_width = 5;
    ModifyableImage::imageDesc.image_row_pitch = 28; // == (4 * 5) row bytes + (4 * 2) extra bytes
    ModifyableImage::imageDesc.image_array_size = 3;
    ModifyableImage::imageFormat.image_channel_order = CL_RGBA;
    ModifyableImage::imageFormat.image_channel_data_type = CL_UNORM_INT8;

    const size_t imageWidth = ModifyableImage::imageDesc.image_width;
    const size_t imageRowPitchInPixels = ModifyableImage::imageDesc.image_row_pitch / 4;
    const size_t imageHeight = 1;
    const size_t imageCount = ModifyableImage::imageDesc.image_array_size;

    size_t imageSize = ModifyableImage::imageDesc.image_row_pitch * imageHeight * imageCount;

    createHostPtrs(imageSize);

    uint32_t *row = static_cast<uint32_t *>(unalignedHostPtr);

    for (uint32_t arrayIndex = 0; arrayIndex < imageCount; ++arrayIndex) {
        for (uint32_t pixelInRow = 0; pixelInRow < imageRowPitchInPixels; ++pixelInRow) {
            if (pixelInRow < imageWidth) {
                row[pixelInRow] = pixelInRow;
            } else {
                row[pixelInRow] = 66;
            }
        }
        row = row + imageRowPitchInPixels;
    }

    ModifyableImage::hostPtr = unalignedHostPtr;
    Image *imageNonZeroCopy = ImageHelper<ImageUseHostPtr<ModifyableImage>>::create();

    ASSERT_NE(nullptr, imageNonZeroCopy);

    void *memoryStorage = imageNonZeroCopy->getCpuAddress();

    ASSERT_NE(memoryStorage, unalignedHostPtr);

    size_t internalSlicePitch = imageNonZeroCopy->getImageDesc().image_slice_pitch;

    // Check twice, once after image create, and second time after transfer from HostPtrToMemoryStorage
    // when these paths are unified, only one check will be enough
    for (size_t run = 0; run < 2; ++run) {

        row = static_cast<uint32_t *>(unalignedHostPtr);
        unsigned char *internalRow = static_cast<unsigned char *>(memoryStorage);

        if (run == 1) {
            auto &imgDesc = imageNonZeroCopy->getImageDesc();
            MemObjOffsetArray copyOffset = {{0, 0, 0}};
            MemObjSizeArray copySize = {{imgDesc.image_width, imgDesc.image_height, imgDesc.image_depth}};
            imageNonZeroCopy->transferDataFromHostPtr(copySize, copyOffset);
        }

        for (size_t arrayIndex = 0; arrayIndex < imageCount; ++arrayIndex) {
            for (size_t pixelInRow = 0; pixelInRow < imageRowPitchInPixels; ++pixelInRow) {
                if (pixelInRow < imageWidth) {
                    if (memcmp(&row[pixelInRow], &internalRow[pixelInRow * 4], 4)) {
                        EXPECT_FALSE(1) << "Data in memory storage did not validate, row: " << pixelInRow << " array: " << arrayIndex << "\n";
                    }
                } else {
                    // Change extra bytes pattern
                    row[pixelInRow] = 55;
                }
            }
            row = row + imageRowPitchInPixels;
            internalRow = internalRow + internalSlicePitch;
        }
    }

    auto &imgDesc = imageNonZeroCopy->getImageDesc();
    MemObjOffsetArray copyOffset = {{0, 0, 0}};
    MemObjSizeArray copySize = {{imgDesc.image_width, imgDesc.image_height, imgDesc.image_depth}};
    imageNonZeroCopy->transferDataToHostPtr(copySize, copyOffset);

    row = static_cast<uint32_t *>(unalignedHostPtr);

    for (size_t arrayIndex = 0; arrayIndex < imageCount; ++arrayIndex) {
        for (size_t pixelInRow = 0; pixelInRow < imageRowPitchInPixels; ++pixelInRow) {
            if (pixelInRow < imageWidth) {
                if (row[pixelInRow] != pixelInRow) {
                    EXPECT_FALSE(1) << "Data under host_ptr did not validate, row: " << pixelInRow << " array: " << arrayIndex << "\n";
                }
            } else {
                if (row[pixelInRow] != 55) {
                    EXPECT_FALSE(1) << "Data under host_ptr corrupted in extra bytes, row: " << pixelInRow << " array: " << arrayIndex << "\n";
                }
            }
        }
        row = row + imageRowPitchInPixels;
    }

    delete imageNonZeroCopy;
}

// Parameterized test that tests image creation with all flags that should be
// valid with a valid host ptr
static cl_mem_flags ValidHostPtrFlags[] = {
    0 | CL_MEM_USE_HOST_PTR,
    CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
    CL_MEM_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_READ_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_USE_HOST_PTR,
    CL_MEM_HOST_NO_ACCESS | CL_MEM_USE_HOST_PTR,
    0 | CL_MEM_COPY_HOST_PTR,
    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
    CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_READ_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_WRITE_ONLY | CL_MEM_COPY_HOST_PTR,
    CL_MEM_HOST_NO_ACCESS | CL_MEM_COPY_HOST_PTR};

INSTANTIATE_TEST_CASE_P(
    CreateImageTest_Create,
    CreateImageHostPtr,
    testing::ValuesIn(ValidHostPtrFlags));

TEST(ImageGetSurfaceFormatInfoTest, givenNullptrFormatWhenGetSurfaceFormatInfoIsCalledThenReturnsNullptr) {
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, nullptr);
    EXPECT_EQ(nullptr, surfaceFormat);
}

class ImageCompressionTests : public ::testing::Test {
  public:
    class MyMemoryManager : public MockMemoryManager {
      public:
        using MockMemoryManager::MockMemoryManager;
        GraphicsAllocation *allocateGraphicsMemoryForImage(ImageInfo &imgInfo, Gmm *gmm) override {
            mockMethodCalled = true;
            capturedImgInfo = imgInfo;
            return OsAgnosticMemoryManager::allocateGraphicsMemoryForImage(imgInfo, gmm);
        }
        ImageInfo capturedImgInfo = {};
        bool mockMethodCalled = false;
    };

    void SetUp() override {
        mockDevice.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(*platformDevices));
        myMemoryManager = new MyMemoryManager(*mockDevice->getExecutionEnvironment());
        mockDevice->injectMemoryManager(myMemoryManager);
        mockContext.reset(new MockContext(mockDevice.get()));
    }

    std::unique_ptr<MockDevice> mockDevice;
    std::unique_ptr<MockContext> mockContext;
    MyMemoryManager *myMemoryManager = nullptr;

    cl_image_desc imageDesc = {};
    cl_image_format imageFormat{CL_RGBA, CL_UNORM_INT8};
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_int retVal = CL_SUCCESS;
};

TEST_F(ImageCompressionTests, givenTiledImageWhenCreatingAllocationThenPreferRenderCompression) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);

    auto image = std::unique_ptr<Image>(Image::create(mockContext.get(), flags, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);
    EXPECT_TRUE(image->isTiledImage);
    EXPECT_TRUE(myMemoryManager->mockMethodCalled);
    EXPECT_TRUE(myMemoryManager->capturedImgInfo.preferRenderCompression);
}

TEST_F(ImageCompressionTests, givenNonTiledImageWhenCreatingAllocationThenDontPreferRenderCompression) {
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 5;

    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);

    auto image = std::unique_ptr<Image>(Image::create(mockContext.get(), flags, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image);
    EXPECT_FALSE(image->isTiledImage);
    EXPECT_TRUE(myMemoryManager->mockMethodCalled);
    EXPECT_FALSE(myMemoryManager->capturedImgInfo.preferRenderCompression);
}

TEST(ImageTest, givenImageWhenAskedForPtrOffsetForGpuMappingThenReturnCorrectValue) {
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&ctx));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin = {{4, 5, 6}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().ImageElementSizeInBytes * origin[0] +
                            image->getHostPtrRowPitch() * origin[1] + image->getHostPtrSlicePitch() * origin[2];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, givenImageWhenAskedForMcsInfoThenDefaultValuesAreReturned) {
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&ctx));

    auto mcsInfo = image->getMcsSurfaceInfo();
    EXPECT_EQ(0u, mcsInfo.multisampleCount);
    EXPECT_EQ(0u, mcsInfo.qPitch);
    EXPECT_EQ(0u, mcsInfo.pitch);
}

TEST(ImageTest, givenImageWhenAskedForPtrOffsetForCpuMappingThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceLinearImages.set(true);
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&ctx));
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin = {{4, 5, 6}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().ImageElementSizeInBytes * origin[0] +
                            image->getImageDesc().image_row_pitch * origin[1] +
                            image->getImageDesc().image_slice_pitch * origin[2];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, given1DArrayImageWhenAskedForPtrOffsetForMappingThenReturnCorrectValue) {
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelper<Image1dArrayDefaults>::create(&ctx));

    MemObjOffsetArray origin = {{4, 5, 0}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().ImageElementSizeInBytes * origin[0] +
                            image->getImageDesc().image_slice_pitch * origin[1];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, givenImageWhenAskedForPtrLengthForGpuMappingThenReturnCorrectValue) {
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&ctx));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjSizeArray region = {{4, 5, 6}};

    auto retLength = image->calculateMappedPtrLength(region);
    size_t expectedLength = image->getSurfaceFormatInfo().ImageElementSizeInBytes * region[0] +
                            image->getHostPtrRowPitch() * region[1] + image->getHostPtrSlicePitch() * region[2];

    EXPECT_EQ(expectedLength, retLength);
}

TEST(ImageTest, givenImageWhenAskedForPtrLengthForCpuMappingThenReturnCorrectValue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.ForceLinearImages.set(true);
    MockContext ctx;
    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&ctx));
    EXPECT_TRUE(image->mappingOnCpuAllowed());

    MemObjSizeArray region = {{4, 5, 6}};

    auto retLength = image->calculateMappedPtrLength(region);
    size_t expectedLength = image->getSurfaceFormatInfo().ImageElementSizeInBytes * region[0] +
                            image->getImageDesc().image_row_pitch * region[1] +
                            image->getImageDesc().image_slice_pitch * region[2];

    EXPECT_EQ(expectedLength, retLength);
}

TEST(ImageTest, givenMipMapImage3DWhenAskedForPtrOffsetForGpuMappingThenReturnOffsetWithSlicePitch) {
    MockContext ctx;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE3D;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_depth = 5;
    imageDesc.num_mip_levels = 2;

    std::unique_ptr<Image> image(ImageHelper<Image3dDefaults>::create(&ctx, &imageDesc));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin{{1, 1, 1}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().ImageElementSizeInBytes * origin[0] +
                            image->getHostPtrRowPitch() * origin[1] + image->getHostPtrSlicePitch() * origin[2];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, givenMipMapImage2DArrayWhenAskedForPtrOffsetForGpuMappingThenReturnOffsetWithSlicePitch) {
    MockContext ctx;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_array_size = 5;
    imageDesc.num_mip_levels = 2;

    std::unique_ptr<Image> image(ImageHelper<Image2dArrayDefaults>::create(&ctx, &imageDesc));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin{{1, 1, 1}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().ImageElementSizeInBytes * origin[0] +
                            image->getHostPtrRowPitch() * origin[1] + image->getHostPtrSlicePitch() * origin[2];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, givenNonMipMapImage2DArrayWhenAskedForPtrOffsetForGpuMappingThenReturnOffsetWithSlicePitch) {
    MockContext ctx;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
    imageDesc.image_width = 5;
    imageDesc.image_height = 5;
    imageDesc.image_array_size = 5;
    imageDesc.num_mip_levels = 1;

    std::unique_ptr<Image> image(ImageHelper<Image2dArrayDefaults>::create(&ctx, &imageDesc));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin{{1, 1, 1}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().ImageElementSizeInBytes * origin[0] +
                            image->getHostPtrRowPitch() * origin[1] + image->getHostPtrSlicePitch() * origin[2];

    EXPECT_EQ(expectedOffset, retOffset);
}

TEST(ImageTest, givenMipMapImage1DArrayWhenAskedForPtrOffsetForGpuMappingThenReturnOffsetWithSlicePitch) {
    MockContext ctx;
    cl_image_desc imageDesc{};
    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
    imageDesc.image_width = 5;
    imageDesc.image_array_size = 5;
    imageDesc.num_mip_levels = 2;

    std::unique_ptr<Image> image(ImageHelper<Image1dArrayDefaults>::create(&ctx, &imageDesc));
    EXPECT_FALSE(image->mappingOnCpuAllowed());

    MemObjOffsetArray origin{{1, 1, 0}};

    auto retOffset = image->calculateOffsetForMapping(origin);
    size_t expectedOffset = image->getSurfaceFormatInfo().ImageElementSizeInBytes * origin[0] + image->getHostPtrSlicePitch() * origin[1];

    EXPECT_EQ(expectedOffset, retOffset);
}

typedef ::testing::TestWithParam<uint32_t> MipLevelCoordinateTest;

TEST_P(MipLevelCoordinateTest, givenMipmappedImageWhenValidateRegionAndOriginIsCalledThenAdditionalOriginCoordinateIsAnalyzed) {
    size_t origin[4]{};
    size_t region[3]{1, 1, 1};
    cl_image_desc desc = {};
    desc.image_type = GetParam();
    desc.num_mip_levels = 2;
    origin[getMipLevelOriginIdx(desc.image_type)] = 1;
    EXPECT_EQ(CL_SUCCESS, Image::validateRegionAndOrigin(origin, region, desc));
    origin[getMipLevelOriginIdx(desc.image_type)] = 2;
    EXPECT_EQ(CL_INVALID_MIP_LEVEL, Image::validateRegionAndOrigin(origin, region, desc));
}

INSTANTIATE_TEST_CASE_P(MipLevelCoordinate,
                        MipLevelCoordinateTest,
                        ::testing::Values(CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE2D,
                                          CL_MEM_OBJECT_IMAGE2D_ARRAY, CL_MEM_OBJECT_IMAGE3D));

typedef ::testing::TestWithParam<std::pair<uint32_t, bool>> HasSlicesTest;

TEST_P(HasSlicesTest, givenMemObjectTypeWhenHasSlicesIsCalledThenReturnsTrueIfTypeDefinesObjectWithSlicePitch) {
    auto pair = GetParam();
    EXPECT_EQ(pair.second, Image::hasSlices(pair.first));
}

INSTANTIATE_TEST_CASE_P(HasSlices,
                        HasSlicesTest,
                        ::testing::Values(std::make_pair(CL_MEM_OBJECT_IMAGE1D, false),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE1D_ARRAY, true),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D, false),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE2D_ARRAY, true),
                                          std::make_pair(CL_MEM_OBJECT_IMAGE3D, true),
                                          std::make_pair(CL_MEM_OBJECT_BUFFER, false),
                                          std::make_pair(CL_MEM_OBJECT_PIPE, false)));

typedef ::testing::Test ImageTransformTest;
HWTEST_F(ImageTransformTest, givenSurfaceStateWhenTransformImage3dTo2dArrayIsCalledThenSurface2dArrayIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelper<Image3dDefaults>::create(&context));
    auto surfaceState = RENDER_SURFACE_STATE::sInit();
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    surfaceState.setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D);
    surfaceState.setSurfaceArray(false);
    imageHw->transformImage3dTo2dArray(reinterpret_cast<void *>(&surfaceState));
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D, surfaceState.getSurfaceType());
    EXPECT_TRUE(surfaceState.getSurfaceArray());
}

HWTEST_F(ImageTransformTest, givenSurfaceStateWhenTransformImage2dArrayTo3dIsCalledThenSurface3dIsSet) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using SURFACE_TYPE = typename RENDER_SURFACE_STATE::SURFACE_TYPE;
    MockContext context;
    auto image = std::unique_ptr<Image>(ImageHelper<Image3dDefaults>::create(&context));
    auto surfaceState = RENDER_SURFACE_STATE::sInit();
    auto imageHw = static_cast<ImageHw<FamilyType> *>(image.get());
    surfaceState.setSurfaceType(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_2D);
    surfaceState.setSurfaceArray(true);
    imageHw->transformImage2dArrayTo3d(reinterpret_cast<void *>(&surfaceState));
    EXPECT_EQ(SURFACE_TYPE::SURFACE_TYPE_SURFTYPE_3D, surfaceState.getSurfaceType());
    EXPECT_FALSE(surfaceState.getSurfaceArray());
}

template <typename FamilyName>
class MockImageHw : public ImageHw<FamilyName> {
  public:
    MockImageHw(Context *context, const cl_image_format &format, const cl_image_desc &desc, SurfaceFormatInfo &surfaceFormatInfo, GraphicsAllocation *graphicsAllocation) : ImageHw<FamilyName>(context, 0, 0, nullptr, format, desc, false, graphicsAllocation, false, false, 0, 0, surfaceFormatInfo) {
    }

    void setClearColorParams(typename FamilyName::RENDER_SURFACE_STATE *surfaceState, const Gmm *gmm) override;
    bool setClearColorParamsCalled = false;
};

template <typename FamilyName>
void MockImageHw<FamilyName>::setClearColorParams(typename FamilyName::RENDER_SURFACE_STATE *surfaceState, const Gmm *gmm) {
    this->setClearColorParamsCalled = true;
}

using HwImageTest = ::testing::Test;
HWTEST_F(HwImageTest, givenImageHwWhenSettingCCSParamsThenSetClearColorParamsIsCalled) {

    MockContext context;
    OsAgnosticMemoryManager memoryManager(false, false, *context.getDevice(0)->getExecutionEnvironment());
    context.setMemoryManager(&memoryManager);

    cl_image_desc imgDesc = {};
    imgDesc.image_height = 4;
    imgDesc.image_width = 4;
    imgDesc.image_depth = 1;
    imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;

    cl_image_format format = {};
    format.image_channel_data_type = CL_UNORM_INT8;
    format.image_channel_order = CL_RGBA;

    auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
    std::unique_ptr<Gmm> gmm = MockGmm::queryImgParams(imgInfo);

    auto graphicsAllocation = memoryManager.allocateGraphicsMemoryForImage(imgInfo, gmm.release());

    SurfaceFormatInfo formatInfo = {};
    std::unique_ptr<MockImageHw<FamilyType>> mockImage(new MockImageHw<FamilyType>(&context, format, imgDesc, formatInfo, graphicsAllocation));

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = RENDER_SURFACE_STATE::sInit();

    EXPECT_FALSE(mockImage->setClearColorParamsCalled);
    mockImage->setAuxParamsForCCS(&surfaceState, graphicsAllocation->gmm);
    EXPECT_TRUE(mockImage->setClearColorParamsCalled);
}
