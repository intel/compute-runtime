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

#include "runtime/api/dispatch.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/sharing_factory.h"
#include "gtest/gtest.h"
#include "runtime/helpers/string.h"

using namespace OCLRT;

class IcdRestore : public SharingFactory {
  public:
    IcdRestore() {
        icdSnapshot = icdGlobalDispatchTable;
        memcpy_s(savedState, sizeof(savedState), sharingContextBuilder, sizeof(sharingContextBuilder));
        for (size_t i = 0; i < sizeof(sharingContextBuilder) / sizeof(*sharingContextBuilder); i++) {
            sharingContextBuilder[i] = nullptr;
        }
    }

    ~IcdRestore() {
        memcpy_s(sharingContextBuilder, sizeof(sharingContextBuilder), savedState, sizeof(savedState));
        icdGlobalDispatchTable = icdSnapshot;
    }

    template <typename F>
    void registerSharing(SharingType type) {
        auto object = std::unique_ptr<F>(new F);
        sharingContextBuilder[type] = object.get();
        sharings.push_back(std::move(object));
    }

  protected:
    decltype(icdGlobalDispatchTable) icdSnapshot;
    decltype(SharingFactory::sharingContextBuilder) savedState;
    std::vector<std::unique_ptr<SharingBuilderFactory>> sharings;
};

void fakeGlF() {
}

class PlatformTestedSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override {
        return nullptr;
    }
    std::string getExtensions() override {
        return "--extension--";
    };
    void fillGlobalDispatchTable() override {
        icdGlobalDispatchTable.clCreateFromGLBuffer = (decltype(icdGlobalDispatchTable.clCreateFromGLBuffer)) & fakeGlF;
    };
    void *getExtensionFunctionAddress(const std::string &functionName) override {
        return nullptr;
    }
};

class MockPlatform : public Platform {
  public:
    void wrapFillGlobalDispatchTable() {
        Platform::fillGlobalDispatchTable();
    }
};

TEST(PlatformIcdTest, WhenPlatformSetupThenDispatchTableInitialization) {
    IcdRestore icdRestore;
    icdGlobalDispatchTable.clCreateFromGLBuffer = nullptr;
    EXPECT_EQ(nullptr, icdGlobalDispatchTable.clCreateFromGLBuffer);

    MockPlatform myPlatform;
    myPlatform.wrapFillGlobalDispatchTable();
    EXPECT_EQ(nullptr, icdGlobalDispatchTable.clCreateFromGLBuffer);

    icdRestore.registerSharing<PlatformTestedSharingBuilderFactory>(SharingType::CLGL_SHARING);
    myPlatform.wrapFillGlobalDispatchTable();
    EXPECT_NE(nullptr, icdGlobalDispatchTable.clCreateFromGLBuffer);
}
