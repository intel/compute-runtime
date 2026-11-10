/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/enable_family_full_ocl.h"

namespace NEO {
struct XeHpgCoreFamily;

using Family = XeHpgCoreFamily;

struct EnableOCLXeHpgCore {
    EnableOCLXeHpgCore() {
        populateFactoryTable<BufferHw<Family>>();
        populateFactoryTable<ClGfxCoreHelperHw<Family>>();
        populateFactoryTable<CommandQueueHw<Family>>();
        populateFactoryTable<ImageHw<Family>>();
        populateFactoryTable<SamplerHw<Family>>();
    }
};

static EnableOCLXeHpgCore enable;
} // namespace NEO
