/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/test/common/fixtures/capturing_command_list.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include <level_zero/ze_api.h>

namespace NEO {
namespace LEO {
namespace ult {

struct FreeMemExtArgs {
    ze_driver_memory_free_policy_ext_flags_t freePolicy;
    void *ptr;

    FreeMemExtArgs(const ze_memory_free_ext_desc_t *freeDesc, void *ptr)
        : freePolicy(freeDesc != nullptr ? freeDesc->freePolicy : static_cast<ze_driver_memory_free_policy_ext_flags_t>(ZE_DRIVER_MEMORY_FREE_POLICY_EXT_FLAG_FORCE_UINT32)),
          ptr(ptr) {}
};

struct CapturingContext : public L0::ult::Mock<L0::Context> {
    using BaseClass = L0::ult::Mock<L0::Context>;

    CapturingContext(L0::DriverHandle *driverHandle, ze_device_handle_t deviceHandle) : BaseClass(driverHandle) {
        this->devices[0] = deviceHandle;
        this->numDevices = 1;
    }

    ze_result_t freeMemExt(const ze_memory_free_ext_desc_t *freeDesc, void *ptr) override {
        this->freeMemExtArgs.records.push_back(FreeMemExtArgs{freeDesc, ptr});
        return freeMemExtResult;
    }

    CapturedApi<FreeMemExtArgs> freeMemExtArgs{};
    ze_result_t freeMemExtResult = ZE_RESULT_SUCCESS;
};

} // namespace ult
} // namespace LEO
} // namespace NEO
