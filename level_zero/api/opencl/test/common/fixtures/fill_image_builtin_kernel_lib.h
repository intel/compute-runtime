/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/test/unit_tests/mocks/mock_builtin_functions_lib_impl.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include <array>
#include <cstring>
#include <limits>

namespace NEO {
namespace LEO {
namespace ult {

struct FillImageRecordingKernel : public L0::ult::Mock<::L0::KernelImp> {
    ze_result_t setArgRedescribedImage(uint32_t argIndex, ze_image_handle_t argVal, bool isPacked, uint32_t mipLevel) override {
        redescribedImageMipLevel = mipLevel;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setArgumentValue(uint32_t argIndex, size_t argSize, const void *pArgValue) override {
        if (argIndex == dstOffsetArgIndex && argSize == sizeof(dstOffset)) {
            memcpy(dstOffset.data(), pArgValue, sizeof(dstOffset));
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY, uint32_t globalSizeZ,
                                 uint32_t *groupSizeX, uint32_t *groupSizeY, uint32_t *groupSizeZ) override {
        *groupSizeX = 1u;
        *groupSizeY = 1u;
        *groupSizeZ = 1u;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ) override {
        return ZE_RESULT_SUCCESS;
    }

    static constexpr uint32_t dstOffsetArgIndex = 2u;

    std::array<uint32_t, 4> dstOffset{};
    uint32_t redescribedImageMipLevel = std::numeric_limits<uint32_t>::max();
};

struct FillImageBuiltInKernelLib : public L0::ult::MockBuiltInKernelLibImpl {
    using MockBuiltInKernelLibImpl::MockBuiltInKernelLibImpl;

    ::L0::Kernel *getImageFunction(::L0::ImageBuiltIn func, const NEO::BuiltIn::AddressingMode &mode) override {
        return &recordingKernel;
    }

    FillImageRecordingKernel recordingKernel{};
};

} // namespace ult
} // namespace LEO
} // namespace NEO
