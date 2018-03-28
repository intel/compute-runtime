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

#include "gtest/gtest.h"
#include "runtime/helpers/string.h"
#include "runtime/sharings/sharing.h"
#include "runtime/sharings/sharing_factory.h"
#include "unit_tests/fixtures/memory_management_fixture.h"

using namespace OCLRT;

class SharingFactoryStateRestore : public SharingFactory {
  public:
    SharingFactoryStateRestore() {
        memcpy_s(savedState, sizeof(savedState), sharingContextBuilder, sizeof(sharingContextBuilder));
    }
    ~SharingFactoryStateRestore() {
        memcpy_s(sharingContextBuilder, sizeof(sharingContextBuilder), savedState, sizeof(savedState));
    }

    void clearCurrentState() {
        for (size_t i = 0; i < sizeof(sharingContextBuilder) / sizeof(*sharingContextBuilder); i++) {
            sharingContextBuilder[i] = nullptr;
        }
    }

    template <typename F>
    void registerSharing(SharingType type) {
        auto object = std::unique_ptr<F>(new F);
        sharingContextBuilder[type] = object.get();
        sharings.push_back(std::move(object));
    }

    template <typename Sharing>
    Sharing *getSharing();

  protected:
    decltype(SharingFactory::sharingContextBuilder) savedState;
    std::vector<std::unique_ptr<SharingBuilderFactory>> sharings;
};

class TestedSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override {
        return nullptr;
    }
    std::string getExtensions() override {
        return extension;
    };
    void fillGlobalDispatchTable() override {
        invocationCount++;
    };
    void *getExtensionFunctionAddress(const std::string &functionName) override {
        if (functionName == "someFunction")
            invocationCount++;
        return nullptr;
    }

    static const std::string extension;
    uint32_t invocationCount = 0u;
};
const std::string TestedSharingBuilderFactory::extension("--extensions--");

template <>
TestedSharingBuilderFactory *SharingFactoryStateRestore::getSharing() {
    return reinterpret_cast<TestedSharingBuilderFactory *>(sharingContextBuilder[SharingType::CLGL_SHARING]);
}

void dummyHandler() {
}

class MockSharingBuilderFactory : public TestedSharingBuilderFactory {
  public:
    void *getExtensionFunctionAddress(const std::string &functionName) override {
        if (functionName == "dummyHandler") {
            return reinterpret_cast<void *>(dummyHandler);
        } else {
            return nullptr;
        }
    }
};

TEST(SharingFactoryTests, givenFactoryWithEmptyTableWhenAskedForExtensionThenEmptyStringIsReturned) {
    SharingFactoryStateRestore stateRestore;

    stateRestore.clearCurrentState();
    auto ext = stateRestore.getExtensions();
    EXPECT_EQ(0u, ext.length());
    EXPECT_STREQ("", ext.c_str());
}

TEST(SharingFactoryTests, givenFactoryWithSharingWhenAskedForExtensionThenStringIsReturned) {
    SharingFactoryStateRestore stateRestore;

    stateRestore.clearCurrentState();
    stateRestore.registerSharing<TestedSharingBuilderFactory>(SharingType::CLGL_SHARING);

    auto ext = stateRestore.getExtensions();
    EXPECT_EQ(TestedSharingBuilderFactory::extension.length(), ext.length());
    EXPECT_STREQ(TestedSharingBuilderFactory::extension.c_str(), ext.c_str());
}

TEST(SharingFactoryTests, givenFactoryWithSharingWhenDispatchFillRequestedThenMethodsAreInvoked) {
    SharingFactoryStateRestore stateRestore;

    stateRestore.clearCurrentState();
    stateRestore.registerSharing<TestedSharingBuilderFactory>(SharingType::CLGL_SHARING);
    auto sharing = stateRestore.getSharing<TestedSharingBuilderFactory>();
    ASSERT_EQ(0u, sharing->invocationCount);

    stateRestore.fillGlobalDispatchTable();

    EXPECT_EQ(1u, sharing->invocationCount);
}

TEST(SharingFactoryTests, givenFactoryWithSharingWhenAskedThenAddressIsReturned) {
    SharingFactoryStateRestore stateRestore;

    stateRestore.clearCurrentState();
    stateRestore.registerSharing<TestedSharingBuilderFactory>(SharingType::CLGL_SHARING);
    auto sharing = stateRestore.getSharing<TestedSharingBuilderFactory>();
    ASSERT_EQ(0u, sharing->invocationCount);

    auto ptr = stateRestore.getExtensionFunctionAddress("someFunction");
    EXPECT_EQ(nullptr, ptr);

    EXPECT_EQ(1u, sharing->invocationCount);
}

TEST(SharingFactoryTests, givenMockFactoryWithSharingWhenAskedThenAddressIsReturned) {
    SharingFactoryStateRestore stateRestore;

    stateRestore.clearCurrentState();
    stateRestore.registerSharing<MockSharingBuilderFactory>(SharingType::CLGL_SHARING);
    auto ptr = stateRestore.getExtensionFunctionAddress("dummyHandler");

    EXPECT_EQ(reinterpret_cast<void *>(dummyHandler), ptr);

    ptr = clGetExtensionFunctionAddress("dummyHandler");
    EXPECT_EQ(reinterpret_cast<void *>(dummyHandler), ptr);
}
