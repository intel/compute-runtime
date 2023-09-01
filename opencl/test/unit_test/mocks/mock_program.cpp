/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_program.h"

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/program/program_info_from_patchtokens.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_limits.h"
#include "shared/test/common/mocks/mock_compiler_interface.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/context/context.h"
#include "opencl/source/program/create.inl"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

namespace NEO {
ClDeviceVector toClDeviceVector(ClDevice &clDevice) {
    ClDeviceVector deviceVector;
    deviceVector.push_back(&clDevice);
    return deviceVector;
}

int MockProgram::getInternalOptionsCalled = 0;

std::string MockProgram::getCachedFileName() const {
    CompilerCache cache(CompilerCacheConfig{});
    std::string igcRevision = "0001";
    size_t igcLibSize = 1000;
    time_t igcLibMTime = 0;
    auto hwInfo = this->context->getDevice(0)->getHardwareInfo();
    auto input = ArrayRef<const char>(this->sourceCode.c_str(), this->sourceCode.size());
    auto opts = ArrayRef<const char>(this->options.c_str(), this->options.size());
    auto internalOptions = getInternalOptions();
    auto internalOpts = ArrayRef<const char>(internalOptions.c_str(), internalOptions.size());
    return cache.getCachedFileName(hwInfo, input, opts, internalOpts, ArrayRef<const char>(), ArrayRef<const char>(), igcRevision, igcLibSize, igcLibMTime);
}

} // namespace NEO
