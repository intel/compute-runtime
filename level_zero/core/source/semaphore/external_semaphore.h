/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/tag_allocator.h"

#include "level_zero/core/source/helpers/api_handle_helper.h"
#include "level_zero/ze_intel_gpu.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <memory>
#include <mutex>

struct _ze_intel_external_semaphore_exp_handle_t {
    const uint64_t objMagic = objMagicValue;
};

namespace L0 {

struct ExternalSemaphore : _ze_intel_external_semaphore_exp_handle_t {
    virtual ~ExternalSemaphore() = default;

    static ze_result_t importExternalSemaphore(ze_device_handle_t device, const ze_intel_external_semaphore_exp_desc_t *semaphoreDesc, ze_intel_external_semaphore_exp_handle_t *phSemaphore);
    virtual ze_result_t releaseExternalSemaphore() = 0;

    virtual ze_result_t appendWait(const NEO::CommandStreamReceiver *csr) = 0;
    virtual ze_result_t appendSignal(const NEO::CommandStreamReceiver *csr) = 0;

    static ExternalSemaphore *fromHandle(ze_intel_external_semaphore_exp_handle_t handle) { return static_cast<ExternalSemaphore *>(handle); }
    inline ze_intel_external_semaphore_exp_handle_t toHandle() { return this; }

  protected:
    ze_intel_external_semaphore_exp_desc_t desc;
};

} // namespace L0
