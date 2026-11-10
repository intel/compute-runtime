/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_family_full_core.h"

namespace NEO {

struct XeHpgCoreFamily;

using Family = XeHpgCoreFamily;

struct EnableCoreXeHpgCore {
    EnableCoreXeHpgCore() {
        populateFactoryTable<GfxCoreHelperHw<Family>>();
        populateFactoryTable<AUBCommandStreamReceiverHw<Family>>();
        populateFactoryTable<CommandStreamReceiverHw<Family>>();
        populateFactoryTable<TbxCommandStreamReceiverHw<Family>>();
        populateFactoryTable<DebuggerL0Hw<Family>>();
        populateFactoryTable<GmmCallbacks<Family>>();
    }
};

static EnableCoreXeHpgCore enable;
} // namespace NEO
