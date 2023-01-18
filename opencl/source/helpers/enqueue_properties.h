/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/blit_properties_container.h"

namespace NEO {

struct EnqueueProperties {
    enum class Operation {
        None,
        Blit,
        ExplicitCacheFlush,
        EnqueueWithoutSubmission,
        DependencyResolveOnGpu,
        GpuKernel,
        ProfilingOnly
    };

    EnqueueProperties() = delete;
    EnqueueProperties(bool blitEnqueue, bool hasKernels, bool isCacheFlushCmd, bool flushDependenciesOnly, bool isMarkerWithEvent,
                      const BlitPropertiesContainer *blitPropertiesContainer) {
        if (blitEnqueue) {
            operation = Operation::Blit;
            this->blitPropertiesContainer = blitPropertiesContainer;
            return;
        }

        if (hasKernels) {
            operation = Operation::GpuKernel;
            this->blitPropertiesContainer = blitPropertiesContainer;
            return;
        }

        if (isCacheFlushCmd) {
            operation = Operation::ExplicitCacheFlush;
            return;
        }

        if (flushDependenciesOnly) {
            operation = Operation::DependencyResolveOnGpu;
            return;
        }

        if (isMarkerWithEvent) {
            operation = Operation::ProfilingOnly;
            return;
        }

        operation = Operation::EnqueueWithoutSubmission;
    }

    bool isFlushWithoutKernelRequired() const {
        return (operation == Operation::Blit) || (operation == Operation::ExplicitCacheFlush) ||
               (operation == Operation::DependencyResolveOnGpu) || (operation == EnqueueProperties::Operation::ProfilingOnly);
    }

    const BlitPropertiesContainer *blitPropertiesContainer = nullptr;
    Operation operation = Operation::EnqueueWithoutSubmission;
};
} // namespace NEO
