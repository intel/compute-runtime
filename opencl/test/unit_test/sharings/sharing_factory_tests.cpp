/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/string.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

#include "opencl/source/context/context.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/source/sharings/sharing_factory.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_sharing_factory.h"

#include "gtest/gtest.h"

using namespace NEO;

class SharingFactoryStateRestore : public SharingFactory {
  public:
    SharingFactoryStateRestore() {
        memcpy_s(savedState, sizeof(savedState), sharingContextBuilder, sizeof(sharingContextBuilder));
    }
    ~SharingFactoryStateRestore() {
        memcpy_s(sharingContextBuilder, sizeof(sharingContextBuilder), savedState, sizeof(savedState));
    }

    void clearCurrentState() {
        for (auto &builder : sharingContextBuilder) {
            builder = nullptr;
        }
    }

    template <typename F>
    void registerSharing(SharingType type) {
        auto object = std::make_unique<F>();
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
    std::string getExtensions(DriverInfo *driverInfo) override {
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
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

bool MockSharingContextBuilder::processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) {
    if (propertyType == clContextPropertyMock) {
        if (propertyValue) {
            value = propertyValue;
            return true;
        }
    }
    return false;
}

class VASharingFunctionsMock : public SharingFunctions {
  public:
    static const uint32_t sharingId = VA_SHARING;
    uint32_t getId() const override { return sharingId; }
};

struct VAMockSharingContextBuilder : public MockSharingContextBuilder {
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

bool VAMockSharingContextBuilder::finalizeProperties(Context &context, int32_t &errcodeRet) {
    auto &mockContext = static_cast<MockContext &>(context);
    mockContext.registerSharingWithId(new VASharingFunctionsMock(), VA_SHARING);
    return true;
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

class VAMockSharingBuilderFactory : public TestedSharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override {
        return std::unique_ptr<SharingContextBuilder>(new VAMockSharingContextBuilder());
    }
};

TEST(SharingFactoryTests, givenFactoryWithEmptyTableWhenAskedForExtensionThenEmptyStringIsReturned) {
    SharingFactoryStateRestore stateRestore;

    stateRestore.clearCurrentState();
    auto ext = stateRestore.getExtensions(nullptr);
    EXPECT_EQ(0u, ext.length());
    EXPECT_STREQ("", ext.c_str());
}

TEST(SharingFactoryTests, givenFactoryWithSharingWhenAskedForExtensionThenStringIsReturned) {
    SharingFactoryStateRestore stateRestore;

    stateRestore.clearCurrentState();
    stateRestore.registerSharing<TestedSharingBuilderFactory>(SharingType::CLGL_SHARING);

    auto ext = stateRestore.getExtensions(nullptr);
    EXPECT_LE(TestedSharingBuilderFactory::extension.length(), ext.length());
    EXPECT_TRUE(hasSubstr(ext, TestedSharingBuilderFactory::extension));
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

TEST(Context, givenMockSharingBuilderWhenContextWithInvalidPropertiesThenContextCreateShouldFail) {
    SharingFactoryStateRestore stateRestore;

    stateRestore.clearCurrentState();
    stateRestore.registerSharing<MockSharingBuilderFactory>(SharingType::CLGL_SHARING);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    cl_device_id clDevice = device.get();
    auto deviceVector = ClDeviceVector(&clDevice, 1);
    cl_int retVal;

    cl_platform_id platformId[] = {platform()};

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platformId[0], clContextPropertyMock, mockContextPassFinalize, 0};
    cl_context_properties invalidProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platformId[0], clContextPropertyMock, 0, 0};
    cl_context_properties invalidPropertiesFailFinalize[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)platformId[0], clContextPropertyMock, mockContextFailFinalize, 0};

    std::unique_ptr<Context> context;

    context.reset(Context::create<Context>(invalidProperties, deviceVector, nullptr, nullptr, retVal));
    EXPECT_EQ(nullptr, context.get());

    context.reset(Context::create<Context>(invalidPropertiesFailFinalize, deviceVector, nullptr, nullptr, retVal));
    EXPECT_EQ(nullptr, context.get());

    context.reset(Context::create<Context>(validProperties, deviceVector, nullptr, nullptr, retVal));
    EXPECT_NE(nullptr, context.get());
};

TEST(SharingFactoryTests, givenDisabledFormatQueryAndFactoryWithSharingWhenAskedForExtensionThenFormatQueryExtensionIsNotReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableFormatQuery.set(false);

    SharingFactoryStateRestore stateRestore;
    stateRestore.clearCurrentState();
    stateRestore.registerSharing<MockSharingBuilderFactory>(SharingType::CLGL_SHARING);

    auto extensionsList = sharingFactory.getExtensions(nullptr);
    EXPECT_FALSE(hasSubstr(extensionsList, Extensions::sharingFormatQuery));
}

TEST(SharingFactoryTests, givenEnabledFormatQueryAndFactoryWithSharingWhenAskedForExtensionThenFormatQueryExtensionIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableFormatQuery.set(true);

    SharingFactoryStateRestore stateRestore;
    stateRestore.clearCurrentState();
    stateRestore.registerSharing<MockSharingBuilderFactory>(SharingType::CLGL_SHARING);

    auto extensionsList = sharingFactory.getExtensions(nullptr);
    EXPECT_TRUE(hasSubstr(extensionsList, Extensions::sharingFormatQuery));
}

TEST(SharingFactoryTests, givenEnabledFormatQueryAndFactoryWithNoSharingsWhenAskedForExtensionThenNoExtensionIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableFormatQuery.set(true);

    SharingFactoryStateRestore sharingFactory;

    sharingFactory.clearCurrentState();

    auto extensionsList = sharingFactory.getExtensions(nullptr);
    EXPECT_FALSE(hasSubstr(extensionsList, Extensions::sharingFormatQuery));
}
