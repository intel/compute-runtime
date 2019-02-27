/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/api/dispatch.h"
#include "runtime/helpers/string.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/sharing_factory.h"

#include "gtest/gtest.h"

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
