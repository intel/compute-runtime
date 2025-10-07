/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_mem_dump.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/ptr_math.h"

namespace AubMemDump {

const uint64_t pageMask = ~(4096ull - 1);

void LrcaHelper::setRingTail(void *pLRCIn, uint32_t ringTail) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetRingRegisters + offsetRingTail);
    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2030);
    *pLRCA++ = ringTail;
}

void LrcaHelper::setRingHead(void *pLRCIn, uint32_t ringHead) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetRingRegisters + offsetRingHead);
    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2034);
    *pLRCA++ = ringHead;
}

void LrcaHelper::setRingBase(void *pLRCIn, uint32_t ringBase) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetRingRegisters + offsetRingBase);
    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2038);
    *pLRCA++ = ringBase;
}

void LrcaHelper::setRingCtrl(void *pLRCIn, uint32_t ringCtrl) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetRingRegisters + offsetRingCtrl);
    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x203c);
    *pLRCA++ = ringCtrl;
}

void LrcaHelper::setPDP0(void *pLRCIn, uint64_t address) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetPageTableRegisters + offsetPDP0);

    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2274);
    *pLRCA++ = address >> 32;
    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2270);
    *pLRCA++ = address & 0xffffffff;
}

void LrcaHelper::setPDP1(void *pLRCIn, uint64_t address) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetPageTableRegisters + offsetPDP1);

    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x227c);
    *pLRCA++ = address >> 32;
    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2278);
    *pLRCA++ = address & 0xffffffff;
}

void LrcaHelper::setPDP2(void *pLRCIn, uint64_t address) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetPageTableRegisters + offsetPDP2);

    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2284);
    *pLRCA++ = address >> 32;
    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2280);
    *pLRCA++ = address & 0xffffffff;
}

void LrcaHelper::setPDP3(void *pLRCIn, uint64_t address) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetPageTableRegisters + offsetPDP3);

    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x228c);
    *pLRCA++ = address >> 32;
    *pLRCA++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2288);
    *pLRCA++ = address & 0xffffffff;
}

void LrcaHelper::setPML4(void *pLRCIn, uint64_t address) const {
    setPDP0(pLRCIn, address);
}

void LrcaHelper::initialize(void *pLRCIn) const {
    auto pLRCABase = reinterpret_cast<uint32_t *>(pLRCIn);

    // Initialize to known but benign garbage
    for (size_t i = 0; i < sizeLRCA / sizeof(uint32_t); i++) {
        pLRCABase[i] = 0x1;
    }

    auto pLRCA = ptrOffset(pLRCABase, offsetContext);

    // Initialize the ring context of the LRCA
    auto pLRI = ptrOffset(pLRCA, offsetLRI0);
    auto numRegs = numRegsLRI0;
    *pLRI++ = 0x11001000 | (2 * numRegs - 1);
    uint32_t ctxSrCtlValue = 0x00010001; // Inhibit context-restore
    setContextSaveRestoreFlags(ctxSrCtlValue);
    while (numRegs-- > 0) {
        *pLRI++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2244); // CTXT_SR_CTL
        *pLRI++ = ctxSrCtlValue;
    }

    // Initialize the other LRI
    DEBUG_BREAK_IF(offsetLRI1 != 0x21 * sizeof(uint32_t));
    pLRI = ptrOffset(pLRCA, offsetLRI1);
    numRegs = numRegsLRI1;
    *pLRI++ = 0x11001000 | (2 * numRegs - 1);
    while (numRegs-- > 0) {
        *pLRI++ = AubMemDump::computeRegisterOffset(mmioBase, 0x20d8); // DEBUG
        *pLRI++ = 0x00200020;
    }

    DEBUG_BREAK_IF(offsetLRI2 != 0x41 * sizeof(uint32_t));
    pLRI = ptrOffset(pLRCA, offsetLRI2);
    numRegs = numRegsLRI2;
    *pLRI++ = 0x11000000 | (2 * numRegs - 1);
    while (numRegs-- > 0) {
        *pLRI++ = AubMemDump::computeRegisterOffset(mmioBase, 0x2094); // NOP ID
        *pLRI++ = 0x00000000;
    }

    setRingHead(pLRCIn, 0);
    setRingTail(pLRCIn, 0);
    setRingBase(pLRCIn, 0);
    setRingCtrl(pLRCIn, 0);

    setPDP0(pLRCIn, 0);
    setPDP1(pLRCIn, 0);
    setPDP2(pLRCIn, 0);
    setPDP3(pLRCIn, 0);
}
} // namespace AubMemDump
