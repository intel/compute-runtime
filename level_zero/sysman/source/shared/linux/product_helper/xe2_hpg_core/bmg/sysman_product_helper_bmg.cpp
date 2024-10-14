/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/pmt_util.h"

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.inl"

namespace L0 {
namespace Sysman {
constexpr static auto gfxProduct = IGFX_BMG;

#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_xe_hp_and_later.inl"

static std::map<std::string, std::map<std::string, uint64_t>> guidToKeyOffsetMap = {
    {"0x1e2f8200", // BMG PUNIT rev 1
     {{"VRAM_BANDWIDTH", 14}}},
    {"0x5e2f8210", // BMG OOBMSM Rev 15
     {{"SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]", 41},
      {"VRAM_TEMPERATURE_0_2_0_GTTMMADR", 42},
      {"reg_PCIESS_rx_bytecount_lsb", 70},
      {"reg_PCIESS_rx_bytecount_msb", 69},
      {"reg_PCIESS_tx_bytecount_lsb", 72},
      {"reg_PCIESS_tx_bytecount_msb", 71},
      {"reg_PCIESS_rx_pktcount_lsb", 74},
      {"reg_PCIESS_rx_pktcount_msb", 73},
      {"reg_PCIESS_tx_pktcount_lsb", 76},
      {"reg_PCIESS_tx_pktcount_msb", 75},
      {"MSU_BITMASK", 922},
      {"GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER", 92},
      {"GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER", 93},
      {"GDDR0_CH0_GT_32B_RD_REQ_UPPER", 94},
      {"GDDR0_CH0_GT_32B_RD_REQ_LOWER", 95},
      {"GDDR1_CH0_GT_32B_RD_REQ_UPPER", 134},
      {"GDDR1_CH0_GT_32B_RD_REQ_LOWER", 135},
      {"GDDR2_CH0_GT_32B_RD_REQ_UPPER", 174},
      {"GDDR2_CH0_GT_32B_RD_REQ_LOWER", 175},
      {"GDDR3_CH0_GT_32B_RD_REQ_UPPER", 214},
      {"GDDR3_CH0_GT_32B_RD_REQ_LOWER", 215},
      {"GDDR4_CH0_GT_32B_RD_REQ_UPPER", 254},
      {"GDDR4_CH0_GT_32B_RD_REQ_LOWER", 255},
      {"GDDR5_CH0_GT_32B_RD_REQ_UPPER", 294},
      {"GDDR5_CH0_GT_32B_RD_REQ_LOWER", 295},
      {"GDDR0_CH1_GT_32B_RD_REQ_UPPER", 114},
      {"GDDR0_CH1_GT_32B_RD_REQ_LOWER", 115},
      {"GDDR1_CH1_GT_32B_RD_REQ_UPPER", 154},
      {"GDDR1_CH1_GT_32B_RD_REQ_LOWER", 155},
      {"GDDR2_CH1_GT_32B_RD_REQ_UPPER", 194},
      {"GDDR2_CH1_GT_32B_RD_REQ_LOWER", 195},
      {"GDDR3_CH1_GT_32B_RD_REQ_UPPER", 234},
      {"GDDR3_CH1_GT_32B_RD_REQ_LOWER", 235},
      {"GDDR4_CH1_GT_32B_RD_REQ_UPPER", 274},
      {"GDDR4_CH1_GT_32B_RD_REQ_LOWER", 275},
      {"GDDR5_CH1_GT_32B_RD_REQ_UPPER", 314},
      {"GDDR5_CH1_GT_32B_RD_REQ_LOWER", 315},
      {"GDDR0_CH0_GT_32B_WR_REQ_UPPER", 98},
      {"GDDR0_CH0_GT_32B_WR_REQ_LOWER", 99},
      {"GDDR1_CH0_GT_32B_WR_REQ_UPPER", 138},
      {"GDDR1_CH0_GT_32B_WR_REQ_LOWER", 139},
      {"GDDR2_CH0_GT_32B_WR_REQ_UPPER", 178},
      {"GDDR2_CH0_GT_32B_WR_REQ_LOWER", 179},
      {"GDDR3_CH0_GT_32B_WR_REQ_UPPER", 218},
      {"GDDR3_CH0_GT_32B_WR_REQ_LOWER", 219},
      {"GDDR4_CH0_GT_32B_WR_REQ_UPPER", 258},
      {"GDDR4_CH0_GT_32B_WR_REQ_LOWER", 259},
      {"GDDR5_CH0_GT_32B_WR_REQ_UPPER", 298},
      {"GDDR5_CH0_GT_32B_WR_REQ_LOWER", 299},
      {"GDDR0_CH1_GT_32B_WR_REQ_UPPER", 118},
      {"GDDR0_CH1_GT_32B_WR_REQ_LOWER", 119},
      {"GDDR1_CH1_GT_32B_WR_REQ_UPPER", 158},
      {"GDDR1_CH1_GT_32B_WR_REQ_LOWER", 159},
      {"GDDR2_CH1_GT_32B_WR_REQ_UPPER", 198},
      {"GDDR2_CH1_GT_32B_WR_REQ_LOWER", 199},
      {"GDDR3_CH1_GT_32B_WR_REQ_UPPER", 238},
      {"GDDR3_CH1_GT_32B_WR_REQ_LOWER", 239},
      {"GDDR4_CH1_GT_32B_WR_REQ_UPPER", 278},
      {"GDDR4_CH1_GT_32B_WR_REQ_LOWER", 279},
      {"GDDR5_CH1_GT_32B_WR_REQ_UPPER", 318},
      {"GDDR5_CH1_GT_32B_WR_REQ_LOWER", 319},
      {"GDDR0_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 102},
      {"GDDR0_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 103},
      {"GDDR1_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 142},
      {"GDDR1_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 143},
      {"GDDR2_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 182},
      {"GDDR2_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 183},
      {"GDDR3_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 222},
      {"GDDR3_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 223},
      {"GDDR4_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 262},
      {"GDDR4_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 263},
      {"GDDR5_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 302},
      {"GDDR5_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 303},
      {"GDDR0_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 122},
      {"GDDR0_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 123},
      {"GDDR1_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 162},
      {"GDDR1_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 163},
      {"GDDR2_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 202},
      {"GDDR2_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 203},
      {"GDDR3_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 242},
      {"GDDR3_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 243},
      {"GDDR4_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 282},
      {"GDDR4_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 283},
      {"GDDR5_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 322},
      {"GDDR5_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 323},
      {"GDDR0_CH0_SOC_32B_RD_REQ_UPPER", 106},
      {"GDDR0_CH0_SOC_32B_RD_REQ_LOWER", 107},
      {"GDDR1_CH0_SOC_32B_RD_REQ_UPPER", 146},
      {"GDDR1_CH0_SOC_32B_RD_REQ_LOWER", 147},
      {"GDDR2_CH0_SOC_32B_RD_REQ_UPPER", 186},
      {"GDDR2_CH0_SOC_32B_RD_REQ_LOWER", 187},
      {"GDDR3_CH0_SOC_32B_RD_REQ_UPPER", 226},
      {"GDDR3_CH0_SOC_32B_RD_REQ_LOWER", 227},
      {"GDDR4_CH0_SOC_32B_RD_REQ_UPPER", 266},
      {"GDDR4_CH0_SOC_32B_RD_REQ_LOWER", 267},
      {"GDDR5_CH0_SOC_32B_RD_REQ_UPPER", 306},
      {"GDDR5_CH0_SOC_32B_RD_REQ_LOWER", 307},
      {"GDDR0_CH1_SOC_32B_RD_REQ_UPPER", 126},
      {"GDDR0_CH1_SOC_32B_RD_REQ_LOWER", 127},
      {"GDDR1_CH1_SOC_32B_RD_REQ_UPPER", 166},
      {"GDDR1_CH1_SOC_32B_RD_REQ_LOWER", 167},
      {"GDDR2_CH1_SOC_32B_RD_REQ_UPPER", 206},
      {"GDDR2_CH1_SOC_32B_RD_REQ_LOWER", 207},
      {"GDDR3_CH1_SOC_32B_RD_REQ_UPPER", 246},
      {"GDDR3_CH1_SOC_32B_RD_REQ_LOWER", 247},
      {"GDDR4_CH1_SOC_32B_RD_REQ_UPPER", 286},
      {"GDDR4_CH1_SOC_32B_RD_REQ_LOWER", 287},
      {"GDDR5_CH1_SOC_32B_RD_REQ_UPPER", 326},
      {"GDDR5_CH1_SOC_32B_RD_REQ_LOWER", 327},
      {"GDDR0_CH0_SOC_32B_WR_REQ_UPPER", 110},
      {"GDDR0_CH0_SOC_32B_WR_REQ_LOWER", 111},
      {"GDDR1_CH0_SOC_32B_WR_REQ_UPPER", 150},
      {"GDDR1_CH0_SOC_32B_WR_REQ_LOWER", 151},
      {"GDDR2_CH0_SOC_32B_WR_REQ_UPPER", 190},
      {"GDDR2_CH0_SOC_32B_WR_REQ_LOWER", 191},
      {"GDDR3_CH0_SOC_32B_WR_REQ_UPPER", 230},
      {"GDDR3_CH0_SOC_32B_WR_REQ_LOWER", 231},
      {"GDDR4_CH0_SOC_32B_WR_REQ_UPPER", 270},
      {"GDDR4_CH0_SOC_32B_WR_REQ_LOWER", 271},
      {"GDDR5_CH0_SOC_32B_WR_REQ_UPPER", 310},
      {"GDDR5_CH0_SOC_32B_WR_REQ_LOWER", 311},
      {"GDDR0_CH1_SOC_32B_WR_REQ_UPPER", 130},
      {"GDDR0_CH1_SOC_32B_WR_REQ_LOWER", 131},
      {"GDDR1_CH1_SOC_32B_WR_REQ_UPPER", 170},
      {"GDDR1_CH1_SOC_32B_WR_REQ_LOWER", 171},
      {"GDDR2_CH1_SOC_32B_WR_REQ_UPPER", 210},
      {"GDDR2_CH1_SOC_32B_WR_REQ_LOWER", 211},
      {"GDDR3_CH1_SOC_32B_WR_REQ_UPPER", 250},
      {"GDDR3_CH1_SOC_32B_WR_REQ_LOWER", 251},
      {"GDDR4_CH1_SOC_32B_WR_REQ_UPPER", 290},
      {"GDDR4_CH1_SOC_32B_WR_REQ_LOWER", 291},
      {"GDDR5_CH1_SOC_32B_WR_REQ_UPPER", 330},
      {"GDDR5_CH1_SOC_32B_WR_REQ_LOWER", 331},
      {"GDDR0_CH0_GT_64B_RD_REQ_UPPER", 96},
      {"GDDR0_CH0_GT_64B_RD_REQ_LOWER", 97},
      {"GDDR1_CH0_GT_64B_RD_REQ_UPPER", 136},
      {"GDDR1_CH0_GT_64B_RD_REQ_LOWER", 137},
      {"GDDR2_CH0_GT_64B_RD_REQ_UPPER", 176},
      {"GDDR2_CH0_GT_64B_RD_REQ_LOWER", 177},
      {"GDDR3_CH0_GT_64B_RD_REQ_UPPER", 216},
      {"GDDR3_CH0_GT_64B_RD_REQ_LOWER", 217},
      {"GDDR4_CH0_GT_64B_RD_REQ_UPPER", 256},
      {"GDDR4_CH0_GT_64B_RD_REQ_LOWER", 257},
      {"GDDR5_CH0_GT_64B_RD_REQ_UPPER", 296},
      {"GDDR5_CH0_GT_64B_RD_REQ_LOWER", 297},
      {"GDDR0_CH1_GT_64B_RD_REQ_UPPER", 116},
      {"GDDR0_CH1_GT_64B_RD_REQ_LOWER", 117},
      {"GDDR1_CH1_GT_64B_RD_REQ_UPPER", 156},
      {"GDDR1_CH1_GT_64B_RD_REQ_LOWER", 157},
      {"GDDR2_CH1_GT_64B_RD_REQ_UPPER", 196},
      {"GDDR2_CH1_GT_64B_RD_REQ_LOWER", 197},
      {"GDDR3_CH1_GT_64B_RD_REQ_UPPER", 236},
      {"GDDR3_CH1_GT_64B_RD_REQ_LOWER", 237},
      {"GDDR4_CH1_GT_64B_RD_REQ_UPPER", 276},
      {"GDDR4_CH1_GT_64B_RD_REQ_LOWER", 277},
      {"GDDR5_CH1_GT_64B_RD_REQ_UPPER", 316},
      {"GDDR5_CH1_GT_64B_RD_REQ_LOWER", 317},
      {"GDDR0_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 104},
      {"GDDR0_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 105},
      {"GDDR1_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 144},
      {"GDDR1_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 145},
      {"GDDR2_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 184},
      {"GDDR2_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 185},
      {"GDDR3_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 224},
      {"GDDR3_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 225},
      {"GDDR4_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 264},
      {"GDDR4_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 265},
      {"GDDR5_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 304},
      {"GDDR5_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 305},
      {"GDDR0_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 124},
      {"GDDR0_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 125},
      {"GDDR1_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 164},
      {"GDDR1_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 165},
      {"GDDR2_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 204},
      {"GDDR2_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 205},
      {"GDDR3_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 244},
      {"GDDR3_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 245},
      {"GDDR4_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 284},
      {"GDDR4_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 285},
      {"GDDR5_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 324},
      {"GDDR5_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 325},
      {"GDDR0_CH0_SOC_64B_RD_REQ_UPPER", 108},
      {"GDDR0_CH0_SOC_64B_RD_REQ_LOWER", 109},
      {"GDDR1_CH0_SOC_64B_RD_REQ_UPPER", 148},
      {"GDDR1_CH0_SOC_64B_RD_REQ_LOWER", 149},
      {"GDDR2_CH0_SOC_64B_RD_REQ_UPPER", 188},
      {"GDDR2_CH0_SOC_64B_RD_REQ_LOWER", 189},
      {"GDDR3_CH0_SOC_64B_RD_REQ_UPPER", 228},
      {"GDDR3_CH0_SOC_64B_RD_REQ_LOWER", 229},
      {"GDDR4_CH0_SOC_64B_RD_REQ_UPPER", 268},
      {"GDDR4_CH0_SOC_64B_RD_REQ_LOWER", 269},
      {"GDDR5_CH0_SOC_64B_RD_REQ_UPPER", 308},
      {"GDDR5_CH0_SOC_64B_RD_REQ_LOWER", 309},
      {"GDDR0_CH1_SOC_64B_RD_REQ_UPPER", 128},
      {"GDDR0_CH1_SOC_64B_RD_REQ_LOWER", 129},
      {"GDDR1_CH1_SOC_64B_RD_REQ_UPPER", 168},
      {"GDDR1_CH1_SOC_64B_RD_REQ_LOWER", 169},
      {"GDDR2_CH1_SOC_64B_RD_REQ_UPPER", 208},
      {"GDDR2_CH1_SOC_64B_RD_REQ_LOWER", 209},
      {"GDDR3_CH1_SOC_64B_RD_REQ_UPPER", 248},
      {"GDDR3_CH1_SOC_64B_RD_REQ_LOWER", 249},
      {"GDDR4_CH1_SOC_64B_RD_REQ_UPPER", 288},
      {"GDDR4_CH1_SOC_64B_RD_REQ_LOWER", 289},
      {"GDDR5_CH1_SOC_64B_RD_REQ_UPPER", 328},
      {"GDDR5_CH1_SOC_64B_RD_REQ_LOWER", 329},
      {"GDDR0_CH0_SOC_64B_WR_REQ_UPPER", 112},
      {"GDDR0_CH0_SOC_64B_WR_REQ_LOWER", 113},
      {"GDDR1_CH0_SOC_64B_WR_REQ_UPPER", 152},
      {"GDDR1_CH0_SOC_64B_WR_REQ_LOWER", 153},
      {"GDDR2_CH0_SOC_64B_WR_REQ_UPPER", 192},
      {"GDDR2_CH0_SOC_64B_WR_REQ_LOWER", 193},
      {"GDDR3_CH0_SOC_64B_WR_REQ_UPPER", 232},
      {"GDDR3_CH0_SOC_64B_WR_REQ_LOWER", 233},
      {"GDDR4_CH0_SOC_64B_WR_REQ_UPPER", 272},
      {"GDDR4_CH0_SOC_64B_WR_REQ_LOWER", 273},
      {"GDDR5_CH0_SOC_64B_WR_REQ_UPPER", 312},
      {"GDDR5_CH0_SOC_64B_WR_REQ_LOWER", 313},
      {"GDDR0_CH1_SOC_64B_WR_REQ_UPPER", 132},
      {"GDDR0_CH1_SOC_64B_WR_REQ_LOWER", 133},
      {"GDDR1_CH1_SOC_64B_WR_REQ_UPPER", 172},
      {"GDDR1_CH1_SOC_64B_WR_REQ_LOWER", 173},
      {"GDDR2_CH1_SOC_64B_WR_REQ_UPPER", 212},
      {"GDDR2_CH1_SOC_64B_WR_REQ_LOWER", 213},
      {"GDDR3_CH1_SOC_64B_WR_REQ_UPPER", 252},
      {"GDDR3_CH1_SOC_64B_WR_REQ_LOWER", 253},
      {"GDDR4_CH1_SOC_64B_WR_REQ_UPPER", 292},
      {"GDDR4_CH1_SOC_64B_WR_REQ_LOWER", 293},
      {"GDDR5_CH1_SOC_64B_WR_REQ_UPPER", 332},
      {"GDDR5_CH1_SOC_64B_WR_REQ_LOWER", 333},
      {"GDDR0_CH0_GT_64B_WR_REQ_UPPER", 100},
      {"GDDR0_CH0_GT_64B_WR_REQ_LOWER", 101},
      {"GDDR1_CH0_GT_64B_WR_REQ_UPPER", 140},
      {"GDDR1_CH0_GT_64B_WR_REQ_LOWER", 141},
      {"GDDR2_CH0_GT_64B_WR_REQ_UPPER", 180},
      {"GDDR2_CH0_GT_64B_WR_REQ_LOWER", 181},
      {"GDDR3_CH0_GT_64B_WR_REQ_UPPER", 220},
      {"GDDR3_CH0_GT_64B_WR_REQ_LOWER", 221},
      {"GDDR4_CH0_GT_64B_WR_REQ_UPPER", 260},
      {"GDDR4_CH0_GT_64B_WR_REQ_LOWER", 261},
      {"GDDR5_CH0_GT_64B_WR_REQ_UPPER", 300},
      {"GDDR5_CH0_GT_64B_WR_REQ_LOWER", 301},
      {"GDDR0_CH1_GT_64B_WR_REQ_UPPER", 120},
      {"GDDR0_CH1_GT_64B_WR_REQ_LOWER", 121},
      {"GDDR1_CH1_GT_64B_WR_REQ_UPPER", 160},
      {"GDDR1_CH1_GT_64B_WR_REQ_LOWER", 161},
      {"GDDR2_CH1_GT_64B_WR_REQ_UPPER", 200},
      {"GDDR2_CH1_GT_64B_WR_REQ_LOWER", 201},
      {"GDDR3_CH1_GT_64B_WR_REQ_UPPER", 240},
      {"GDDR3_CH1_GT_64B_WR_REQ_LOWER", 241},
      {"GDDR4_CH1_GT_64B_WR_REQ_UPPER", 280},
      {"GDDR4_CH1_GT_64B_WR_REQ_LOWER", 281},
      {"GDDR5_CH1_GT_64B_WR_REQ_UPPER", 320},
      {"GDDR5_CH1_GT_64B_WR_REQ_LOWER", 321}}}};

template <>
const std::map<std::string, std::map<std::string, uint64_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getGtRasUtilInterface() {
    return RasInterfaceType::netlink;
}

template <>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getHbmRasUtilInterface() {
    return RasInterfaceType::netlink;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isUpstreamPortConnected() {
    return true;
}

static ze_result_t getPciStatsValues(zes_pci_stats_t *pStats, std::map<std::string, uint64_t> &keyOffsetMap, const std::string &telemNodeDir, uint64_t telemOffset) {
    uint32_t rxCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_bytecount_lsb", telemOffset, rxCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t rxCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_bytecount_msb", telemOffset, rxCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t rxCounter = packInto64Bit(rxCounterMsb, rxCounterLsb);

    uint32_t txCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_bytecount_lsb", telemOffset, txCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t txCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_bytecount_msb", telemOffset, txCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t txCounter = packInto64Bit(txCounterMsb, txCounterLsb);

    uint32_t rxPacketCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_pktcount_lsb", telemOffset, rxPacketCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t rxPacketCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_rx_pktcount_msb", telemOffset, rxPacketCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t rxPacketCounter = packInto64Bit(rxPacketCounterMsb, rxPacketCounterLsb);

    uint32_t txPacketCounterLsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_pktcount_lsb", telemOffset, txPacketCounterLsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint32_t txPacketCounterMsb = 0;
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemNodeDir, "reg_PCIESS_tx_pktcount_msb", telemOffset, txPacketCounterMsb)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    uint64_t txPacketCounter = packInto64Bit(txPacketCounterMsb, txPacketCounterLsb);

    pStats->speed.gen = -1;
    pStats->speed.width = -1;
    pStats->speed.maxBandwidth = -1;
    pStats->replayCounter = 0;
    pStats->rxCounter = rxCounter;
    pStats->txCounter = txCounter;
    pStats->packetCounter = rxPacketCounter + txPacketCounter;
    pStats->timestamp = SysmanDevice::getSysmanTimestamp();

    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPciStats(zes_pci_stats_t *pStats, LinuxSysmanImp *pLinuxSysmanImp) {
    std::string &rootPath = pLinuxSysmanImp->getPciRootPath();
    std::map<uint32_t, std::string> telemNodes;
    NEO::PmtUtil::getTelemNodesInPciPath(std::string_view(rootPath), telemNodes);
    if (telemNodes.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    for (auto &it : telemNodes) {
        std::string telemNodeDir = it.second;

        std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
        if (!NEO::PmtUtil::readGuid(telemNodeDir, guidString)) {
            continue;
        }

        auto keyOffsetMapIterator = guidToKeyOffsetMap.find(guidString.data());
        if (keyOffsetMapIterator == guidToKeyOffsetMap.end()) {
            continue;
        }

        uint64_t telemOffset = 0;
        if (!NEO::PmtUtil::readOffset(telemNodeDir, telemOffset)) {
            break;
        }

        result = getPciStatsValues(pStats, keyOffsetMapIterator->second, telemNodeDir, telemOffset);
        if (result == ZE_RESULT_SUCCESS) {
            break;
        }
    }

    return result;
};

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGpuMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {
    std::string telemDir = "";
    std::string guid = "";
    uint64_t telemOffset = 0;

    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, telemOffset)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    auto keyOffsetMapEntry = guidToKeyOffsetMap.find(guid);
    if (keyOffsetMapEntry == guidToKeyOffsetMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    keyOffsetMap = keyOffsetMapEntry->second;

    uint32_t gpuMaxTemperature = 0;
    std::string key("SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]");
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, gpuMaxTemperature)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    *pTemperature = static_cast<double>(gpuMaxTemperature);
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {
    std::string telemDir = "";
    std::string guid = "";
    uint64_t telemOffset = 0;

    if (!pLinuxSysmanImp->getTelemData(subdeviceId, telemDir, guid, telemOffset)) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    auto keyOffsetMapEntry = guidToKeyOffsetMap.find(guid);
    if (keyOffsetMapEntry == guidToKeyOffsetMap.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    keyOffsetMap = keyOffsetMapEntry->second;

    uint32_t memoryMaxTemperature = 0;
    std::string key("VRAM_TEMPERATURE_0_2_0_GTTMMADR");
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, telemDir, key, telemOffset, memoryMaxTemperature)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    memoryMaxTemperature &= 0xFFu; // Extract least significant 8 bits
    *pTemperature = static_cast<double>(memoryMaxTemperature);
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGlobalMaxTemperature(LinuxSysmanImp *pLinuxSysmanImp, double *pTemperature, uint32_t subdeviceId) {
    double gpuMaxTemperature = 0;
    ze_result_t result = this->getGpuMaxTemperature(pLinuxSysmanImp, &gpuMaxTemperature, subdeviceId);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    double memoryMaxTemperature = 0;
    result = this->getMemoryMaxTemperature(pLinuxSysmanImp, &memoryMaxTemperature, subdeviceId);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }

    *pTemperature = std::max(gpuMaxTemperature, memoryMaxTemperature);
    return result;
}

static ze_result_t getMemoryMaxBandwidth(const std::map<std::string, uint64_t> &keyOffsetMap, std::unordered_map<std::string, std::pair<std::string, uint64_t>> &keyTelemInfoMap,
                                         zes_mem_bandwidth_t *pBandwidth) {
    uint32_t maxBandwidth = 0;
    std::string key = "VRAM_BANDWIDTH";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[key].first, key, keyTelemInfoMap[key].second, maxBandwidth)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    maxBandwidth = maxBandwidth >> 16;
    pBandwidth->maxBandwidth = static_cast<uint64_t>(maxBandwidth) * megaBytesToBytes * 100;

    return ZE_RESULT_SUCCESS;
}

static ze_result_t getMemoryBandwidthTimestamp(const std::map<std::string, uint64_t> &keyOffsetMap, std::unordered_map<std::string, std::pair<std::string, uint64_t>> &keyTelemInfoMap,
                                               zes_mem_bandwidth_t *pBandwidth) {
    uint32_t timeStampH = 0;
    uint32_t timeStampL = 0;
    pBandwidth->timestamp = 0;

    std::string key = "GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[key].first, key, keyTelemInfoMap[key].second, timeStampH)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    key = "GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[key].first, key, keyTelemInfoMap[key].second, timeStampL)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    pBandwidth->timestamp = packInto64Bit(timeStampH, timeStampL);

    return ZE_RESULT_SUCCESS;
}

static ze_result_t getCounterValues(const std::vector<std::pair<const std::string, const std::string>> &registerList, const std::string &keyPrefix,
                                    const std::map<std::string, uint64_t> &keyOffsetMap, std::unordered_map<std::string, std::pair<std::string, uint64_t>> &keyTelemInfoMap, uint64_t &totalCounter) {
    for (const auto &regPair : registerList) {
        uint32_t regL = 0;
        uint32_t regH = 0;

        std::string keyL = keyPrefix + regPair.first;
        if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[keyL].first, keyL, keyTelemInfoMap[keyL].second, regL)) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        std::string keyH = keyPrefix + regPair.second;
        if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[keyH].first, keyH, keyTelemInfoMap[keyH].second, regH)) {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }

        totalCounter += packInto64Bit(regH, regL);
    }

    return ZE_RESULT_SUCCESS;
}

static ze_result_t getMemoryBandwidthCounterValues(const std::map<std::string, uint64_t> &keyOffsetMap, std::unordered_map<std::string, std::pair<std::string, uint64_t>> &keyTelemInfoMap,
                                                   const uint32_t &supportedMsu, zes_mem_bandwidth_t *pBandwidth) {
    const std::vector<std::pair<const std::string, const std::string>> readBwRegisterList{
        {"_CH0_GT_32B_RD_REQ_LOWER", "_CH0_GT_32B_RD_REQ_UPPER"},
        {"_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", "_CH0_DISPLAYVC0_32B_RD_REQ_UPPER"},
        {"_CH0_SOC_32B_RD_REQ_LOWER", "_CH0_SOC_32B_RD_REQ_UPPER"},
        {"_CH1_GT_32B_RD_REQ_LOWER", "_CH1_GT_32B_RD_REQ_UPPER"},
        {"_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", "_CH1_DISPLAYVC0_32B_RD_REQ_UPPER"},
        {"_CH1_SOC_32B_RD_REQ_LOWER", "_CH1_SOC_32B_RD_REQ_UPPER"},
        {"_CH0_GT_64B_RD_REQ_LOWER", "_CH0_GT_64B_RD_REQ_UPPER"},
        {"_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", "_CH0_DISPLAYVC0_64B_RD_REQ_UPPER"},
        {"_CH0_SOC_64B_RD_REQ_LOWER", "_CH0_SOC_64B_RD_REQ_UPPER"},
        {"_CH1_GT_64B_RD_REQ_LOWER", "_CH1_GT_64B_RD_REQ_UPPER"},
        {"_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", "_CH1_DISPLAYVC0_64B_RD_REQ_UPPER"},
        {"_CH1_SOC_64B_RD_REQ_LOWER", "_CH1_SOC_64B_RD_REQ_UPPER"}};

    const std::vector<std::pair<const std::string, const std::string>> writeBwRegisterList{
        {"_CH0_GT_32B_WR_REQ_LOWER", "_CH0_GT_32B_WR_REQ_UPPER"},
        {"_CH0_SOC_32B_WR_REQ_LOWER", "_CH0_SOC_32B_WR_REQ_UPPER"},
        {"_CH1_GT_32B_WR_REQ_LOWER", "_CH1_GT_32B_WR_REQ_UPPER"},
        {"_CH1_SOC_32B_WR_REQ_LOWER", "_CH1_SOC_32B_WR_REQ_UPPER"},
        {"_CH0_GT_64B_WR_REQ_LOWER", "_CH0_GT_64B_WR_REQ_UPPER"},
        {"_CH0_SOC_64B_WR_REQ_LOWER", "_CH0_SOC_64B_WR_REQ_UPPER"},
        {"_CH1_GT_64B_WR_REQ_LOWER", "_CH1_GT_64B_WR_REQ_UPPER"},
        {"_CH1_SOC_64B_WR_REQ_LOWER", "_CH1_SOC_64B_WR_REQ_UPPER"}};

    constexpr uint64_t maxSupportedMsu = 8;
    constexpr uint64_t transactionSize = 32;

    pBandwidth->readCounter = 0;
    pBandwidth->writeCounter = 0;

    for (uint32_t i = 0; i < maxSupportedMsu; i++) {
        if (supportedMsu & (1 << i)) {
            std::ostringstream keyStream;
            keyStream << "GDDR" << i;
            std::string keyPrefix = keyStream.str();

            if (ZE_RESULT_SUCCESS != getCounterValues(readBwRegisterList, keyPrefix, keyOffsetMap, keyTelemInfoMap, pBandwidth->readCounter)) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }

            if (ZE_RESULT_SUCCESS != getCounterValues(writeBwRegisterList, keyPrefix, keyOffsetMap, keyTelemInfoMap, pBandwidth->writeCounter)) {
                return ZE_RESULT_ERROR_NOT_AVAILABLE;
            }
        }
    }

    pBandwidth->readCounter = (pBandwidth->readCounter * transactionSize) / microFactor;
    pBandwidth->writeCounter = (pBandwidth->writeCounter * transactionSize) / microFactor;

    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, LinuxSysmanImp *pLinuxSysmanImp, uint32_t subdeviceId) {

    std::string &rootPath = pLinuxSysmanImp->getPciRootPath();
    std::map<uint32_t, std::string> telemNodes;
    NEO::PmtUtil::getTelemNodesInPciPath(std::string_view(rootPath), telemNodes);
    if (telemNodes.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::map<std::string, uint64_t> keyOffsetMap;
    std::unordered_map<std::string, std::pair<std::string, uint64_t>> keyTelemInfoMap;

    // Iterate through all the TelemNodes to find both OOBMSM and PUNIT guids along with their keyOffsetMap
    for (const auto &it : telemNodes) {
        std::string telemNodeDir = it.second;

        std::array<char, NEO::PmtUtil::guidStringSize> guidString = {};
        if (!NEO::PmtUtil::readGuid(telemNodeDir, guidString)) {
            continue;
        }

        uint64_t telemOffset = 0;
        if (!NEO::PmtUtil::readOffset(telemNodeDir, telemOffset)) {
            continue;
        }

        auto keyOffsetMapIterator = guidToKeyOffsetMap.find(guidString.data());
        if (keyOffsetMapIterator == guidToKeyOffsetMap.end()) {
            continue;
        }

        const auto &tempKeyOffsetMap = keyOffsetMapIterator->second;
        for (auto it = tempKeyOffsetMap.begin(); it != tempKeyOffsetMap.end(); it++) {
            keyOffsetMap[it->first] = it->second;
            keyTelemInfoMap[it->first] = std::make_pair(telemNodeDir, telemOffset);
        }
    }

    if (keyOffsetMap.empty()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    // Get Memory Subsystem Bitmask
    uint32_t supportedMsu = 0;
    std::string key = "MSU_BITMASK";
    if (!PlatformMonitoringTech::readValue(keyOffsetMap, keyTelemInfoMap[key].first, key, keyTelemInfoMap[key].second, supportedMsu)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // Get Read and Write Counter Values
    if (ZE_RESULT_SUCCESS != getMemoryBandwidthCounterValues(keyOffsetMap, keyTelemInfoMap, supportedMsu, pBandwidth)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // Get Timestamp Values
    if (ZE_RESULT_SUCCESS != getMemoryBandwidthTimestamp(keyOffsetMap, keyTelemInfoMap, pBandwidth)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    // Get Max Bandwidth
    if (ZE_RESULT_SUCCESS != getMemoryMaxBandwidth(keyOffsetMap, keyTelemInfoMap, pBandwidth)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    return ZE_RESULT_SUCCESS;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0
