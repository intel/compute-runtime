/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/external_semaphore.h"
#include "shared/source/utilities/stackvec.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/semaphore/external_semaphore.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <memory>

namespace L0 {
struct Device;

struct ExternalSemaphoreOperationData;

class ExternalSemaphoreImp : public ExternalSemaphore {
  public:
    ze_result_t initialize(ze_device_handle_t device, const ze_external_semaphore_ext_desc_t *semaphoreDesc);
    ze_result_t releaseExternalSemaphore() override;

    static void semaphoreWait(NEO::DriverModel &driverModel, const ExternalSemaphoreOperationData &operationData);
    static void semaphoreSignal(NEO::DriverModel &driverModel, const ExternalSemaphoreOperationData &operationData);

    ExternalSemaphore *toBase() { return static_cast<ExternalSemaphore *>(this); }
    static bool areImportedToDevice(Device &device, uint32_t numSemaphores, const ze_external_semaphore_ext_handle_t *hSemaphores);

    std::unique_ptr<NEO::ExternalSemaphore> neoExternalSemaphore;

  protected:
    Device *device = nullptr;
};

struct ExternalSemaphoreOperationData {
    void add(const ExternalSemaphoreImp &semaphore, uint64_t fenceValue);

    StackVec<NEO::ExternalSemaphoreOperation, 4> operations;
};

} // namespace L0
