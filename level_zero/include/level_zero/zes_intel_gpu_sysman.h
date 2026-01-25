/*
 * Copyright (C) 2025-2026 Intel Corporation
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
#ifndef ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE_NAME
/// @brief PCI link speed downgrade state extension name
#define ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE_NAME "ZES_intel_experimental_pci_link_speed_downgrade_state"
#endif // ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_STATE_NAME

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
#ifndef ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTY_NAME
/// @brief PCI link speed downgrade property extension name
#define ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTY_NAME "ZES_intel_experimental_pci_link_speed_downgrade_property"
#endif // ZES_INTEL_PCI_LINK_SPEED_DOWNGRADE_EXP_PROPERTY_NAME

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

///////////////////////////////////////////////////////////////////////////////
#ifndef ZES_INTEL_DRIVER_NAME_EXP_PROPERTY_NAME
/// @brief Driver name property extension name
#define ZES_INTEL_DRIVER_NAME_EXP_PROPERTY_NAME "ZES_intel_experimental_driver_name_property"
#endif // ZES_INTEL_DRIVER_NAME_EXP_PROPERTY_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Query driver name extension Version(s)
typedef enum _zes_intel_driver_name_exp_properties_version_t {
    ZES_INTEL_DRIVER_NAME_EXP_PROPERTIES_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_DRIVER_NAME_EXP_PROPERTIES_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_DRIVER_NAME_EXP_PROPERTIES_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_driver_name_exp_properties_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Query driver name.
/// This structure can be passed in the 'pNext' of zes_device_properties_t
typedef struct _zes_intel_driver_name_exp_properties_t {
    zes_structure_type_ext_t stype;            ///< [in] type of this structure
    void *pNext;                               ///< [in][optional] must be null or a pointer to an extension-specific
                                               ///< structure (i.e. contains stype and pNext).
    char driverName[ZES_STRING_PROPERTY_SIZE]; ///< [out] Installed driver name (NULL terminated string value). Will be
                                               ///< set to the string "unknown" if this cannot be determined for the
                                               ///< device.
} zes_intel_driver_name_exp_properties_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Frequency Detailed Throttle Reasons Extension Version(s)
typedef enum _zes_intel_freq_throttle_detailed_reason_exp_version_t {
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_freq_throttle_detailed_reason_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Frequency Aggregated Throttle Reasons
typedef enum _zes_intel_freq_throttle_aggregated_reason_exp_flag_t {
    ZES_INTEL_FREQ_THROTTLE_AGGREGATED_REASON_EXP_FLAG_POWER = ZE_BIT(7),       ///< frequency throttled due to power limits (PL1/PL2/PL4/ICC/VMode)
    ZES_INTEL_FREQ_THROTTLE_AGGREGATED_REASON_EXP_FLAG_VOLTAGE = ZE_BIT(8),     ///< frequency throttled due to Voltage
    ZES_INTEL_FREQ_THROTTLE_AGGREGATED_REASON_EXP_FLAG_FORCE_UINT32 = 0x7ffffff ///< Value marking end of ZES_INTEL_FREQ_THROTTLE_REASON_FLAG_* ENUMs
} zes_intel_freq_throttle_aggregated_reason_flag_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Frequency Detailed Throttle Reasons
typedef uint64_t zes_intel_freq_throttle_detailed_reason_exp_flags_t;
typedef enum _zes_intel_freq_throttle_detailed_reason_exp_flag_t {
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL1 = ZE_BIT(0),    ///< frequency throttled due to Card PL1 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL2 = ZE_BIT(1),    ///< frequency throttled due to Card PL2 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL4 = ZE_BIT(2),    ///< frequency throttled due to CARD PL4 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL1 = ZE_BIT(3), ///< frequency throttled due to PACKAGE PL1 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL2 = ZE_BIT(4), ///< frequency throttled due to PACKAGE PL2 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL4 = ZE_BIT(5), ///< frequency throttled due to PACKAGE PL4 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_ICCMAX = ZE_BIT(6),      ///< frequency throttled due to ICC max power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_FAST_VMODE = ZE_BIT(7),  ///< frequency throttled due to fast Vmode power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_MEMORY = ZE_BIT(8),    ///< frequency throttled due to memory thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_PROCHOT = ZE_BIT(9),   ///< frequency throttled due to Prochot thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_RATL = ZE_BIT(10),     ///< frequency throttled due to RATL thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_SOC = ZE_BIT(11),      ///< frequency throttled due to SoC thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_SOC_AVG = ZE_BIT(12),  ///< frequency throttled due to SoC average thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_VR = ZE_BIT(13),       ///< frequency throttled due to VR thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_VOLTAGE_P0_FREQ = ZE_BIT(14),  ///< frequency throttled due to P0 frequency
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_FORCE_UINT32 = 0x7fffffff      ///< Value marking end of ZES_INTEL_FREQ_THROTTLE_REASON_DETAILED_FLAG_* ENUMs
} zes_intel_freq_throttle_detailed_reason_exp_flag_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Detailed Frequency Throttle Reasons.
/// This structure can be passed in the 'pNext' of zes_intel_freq_state_t
typedef struct _zes_intel_freq_throttle_detailed_reason_exp_t {
    zes_structure_type_ext_t stype;                                      ///< [in] type of this structure
    void *pNext;                                                         ///< [in][optional] must be null or a pointer to an extension-specific
                                                                         ///< structure (i.e. contains stype and pNext).
    zes_intel_freq_throttle_detailed_reason_exp_flags_t detailedReasons; ///< [out] Returns the detailed frequency throttle reasons.
} zes_intel_freq_throttle_detailed_reason_exp_t;

///////////////////////////////////////////////////////////////////////////////
#ifndef ZES_INTEL_RAS_ERROR_THRESHOLD_MANAGEMENT_EXTENSION_NAME
/// @brief RAS Error Threshold Management Extension Name
#define ZES_INTEL_RAS_ERROR_THRESHOLD_MANAGEMENT_EXTENSION_NAME "ZES_intel_experimental_ras_error_threshold_management"
#endif // ZES_INTEL_RAS_ERROR_THRESHOLD_MANAGEMENT_EXTENSION_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Ras Config Driver Experimental Extension Version(s)
typedef enum _zes_intel_ras_config_exp_version_t {
    ZES_INTEL_RAS_CONFIG_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_RAS_CONFIG_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_RAS_CONFIG_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_ras_config_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Ras Config Driver Experimental Extension Structure
typedef struct _zes_intel_ras_config_exp_t {
    zes_structure_type_ext_t stype;        ///< [in] type of this structure
    void *pNext;                           ///< [in][optional] must be null or a pointer to an extension-specific
                                           ///< structure (i.e. contains stype and pNext).
    zes_ras_error_category_exp_t category; ///< [in] RAS error category
    uint64_t threshold;                    ///< [in][out] Error count threshold to trigger RAS action
                                           ///< [in] when calling zesIntelRasSetConfigExp
                                           ///< [out] when calling zesIntelRasGetConfigExp
} zes_intel_ras_config_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Ras State Driver Experimental Extension Version
typedef enum _zes_intel_ras_state_exp_version_t {
    ZES_INTEL_RAS_STATE_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_RAS_STATE_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_RAS_STATE_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_ras_state_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Ras State Driver Experimental Extension Structure
typedef struct _zes_intel_ras_state_exp_t {
    zes_structure_type_ext_t stype;        ///< [in] type of this structure
    void *pNext;                           ///< [in][optional] must be null or a pointer to an extension-specific
                                           ///< structure (i.e. contains stype and pNext).
    zes_ras_error_category_exp_t category; ///< [in] Error category.
    uint64_t errorCounter;                 ///< [out] Current value of RAS counter for specific error category.
} zes_intel_ras_state_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief RAS Get Supported Driver Experimental Extension Error Categories
///
/// @details
///     - This function retrieves the supported RAS error categories.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hRas`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pCount`
ze_result_t ZE_APICALL zesIntelRasGetSupportedCategoriesExp(
    zes_ras_handle_t hRas,                    ///< [in] Handle for the RAS module.
    uint32_t *pCount,                         ///< [in,out] pointer to the number of categories.
                                              ///< if count is zero, then the driver shall update the value with the
                                              ///< total number of categories supported.
                                              ///< if count is non-zero, then driver shall only retrieve that number
                                              ///< of categories.
    zes_ras_error_category_exp_t *pCategories ///< [in][out][optional] array of category types.
                                              ///< if count is less than the number of categories supported, then
                                              ///< driver shall only retrieve that number of categories.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief RAS Get Driver Experimental Extension Config
///
/// @details
///     - This function retrieves the RAS error thresholds for the given RAS error categories.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hRas`
ze_result_t ZE_APICALL zesIntelRasGetConfigExp(
    zes_ras_handle_t hRas,              ///< [in] Handle for the RAS module.
    const uint32_t count,               ///< [in] Number of elements in the pConfig array.
    zes_intel_ras_config_exp_t *pConfig ///< [in][out] Array of RAS configurations to get.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief RAS Set Driver Experimental Extension Config
///
/// @details
///     - This function sets the RAS error thresholds for the given RAS error categories.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hRas`
ze_result_t ZE_APICALL zesIntelRasSetConfigExp(
    zes_ras_handle_t hRas,                    ///< [in] Handle for the RAS module.
    const uint32_t count,                     ///< [in] Number of elements in the pConfig array.
    const zes_intel_ras_config_exp_t *pConfig ///< [in] Array of RAS configurations to set.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Ras Get Driver Experimental Extension State
///
/// @details
///     - This function retrieves error counters for different RAS error categories.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hRas`
ze_result_t ZE_APICALL zesIntelRasGetStateExp(
    zes_ras_handle_t hRas,            ///< [in] Handle for the RAS module.
    const uint32_t count,             ///< [in] Number of elements in the pState array.
    zes_intel_ras_state_exp_t *pState ///< [in][out] Array of RAS error states.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Get Experimental Power Limits
///
/// @details
///     - This function returns the power limit associated with the supplied power domain.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
///     - ::ZE_RESULT_ERROR_INVALID_ARGUMENT
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
///     - ::ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE
///     - ::ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS
///     - ::ZE_RESULT_ERROR_NOT_AVAILABLE
///     - ::ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET
///     - ::ZE_RESULT_ERROR_DEVICE_IN_LOW_POWER_STATE
///     - ::ZE_RESULT_ERROR_UNKNOWN
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hPower`
ze_result_t ZE_APICALL zesIntelPowerGetLimitsExp(
    zes_pwr_handle_t hPower, ///< [in] Power domain handle instance.
    uint32_t *pLimit         ///< [out] Returns limit value in milliwatts for given power domain.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Set Experimental Power Limits
///
/// @details
///     - This function sets the power limit associated with the supplied power domain.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
///     - ::ZE_RESULT_ERROR_INVALID_ARGUMENT
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
///     - ::ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE
///     - ::ZE_RESULT_ERROR_DEVICE_REQUIRES_RESET
///     - ::ZE_RESULT_ERROR_DEVICE_IN_LOW_POWER_STATE
///     - ::ZE_RESULT_ERROR_UNKNOWN
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hPower`
///     - ::ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS
///         + User does not have permissions to make these modifications.
///     - ::ZE_RESULT_ERROR_NOT_AVAILABLE
///         + The device is in use, meaning that the GPU is under Over clocking, applying power limits under overclocking is not supported.
ze_result_t ZE_APICALL zesIntelPowerSetLimitsExp(
    zes_pwr_handle_t hPower, ///< [in] Power domain handle instance.
    const uint32_t limit     ///< [in] Limit value in milliwatts to be set for given power domain.
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZES_INTEL_GPU_SYSMAN_H
