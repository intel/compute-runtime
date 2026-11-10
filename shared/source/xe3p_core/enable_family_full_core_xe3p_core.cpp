/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_family_full_core.h"

namespace NEO {

struct Xe3pCoreFamily;

using Family = Xe3pCoreFamily;

struct EnableCoreXe3pHpgCore {
    EnableCoreXe3pHpgCore() {
        populateFactoryTable<GfxCoreHelperHw<Family>>();
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<DebuggerL0Hw<Family>>();
        populateFactoryTable<GmmCallbacks<Family>>();
    }
};

static EnableCoreXe3pHpgCore enable;
} // namespace NEO
