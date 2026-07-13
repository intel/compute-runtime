/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ptr_math.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_load_register_imm_hw.h"

namespace L0::MCL {

template <typename GfxFamily>
MutableLoadRegisterImmHw<GfxFamily>::~MutableLoadRegisterImmHw() {
    if (this->commandView) {
        NEO::EncodeSetMMIO<GfxFamily>::deallocateLoadRegisterImmCommand(this->commandView);
    }
}

template <typename GfxFamily>
void MutableLoadRegisterImmHw<GfxFamily>::noop() {
    if (this->commandView) {
        memset(this->commandView, 0, sizeof(LoadRegisterImm));
    } else {
        memset(this->loadRegImm, 0, sizeof(LoadRegisterImm));
    }
}

template <typename GfxFamily>
void MutableLoadRegisterImmHw<GfxFamily>::restore() {
    if (this->commandView) {
        NEO::LriHelper<GfxFamily>::program(reinterpret_cast<LoadRegisterImm *>(this->commandView), this->registerAddress, 0, true, false);
    } else {
        NEO::LriHelper<GfxFamily>::program(reinterpret_cast<LoadRegisterImm *>(this->loadRegImm), this->registerAddress, 0, true, false);
    }
}

template <typename GfxFamily>
void MutableLoadRegisterImmHw<GfxFamily>::setValue(uint32_t value) {
    constexpr uint32_t dataDwordIndex = 2;

    if (this->commandView) {
        auto loadRegImmCmd = reinterpret_cast<LoadRegisterImm *>(this->commandView);
        loadRegImmCmd->setDataDword(value);
    } else {
        auto loadRegImmCmd = reinterpret_cast<LoadRegisterImm *>(this->loadRegImm);
        loadRegImmCmd->getRawData(dataDwordIndex) = value;
    }
}

} // namespace L0::MCL
