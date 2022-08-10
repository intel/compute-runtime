/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_program.h"

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/hash.h"
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
    auto hwInfo = this->context->getDevice(0)->getHardwareInfo();
    auto input = ArrayRef<const char>(this->sourceCode.c_str(), this->sourceCode.size());
    auto opts = ArrayRef<const char>(this->options.c_str(), this->options.size());
    auto internalOptions = getInternalOptions();
    auto internalOpts = ArrayRef<const char>(internalOptions.c_str(), internalOptions.size());
    return cache.getCachedFileName(hwInfo, input, opts, internalOpts);
}

} // namespace NEO
