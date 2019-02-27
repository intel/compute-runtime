/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/memory_management.h"

#include "gtest/gtest.h"

using namespace OCLRT;

class DestructorCallbackFixture : public MemoryManagementFixture {
  public:
    DestructorCallbackFixture() {
    }

    void SetUp() override {
        MemoryManagementFixture::SetUp();
        BufferDefaults::context = new MockContext;
    }

    void TearDown() override {
        delete BufferDefaults::context;
        MemoryManagementFixture::TearDown();
    }

  protected:
    cl_int retVal = CL_SUCCESS;
};

typedef Test<DestructorCallbackFixture> DestructorCallbackTest;

static std::vector<int> calls(32);
void CL_CALLBACK callBack1(cl_mem memObj, void *userData) {
    calls.push_back(1);
}
void CL_CALLBACK callBack2(cl_mem memObj, void *userData) {
    calls.push_back(2);
}
void CL_CALLBACK callBack3(cl_mem memObj, void *userData) {
    calls.push_back(3);
}
TEST_F(DestructorCallbackTest, checkCallOrder) {
    auto buffer = BufferHelper<BufferUseHostPtr<>>::create();

    auto address = buffer->getCpuAddress();
    EXPECT_NE(nullptr, address);

    calls.clear();

    retVal = buffer->setDestructorCallback(callBack1, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = buffer->setDestructorCallback(callBack2, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = buffer->setDestructorCallback(callBack3, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    delete buffer;

    ASSERT_EQ(3u, calls.size());
    EXPECT_EQ(3, calls[0]);
    EXPECT_EQ(2, calls[1]);
    EXPECT_EQ(1, calls[2]);

    calls.clear();
}

TEST_F(DestructorCallbackTest, doFailAllocations) {
    std::shared_ptr<MockContext> context(new MockContext);
    InjectedFunction method = [this, context](size_t failureIndex) {
        char hostPtr[42];
        auto buffer = Buffer::create(
            context.get(),
            CL_MEM_USE_HOST_PTR,
            sizeof(hostPtr),
            hostPtr,
            retVal);

        // if failures are injected into Buffer::create, we ignore them
        // we are only interested in setDestructorCallback
        if (retVal == CL_SUCCESS && buffer != nullptr) {
            auto address = buffer->getCpuAddress();
            EXPECT_NE(nullptr, address);

            calls.clear();

            retVal = buffer->setDestructorCallback(callBack1, nullptr);

            if (nonfailingAllocation == failureIndex) {
                EXPECT_EQ(CL_SUCCESS, retVal);
            } else {
                EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal) << "for allocation " << failureIndex;
            }

            delete buffer;

            if (nonfailingAllocation == failureIndex) {
                EXPECT_EQ(1u, calls.size());
            } else {
                EXPECT_EQ(0u, calls.size());
            }
        }
    };
    injectFailures(method);
}
