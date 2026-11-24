/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/registry/built_ins_registry.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/program/program_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include <string_view>

using namespace NEO;

TEST(MockBuiltinBinariesTest, givenEmbeddedBuiltinSourcesWhenUnpackingAndDecodingRegisteredBinariesThenEveryKernelFromSourceIsDecoded) {
    MockExecutionEnvironment executionEnvironment;
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    size_t decodedBinaries = 0;

    for (uint32_t kernelIndex = 0; kernelIndex < BuiltIn::toIndex(BuiltIn::BaseKernel::count); ++kernelIndex) {
        const auto baseKernel = static_cast<BuiltIn::BaseKernel>(kernelIndex);
        auto source = RegisterEmbeddedResource::find(BuiltIn::getResourceNames(baseKernel, {}, BuiltIn::CodeType::source, hwInfo).back());
        ASSERT_NE(nullptr, source) << BuiltIn::getAsString(baseKernel);

        std::string_view sourceCode(source->resource, source->resourceLength);
        size_t kernelsInSource = 0;
        for (auto position = sourceCode.find("__kernel"); position != std::string_view::npos; position = sourceCode.find("__kernel", position + 1)) {
            ++kernelsInSource;
        }

        for (const auto &mode : BuiltIn::allAddressingModes) {
            auto binary = RegisterEmbeddedResource::find(BuiltIn::getResourceNames(baseKernel, mode, BuiltIn::CodeType::binary, hwInfo)[0]);
            if (binary == nullptr) {
                continue;
            }

            std::string decodeErrors;
            std::string decodeWarnings;
            auto deviceBinary = unpackSingleDeviceBinary(ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary->resource), binary->resourceLength), {}, getTargetDevice(rootDeviceEnvironment), decodeErrors, decodeWarnings);
            ASSERT_FALSE(deviceBinary.deviceBinary.empty()) << BuiltIn::getAsString(baseKernel) << " " << mode.toString() << ": " << decodeErrors;

            ProgramInfo programInfo;
            auto decodeError = decodeSingleDeviceBinary(programInfo, deviceBinary, decodeErrors, decodeWarnings, rootDeviceEnvironment.getHelper<GfxCoreHelper>()).first;

            EXPECT_EQ(DecodeError::success, decodeError) << BuiltIn::getAsString(baseKernel) << " " << mode.toString() << ": " << decodeErrors;
            EXPECT_TRUE(decodeWarnings.empty()) << BuiltIn::getAsString(baseKernel) << " " << mode.toString() << ": " << decodeWarnings;
            EXPECT_EQ(kernelsInSource, programInfo.kernelInfos.size()) << BuiltIn::getAsString(baseKernel) << " " << mode.toString();
            ++decodedBinaries;
        }
    }
    EXPECT_NE(0u, decodedBinaries);
}
