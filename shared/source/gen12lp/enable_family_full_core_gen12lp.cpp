/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/ail/ail_configuration.inl"
#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/hw_helper.h"

#include <map>
namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef TGLLPFamily Family;
static auto gfxFamily = IGFX_GEN12LP_CORE;

struct EnableCoreGen12LP {
    EnableCoreGen12LP() {
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
    }
};

static EnableCoreGen12LP enable;

extern AILConfiguration *ailConfigurationTable[IGFX_MAX_PRODUCT];

#ifdef SUPPORT_DG1
std::map<std::string_view, std::vector<AILEnumeration>> applicationMapDG1 = {};

template <>
void AILConfigurationHw<IGFX_DG1>::applyExt(RuntimeCapabilityTable &runtimeCapabilityTable) {
}

static EnableAIL<IGFX_DG1> enableDG1;
#endif

#ifdef SUPPORT_TGLLP
std::map<std::string_view, std::vector<AILEnumeration>> applicationMapTGLLP = {};

static EnableAIL<IGFX_TIGERLAKE_LP> enableTgllp;
#endif
} // namespace NEO
