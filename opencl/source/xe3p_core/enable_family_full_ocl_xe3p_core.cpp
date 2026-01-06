/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/enable_family_full_ocl.h"

#include "hw_cmds_xe3p_core.h"

namespace NEO {
using Family = Xe3pCoreFamily;

struct EnableOCLXe3pCore {
    EnableOCLXe3pCore() {
        populateFactoryTable<BufferHw<Family>>();
        populateFactoryTable<ClGfxCoreHelperHw<Family>>();
        populateFactoryTable<CommandQueueHw<Family>>();
        populateFactoryTable<ImageHw<Family>>();
        populateFactoryTable<SamplerHw<Family>>();
    }
};

static EnableOCLXe3pCore enable;
} // namespace NEO
