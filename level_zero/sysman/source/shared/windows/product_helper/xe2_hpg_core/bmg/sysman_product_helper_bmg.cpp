/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.inl"
#include "level_zero/sysman/source/sysman_const.h"

#include <utility>
#include <vector>

namespace L0 {
namespace Sysman {

#define PACK_INTO_64BIT(valueH, valueL) ((static_cast<uint64_t>(valueH) << 32) | static_cast<uint64_t>(valueL))

constexpr static auto gfxProduct = IGFX_BMG;

static std::map<unsigned long, std::map<std::string, uint32_t>> guidToKeyOffsetMap = {
    {0x1e2f8200, // BMG PUNIT rev 1
     {{"VRAM_BANDWIDTH", 14}}},
    {0x5e2f8210, // BMG OOBMSM rev 15
     {{"SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]", 42},
      {"VRAM_TEMPERATURE_0_2_0_GTTMMADR", 43},
      {"rx_byte_count_lsb", 70},
      {"rx_byte_count_msb", 69},
      {"tx_byte_count_lsb", 72},
      {"tx_byte_count_msb", 71},
      {"rx_pkt_count_lsb", 74},
      {"rx_pkt_count_msb", 73},
      {"tx_pkt_count_lsb", 76},
      {"tx_pkt_count_msb", 75},
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
      {"MSU_BITMASK", 922},
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

ze_result_t getGpuMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    uint32_t gpuMaxTemperature = 0;
    std::string key("SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]");
    auto result = pPmt->readValue(key, gpuMaxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "readValue call failed for register key %s\n", key.c_str());
        return result;
    }
    *pTemperature = static_cast<double>(gpuMaxTemperature);
    return result;
}

ze_result_t getMemoryMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    uint32_t memoryMaxTemperature = 0;
    std::string key("VRAM_TEMPERATURE_0_2_0_GTTMMADR");
    auto result = pPmt->readValue(key, memoryMaxTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "readValue call failed for register key %s\n", key.c_str());
        return result;
    }
    memoryMaxTemperature &= 0xFFu; // Extract least significant 8 bits
    *pTemperature = static_cast<double>(memoryMaxTemperature);
    return result;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getSensorTemperature(double *pTemperature, zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    std::string key;

    PlatformMonitoringTech *pPmt = pWddmSysmanImp->getSysmanPmt();
    if (pPmt == nullptr) {
        *pTemperature = 0;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    switch (type) {
    case ZES_TEMP_SENSORS_GLOBAL: {
        double gpuMaxTemperature = 0;
        double memoryMaxTemperature = 0;

        ze_result_t result = getGpuMaxTemperature(pPmt, &gpuMaxTemperature);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }

        result = getMemoryMaxTemperature(pPmt, &memoryMaxTemperature);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }

        *pTemperature = std::max(gpuMaxTemperature, memoryMaxTemperature);
        break;
    }
    case ZES_TEMP_SENSORS_GPU:
        status = getGpuMaxTemperature(pPmt, pTemperature);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        break;
    case ZES_TEMP_SENSORS_MEMORY:
        status = getMemoryMaxTemperature(pPmt, pTemperature);
        if (status != ZE_RESULT_SUCCESS) {
            return status;
        }
        break;
    default:
        DEBUG_BREAK_IF(true);
        *pTemperature = 0;
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    return status;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isTempModuleSupported(zes_temp_sensors_t type, WddmSysmanImp *pWddmSysmanImp) {
    if (type != ZES_TEMP_SENSORS_GLOBAL && type != ZES_TEMP_SENSORS_GPU && type != ZES_TEMP_SENSORS_MEMORY) {
        return false;
    }
    PlatformMonitoringTech *pPmt = pWddmSysmanImp->getSysmanPmt();
    return (pPmt != nullptr);
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPciStats(zes_pci_stats_t *pStats, WddmSysmanImp *pWddmSysmanImp) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    pStats->speed.gen = -1;
    pStats->speed.width = -1;
    pStats->speed.maxBandwidth = -1;
    pStats->replayCounter = 0;

    PlatformMonitoringTech *pPmt = pWddmSysmanImp->getSysmanPmt();
    if (pPmt == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    // rx counter calculation
    uint32_t rxCounterL = 0;
    uint32_t rxCounterH = 0;
    status = pPmt->readValue("rx_byte_count_lsb", rxCounterL);
    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "readValue call failed for register key rx_byte_count_lsb\n");
        return status;
    }

    status = pPmt->readValue("rx_byte_count_msb", rxCounterH);
    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "readValue call failed for register key rx_byte_count_msb\n");
        return status;
    }
    pStats->rxCounter = PACK_INTO_64BIT(rxCounterH, rxCounterL);

    // tx counter calculation
    uint32_t txCounterL = 0;
    uint32_t txCounterH = 0;
    status = pPmt->readValue("tx_byte_count_lsb", txCounterL);
    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "readValue call failed for register key tx_byte_count_lsb\n");
        return status;
    }

    status = pPmt->readValue("tx_byte_count_msb", txCounterH);
    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "readValue call failed for register key tx_byte_count_msb\n");
        return status;
    }
    pStats->txCounter = PACK_INTO_64BIT(txCounterH, txCounterL);

    // packet counter calculation
    uint32_t rxPacketCounterL = 0;
    uint32_t rxPacketCounterH = 0;
    status = pPmt->readValue("rx_pkt_count_lsb", rxPacketCounterL);
    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "readValue call failed for register key rx_pkt_count_lsb\n");
        return status;
    }

    status = pPmt->readValue("rx_pkt_count_msb", rxPacketCounterH);
    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "readValue call failed for register key rx_pkt_count_msb\n");
        return status;
    }

    uint32_t txPacketCounterL = 0;
    uint32_t txPacketCounterH = 0;
    status = pPmt->readValue("tx_pkt_count_lsb", txPacketCounterL);
    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "readValue call failed for register key tx_pkt_count_lsb\n");
        return status;
    }

    status = pPmt->readValue("tx_pkt_count_msb", txPacketCounterH);
    if (status != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "readValue call failed for register key tx_pkt_count_msb\n");
        return status;
    }

    pStats->packetCounter = PACK_INTO_64BIT(txPacketCounterH, txPacketCounterL) + PACK_INTO_64BIT(rxPacketCounterH, rxPacketCounterL);
    pStats->timestamp = SysmanDevice::getSysmanTimestamp();

    return status;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPciProperties(zes_pci_properties_t *properties) {
    properties->haveBandwidthCounters = true;
    properties->havePacketCounters = true;
    properties->haveReplayCounters = true;
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandWidth(zes_mem_bandwidth_t *pBandwidth, WddmSysmanImp *pWddmSysmanImp) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    PlatformMonitoringTech *pPmt = pWddmSysmanImp->getSysmanPmt();
    if (pPmt == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    uint32_t supportedMsu = 0;
    status = pPmt->readValue("MSU_BITMASK", supportedMsu);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    constexpr uint64_t transactionSize = 32;
    constexpr uint64_t maxSupportedMsu = 8;
    memset(pBandwidth, 0, sizeof(zes_mem_bandwidth_t));

    std::vector<std::pair<std::string, std::string>> readBwRegisterList{{"_CH0_GT_32B_RD_REQ_LOWER", "_CH0_GT_32B_RD_REQ_UPPER"},
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

    std::vector<std::pair<std::string, std::string>> writeBwRegisterList{{"_CH0_GT_32B_WR_REQ_LOWER", "_CH0_GT_32B_WR_REQ_UPPER"},
                                                                         {"_CH0_SOC_32B_WR_REQ_LOWER", "_CH0_SOC_32B_WR_REQ_UPPER"},
                                                                         {"_CH1_GT_32B_WR_REQ_LOWER", "_CH1_GT_32B_WR_REQ_UPPER"},
                                                                         {"_CH1_SOC_32B_WR_REQ_LOWER", "_CH1_SOC_32B_WR_REQ_UPPER"},
                                                                         {"_CH0_GT_64B_WR_REQ_LOWER", "_CH0_GT_64B_WR_REQ_UPPER"},
                                                                         {"_CH0_SOC_64B_WR_REQ_LOWER", "_CH0_SOC_64B_WR_REQ_UPPER"},
                                                                         {"_CH1_GT_64B_WR_REQ_LOWER", "_CH1_GT_64B_WR_REQ_UPPER"},
                                                                         {"_CH1_SOC_64B_WR_REQ_LOWER", "_CH1_SOC_64B_WR_REQ_UPPER"}};

    for (uint16_t i = 0; i < maxSupportedMsu; i++) {
        if (supportedMsu & 1 << i) {
            std::string memorySubSystemUnit = "GDDR" + std::to_string(i);

            // Read BW calculation
            for (uint32_t j = 0; j < readBwRegisterList.size(); j++) {
                uint32_t readRegisterL = 0;
                uint32_t readRegisterH = 0;

                std::string readCounterKey = memorySubSystemUnit + readBwRegisterList[j].first;
                status = pPmt->readValue(readCounterKey, readRegisterL);
                if (status != ZE_RESULT_SUCCESS) {
                    return status;
                }

                readCounterKey = memorySubSystemUnit + readBwRegisterList[j].second;
                status = pPmt->readValue(readCounterKey, readRegisterH);
                if (status != ZE_RESULT_SUCCESS) {
                    return status;
                }

                uint64_t readCounter = PACK_INTO_64BIT(readRegisterH, readRegisterL);

                pBandwidth->readCounter += readCounter;
            }

            // Write BW calculation
            for (uint32_t j = 0; j < writeBwRegisterList.size(); j++) {
                uint32_t writeRegisterL = 0;
                uint32_t writeRegisterH = 0;

                std::string readCounterKey = memorySubSystemUnit + writeBwRegisterList[j].first;
                status = pPmt->readValue(readCounterKey, writeRegisterL);
                if (status != ZE_RESULT_SUCCESS) {
                    return status;
                }

                readCounterKey = memorySubSystemUnit + writeBwRegisterList[j].second;
                status = pPmt->readValue(readCounterKey, writeRegisterH);
                if (status != ZE_RESULT_SUCCESS) {
                    return status;
                }

                uint64_t writeCounter = PACK_INTO_64BIT(writeRegisterH, writeRegisterL);

                pBandwidth->writeCounter += writeCounter;
            }
        }
    }

    pBandwidth->readCounter = (pBandwidth->readCounter * transactionSize) / microFactor;
    pBandwidth->writeCounter = (pBandwidth->writeCounter * transactionSize) / microFactor;

    // Max BW
    uint32_t maxBandwidth = 0;
    status = pPmt->readValue("VRAM_BANDWIDTH", maxBandwidth);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    maxBandwidth = maxBandwidth >> 16;
    pBandwidth->maxBandwidth = static_cast<uint64_t>(maxBandwidth) * MemoryConstants::megaByte;

    // timestamp calcuation
    uint32_t timeStampL = 0;
    uint32_t timeStampH = 0;

    status = pPmt->readValue("GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER", timeStampH);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    status = pPmt->readValue("GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER", timeStampL);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    pBandwidth->timestamp = PACK_INTO_64BIT(timeStampH, timeStampL);

    return status;
}

template <>
std::map<unsigned long, std::map<std::string, uint32_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0
