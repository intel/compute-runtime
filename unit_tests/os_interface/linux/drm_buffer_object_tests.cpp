/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/drm_buffer_object.h"
#include "test.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include "drm/i915_drm.h"

#include <memory>

using namespace NEO;

class TestedBufferObject : public BufferObject {
  public:
    TestedBufferObject(Drm *drm) : BufferObject(drm, 1, 0) {
    }

    void tileBy(uint32_t mode) {
        this->tiling_mode = mode;
    }

    void fillExecObject(drm_i915_gem_exec_object2 &execObject, uint32_t drmContextId) override {
        BufferObject::fillExecObject(execObject, drmContextId);
        execObjectPointerFilled = &execObject;
    }

    void setSize(size_t size) {
        this->size = size;
    }

    drm_i915_gem_exec_object2 *execObjectPointerFilled = nullptr;
};

class DrmBufferObjectFixture {
  public:
    std::unique_ptr<DrmMockCustom> mock;
    TestedBufferObject *bo;
    drm_i915_gem_exec_object2 execObjectsStorage[256];

    void SetUp() {
        this->mock = std::make_unique<DrmMockCustom>();
        ASSERT_NE(nullptr, this->mock);
        bo = new TestedBufferObject(this->mock.get());
        ASSERT_NE(nullptr, bo);
    }

    void TearDown() {
        delete bo;
        if (this->mock->ioctl_expected.total >= 0) {
            EXPECT_EQ(this->mock->ioctl_expected.total, this->mock->ioctl_cnt.total);
        }
    }
};

typedef Test<DrmBufferObjectFixture> DrmBufferObjectTest;

TEST_F(DrmBufferObjectTest, exec) {
    mock->ioctl_expected.total = 1;
    mock->ioctl_res = 0;

    drm_i915_gem_exec_object2 execObjectsStorage = {};
    auto ret = bo->exec(0, 0, 0, false, 1, nullptr, 0u, &execObjectsStorage);
    EXPECT_EQ(mock->ioctl_res, ret);
    EXPECT_EQ(0u, mock->execBuffer.flags);
}

TEST_F(DrmBufferObjectTest, exec_ioctlFailed) {
    mock->ioctl_expected.total = 1;
    mock->ioctl_res = -1;
    mock->errnoValue = EFAULT;
    drm_i915_gem_exec_object2 execObjectsStorage = {};
    EXPECT_EQ(EFAULT, bo->exec(0, 0, 0, false, 1, nullptr, 0u, &execObjectsStorage));
}

TEST_F(DrmBufferObjectTest, setTiling_success) {
    mock->ioctl_expected.total = 1; //set_tiling
    auto ret = bo->setTiling(I915_TILING_X, 0);
    EXPECT_TRUE(ret);
}

TEST_F(DrmBufferObjectTest, setTiling_theSameTiling) {
    mock->ioctl_expected.total = 0; //set_tiling
    bo->tileBy(I915_TILING_X);
    auto ret = bo->setTiling(I915_TILING_X, 0);
    EXPECT_TRUE(ret);
}

TEST_F(DrmBufferObjectTest, setTiling_ioctlFailed) {
    mock->ioctl_expected.total = 1; //set_tiling
    mock->ioctl_res = -1;
    auto ret = bo->setTiling(I915_TILING_X, 0);
    EXPECT_FALSE(ret);
}

TEST_F(DrmBufferObjectTest, givenAddressThatWhenSizeIsAddedCrosses32BitBoundaryWhenExecIsCalledThen48BitFlagIsSet) {
    drm_i915_gem_exec_object2 execObject;

    memset(&execObject, 0, sizeof(execObject));
    bo->setAddress(((uint64_t)1u << 32) - 0x1000u);
    bo->setSize(0x1000);
    bo->fillExecObject(execObject, 1);
    //base address + size > size of 32bit address space
    EXPECT_TRUE(execObject.flags & EXEC_OBJECT_SUPPORTS_48B_ADDRESS);
}

TEST_F(DrmBufferObjectTest, givenAddressThatWhenSizeIsAddedWithin32BitBoundaryWhenExecIsCalledThen48BitFlagSet) {
    drm_i915_gem_exec_object2 execObject;

    memset(&execObject, 0, sizeof(execObject));
    bo->setAddress(((uint64_t)1u << 32) - 0x1000u);
    bo->setSize(0xFFF);
    bo->fillExecObject(execObject, 1);
    //base address + size < size of 32bit address space
    EXPECT_TRUE(execObject.flags & EXEC_OBJECT_SUPPORTS_48B_ADDRESS);
}

TEST_F(DrmBufferObjectTest, onPinIoctlFailed) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[1024]);

    mock->ioctl_expected.total = 1;
    mock->ioctl_res = -1;
    this->mock->errnoValue = EINVAL;

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(this->mock.get()));
    ASSERT_NE(nullptr, boToPin.get());

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    BufferObject *boArray[1] = {boToPin.get()};
    auto ret = bo->pin(boArray, 1, 1);
    EXPECT_EQ(EINVAL, ret);
}

TEST(DrmBufferObjectSimpleTest, givenInvalidBoWhenPinIsCalledThenErrorIsReturned) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[256]);
    std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom);
    ASSERT_NE(nullptr, mock.get());
    std::unique_ptr<TestedBufferObject> bo(new TestedBufferObject(mock.get()));
    ASSERT_NE(nullptr, bo.get());

    // fail DRM_IOCTL_I915_GEM_EXECBUFFER2 in pin
    mock->ioctl_res = -1;

    std::unique_ptr<BufferObject> boToPin(new TestedBufferObject(mock.get()));
    ASSERT_NE(nullptr, boToPin.get());

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    mock->errnoValue = EFAULT;

    BufferObject *boArray[1] = {boToPin.get()};
    auto ret = bo->pin(boArray, 1, 1);
    EXPECT_EQ(EFAULT, ret);
}

TEST(DrmBufferObjectSimpleTest, givenBufferObjectWhenConstructedWithASizeThenTheSizeIsInitialized) {
    std::unique_ptr<DrmMockCustom> drmMock(new DrmMockCustom);
    std::unique_ptr<BufferObject> bo(new BufferObject(drmMock.get(), 1, 0x1000, 0));

    EXPECT_EQ(0x1000u, bo->peekSize());
}

TEST(DrmBufferObjectSimpleTest, givenArrayOfBosWhenPinnedThenAllBosArePinned) {
    std::unique_ptr<uint32_t[]> buff(new uint32_t[256]);
    std::unique_ptr<DrmMockCustom> mock(new DrmMockCustom);
    ASSERT_NE(nullptr, mock.get());
    std::unique_ptr<TestedBufferObject> bo(new TestedBufferObject(mock.get()));
    ASSERT_NE(nullptr, bo.get());
    mock->ioctl_res = 0;

    std::unique_ptr<TestedBufferObject> boToPin(new TestedBufferObject(mock.get()));
    std::unique_ptr<TestedBufferObject> boToPin2(new TestedBufferObject(mock.get()));
    std::unique_ptr<TestedBufferObject> boToPin3(new TestedBufferObject(mock.get()));

    ASSERT_NE(nullptr, boToPin.get());
    ASSERT_NE(nullptr, boToPin2.get());
    ASSERT_NE(nullptr, boToPin3.get());

    BufferObject *array[3] = {boToPin.get(), boToPin2.get(), boToPin3.get()};

    bo->setAddress(reinterpret_cast<uint64_t>(buff.get()));
    auto ret = bo->pin(array, 3, 1);
    EXPECT_EQ(mock->ioctl_res, ret);
    uint32_t bb_end = 0x05000000;
    EXPECT_EQ(buff[0], bb_end);

    EXPECT_LT(0u, mock->execBuffer.batch_len);
    EXPECT_EQ(4u, mock->execBuffer.buffer_count); // 3 bos to pin plus 1 exec bo
    EXPECT_EQ(reinterpret_cast<uintptr_t>(boToPin->execObjectPointerFilled), mock->execBuffer.buffers_ptr);
    EXPECT_NE(nullptr, boToPin2->execObjectPointerFilled);
    EXPECT_NE(nullptr, boToPin3->execObjectPointerFilled);

    bo->setAddress(0llu);
}
