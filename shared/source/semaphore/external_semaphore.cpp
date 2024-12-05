/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/semaphore/external_semaphore.h"

#include "shared/source/device/device.h"
#include "shared/source/os_interface/external_semaphore_helper.h"

namespace NEO {

std::unique_ptr<ExternalSemaphore> ExternalSemaphore::create(OSInterface *osInterface, ExternalSemaphoreHelper::Type type, void *handle, int fd) {
    auto extSemaphore = std::make_unique<ExternalSemaphore>();
    extSemaphore->externalSemaphoreHelper = ExternalSemaphoreHelper::create(osInterface);

    if (extSemaphore->externalSemaphoreHelper == nullptr) {
        return nullptr;
    }

    bool result = extSemaphore->externalSemaphoreHelper->importSemaphore(handle, fd, 0, nullptr, type, false, extSemaphore->getHandle());
    if (result == false) {
        return nullptr;
    }

    return extSemaphore;
}

bool ExternalSemaphore::enqueueWait(CommandStreamReceiver *csr) {
    return false;
}

bool ExternalSemaphore::enqueueSignal(CommandStreamReceiver *csr) {
    return false;
}

} // namespace NEO