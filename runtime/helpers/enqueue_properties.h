/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/blit_commands_helper.h"

namespace NEO {

struct EnqueueProperties {
    enum class Operation {
        Blit,
        ExplicitCacheFlush,
        EnqueueWithoutSubmission,
        DependencyResolveOnGpu,
        GpuKernel,
    };

    EnqueueProperties() = delete;
    EnqueueProperties(bool blitEnqueue, bool hasKernels, bool isCacheFlushCmd, bool flushDependenciesOnly,
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

        operation = Operation::EnqueueWithoutSubmission;
    }

    bool isFlushWithoutKernelRequired() const {
        return (operation == Operation::Blit) || (operation == Operation::ExplicitCacheFlush) ||
               (operation == Operation::DependencyResolveOnGpu);
    }

    const BlitPropertiesContainer *blitPropertiesContainer = nullptr;
    Operation operation = Operation::EnqueueWithoutSubmission;
};
} // namespace NEO
