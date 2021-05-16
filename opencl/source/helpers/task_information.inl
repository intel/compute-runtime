/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/task_information.h"

namespace NEO {
template <typename ObjectT>
void KernelOperation::ResourceCleaner::operator()(ObjectT *object) {
    storageForAllocations->storeAllocation(std::unique_ptr<GraphicsAllocation>(object->getGraphicsAllocation()),
                                           REUSABLE_ALLOCATION);
    delete object;
}
} // namespace NEO
