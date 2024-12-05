/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/semaphore/external_semaphore.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/semaphore/external_semaphore.h"
#include <level_zero/ze_api.h>
#include <level_zero/zet_api.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace L0 {

class ExternalSemaphoreImp : public ExternalSemaphore {
  public:
    ze_result_t initialize(ze_device_handle_t device, const ze_intel_external_semaphore_exp_desc_t *semaphoreDesc);
    ze_result_t releaseExternalSemaphore() override;

    ze_result_t appendWait(const NEO::CommandStreamReceiver *csr) override;
    ze_result_t appendSignal(const NEO::CommandStreamReceiver *csr) override;

  protected:
    NEO::Device *neoDevice = nullptr;
    const ze_intel_external_semaphore_exp_desc_t *desc;
    std::unique_ptr<NEO::ExternalSemaphore> neoExternalSemaphore;
};

} // namespace L0