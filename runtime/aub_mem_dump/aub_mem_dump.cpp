/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_mem_dump.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace AubMemDump {

const uint64_t g_pageMask = ~(4096ull - 1);

const size_t g_dwordCountMax = 65536;

// Some page table constants used in virtualizing the page tables.
// clang-format off
// 32 bit page table traits
const uint64_t PageTableTraits<32>::physicalMemory = 0; // 1ull <<addressingBits;

const uint64_t PageTableTraits<32>::numPTEntries  = BIT(PageTableTraits<32>::addressingBits - PageTableTraits<32>::NUM_OFFSET_BITS);
const uint64_t PageTableTraits<32>::sizePT        = BIT(PageTableTraits<32>::addressingBits - PageTableTraits<32>::NUM_OFFSET_BITS) * sizeof(uint64_t);
const uint64_t PageTableTraits<32>::ptBaseAddress = BIT(38);

const uint64_t PageTableTraits<32>::numPDEntries  = BIT(PageTableTraits<32>::addressingBits - PageTableTraits<32>::NUM_OFFSET_BITS - PageTableTraits<32>::NUM_PTE_BITS);
const uint64_t PageTableTraits<32>::sizePD        = BIT(PageTableTraits<32>::addressingBits - PageTableTraits<32>::NUM_OFFSET_BITS - PageTableTraits<32>::NUM_PTE_BITS) * sizeof(uint64_t);
const uint64_t PageTableTraits<32>::pdBaseAddress = BIT(37);

const uint64_t PageTableTraits<32>::numPDPEntries  = BIT(PageTableTraits<32>::addressingBits - PageTableTraits<32>::NUM_OFFSET_BITS - PageTableTraits<32>::NUM_PTE_BITS - PageTableTraits<32>::NUM_PDE_BITS);
const uint64_t PageTableTraits<32>::sizePDP        = BIT(PageTableTraits<32>::addressingBits - PageTableTraits<32>::NUM_OFFSET_BITS - PageTableTraits<32>::NUM_PTE_BITS - PageTableTraits<32>::NUM_PDE_BITS) * sizeof(uint64_t);
const uint64_t PageTableTraits<32>::pdpBaseAddress = BIT(36);

// 48 bit page table traits
const uint64_t PageTableTraits<48>::physicalMemory = 0; // 1ull <<addressingBits;

const uint64_t PageTableTraits<48>::numPTEntries  = BIT(PageTableTraits<48>::addressingBits - PageTableTraits<48>::NUM_OFFSET_BITS);
const uint64_t PageTableTraits<48>::sizePT        = BIT(PageTableTraits<48>::addressingBits - PageTableTraits<48>::NUM_OFFSET_BITS) * sizeof(uint64_t);
const uint64_t PageTableTraits<48>::ptBaseAddress = BIT(32);

const uint64_t PageTableTraits<48>::numPDEntries  = BIT(PageTableTraits<48>::addressingBits - PageTableTraits<48>::NUM_OFFSET_BITS - PageTableTraits<48>::NUM_PTE_BITS);
const uint64_t PageTableTraits<48>::sizePD        = BIT(PageTableTraits<48>::addressingBits - PageTableTraits<48>::NUM_OFFSET_BITS - PageTableTraits<48>::NUM_PTE_BITS) * sizeof(uint64_t);
const uint64_t PageTableTraits<48>::pdBaseAddress = BIT(31);

const uint64_t PageTableTraits<48>::numPDPEntries  = BIT(PageTableTraits<48>::addressingBits - PageTableTraits<48>::NUM_OFFSET_BITS - PageTableTraits<48>::NUM_PTE_BITS - PageTableTraits<48>::NUM_PDE_BITS);
const uint64_t PageTableTraits<48>::sizePDP        = BIT(PageTableTraits<48>::addressingBits - PageTableTraits<48>::NUM_OFFSET_BITS - PageTableTraits<48>::NUM_PTE_BITS - PageTableTraits<48>::NUM_PDE_BITS) * sizeof(uint64_t);
const uint64_t PageTableTraits<48>::pdpBaseAddress = BIT(30);
const uint64_t PageTableTraits<48>::numPML4Entries  = BIT(NUM_PML4_BITS);
const uint64_t PageTableTraits<48>::sizePML4        = BIT(NUM_PML4_BITS) * sizeof(uint64_t);
const uint64_t PageTableTraits<48>::pml4BaseAddress = BIT(29);
// clang-format on

void LrcaHelper::setRingTail(void *pLRCIn, uint32_t ringTail) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetRingRegisters + offsetRingTail);
    *pLRCA++ = mmioBase + 0x2030;
    *pLRCA++ = ringTail;
}

void LrcaHelper::setRingHead(void *pLRCIn, uint32_t ringHead) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetRingRegisters + offsetRingHead);
    *pLRCA++ = mmioBase + 0x2034;
    *pLRCA++ = ringHead;
}

void LrcaHelper::setRingBase(void *pLRCIn, uint32_t ringBase) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetRingRegisters + offsetRingBase);
    *pLRCA++ = mmioBase + 0x2038;
    *pLRCA++ = ringBase;
}

void LrcaHelper::setRingCtrl(void *pLRCIn, uint32_t ringCtrl) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetRingRegisters + offsetRingCtrl);
    *pLRCA++ = mmioBase + 0x203c;
    *pLRCA++ = ringCtrl;
}

void LrcaHelper::setPDP0(void *pLRCIn, uint64_t address) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetPageTableRegisters + offsetPDP0);

    *pLRCA++ = mmioBase + 0x2274;
    *pLRCA++ = address >> 32;
    *pLRCA++ = mmioBase + 0x2270;
    *pLRCA++ = address & 0xffffffff;
}

void LrcaHelper::setPDP1(void *pLRCIn, uint64_t address) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetPageTableRegisters + offsetPDP1);

    *pLRCA++ = mmioBase + 0x227c;
    *pLRCA++ = address >> 32;
    *pLRCA++ = mmioBase + 0x2278;
    *pLRCA++ = address & 0xffffffff;
}

void LrcaHelper::setPDP2(void *pLRCIn, uint64_t address) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetPageTableRegisters + offsetPDP2);

    *pLRCA++ = mmioBase + 0x2284;
    *pLRCA++ = address >> 32;
    *pLRCA++ = mmioBase + 0x2280;
    *pLRCA++ = address & 0xffffffff;
}

void LrcaHelper::setPDP3(void *pLRCIn, uint64_t address) const {
    auto pLRCA = ptrOffset(reinterpret_cast<uint32_t *>(pLRCIn),
                           offsetContext + offsetPageTableRegisters + offsetPDP3);

    *pLRCA++ = mmioBase + 0x228c;
    *pLRCA++ = address >> 32;
    *pLRCA++ = mmioBase + 0x2288;
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
        *pLRI++ = mmioBase + 0x2244; // CTXT_SR_CTL
        *pLRI++ = ctxSrCtlValue;
    }

    // Initialize the other LRI
    DEBUG_BREAK_IF(offsetLRI1 != 0x21 * sizeof(uint32_t));
    pLRI = ptrOffset(pLRCA, offsetLRI1);
    numRegs = numRegsLRI1;
    *pLRI++ = 0x11001000 | (2 * numRegs - 1);
    while (numRegs-- > 0) {
        *pLRI++ = mmioBase + 0x20d8; // DEBUG
        *pLRI++ = 0x00200020;
    }

    DEBUG_BREAK_IF(offsetLRI2 != 0x41 * sizeof(uint32_t));
    pLRI = ptrOffset(pLRCA, offsetLRI2);
    numRegs = numRegsLRI2;
    *pLRI++ = 0x11000000 | (2 * numRegs - 1);
    while (numRegs-- > 0) {
        *pLRI++ = mmioBase + 0x2094; // NOP ID
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

void AubStream::writeMMIO(uint32_t offset, uint32_t value) {
    auto dbgOffset = OCLRT::DebugManager.flags.AubDumpOverrideMmioRegister.get();
    if (dbgOffset > 0) {
        if (offset == static_cast<uint32_t>(dbgOffset)) {
            offset = static_cast<uint32_t>(dbgOffset);
            value = static_cast<uint32_t>(OCLRT::DebugManager.flags.AubDumpOverrideMmioRegisterValue.get());
        }
    }
    writeMMIOImpl(offset, value);
}
} // namespace AubMemDump
