/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZES_INTEL_GPU_SYSMAN_H
#define _ZES_INTEL_GPU_SYSMAN_H

#include "level_zero/ze_stypes.h"
#include <level_zero/zes_api.h>

#if defined(__cplusplus)
#pragma once
extern "C" {
#endif

#include <stdint.h>

#define ZES_INTEL_GPU_SYSMAN_VERSION_MAJOR 0
#define ZES_INTEL_GPU_SYSMAN_VERSION_MINOR 1

///////////////////////////////////////////////////////////////////////////////
/// @brief Query pcie downgrade status extension Version(s)
typedef enum _zes_intel_pci_link_speed_downgrade_exp_state_version_t {
    ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_pci_link_speed_downgrade_exp_state_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Query pcie downgrade status.
/// This structure can be passed in the 'pNext' of zes_pci_state_t
typedef struct _zes_intel_pci_link_speed_downgrade_exp_state_t {
    zes_structure_type_ext_t stype;        ///< [in] type of this structure
    void *pNext;                           ///< [in][optional] must be null or a pointer to an extension-specific
                                           ///< structure (i.e. contains stype and pNext).
    ze_bool_t pciLinkSpeedDowngradeStatus; ///< [out] Returns the current PCIe downgrade status .
} zes_intel_pci_link_speed_downgrade_exp_state_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Query pcie downgrade capability extension Version(s)
typedef enum _zes_intel_pci_link_speed_downgrade_exp_properties_version_t {
    ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTIES_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTIES_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTIES_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_pci_link_speed_downgrade_exp_properties_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Query pcie downgrade capability.
/// This structure can be passed in the 'pNext' of zes_pci_properties_t
typedef struct _zes_intel_pci_link_speed_downgrade_exp_properties_t {
    zes_structure_type_ext_t stype;      ///< [in] type of this structure
    void *pNext;                         ///< [in][optional] must be null or a pointer to an extension-specific
                                         ///< structure (i.e. contains stype and pNext).
    ze_bool_t pciLinkSpeedUpdateCapable; ///< [out] Returns if PCIe downgrade capability is available.
    int32_t maxPciGenSupported;          ///< [out] Returns the max supported PCIe generation of the device. -1 indicated the information is not available
} zes_intel_pci_link_speed_downgrade_exp_properties_t;

ze_result_t ZE_APICALL zesIntelDevicePciLinkSpeedUpdateExp(
    zes_device_handle_t hDevice,       ///< [in] handle of the device
    ze_bool_t downgradeUpgrade,        ///< [in] boolean value to decide whether to perform PCIe downgrade(true) or upgrade(false)
    zes_device_action_t *pendingAction ///< [out] Pending action
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif
