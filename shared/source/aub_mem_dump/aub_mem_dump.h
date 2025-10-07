/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_data.h"
#include "shared/source/aub_mem_dump/aub_header.h"

#include <fstream>
#include <string>

namespace NEO {
class AubHelper;
}

namespace AubMemDump {
inline constexpr uint32_t rcsRegisterBase = 0x2000;

#ifndef BIT
#define BIT(x) (((uint64_t)1) << (x))
#endif

inline uint32_t computeRegisterOffset(uint32_t mmioBase, uint32_t rcsRegisterOffset) {
    return mmioBase + rcsRegisterOffset - rcsRegisterBase;
}

typedef CmdServicesMemTraceVersion::SteppingValues SteppingValues;
typedef CmdServicesMemTraceMemoryWrite::DataTypeHintValues DataTypeHintValues;

struct LrcaHelper {
    LrcaHelper(uint32_t base) : mmioBase(base) {
    }

    std::string name = "XCS";
    uint32_t mmioBase = 0;

    size_t sizeLRCA = 0x2000;
    uint32_t alignLRCA = 0x1000;
    uint32_t offsetContext = 0x1000;

    uint32_t offsetLRI0 = 0x01 * sizeof(uint32_t);
    uint32_t numRegsLRI0 = 14;

    uint32_t numNoops0 = 3;

    uint32_t offsetLRI1 = offsetLRI0 + (1 + numRegsLRI0 * 2 + numNoops0) * sizeof(uint32_t); // offsetLRI == 0x21 * sizeof(uint32_t);
    uint32_t numRegsLRI1 = 9;

    uint32_t numNoops1 = 13;

    uint32_t offsetLRI2 = offsetLRI1 + (1 + numRegsLRI1 * 2 + numNoops1) * sizeof(uint32_t); // offsetLR2 == 0x41 * sizeof(uint32_t);
    uint32_t numRegsLRI2 = 1;

    uint32_t offsetRingRegisters = offsetLRI0 + (3 * sizeof(uint32_t));
    uint32_t offsetRingHead = 0x0 * sizeof(uint32_t);
    uint32_t offsetRingTail = 0x2 * sizeof(uint32_t);
    uint32_t offsetRingBase = 0x4 * sizeof(uint32_t);
    uint32_t offsetRingCtrl = 0x6 * sizeof(uint32_t);

    uint32_t offsetPageTableRegisters = offsetLRI1 + (3 * sizeof(uint32_t));
    uint32_t offsetPDP0 = 0xc * sizeof(uint32_t);
    uint32_t offsetPDP1 = 0x8 * sizeof(uint32_t);
    uint32_t offsetPDP2 = 0x4 * sizeof(uint32_t);
    uint32_t offsetPDP3 = 0x0 * sizeof(uint32_t);

    void initialize(void *pLRCIn) const;
    void setRingHead(void *pLRCIn, uint32_t ringHead) const;
    void setRingTail(void *pLRCIn, uint32_t ringTail) const;
    void setRingBase(void *pLRCIn, uint32_t ringBase) const;
    void setRingCtrl(void *pLRCIn, uint32_t ringCtrl) const;

    void setPDP0(void *pLRCIn, uint64_t address) const;
    void setPDP1(void *pLRCIn, uint64_t address) const;
    void setPDP2(void *pLRCIn, uint64_t address) const;
    void setPDP3(void *pLRCIn, uint64_t address) const;

    void setPML4(void *pLRCIn, uint64_t address) const;
    MOCKABLE_VIRTUAL void setContextSaveRestoreFlags(uint32_t &value) const;
};

struct LrcaHelperRcs : public LrcaHelper {
    LrcaHelperRcs(uint32_t base) : LrcaHelper(base) {
        sizeLRCA = 0x11000;
        name = "RCS";
    }
};

struct LrcaHelperBcs : public LrcaHelper {
    LrcaHelperBcs(uint32_t base) : LrcaHelper(base) {
        name = "BCS";
    }
};

struct LrcaHelperVcs : public LrcaHelper {
    LrcaHelperVcs(uint32_t base) : LrcaHelper(base) {
        name = "VCS";
    }
};

struct LrcaHelperVecs : public LrcaHelper {
    LrcaHelperVecs(uint32_t base) : LrcaHelper(base) {
        name = "VECS";
    }
};

struct LrcaHelperCcs : public LrcaHelper {
    LrcaHelperCcs(uint32_t base) : LrcaHelper(base) {
        name = "CCS";
    }
};

struct LrcaHelperLinkBcs : public LrcaHelperBcs {
    LrcaHelperLinkBcs(uint32_t base, uint32_t engineId) : LrcaHelperBcs(base) {
        name = "BCS" + std::to_string(engineId);
    }
};

struct LrcaHelperCccs : public LrcaHelper {
    LrcaHelperCccs(uint32_t base) : LrcaHelper(base) {
        name = "CCCS";
    }
};

extern const uint64_t pageMask;
} // namespace AubMemDump
