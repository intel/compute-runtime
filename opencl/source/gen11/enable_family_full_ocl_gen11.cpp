/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sampler/sampler.h"

namespace NEO {

using Family = ICLFamily;

struct EnableOCLGen11 {
    EnableOCLGen11() {
        populateFactoryTable<BufferHw<Family>>();
        populateFactoryTable<ClHwHelperHw<Family>>();
        populateFactoryTable<CommandQueueHw<Family>>();
        populateFactoryTable<ImageHw<Family>>();
        populateFactoryTable<SamplerHw<Family>>();
    }
};

static EnableOCLGen11 enable;
} // namespace NEO
