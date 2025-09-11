/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/blit_properties_container.h"

namespace NEO {

struct EnqueueProperties {
    enum class Operation {
        none,
        blit,
        explicitCacheFlush,
        enqueueWithoutSubmission,
        dependencyResolveOnGpu,
        gpuKernel,
        profilingOnly
    };

    EnqueueProperties() = delete;
    EnqueueProperties(bool blitEnqueue, bool hasKernels, bool isCacheFlushCmd, bool flushDependenciesOnly, bool isFlushWithEvent, bool hasStallingCmds,
                      const BlitPropertiesContainer *blitPropertiesContainer) : hasStallingCmds(hasStallingCmds) {
        if (blitEnqueue) {
            operation = Operation::blit;
            this->blitPropertiesContainer = blitPropertiesContainer;
            return;
        }

        if (hasKernels) {
            operation = Operation::gpuKernel;
            this->blitPropertiesContainer = blitPropertiesContainer;
            return;
        }

        if (isCacheFlushCmd) {
            operation = Operation::explicitCacheFlush;
            return;
        }

        if (flushDependenciesOnly) {
            operation = Operation::dependencyResolveOnGpu;
            return;
        }

        if (isFlushWithEvent) {
            operation = Operation::profilingOnly;
            return;
        }

        operation = Operation::enqueueWithoutSubmission;
    }

    bool isFlushWithoutKernelRequired() const {
        return (operation == Operation::blit) || (operation == Operation::explicitCacheFlush) ||
               (operation == Operation::dependencyResolveOnGpu) || (operation == EnqueueProperties::Operation::profilingOnly);
    }

    bool isStartTimestampOnCpuRequired() const {
        return operation == Operation::enqueueWithoutSubmission;
    }

    const BlitPropertiesContainer *blitPropertiesContainer = nullptr;
    Operation operation = Operation::enqueueWithoutSubmission;
    const bool hasStallingCmds;
};
} // namespace NEO
