/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds.h"

#include "opencl/source/helpers/enable_family_full_ocl.h"

namespace NEO {
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
