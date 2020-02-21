/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"

#include "test.h"

using namespace NEO;

template <typename FamilyType>
void whenGetGpgpuEnginesThenReturnTwoRcsEngines(const HardwareInfo &hwInfo) {
    auto gpgpuEngines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(3u, gpgpuEngines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[0]);
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[1]);
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[2]);
}
