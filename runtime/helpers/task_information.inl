/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/task_information.h"

namespace NEO {
template <typename ObjectT>
void KernelOperation::ResourceCleaner::operator()(ObjectT *object) {
    storageForAllocations->storeAllocation(std::unique_ptr<GraphicsAllocation>(object->getGraphicsAllocation()),
                                           REUSABLE_ALLOCATION);
    delete object;
}
} // namespace NEO
