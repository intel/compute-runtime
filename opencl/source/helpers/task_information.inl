/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
template <typename ObjectT>
void KernelOperation::ResourceCleaner::operator()(ObjectT *object) {
    auto *allocation = object->getGraphicsAllocation();
    auto &commandStreamReceiver = storageForAllocations->getCommandStreamReceiver();
    commandStreamReceiver.releaseHeapAllocation(allocation);
    delete object;
}
} // namespace NEO
