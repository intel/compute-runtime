/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <map>
#include <string>

namespace L0 {

// Each entry of this map corresponds to one particular graphics card type for example, DG1 A or B step.
// or XeHP_SDV. GUID string will help in identify the card type
const std::map<std::string, std::map<std::string, uint64_t>> guidToKeyOffsetMap = {
    {"0x490e01", // DG1 B stepping
     {{"PACKAGE_ENERGY", 0x420},
      {"COMPUTE_TEMPERATURES", 0x68},
      {"SOC_TEMPERATURES", 0x60},
      {"CORE_TEMPERATURES", 0x6c}}},
    {"0x490e", // DG1 A stepping
     {{"PACKAGE_ENERGY", 0x400},
      {"COMPUTE_TEMPERATURES", 0x68},
      {"SOC_TEMPERATURES", 0x60},
      {"CORE_TEMPERATURES", 0x6c}}},
    {"0x4f95", // For DG2 device
     {{"PACKAGE_ENERGY", 1032},
      {"SOC_TEMPERATURES", 56}}}, // SOC_TEMPERATURE contains GT_TEMP, DRAM_TEMP, SA_TEMP, DE_TEMP, PCIE_TEMP, TYPEC_TEMP
    {"0x4f9301",                  // For ATSM device
     {{"PACKAGE_ENERGY", 1032},
      {"SOC_TEMPERATURES", 56}}}, // SOC_TEMPERATURE contains GT_TEMP, DRAM_TEMP, SA_TEMP, DE_TEMP, PCIE_TEMP, TYPEC_TEMP
    {"0xfdc76194",                // For XeHP_SDV device
     {{"HBM0MaxDeviceTemperature", 28},
      {"HBM1MaxDeviceTemperature", 36},
      {"TileMinTemperature", 40},
      {"TileMaxTemperature", 44},
      {"GTMinTemperature", 48},
      {"GTMaxTemperature", 52},
      {"VF0_VFID", 88},
      {"VF0_HBM0_READ", 92},
      {"VF0_HBM0_WRITE", 96},
      {"VF0_HBM1_READ", 104},
      {"VF0_HBM1_WRITE", 108},
      {"VF0_TIMESTAMP_L", 168},
      {"VF0_TIMESTAMP_H", 172},
      {"VF1_VFID", 176},
      {"VF1_HBM0_READ", 180},
      {"VF1_HBM0_WRITE", 184},
      {"VF1_HBM1_READ", 192},
      {"VF1_HBM1_WRITE", 196},
      {"VF1_TIMESTAMP_L", 256},
      {"VF1_TIMESTAMP_H", 260}}},
    {"0xfdc76196", // For XeHP_SDV B0 device
     {{"HBM0MaxDeviceTemperature", 28},
      {"HBM1MaxDeviceTemperature", 36},
      {"TileMinTemperature", 40},
      {"TileMaxTemperature", 44},
      {"GTMinTemperature", 48},
      {"GTMaxTemperature", 52},
      {"VF0_VFID", 88},
      {"VF0_HBM0_READ", 92},
      {"VF0_HBM0_WRITE", 96},
      {"VF0_HBM1_READ", 104},
      {"VF0_HBM1_WRITE", 108},
      {"VF0_TIMESTAMP_L", 168},
      {"VF0_TIMESTAMP_H", 172},
      {"VF1_VFID", 176},
      {"VF1_HBM0_READ", 180},
      {"VF1_HBM0_WRITE", 184},
      {"VF1_HBM1_READ", 192},
      {"VF1_HBM1_WRITE", 196},
      {"VF1_TIMESTAMP_L", 256},
      {"VF1_TIMESTAMP_H", 260}}},
    {"0xb15a0edc", // For PVC device
     {{"HBM0MaxDeviceTemperature", 28},
      {"HBM1MaxDeviceTemperature", 36},
      {"TileMinTemperature", 40},
      {"TileMaxTemperature", 44},
      {"GTMinTemperature", 48},
      {"GTMaxTemperature", 52},
      {"VF0_VFID", 88},
      {"VF0_HBM0_READ", 92},
      {"VF0_HBM0_WRITE", 96},
      {"VF0_HBM1_READ", 104},
      {"VF0_HBM1_WRITE", 108},
      {"VF0_TIMESTAMP_L", 168},
      {"VF0_TIMESTAMP_H", 172},
      {"VF1_VFID", 176},
      {"VF1_HBM0_READ", 180},
      {"VF1_HBM0_WRITE", 184},
      {"VF1_HBM1_READ", 192},
      {"VF1_HBM1_WRITE", 196},
      {"VF1_TIMESTAMP_L", 256},
      {"VF1_TIMESTAMP_H", 260},
      {"HBM2MaxDeviceTemperature", 300},
      {"HBM3MaxDeviceTemperature", 308},
      {"VF0_HBM2_READ", 312},
      {"VF0_HBM2_WRITE", 316},
      {"VF0_HBM3_READ", 328},
      {"VF0_HBM3_WRITE", 332},
      {"VF1_HBM2_READ", 344},
      {"VF1_HBM2_WRITE", 348},
      {"VF1_HBM3_READ", 360},
      {"VF1_HBM3_WRITE", 364}}}};

} // namespace L0