/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/enable_family_full_ocl.h"

namespace NEO {
struct Xe2HpgCoreFamily;

using Family = Xe2HpgCoreFamily;

struct EnableOCLXe2HpgCore {
    EnableOCLXe2HpgCore() {
        populateFactoryTable<BufferHw<Family>>();
        populateFactoryTable<ClGfxCoreHelperHw<Family>>();
        populateFactoryTable<CommandQueueHw<Family>>();
        populateFactoryTable<ImageHw<Family>>();
        populateFactoryTable<SamplerHw<Family>>();
    }
};

static EnableOCLXe2HpgCore enable;
} // namespace NEO
