/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/aligned_memory.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/helpers/memory_properties_flags_helpers.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/mock_context.h"

using namespace NEO;

namespace ULT {

template <typename FamilyType>
class CommandStreamReceiverMock : public UltCommandStreamReceiver<FamilyType> {
  private:
    std::vector<GraphicsAllocation *> toFree; // pointers to be freed on destruction
    Device *pDevice;
    ClDevice *pClDevice;

  public:
    size_t expectedToFreeCount = (size_t)-1;
    CommandStreamReceiverMock(Device *pDevice) : UltCommandStreamReceiver<FamilyType>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex()) {
        this->pDevice = pDevice;
        this->pClDevice = platform()->clDeviceMap[pDevice];
    }

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        EXPECT_NE(nullptr, batchBuffer.commandBufferAllocation->getUnderlyingBuffer());

        toFree.push_back(batchBuffer.commandBufferAllocation);
        batchBuffer.stream->replaceBuffer(nullptr, 0);
        batchBuffer.stream->replaceGraphicsAllocation(nullptr);

        EXPECT_TRUE(this->ownershipMutex.try_lock());
        this->ownershipMutex.unlock();
        return true;
    }

    ~CommandStreamReceiverMock() override {
        EXPECT_FALSE(pClDevice->hasOwnership());
        if (expectedToFreeCount == (size_t)-1) {
            EXPECT_GT(toFree.size(), 0u); //make sure flush was called
        } else {
            EXPECT_EQ(toFree.size(), expectedToFreeCount);
        }

        auto memoryManager = this->getMemoryManager();
        //Now free memory. if CQ/CSR did the same, we will hit double-free
        for (auto p : toFree)
            memoryManager->freeGraphicsMemory(p);
    }
};

struct EnqueueThreadingFixture : public DeviceFixture {
    void SetUp() {
        DeviceFixture::SetUp();
        context = new MockContext(pClDevice);
        pCmdQ = nullptr;
    }

    void TearDown() {
        delete pCmdQ;
        context->release();
        DeviceFixture::TearDown();
    }

    template <typename FamilyType>
    class MyCommandQueue : public CommandQueueHw<FamilyType> {
      public:
        MyCommandQueue(Context *context,
                       ClDevice *device,
                       const cl_queue_properties *props) : CommandQueueHw<FamilyType>(context, device, props), kernel(nullptr) {
        }

        static CommandQueue *create(Context *context,
                                    ClDevice *device,
                                    cl_command_queue_properties props) {
            const cl_queue_properties properties[3] = {CL_QUEUE_PROPERTIES, props, 0};
            return new MyCommandQueue<FamilyType>(context, device, properties);
        }

      protected:
        ~MyCommandQueue() override {
            if (kernel) {
                EXPECT_FALSE(kernel->hasOwnership());
            }
        }
        void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &multiDispatchInfo) override {
            for (auto &dispatchInfo : multiDispatchInfo) {
                auto &kernel = *dispatchInfo.getKernel();
                EXPECT_TRUE(kernel.hasOwnership());
            }
        }

        Kernel *kernel;
    };

    CommandQueue *pCmdQ;
    MockContext *context;

    template <typename FamilyType>
    void createCQ() {
        pCmdQ = MyCommandQueue<FamilyType>::create(context, pClDevice, 0);
        ASSERT_NE(nullptr, pCmdQ);

        auto pCommandStreamReceiver = new CommandStreamReceiverMock<FamilyType>(pDevice);
        pDevice->resetCommandStreamReceiver(pCommandStreamReceiver);
    }
};

typedef Test<EnqueueThreadingFixture> EnqueueThreading;

HWTEST_F(EnqueueThreading, enqueueReadBuffer) {
    createCQ<FamilyType>();

    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, buffer.get());

    void *ptr = ::alignedMalloc(1024u, 4096);
    ASSERT_NE(nullptr, ptr);

    buffer->forceDisallowCPUCopy = true;
    pCmdQ->enqueueReadBuffer(buffer.get(),
                             true,
                             0,
                             1024u,
                             ptr,
                             nullptr,
                             0,
                             nullptr,
                             nullptr);

    alignedFree(ptr);
}

HWTEST_F(EnqueueThreading, enqueueWriteBuffer) {
    createCQ<FamilyType>();

    cl_int retVal;
    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, buffer.get());

    void *ptr = ::alignedMalloc(1024u, 4096);
    ASSERT_NE(nullptr, ptr);

    buffer->forceDisallowCPUCopy = true;
    pCmdQ->enqueueWriteBuffer(buffer.get(),
                              true,
                              0,
                              1024u,
                              ptr,
                              nullptr,
                              0,
                              nullptr,
                              nullptr);

    alignedFree(ptr);
}

HWTEST_F(EnqueueThreading, enqueueCopyBuffer) {
    createCQ<FamilyType>();

    cl_int retVal;
    std::unique_ptr<Buffer> srcBuffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, srcBuffer.get());
    std::unique_ptr<Buffer> dstBuffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, dstBuffer.get());

    pCmdQ->enqueueCopyBuffer(srcBuffer.get(), dstBuffer.get(), 0, 0, 1024u, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueThreading, enqueueCopyBufferRect) {
    createCQ<FamilyType>();

    cl_int retVal;
    std::unique_ptr<Buffer> srcBuffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, srcBuffer.get());
    std::unique_ptr<Buffer> dstBuffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, dstBuffer.get());

    size_t srcOrigin[3] = {1024u, 1, 0};
    size_t dstOrigin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    pCmdQ->enqueueCopyBufferRect(srcBuffer.get(), dstBuffer.get(), srcOrigin, dstOrigin, region, 0, 0, 0, 0, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueThreading, enqueueCopyBufferToImage) {
    createCQ<FamilyType>();
    cl_int retVal;

    std::unique_ptr<Buffer> srcBuffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, srcBuffer.get());
    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1024u;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> dstImage(Image::create(context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, dstImage.get());

    size_t dstOrigin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    pCmdQ->enqueueCopyBufferToImage(srcBuffer.get(), dstImage.get(), 0, dstOrigin, region, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueThreading, enqueueCopyImage) {
    createCQ<FamilyType>();
    cl_int retVal;

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1024u;
    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> srcImage(Image::create(context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image::create(context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, srcImage.get());

    size_t srcOrigin[3] = {1024u, 1, 0};
    size_t dstOrigin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    pCmdQ->enqueueCopyImage(srcImage.get(), dstImage.get(), srcOrigin, dstOrigin, region, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueThreading, enqueueCopyImageToBuffer) {
    createCQ<FamilyType>();
    cl_int retVal;

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1024u;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> srcImage(Image::create(context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, srcImage.get());

    std::unique_ptr<Buffer> dstBuffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, dstBuffer.get());

    size_t srcOrigin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    pCmdQ->enqueueCopyImageToBuffer(srcImage.get(), dstBuffer.get(), srcOrigin, region, 0, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueThreading, enqueueFillBuffer) {
    createCQ<FamilyType>();
    cl_int retVal;

    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, buffer.get());

    cl_int pattern = 0xDEADBEEF;
    pCmdQ->enqueueFillBuffer(buffer.get(), &pattern, sizeof(pattern), 0, 1024u, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueThreading, enqueueFillImage) {
    createCQ<FamilyType>();
    cl_int retVal;

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1024u;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> image(Image::create(context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image.get());

    size_t origin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    int32_t fillColor[4] = {0xCC, 0xCC, 0xCC, 0xCC};

    pCmdQ->enqueueFillImage(image.get(), &fillColor, origin, region, 0, nullptr, nullptr);
}

HWTEST_F(EnqueueThreading, enqueueReadBufferRect) {
    createCQ<FamilyType>();
    cl_int retVal;

    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, buffer.get());

    void *ptr = ::alignedMalloc(1024u, 4096);
    ASSERT_NE(nullptr, ptr);

    size_t bufferOrigin[3] = {1024u, 1, 0};
    size_t hostOrigin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    pCmdQ->enqueueReadBufferRect(buffer.get(), CL_TRUE, bufferOrigin, hostOrigin, region, 0, 0, 0, 0, ptr, 0, nullptr, nullptr);

    ::alignedFree(ptr);
}

HWTEST_F(EnqueueThreading, enqueueReadImage) {
    createCQ<FamilyType>();
    cl_int retVal;

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1024u;

    cl_mem_flags flags = CL_MEM_WRITE_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> image(Image::create(context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image.get());

    void *ptr = ::alignedMalloc(1024u, 4096);
    ASSERT_NE(nullptr, ptr);

    size_t origin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    pCmdQ->enqueueReadImage(image.get(), CL_TRUE, origin, region, 0, 0, ptr, nullptr, 0, nullptr, nullptr);

    ::alignedFree(ptr);
}

HWTEST_F(EnqueueThreading, enqueueWriteBufferRect) {
    createCQ<FamilyType>();
    cl_int retVal;

    std::unique_ptr<Buffer> buffer(Buffer::create(context, CL_MEM_READ_WRITE, 1024u, nullptr, retVal));
    ASSERT_NE(nullptr, buffer.get());

    size_t bufferOrigin[3] = {1024u, 1, 0};
    size_t hostOrigin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    auto hostPtrSize = Buffer::calculateHostPtrSize(hostOrigin, region, 0, 0);
    void *ptr = ::alignedMalloc(hostPtrSize, MemoryConstants::pageSize);
    ASSERT_NE(nullptr, ptr);

    pCmdQ->enqueueWriteBufferRect(buffer.get(), CL_TRUE, bufferOrigin, hostOrigin, region, 0, 0, 0, 0, ptr, 0, nullptr, nullptr);

    ::alignedFree(ptr);
}

HWTEST_F(EnqueueThreading, enqueueWriteImage) {
    createCQ<FamilyType>();
    cl_int retVal;

    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNORM_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc imageDesc;
    memset(&imageDesc, 0, sizeof(imageDesc));

    imageDesc.image_type = CL_MEM_OBJECT_IMAGE1D;
    imageDesc.image_width = 1024u;

    cl_mem_flags flags = CL_MEM_READ_ONLY;
    auto surfaceFormat = Image::getSurfaceFormatFromTable(flags, &imageFormat);
    std::unique_ptr<Image> image(Image::create(context, MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0), flags, 0, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image.get());

    void *ptr = ::alignedMalloc(1024u, 4096);
    ASSERT_NE(nullptr, ptr);

    size_t origin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    pCmdQ->enqueueWriteImage(image.get(), CL_TRUE, origin, region, 0, 0, ptr, nullptr, 0, nullptr, nullptr);

    ::alignedFree(ptr);
}

HWTEST_F(EnqueueThreading, finish) {
    createCQ<FamilyType>();

    // set something to finish
    pCmdQ->taskCount = 1;
    pCmdQ->taskLevel = 1;
    auto csr = (CommandStreamReceiverMock<FamilyType> *)&this->pCmdQ->getGpgpuCommandStreamReceiver();
    csr->expectedToFreeCount = 0u;

    pCmdQ->finish();
}
} // namespace ULT
