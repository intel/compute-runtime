/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_device.h"

#include "gtest/gtest.h"

namespace NEO {
class OSInterface;
} // namespace NEO

using namespace NEO;

struct DeferredOsContextCreationL0Tests : ::testing::Test {
    void SetUp() override {
        device = std::unique_ptr<MockDevice>{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())};
        DeviceFactory::prepareDeviceEnvironments(*device->getExecutionEnvironment());
    }

    std::unique_ptr<OsContext> createOsContext(EngineTypeUsage engineTypeUsage, bool defaultEngine) {
        OSInterface *osInterface = device->getRootDeviceEnvironment().osInterface.get();
        std::unique_ptr<OsContext> osContext{OsContext::create(osInterface, 0, 0, EngineDescriptorHelper::getDefaultDescriptor(engineTypeUsage))};
        EXPECT_FALSE(osContext->isInitialized());
        return osContext;
    }

    void expectContextCreation(EngineTypeUsage engineTypeUsage, bool defaultEngine, bool expectedImmediate) {
        auto osContext = createOsContext(engineTypeUsage, defaultEngine);
        const bool immediate = osContext->isImmediateContextInitializationEnabled(defaultEngine);
        EXPECT_EQ(expectedImmediate, immediate);
        if (immediate) {
            osContext->ensureContextInitialized(false);
            EXPECT_TRUE(osContext->isInitialized());
        }
    }

    void expectDeferredContextCreation(EngineTypeUsage engineTypeUsage, bool defaultEngine) {
        expectContextCreation(engineTypeUsage, defaultEngine, false);
    }

    void expectImmediateContextCreation(EngineTypeUsage engineTypeUsage, bool defaultEngine) {
        expectContextCreation(engineTypeUsage, defaultEngine, true);
    }

    std::unique_ptr<MockDevice> device;
    static inline const EngineTypeUsage engineTypeUsageBlitter{aub_stream::ENGINE_BCS, EngineUsage::regular};
};

TEST_F(DeferredOsContextCreationL0Tests, givenBlitterEngineWhenCreatingOsContextThenOsContextInitializationIsDeferred) {
    DebugManagerStateRestore restore{};

    expectDeferredContextCreation(engineTypeUsageBlitter, false);

    debugManager.flags.DeferOsContextInitialization.set(1);
    expectDeferredContextCreation(engineTypeUsageBlitter, false);

    debugManager.flags.DeferOsContextInitialization.set(0);
    expectImmediateContextCreation(engineTypeUsageBlitter, false);
}
