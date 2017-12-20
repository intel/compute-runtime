/*
 * Copyright (c) 2017, Intel Corporation
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

#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/helpers/memory_management.h"
#include "test.h"
#include "unit_tests/fixtures/buffer_fixture.h"
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
