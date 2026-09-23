/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/compiler_interface/spirv_extensions_yaml_igc_sample.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>

using namespace NEO;

TEST(DeviceSpirvQueriesMtTest, givenConcurrentInitializationsWhenQueryingSpirvInfoThenCompilerIsQueriedOnce) {
    UltDeviceFactory factory{1, 0};
    auto *device = factory.rootDevices[0];
    auto compiler = std::make_unique<MockCompilerInterface>();
    auto *compilerPtr = compiler.get();
    compiler->spirvExtensionsYAMLOverride = spirvExtensionsYamlIgcSample;
    device->getRootDeviceEnvironmentRef().compilerInterface = std::move(compiler);
    std::atomic_bool started = false;
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < 8; ++i) {
        threads.emplace_back([&]() {
            while (!started.load()) {
                std::this_thread::yield();
            }
            device->initializeSpirvQueries();
            const auto &info = device->getDeviceInfo();
            EXPECT_EQ(spirvExtensionsYamlIgcSampleExtensionCount, info.spirvExtensions.size());
            EXPECT_EQ(spirvExtensionsYamlIgcSampleCapabilityCount + device->getSpirvBaseCapabilities().size() - 1, info.spirvCapabilities.size());
        });
    }
    started = true;
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(1u, compilerPtr->getSpirvExtensionsYAMLCalled);
}
