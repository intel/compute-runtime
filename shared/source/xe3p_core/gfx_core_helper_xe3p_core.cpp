/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"
using Family = NEO::Xe3pCoreFamily;
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/gfx_core_helper_base.inl"
#include "shared/source/helpers/gfx_core_helper_dg2_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_pvc_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_tgllp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xe2_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xe3_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xe3p_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xehp_and_later.inl"

namespace NEO {
template <>
uint32_t GfxCoreHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Xe3P);
}

#include "shared/source/helpers/gfx_core_helper_from_xe3_to_xe3p.inl"

} // namespace NEO

namespace NEO {
#include "shared/source/helpers/gfx_core_helper_factory_init.inl"

template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
