/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_family_full_core.h"

namespace NEO {

struct Gen12LpFamily;

using Family = Gen12LpFamily;

struct EnableCoreGen12LP {
    EnableCoreGen12LP() {
        populateFactoryTable<GfxCoreHelperHw<Family>>();
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<DebuggerL0Hw<Family>>();
        populateFactoryTable<GmmCallbacks<Family>>();
    }
};

static EnableCoreGen12LP enable;
} // namespace NEO
