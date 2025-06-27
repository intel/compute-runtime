/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper_hw.inl"
#include "level_zero/sysman/source/sysman_const.h"

#include <utility>
#include <vector>

namespace L0 {
namespace Sysman {

constexpr static auto gfxProduct = IGFX_BMG;

// XTAL clock frequency is denoted as an integer between [0-3] with a predefined value for each number. This vector defines the predefined value for each integer represented by the index of the vector.
static const std::vector<double> indexToXtalClockFrequecyMap = {24, 19.2, 38.4, 25};

static std::map<unsigned long, std::map<std::string, uint32_t>> guidToKeyOffsetMap = {
    {0x1e2f8200, // BMG PUNIT rev 1
     {{"XTAL_CLK_FREQUENCY", 1},
      {"VRAM_BANDWIDTH", 14},
      {"XTAL_COUNT", 128},
      {"VCCGT_ENERGY_ACCUMULATOR", 407},
      {"VCCDDR_ENERGY_ACCUMULATOR", 410}}},
    {0x1e2f8201, // BMG PUNIT rev 2
     {{"XTAL_CLK_FREQUENCY", 1},
      {"ACCUM_PACKAGE_ENERGY", 12},
      {"ACCUM_PSYS_ENERGY", 13},
      {"VRAM_BANDWIDTH", 14},
      {"XTAL_COUNT", 128},
      {"VCCGT_ENERGY_ACCUMULATOR", 407},
      {"VCCDDR_ENERGY_ACCUMULATOR", 410}}},
    {0x1e2f8202, // BMG PUNIT rev 3
     {{"XTAL_CLK_FREQUENCY", 1},
      {"ACCUM_PACKAGE_ENERGY", 12},
      {"ACCUM_PSYS_ENERGY", 13},
      {"VRAM_BANDWIDTH", 14},
      {"XTAL_COUNT", 128},
      {"VCCGT_ENERGY_ACCUMULATOR", 407},
      {"VCCDDR_ENERGY_ACCUMULATOR", 410}}},
    {0x5e2f8210, // BMG OOBMSM rev 15
     {{"PACKAGE_ENERGY_STATUS_SKU", 34},
      {"PLATFORM_ENERGY_STATUS", 35},
      {"SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]", 41},
      {"VRAM_TEMPERATURE_0_2_0_GTTMMADR", 42},
      {"rx_byte_count_lsb", 70},
      {"rx_byte_count_msb", 69},
      {"tx_byte_count_lsb", 72},
      {"tx_byte_count_msb", 71},
      {"rx_pkt_count_lsb", 74},
      {"rx_pkt_count_msb", 73},
      {"tx_pkt_count_lsb", 76},
      {"tx_pkt_count_msb", 75},
      {"GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER", 93},
      {"GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER", 92},
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
      {"GDDR5_CH1_GT_64B_WR_REQ_LOWER", 321}}},
    {0x5e2f8211, // BMG OOBMSM rev 16
     {{"PACKAGE_ENERGY_STATUS_SKU", 34},
      {"PLATFORM_ENERGY_STATUS", 35},
      {"SOC_THERMAL_SENSORS_TEMPERATURE_0_2_0_GTTMMADR[1]", 41},
      {"VRAM_TEMPERATURE_0_2_0_GTTMMADR", 42},
      {"rx_byte_count_lsb", 70},
      {"rx_byte_count_msb", 71},
      {"tx_byte_count_lsb", 72},
      {"tx_byte_count_msb", 73},
      {"rx_pkt_count_lsb", 74},
      {"rx_pkt_count_msb", 75},
      {"tx_pkt_count_lsb", 76},
      {"tx_pkt_count_msb", 77},
      {"GDDR_TELEM_CAPTURE_TIMESTAMP_UPPER", 93},
      {"GDDR_TELEM_CAPTURE_TIMESTAMP_LOWER", 92},
      {"GDDR0_CH0_GT_32B_RD_REQ_UPPER", 95},
      {"GDDR0_CH0_GT_32B_RD_REQ_LOWER", 94},
      {"GDDR1_CH0_GT_32B_RD_REQ_UPPER", 135},
      {"GDDR1_CH0_GT_32B_RD_REQ_LOWER", 134},
      {"GDDR2_CH0_GT_32B_RD_REQ_UPPER", 175},
      {"GDDR2_CH0_GT_32B_RD_REQ_LOWER", 174},
      {"GDDR3_CH0_GT_32B_RD_REQ_UPPER", 215},
      {"GDDR3_CH0_GT_32B_RD_REQ_LOWER", 214},
      {"GDDR4_CH0_GT_32B_RD_REQ_UPPER", 255},
      {"GDDR4_CH0_GT_32B_RD_REQ_LOWER", 254},
      {"GDDR5_CH0_GT_32B_RD_REQ_UPPER", 295},
      {"GDDR5_CH0_GT_32B_RD_REQ_LOWER", 294},
      {"GDDR0_CH1_GT_32B_RD_REQ_UPPER", 115},
      {"GDDR0_CH1_GT_32B_RD_REQ_LOWER", 114},
      {"GDDR1_CH1_GT_32B_RD_REQ_UPPER", 155},
      {"GDDR1_CH1_GT_32B_RD_REQ_LOWER", 154},
      {"GDDR2_CH1_GT_32B_RD_REQ_UPPER", 195},
      {"GDDR2_CH1_GT_32B_RD_REQ_LOWER", 194},
      {"GDDR3_CH1_GT_32B_RD_REQ_UPPER", 235},
      {"GDDR3_CH1_GT_32B_RD_REQ_LOWER", 234},
      {"GDDR4_CH1_GT_32B_RD_REQ_UPPER", 275},
      {"GDDR4_CH1_GT_32B_RD_REQ_LOWER", 274},
      {"GDDR5_CH1_GT_32B_RD_REQ_UPPER", 315},
      {"GDDR5_CH1_GT_32B_RD_REQ_LOWER", 314},
      {"GDDR0_CH0_GT_32B_WR_REQ_UPPER", 99},
      {"GDDR0_CH0_GT_32B_WR_REQ_LOWER", 98},
      {"GDDR1_CH0_GT_32B_WR_REQ_UPPER", 139},
      {"GDDR1_CH0_GT_32B_WR_REQ_LOWER", 138},
      {"GDDR2_CH0_GT_32B_WR_REQ_UPPER", 179},
      {"GDDR2_CH0_GT_32B_WR_REQ_LOWER", 178},
      {"GDDR3_CH0_GT_32B_WR_REQ_UPPER", 219},
      {"GDDR3_CH0_GT_32B_WR_REQ_LOWER", 218},
      {"GDDR4_CH0_GT_32B_WR_REQ_UPPER", 259},
      {"GDDR4_CH0_GT_32B_WR_REQ_LOWER", 258},
      {"GDDR5_CH0_GT_32B_WR_REQ_UPPER", 299},
      {"GDDR5_CH0_GT_32B_WR_REQ_LOWER", 298},
      {"GDDR0_CH1_GT_32B_WR_REQ_UPPER", 119},
      {"GDDR0_CH1_GT_32B_WR_REQ_LOWER", 118},
      {"GDDR1_CH1_GT_32B_WR_REQ_UPPER", 159},
      {"GDDR1_CH1_GT_32B_WR_REQ_LOWER", 158},
      {"GDDR2_CH1_GT_32B_WR_REQ_UPPER", 199},
      {"GDDR2_CH1_GT_32B_WR_REQ_LOWER", 198},
      {"GDDR3_CH1_GT_32B_WR_REQ_UPPER", 239},
      {"GDDR3_CH1_GT_32B_WR_REQ_LOWER", 238},
      {"GDDR4_CH1_GT_32B_WR_REQ_UPPER", 279},
      {"GDDR4_CH1_GT_32B_WR_REQ_LOWER", 278},
      {"GDDR5_CH1_GT_32B_WR_REQ_UPPER", 319},
      {"GDDR5_CH1_GT_32B_WR_REQ_LOWER", 318},
      {"GDDR0_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 103},
      {"GDDR0_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 102},
      {"GDDR1_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 143},
      {"GDDR1_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 142},
      {"GDDR2_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 183},
      {"GDDR2_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 182},
      {"GDDR3_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 223},
      {"GDDR3_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 222},
      {"GDDR4_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 263},
      {"GDDR4_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 262},
      {"GDDR5_CH0_DISPLAYVC0_32B_RD_REQ_UPPER", 303},
      {"GDDR5_CH0_DISPLAYVC0_32B_RD_REQ_LOWER", 302},
      {"GDDR0_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 123},
      {"GDDR0_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 122},
      {"GDDR1_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 163},
      {"GDDR1_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 162},
      {"GDDR2_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 203},
      {"GDDR2_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 202},
      {"GDDR3_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 243},
      {"GDDR3_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 242},
      {"GDDR4_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 283},
      {"GDDR4_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 282},
      {"GDDR5_CH1_DISPLAYVC0_32B_RD_REQ_UPPER", 323},
      {"GDDR5_CH1_DISPLAYVC0_32B_RD_REQ_LOWER", 322},
      {"GDDR0_CH0_SOC_32B_RD_REQ_UPPER", 107},
      {"GDDR0_CH0_SOC_32B_RD_REQ_LOWER", 106},
      {"GDDR1_CH0_SOC_32B_RD_REQ_UPPER", 147},
      {"GDDR1_CH0_SOC_32B_RD_REQ_LOWER", 146},
      {"GDDR2_CH0_SOC_32B_RD_REQ_UPPER", 187},
      {"GDDR2_CH0_SOC_32B_RD_REQ_LOWER", 186},
      {"GDDR3_CH0_SOC_32B_RD_REQ_UPPER", 227},
      {"GDDR3_CH0_SOC_32B_RD_REQ_LOWER", 226},
      {"GDDR4_CH0_SOC_32B_RD_REQ_UPPER", 267},
      {"GDDR4_CH0_SOC_32B_RD_REQ_LOWER", 266},
      {"GDDR5_CH0_SOC_32B_RD_REQ_UPPER", 307},
      {"GDDR5_CH0_SOC_32B_RD_REQ_LOWER", 306},
      {"GDDR0_CH1_SOC_32B_RD_REQ_UPPER", 127},
      {"GDDR0_CH1_SOC_32B_RD_REQ_LOWER", 126},
      {"GDDR1_CH1_SOC_32B_RD_REQ_UPPER", 167},
      {"GDDR1_CH1_SOC_32B_RD_REQ_LOWER", 166},
      {"GDDR2_CH1_SOC_32B_RD_REQ_UPPER", 207},
      {"GDDR2_CH1_SOC_32B_RD_REQ_LOWER", 206},
      {"GDDR3_CH1_SOC_32B_RD_REQ_UPPER", 247},
      {"GDDR3_CH1_SOC_32B_RD_REQ_LOWER", 246},
      {"GDDR4_CH1_SOC_32B_RD_REQ_UPPER", 287},
      {"GDDR4_CH1_SOC_32B_RD_REQ_LOWER", 286},
      {"GDDR5_CH1_SOC_32B_RD_REQ_UPPER", 327},
      {"GDDR5_CH1_SOC_32B_RD_REQ_LOWER", 326},
      {"GDDR0_CH0_SOC_32B_WR_REQ_UPPER", 111},
      {"GDDR0_CH0_SOC_32B_WR_REQ_LOWER", 110},
      {"GDDR1_CH0_SOC_32B_WR_REQ_UPPER", 151},
      {"GDDR1_CH0_SOC_32B_WR_REQ_LOWER", 150},
      {"GDDR2_CH0_SOC_32B_WR_REQ_UPPER", 191},
      {"GDDR2_CH0_SOC_32B_WR_REQ_LOWER", 190},
      {"GDDR3_CH0_SOC_32B_WR_REQ_UPPER", 231},
      {"GDDR3_CH0_SOC_32B_WR_REQ_LOWER", 230},
      {"GDDR4_CH0_SOC_32B_WR_REQ_UPPER", 271},
      {"GDDR4_CH0_SOC_32B_WR_REQ_LOWER", 270},
      {"GDDR5_CH0_SOC_32B_WR_REQ_UPPER", 311},
      {"GDDR5_CH0_SOC_32B_WR_REQ_LOWER", 310},
      {"GDDR0_CH1_SOC_32B_WR_REQ_UPPER", 131},
      {"GDDR0_CH1_SOC_32B_WR_REQ_LOWER", 130},
      {"GDDR1_CH1_SOC_32B_WR_REQ_UPPER", 171},
      {"GDDR1_CH1_SOC_32B_WR_REQ_LOWER", 170},
      {"GDDR2_CH1_SOC_32B_WR_REQ_UPPER", 211},
      {"GDDR2_CH1_SOC_32B_WR_REQ_LOWER", 210},
      {"GDDR3_CH1_SOC_32B_WR_REQ_UPPER", 251},
      {"GDDR3_CH1_SOC_32B_WR_REQ_LOWER", 250},
      {"GDDR4_CH1_SOC_32B_WR_REQ_UPPER", 291},
      {"GDDR4_CH1_SOC_32B_WR_REQ_LOWER", 290},
      {"GDDR5_CH1_SOC_32B_WR_REQ_UPPER", 331},
      {"GDDR5_CH1_SOC_32B_WR_REQ_LOWER", 330},
      {"MSU_BITMASK", 922},
      {"GDDR0_CH0_GT_64B_RD_REQ_UPPER", 97},
      {"GDDR0_CH0_GT_64B_RD_REQ_LOWER", 96},
      {"GDDR1_CH0_GT_64B_RD_REQ_UPPER", 137},
      {"GDDR1_CH0_GT_64B_RD_REQ_LOWER", 136},
      {"GDDR2_CH0_GT_64B_RD_REQ_UPPER", 177},
      {"GDDR2_CH0_GT_64B_RD_REQ_LOWER", 176},
      {"GDDR3_CH0_GT_64B_RD_REQ_UPPER", 217},
      {"GDDR3_CH0_GT_64B_RD_REQ_LOWER", 216},
      {"GDDR4_CH0_GT_64B_RD_REQ_UPPER", 257},
      {"GDDR4_CH0_GT_64B_RD_REQ_LOWER", 256},
      {"GDDR5_CH0_GT_64B_RD_REQ_UPPER", 297},
      {"GDDR5_CH0_GT_64B_RD_REQ_LOWER", 296},
      {"GDDR0_CH1_GT_64B_RD_REQ_UPPER", 117},
      {"GDDR0_CH1_GT_64B_RD_REQ_LOWER", 116},
      {"GDDR1_CH1_GT_64B_RD_REQ_UPPER", 157},
      {"GDDR1_CH1_GT_64B_RD_REQ_LOWER", 156},
      {"GDDR2_CH1_GT_64B_RD_REQ_UPPER", 197},
      {"GDDR2_CH1_GT_64B_RD_REQ_LOWER", 196},
      {"GDDR3_CH1_GT_64B_RD_REQ_UPPER", 237},
      {"GDDR3_CH1_GT_64B_RD_REQ_LOWER", 236},
      {"GDDR4_CH1_GT_64B_RD_REQ_UPPER", 277},
      {"GDDR4_CH1_GT_64B_RD_REQ_LOWER", 276},
      {"GDDR5_CH1_GT_64B_RD_REQ_UPPER", 317},
      {"GDDR5_CH1_GT_64B_RD_REQ_LOWER", 316},
      {"GDDR0_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 105},
      {"GDDR0_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 104},
      {"GDDR1_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 145},
      {"GDDR1_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 144},
      {"GDDR2_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 185},
      {"GDDR2_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 184},
      {"GDDR3_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 225},
      {"GDDR3_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 224},
      {"GDDR4_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 265},
      {"GDDR4_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 264},
      {"GDDR5_CH0_DISPLAYVC0_64B_RD_REQ_UPPER", 305},
      {"GDDR5_CH0_DISPLAYVC0_64B_RD_REQ_LOWER", 304},
      {"GDDR0_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 125},
      {"GDDR0_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 124},
      {"GDDR1_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 165},
      {"GDDR1_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 164},
      {"GDDR2_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 205},
      {"GDDR2_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 204},
      {"GDDR3_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 245},
      {"GDDR3_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 244},
      {"GDDR4_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 285},
      {"GDDR4_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 284},
      {"GDDR5_CH1_DISPLAYVC0_64B_RD_REQ_UPPER", 325},
      {"GDDR5_CH1_DISPLAYVC0_64B_RD_REQ_LOWER", 324},
      {"GDDR0_CH0_SOC_64B_RD_REQ_UPPER", 109},
      {"GDDR0_CH0_SOC_64B_RD_REQ_LOWER", 108},
      {"GDDR1_CH0_SOC_64B_RD_REQ_UPPER", 149},
      {"GDDR1_CH0_SOC_64B_RD_REQ_LOWER", 148},
      {"GDDR2_CH0_SOC_64B_RD_REQ_UPPER", 189},
      {"GDDR2_CH0_SOC_64B_RD_REQ_LOWER", 188},
      {"GDDR3_CH0_SOC_64B_RD_REQ_UPPER", 229},
      {"GDDR3_CH0_SOC_64B_RD_REQ_LOWER", 228},
      {"GDDR4_CH0_SOC_64B_RD_REQ_UPPER", 269},
      {"GDDR4_CH0_SOC_64B_RD_REQ_LOWER", 268},
      {"GDDR5_CH0_SOC_64B_RD_REQ_UPPER", 309},
      {"GDDR5_CH0_SOC_64B_RD_REQ_LOWER", 308},
      {"GDDR0_CH1_SOC_64B_RD_REQ_UPPER", 129},
      {"GDDR0_CH1_SOC_64B_RD_REQ_LOWER", 128},
      {"GDDR1_CH1_SOC_64B_RD_REQ_UPPER", 169},
      {"GDDR1_CH1_SOC_64B_RD_REQ_LOWER", 168},
      {"GDDR2_CH1_SOC_64B_RD_REQ_UPPER", 209},
      {"GDDR2_CH1_SOC_64B_RD_REQ_LOWER", 208},
      {"GDDR3_CH1_SOC_64B_RD_REQ_UPPER", 249},
      {"GDDR3_CH1_SOC_64B_RD_REQ_LOWER", 248},
      {"GDDR4_CH1_SOC_64B_RD_REQ_UPPER", 289},
      {"GDDR4_CH1_SOC_64B_RD_REQ_LOWER", 288},
      {"GDDR5_CH1_SOC_64B_RD_REQ_UPPER", 329},
      {"GDDR5_CH1_SOC_64B_RD_REQ_LOWER", 328},
      {"GDDR0_CH0_SOC_64B_WR_REQ_UPPER", 113},
      {"GDDR0_CH0_SOC_64B_WR_REQ_LOWER", 112},
      {"GDDR1_CH0_SOC_64B_WR_REQ_UPPER", 153},
      {"GDDR1_CH0_SOC_64B_WR_REQ_LOWER", 152},
      {"GDDR2_CH0_SOC_64B_WR_REQ_UPPER", 193},
      {"GDDR2_CH0_SOC_64B_WR_REQ_LOWER", 192},
      {"GDDR3_CH0_SOC_64B_WR_REQ_UPPER", 233},
      {"GDDR3_CH0_SOC_64B_WR_REQ_LOWER", 232},
      {"GDDR4_CH0_SOC_64B_WR_REQ_UPPER", 273},
      {"GDDR4_CH0_SOC_64B_WR_REQ_LOWER", 272},
      {"GDDR5_CH0_SOC_64B_WR_REQ_UPPER", 313},
      {"GDDR5_CH0_SOC_64B_WR_REQ_LOWER", 312},
      {"GDDR0_CH1_SOC_64B_WR_REQ_UPPER", 133},
      {"GDDR0_CH1_SOC_64B_WR_REQ_LOWER", 132},
      {"GDDR1_CH1_SOC_64B_WR_REQ_UPPER", 173},
      {"GDDR1_CH1_SOC_64B_WR_REQ_LOWER", 172},
      {"GDDR2_CH1_SOC_64B_WR_REQ_UPPER", 213},
      {"GDDR2_CH1_SOC_64B_WR_REQ_LOWER", 212},
      {"GDDR3_CH1_SOC_64B_WR_REQ_UPPER", 253},
      {"GDDR3_CH1_SOC_64B_WR_REQ_LOWER", 252},
      {"GDDR4_CH1_SOC_64B_WR_REQ_UPPER", 293},
      {"GDDR4_CH1_SOC_64B_WR_REQ_LOWER", 292},
      {"GDDR5_CH1_SOC_64B_WR_REQ_UPPER", 333},
      {"GDDR5_CH1_SOC_64B_WR_REQ_LOWER", 332},
      {"GDDR0_CH0_GT_64B_WR_REQ_UPPER", 101},
      {"GDDR0_CH0_GT_64B_WR_REQ_LOWER", 100},
      {"GDDR1_CH0_GT_64B_WR_REQ_UPPER", 141},
      {"GDDR1_CH0_GT_64B_WR_REQ_LOWER", 140},
      {"GDDR2_CH0_GT_64B_WR_REQ_UPPER", 181},
      {"GDDR2_CH0_GT_64B_WR_REQ_LOWER", 180},
      {"GDDR3_CH0_GT_64B_WR_REQ_UPPER", 221},
      {"GDDR3_CH0_GT_64B_WR_REQ_LOWER", 220},
      {"GDDR4_CH0_GT_64B_WR_REQ_UPPER", 261},
      {"GDDR4_CH0_GT_64B_WR_REQ_LOWER", 260},
      {"GDDR5_CH0_GT_64B_WR_REQ_UPPER", 301},
      {"GDDR5_CH0_GT_64B_WR_REQ_LOWER", 300},
      {"GDDR0_CH1_GT_64B_WR_REQ_UPPER", 121},
      {"GDDR0_CH1_GT_64B_WR_REQ_LOWER", 120},
      {"GDDR1_CH1_GT_64B_WR_REQ_UPPER", 161},
      {"GDDR1_CH1_GT_64B_WR_REQ_LOWER", 160},
      {"GDDR2_CH1_GT_64B_WR_REQ_UPPER", 201},
      {"GDDR2_CH1_GT_64B_WR_REQ_LOWER", 200},
      {"GDDR3_CH1_GT_64B_WR_REQ_UPPER", 241},
      {"GDDR3_CH1_GT_64B_WR_REQ_LOWER", 240},
      {"GDDR4_CH1_GT_64B_WR_REQ_UPPER", 281},
      {"GDDR4_CH1_GT_64B_WR_REQ_LOWER", 280},
      {"GDDR5_CH1_GT_64B_WR_REQ_UPPER", 321},
      {"GDDR5_CH1_GT_64B_WR_REQ_LOWER", 320}}},
    {0x1e2f8301, // BMG G31 PUNIT rev 2
     {{"XTAL_CLK_FREQUENCY", 1},
      {"ACCUM_PACKAGE_ENERGY", 12},
      {"ACCUM_PSYS_ENERGY", 13},
      {"VRAM_BANDWIDTH", 14},
      {"XTAL_COUNT", 128},
      {"VCCGT_ENERGY_ACCUMULATOR", 407},
      {"VCCDDR_ENERGY_ACCUMULATOR", 410}}},
    {0x1e2f8302, // BMG G31 PUNIT rev 3
     {{"XTAL_CLK_FREQUENCY", 1},
      {"ACCUM_PACKAGE_ENERGY", 12},
      {"ACCUM_PSYS_ENERGY", 13},
      {"VRAM_BANDWIDTH", 14},
      {"XTAL_COUNT", 128},
      {"VCCGT_ENERGY_ACCUMULATOR", 407},
      {"VCCDDR_ENERGY_ACCUMULATOR", 410}}}};

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
    pStats->rxCounter = packInto64Bit(rxCounterH, rxCounterL);

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
    pStats->txCounter = packInto64Bit(txCounterH, txCounterL);

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

    pStats->packetCounter = packInto64Bit(txPacketCounterH, txPacketCounterL) + packInto64Bit(rxPacketCounterH, rxPacketCounterL);

    // timestamp calculation
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
    // timestamp from PMT is in milli seconds
    pStats->timestamp = packInto64Bit(timeStampH, timeStampL) * milliSecsToMicroSecs;

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

                uint64_t readCounter = packInto64Bit(readRegisterH, readRegisterL);

                readCounterKey.find("_32B_") != std::string::npos ? readCounter *= 32 : readCounter *= 64;
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

                uint64_t writeCounter = packInto64Bit(writeRegisterH, writeRegisterL);

                readCounterKey.find("_32B_") != std::string::npos ? writeCounter *= 32 : writeCounter *= 64;
                pBandwidth->writeCounter += writeCounter;
            }
        }
    }

    // Max BW
    uint32_t maxBandwidth = 0;
    status = pPmt->readValue("VRAM_BANDWIDTH", maxBandwidth);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    maxBandwidth = maxBandwidth >> 16;

    // PMT reports maxBandwidth in units of 100 MBps (decimal). Need to convert it into Bytes/sec, unit to be returned by sysman.
    pBandwidth->maxBandwidth = static_cast<uint64_t>(maxBandwidth) * megaBytesToBytes * 100;

    // Get timestamp
    pBandwidth->timestamp = SysmanDevice::getSysmanTimestamp();

    return status;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPowerPropertiesFromPmt(zes_power_properties_t *pProperties) {
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;
    pProperties->canControl = false;
    pProperties->isEnergyThresholdSupported = false;
    pProperties->defaultLimit = -1;
    pProperties->minLimit = -1;
    pProperties->maxLimit = -1;
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPowerPropertiesExtFromPmt(zes_power_ext_properties_t *pExtPoperties, zes_power_domain_t powerDomain) {
    pExtPoperties->domain = powerDomain;
    if (pExtPoperties->defaultLimit) {
        pExtPoperties->defaultLimit->limit = -1;
        pExtPoperties->defaultLimit->limitUnit = ZES_LIMIT_UNIT_POWER;
        pExtPoperties->defaultLimit->enabledStateLocked = true;
        pExtPoperties->defaultLimit->intervalValueLocked = true;
        pExtPoperties->defaultLimit->limitValueLocked = true;
        pExtPoperties->defaultLimit->source = ZES_POWER_SOURCE_ANY;
        pExtPoperties->defaultLimit->level = ZES_POWER_LEVEL_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

template <>
ze_result_t SysmanProductHelperHw<gfxProduct>::getPowerEnergyCounter(zes_power_energy_counter_t *pEnergy, zes_power_domain_t powerDomain, WddmSysmanImp *pWddmSysmanImp) {
    ze_result_t status = ZE_RESULT_SUCCESS;
    uint32_t energyCounter = 0;

    PlatformMonitoringTech *pPmt = pWddmSysmanImp->getSysmanPmt();
    if (pPmt == nullptr) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    switch (powerDomain) {
    case ZES_POWER_DOMAIN_PACKAGE:
        status = pPmt->readValue("ACCUM_PACKAGE_ENERGY", energyCounter);

        if (status != ZE_RESULT_SUCCESS) {
            status = pPmt->readValue("PACKAGE_ENERGY_STATUS_SKU", energyCounter);
        }
        break;
    case ZES_POWER_DOMAIN_CARD:
        status = pPmt->readValue("ACCUM_PSYS_ENERGY", energyCounter);

        if (status != ZE_RESULT_SUCCESS) {
            status = pPmt->readValue("PLATFORM_ENERGY_STATUS", energyCounter);
        }
        break;
    case ZES_POWER_DOMAIN_MEMORY:
        status = pPmt->readValue("VCCDDR_ENERGY_ACCUMULATOR", energyCounter);
        break;
    case ZES_POWER_DOMAIN_GPU:
        status = pPmt->readValue("VCCGT_ENERGY_ACCUMULATOR", energyCounter);
        break;
    default:
        DEBUG_BREAK_IF(true);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        break;
    }

    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }

    // Energy counter is in U(18.14) format. Need to convert it into uint64_t
    uint32_t integerPart = static_cast<uint32_t>(energyCounter >> 14);

    uint32_t decimalBits = static_cast<uint32_t>((energyCounter & 0x3FFF));
    double decimalPart = static_cast<double>(decimalBits) / (1 << 14);

    double result = static_cast<double>(integerPart + decimalPart);
    pEnergy->energy = static_cast<uint64_t>((result * convertJouleToMicroJoule));

    // timestamp calcuation
    uint64_t timestamp64 = 0;
    uint32_t frequency = 0;

    status = pPmt->readValue("XTAL_COUNT", timestamp64);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    status = pPmt->readValue("XTAL_CLK_FREQUENCY", frequency);
    if (status != ZE_RESULT_SUCCESS) {
        return status;
    }
    double timestamp = timestamp64 / indexToXtalClockFrequecyMap[frequency & 0x2];
    pEnergy->timestamp = static_cast<uint64_t>(timestamp);
    return status;
}

template <>
std::map<unsigned long, std::map<std::string, uint32_t>> *SysmanProductHelperHw<gfxProduct>::getGuidToKeyOffsetMap() {
    return &guidToKeyOffsetMap;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isZesInitSupported() {
    return true;
}

template <>
bool SysmanProductHelperHw<gfxProduct>::isLateBindingSupported() {
    return true;
}

template class SysmanProductHelperHw<gfxProduct>;

} // namespace Sysman
} // namespace L0
