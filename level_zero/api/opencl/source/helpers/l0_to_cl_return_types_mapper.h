/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

#include "CL/cl.h"

#include <array>

struct L0ToClResultMapper {
    constexpr explicit L0ToClResultMapper(ze_result_t result) noexcept : result(result) {}

    L0ToClResultMapper() = delete;
    constexpr L0ToClResultMapper(const L0ToClResultMapper &other) noexcept = default;
    constexpr L0ToClResultMapper(L0ToClResultMapper &&other) noexcept = default;
    ~L0ToClResultMapper() = default;

    [[nodiscard]] constexpr operator cl_int() const noexcept {
        return mapResult(result);
    }

  private:
    struct ResultMapping {
        ze_result_t zeResult;
        cl_int clResult;
    };

    static constexpr std::array<ResultMapping, 35> mappingTable{{{ZE_RESULT_SUCCESS, CL_SUCCESS},
                                                                 {ZE_RESULT_NOT_READY, CL_PROFILING_INFO_NOT_AVAILABLE},
                                                                 {ZE_RESULT_ERROR_DEVICE_LOST, CL_DEVICE_NOT_AVAILABLE},
                                                                 {ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, CL_OUT_OF_HOST_MEMORY},
                                                                 {ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, CL_OUT_OF_RESOURCES},
                                                                 {ZE_RESULT_ERROR_MODULE_BUILD_FAILURE, CL_BUILD_PROGRAM_FAILURE},
                                                                 {ZE_RESULT_ERROR_MODULE_LINK_FAILURE, CL_LINKER_NOT_AVAILABLE},
                                                                 {ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET, CL_DEVICE_NOT_AVAILABLE},
                                                                 {ZE_RESULT_ERROR_DEVICE_IN_LOW_POWER_STATE, CL_DEVICE_NOT_AVAILABLE},
                                                                 {ZE_RESULT_ERROR_INVALID_NATIVE_BINARY, CL_INVALID_BINARY},
                                                                 {ZE_RESULT_ERROR_INVALID_KERNEL_NAME, CL_INVALID_KERNEL_NAME},
                                                                 {ZE_RESULT_ERROR_UNSUPPORTED_IMAGE_FORMAT, CL_IMAGE_FORMAT_NOT_SUPPORTED},
                                                                 {ZE_RESULT_ERROR_INVALID_SIZE, CL_INVALID_BUFFER_SIZE},
                                                                 {ZE_RESULT_ERROR_UNSUPPORTED_SIZE, CL_INVALID_BUFFER_SIZE},
                                                                 {ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, CL_INVALID_OPERATION},
                                                                 {ZE_RESULT_ERROR_INVALID_SYNCHRONIZATION_OBJECT, CL_INVALID_EVENT},
                                                                 {ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, CL_INVALID_WORK_GROUP_SIZE},
                                                                 {ZE_RESULT_ERROR_INVALID_GLOBAL_WIDTH_DIMENSION, CL_INVALID_GLOBAL_WORK_SIZE},
                                                                 {ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX, CL_INVALID_ARG_INDEX},
                                                                 {ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_SIZE, CL_INVALID_ARG_SIZE},
                                                                 {ZE_RESULT_ERROR_UNINITIALIZED, CL_INVALID_VALUE},
                                                                 {ZE_RESULT_ERROR_INVALID_ARGUMENT, CL_INVALID_VALUE},
                                                                 {ZE_RESULT_ERROR_INVALID_NULL_HANDLE, CL_INVALID_VALUE},
                                                                 {ZE_RESULT_ERROR_INVALID_NULL_POINTER, CL_INVALID_VALUE},
                                                                 {ZE_RESULT_ERROR_INVALID_ENUMERATION, CL_INVALID_VALUE},
                                                                 {ZE_RESULT_ERROR_UNSUPPORTED_ENUMERATION, CL_INVALID_VALUE},
                                                                 {ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT, CL_INVALID_VALUE},
                                                                 {ZE_RESULT_ERROR_INVALID_GLOBAL_NAME, CL_INVALID_KERNEL_NAME},
                                                                 {ZE_RESULT_ERROR_INVALID_FUNCTION_NAME, CL_INVALID_KERNEL_NAME},
                                                                 {ZE_RESULT_ERROR_INVALID_KERNEL_ATTRIBUTE_VALUE, CL_INVALID_KERNEL},
                                                                 {ZE_RESULT_ERROR_INVALID_MODULE_UNLINKED, CL_INVALID_PROGRAM},
                                                                 {ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE, CL_INVALID_COMMAND_QUEUE},
                                                                 {ZE_RESULT_ERROR_OVERLAPPING_REGIONS, CL_MEM_COPY_OVERLAP},
                                                                 {ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, CL_INVALID_VALUE},
                                                                 {ZE_RESULT_ERROR_NOT_AVAILABLE, CL_INVALID_VALUE}}};

    static constexpr cl_int mapResult(ze_result_t zeResult) noexcept {
        if (zeResult == ZE_RESULT_SUCCESS) [[likely]] {
            return CL_SUCCESS;
        }

        for (const auto &mapping : mappingTable) {
            if (mapping.zeResult == zeResult) {
                return mapping.clResult;
            }
        }

        return CL_INVALID_VALUE;
    }

    const ze_result_t result{};
};
