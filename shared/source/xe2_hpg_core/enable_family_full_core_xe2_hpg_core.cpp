/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_family_full_core.h"

namespace NEO {

struct Xe2HpgCoreFamily;

using Family = Xe2HpgCoreFamily;

struct EnableCoreXe2HpgCore {
    EnableCoreXe2HpgCore() {
        populateFactoryTable<GfxCoreHelperHw<Family>>();
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<DebuggerL0Hw<Family>>();
        populateFactoryTable<GmmCallbacks<Family>>();
    }
};

static EnableCoreXe2HpgCore enable;
} // namespace NEO
