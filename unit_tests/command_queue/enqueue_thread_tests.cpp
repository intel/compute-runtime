/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

using namespace OCLRT;

namespace ULT {

template <typename FamilyType>
class CommandStreamReceiverMock : public UltCommandStreamReceiver<FamilyType> {
  private:
    std::vector<GraphicsAllocation *> toFree; // pointers to be freed on destruction
    Device *pDevice;

  public:
    size_t expectedToFreeCount = (size_t)-1;
    CommandStreamReceiverMock(Device *pDevice) : UltCommandStreamReceiver<FamilyType>(*platformDevices[0]) {
        this->pDevice = pDevice;
    }

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer *allocationsForResidency) override {
        EXPECT_NE(nullptr, batchBuffer.commandBufferAllocation->getUnderlyingBuffer());

        toFree.push_back(batchBuffer.commandBufferAllocation);
        batchBuffer.stream->replaceBuffer(nullptr, 0);
        batchBuffer.stream->replaceGraphicsAllocation(nullptr);

        EXPECT_TRUE(pDevice->hasOwnership());
        return 0;
    }

    ~CommandStreamReceiverMock() override {
        EXPECT_FALSE(pDevice->hasOwnership());
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
        context = new MockContext(pDevice);
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
                       Device *device,
                       const cl_queue_properties *props) : CommandQueueHw<FamilyType>(context, device, props), kernel(nullptr) {
        }

        static CommandQueue *create(Context *context,
                                    Device *device,
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
        pCmdQ = MyCommandQueue<FamilyType>::create(context, pDevice, 0);
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
    std::unique_ptr<Image> dstImage(Image::create(context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
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
    std::unique_ptr<Image> srcImage(Image::create(context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image::create(context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
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
    std::unique_ptr<Image> srcImage(Image::create(context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
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
    std::unique_ptr<Image> image(Image::create(context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
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
    std::unique_ptr<Image> image(Image::create(context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image.get());

    void *ptr = ::alignedMalloc(1024u, 4096);
    ASSERT_NE(nullptr, ptr);

    size_t origin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    pCmdQ->enqueueReadImage(image.get(), CL_TRUE, origin, region, 0, 0, ptr, 0, nullptr, nullptr);

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

    void *ptr = ::alignedMalloc(1024u, 4096);
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
    std::unique_ptr<Image> image(Image::create(context, flags, surfaceFormat, &imageDesc, nullptr, retVal));
    ASSERT_NE(nullptr, image.get());

    void *ptr = ::alignedMalloc(1024u, 4096);
    ASSERT_NE(nullptr, ptr);

    size_t origin[3] = {1024u, 1, 0};
    size_t region[3] = {1024u, 1, 1};

    pCmdQ->enqueueWriteImage(image.get(), CL_TRUE, origin, region, 0, 0, ptr, 0, nullptr, nullptr);

    ::alignedFree(ptr);
}

HWTEST_F(EnqueueThreading, finish) {
    createCQ<FamilyType>();

    // set something to finish
    pCmdQ->taskCount = 1;
    pCmdQ->taskLevel = 1;
    auto csr = (CommandStreamReceiverMock<FamilyType> *)&this->pDevice->getCommandStreamReceiver();
    csr->expectedToFreeCount = 0u;

    pCmdQ->finish(false);
}

HWTEST_F(EnqueueThreading, flushWaitList) {
    createCQ<FamilyType>();
    auto csr = (CommandStreamReceiverMock<FamilyType> *)&this->pDevice->getCommandStreamReceiver();
    csr->expectedToFreeCount = 0u;
    pCmdQ->flushWaitList(0, nullptr, 0);
}

HWTEST_F(EnqueueThreading, flushWaitList_ReleaseOwnershipWhenQueueIsBlocked) {

    class MyMockDevice : public MockDevice {

      public:
        MyMockDevice() : MockDevice(*platformDevices[0]) {}

        void setTagAllocation(GraphicsAllocation *tagAllocation) {
            this->tagAllocation = tagAllocation;
        }

        bool takeOwnership(bool waitUntilGet) const override {
            ++takeOwnershipCount;
            Device::takeOwnership(true);
            return true;
        }

        void releaseOwnership() const override {
            ++releaseOwnershipCount;
            Device::releaseOwnership();
        }

        mutable uint32_t takeOwnershipCount = 0;
        mutable uint32_t releaseOwnershipCount = 0;
    };

    class MockCommandQueue : public CommandQueue {

      public:
        MockCommandQueue(MyMockDevice *device) : CommandQueue(nullptr, device, 0) {
            this->myDevice = device;
        }

        bool isQueueBlocked() override {
            isQueueBlockedCount++;
            return (isQueueBlockedCount < 5) ? true : false;
        }
        uint32_t isQueueBlockedCount = 0;
        MyMockDevice *myDevice;
    };

    auto pMyDevice = new MyMockDevice();
    ASSERT_NE(nullptr, pMyDevice);

    auto pCommandStreamReceiver = new CommandStreamReceiverMock<FamilyType>(pMyDevice);
    ASSERT_NE(nullptr, pCommandStreamReceiver);

    auto memoryManager = pCommandStreamReceiver->createMemoryManager(false);
    memoryManager->device = pMyDevice;
    ASSERT_NE(nullptr, memoryManager);
    pMyDevice->setMemoryManager(memoryManager);

    auto pTagAllocation = memoryManager->allocateGraphicsMemory(sizeof(uint32_t));
    *(uint32_t *)(pTagAllocation->getUnderlyingBuffer()) = initialHardwareTag;
    ASSERT_NE(nullptr, pTagAllocation);
    pMyDevice->setTagAllocation(pTagAllocation);

    pMyDevice->resetCommandStreamReceiver(pCommandStreamReceiver);

    auto pMyCmdQ = new MockCommandQueue(pMyDevice);
    ASSERT_NE(nullptr, pMyCmdQ);

    EXPECT_TRUE(pMyCmdQ->isQueueBlocked());

    auto csr = (CommandStreamReceiverMock<FamilyType> *)&pMyDevice->getCommandStreamReceiver();
    csr->expectedToFreeCount = 0u;

    pMyCmdQ->flushWaitList(0, nullptr, 0);

    EXPECT_EQ(pMyDevice->takeOwnershipCount, 0u);
    EXPECT_EQ(pMyDevice->releaseOwnershipCount, 0u);

    delete pMyCmdQ;

    delete pMyDevice;
}
} // namespace ULT
