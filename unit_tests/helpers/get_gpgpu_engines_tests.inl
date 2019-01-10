/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_helper.h"
#include "test.h"

using namespace OCLRT;

template <typename FamilyType>
void whenGetGpgpuEnginesThenReturnTwoRcsEngines() {
    auto &hwHelper = HwHelperHw<FamilyType>::get();
    auto &gpgpuEngines = hwHelper.getGpgpuEngineInstances();
    EXPECT_EQ(2u, gpgpuEngines.size());
    int numRcsEngines = 0;
    for (auto &engine : gpgpuEngines) {
        if (ENGINE_RCS == engine.type) {
            EXPECT_EQ(numRcsEngines, engine.id);
            numRcsEngines++;
        }
        EXPECT_EQ(ENGINE_RCS, engine.type);
    }
    EXPECT_EQ(2, numRcsEngines);
    EXPECT_EQ(gpgpuEngines[EngineInstanceConstants::lowPriorityGpgpuEngineIndex].type, lowPriorityGpgpuEngine.type);
    EXPECT_EQ(gpgpuEngines[EngineInstanceConstants::lowPriorityGpgpuEngineIndex].id, lowPriorityGpgpuEngine.id);
}
