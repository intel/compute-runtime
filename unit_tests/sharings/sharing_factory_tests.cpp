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
#include "runtime/context/context.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/device/device.h"
#include "runtime/helpers/string.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/sharing.h"
#include "runtime/sharings/sharing_factory.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/mocks/mock_device.h"

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

const cl_context_properties mockContextPassFinalize = 1;
const cl_context_properties mockContextFailFinalize = 2;
const cl_context_properties clContextPropertyMock = 0x2000;

class MockSharingContextBuilder : public SharingContextBuilder {
    cl_context_properties value;

  public:
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

bool MockSharingContextBuilder::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) {
    if (propertyType == clContextPropertyMock) {
        if (propertyValue) {
            value = propertyValue;
            return true;
        }
    }
    return false;
}

bool MockSharingContextBuilder::finalizeProperties(Context &context, int32_t &errcodeRet) {
    if (value == mockContextPassFinalize) {
        return true;
    } else {
        return false;
    }
}

class MockSharingBuilderFactory : public TestedSharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override {
        return std::unique_ptr<SharingContextBuilder>(new MockSharingContextBuilder());
    }
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

TEST(SharingFactoryTests, givenSharingFactoryWhenSharingIsRegisteredThenIsSharingPresentReflectsThatStatus) {
    SharingFactoryStateRestore stateRestore;
    stateRestore.clearCurrentState();
    EXPECT_FALSE(stateRestore.isSharingPresent(SharingType::CLGL_SHARING));
    stateRestore.registerSharing<MockSharingBuilderFactory>(SharingType::CLGL_SHARING);
    EXPECT_TRUE(stateRestore.isSharingPresent(SharingType::CLGL_SHARING));
}

TEST(Context, givenMockSharingBuilderWhenContextWithInvalidPropertiesThenContextCreateShouldFail) {
    SharingFactoryStateRestore stateRestore;

    stateRestore.clearCurrentState();
    stateRestore.registerSharing<MockSharingBuilderFactory>(SharingType::CLGL_SHARING);

    auto device = std::make_unique<MockDevice>(*platformDevices[0]);
    cl_device_id clDevice = static_cast<cl_device_id>(device.get());
    DeviceVector deviceVector((cl_device_id *)&clDevice, 1);
    cl_int retVal;

    auto pPlatform = OCLRT::platform();
    cl_platform_id platformId[1];
    platformId[0] = pPlatform;

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platformId[0], clContextPropertyMock, mockContextPassFinalize, 0};
    cl_context_properties inValidProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platformId[0], clContextPropertyMock, 0, 0};
    cl_context_properties inValidPropertiesFailFinalize[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platformId[0], clContextPropertyMock, mockContextFailFinalize, 0};

    std::unique_ptr<Context> context;

    context.reset(Context::create<Context>(inValidProperties, deviceVector, nullptr, nullptr, retVal));
    EXPECT_EQ(nullptr, context.get());

    context.reset(Context::create<Context>(inValidPropertiesFailFinalize, deviceVector, nullptr, nullptr, retVal));
    EXPECT_EQ(nullptr, context.get());

    context.reset(Context::create<Context>(validProperties, deviceVector, nullptr, nullptr, retVal));
    EXPECT_NE(nullptr, context.get());
};

TEST(Context, GivenVaContextWhenItIsCreatedItInitializesPowerSavingMode) {
    SharingFactoryStateRestore stateRestore;

    stateRestore.clearCurrentState();
    stateRestore.registerSharing<MockSharingBuilderFactory>(SharingType::VA_SHARING);

    auto device = std::make_unique<MockDevice>(*platformDevices[0]);
    cl_device_id clDevice = static_cast<cl_device_id>(device.get());
    DeviceVector deviceVector((cl_device_id *)&clDevice, 1);
    cl_int retVal;

    auto pPlatform = OCLRT::platform();
    cl_platform_id platformId[1];
    platformId[0] = pPlatform;

    auto &commandStreamReceiver = device->getCommandStreamReceiver();
    auto kmdNotifyHelper = commandStreamReceiver.peekKmdNotifyHelper();

    int64_t timeout = 0;
    kmdNotifyHelper->obtainTimeoutParams(timeout, true, 1, 10, 2);
    EXPECT_NE(1, timeout);

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platformId[0],
                                                clContextPropertyMock, mockContextPassFinalize, 0};

    std::unique_ptr<Context> ctx(Context::create<Context>(validProperties, deviceVector, nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, ctx);
    kmdNotifyHelper->obtainTimeoutParams(timeout, true, 1, 10, 2);
    EXPECT_EQ(1, timeout);
}
