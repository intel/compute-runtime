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
/// @brief Experimental init flag to allow zesInit() to succeed when no GPU devices are present.
/// @details
///     - Enables deferred device discovery mode.
///     - Device discovery is deferred until the first call to zesDeviceGet() or related APIs.
///     - This flag uses bit 16 to avoid conflicts with standard flags (bits 0-15).
#define ZES_INTEL_INIT_FLAG_EXP_NO_GPUS ZE_BIT(16)

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

///////////////////////////////////////////////////////////////////////////////
#ifndef ZES_INTEL_PCI_LINK_SPEED_UPDATE_EXP_NAME
/// @brief PCI link speed update extension name
#define ZES_INTEL_PCI_LINK_SPEED_UPDATE_EXP_NAME "ZES_intel_experimental_pci_link_speed_update"
#endif // ZES_INTEL_PCI_LINK_SPEED_UPDATE_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief PCI link speed update extension Version(s)
typedef enum _zes_intel_pci_link_speed_update_exp_version_t {
    ZES_INTEL_PCI_LINK_SPEED_UPDATE_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_PCI_LINK_SPEED_UPDATE_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_PCI_LINK_SPEED_UPDATE_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_pci_link_speed_update_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Update PCIe Link Speed
///
/// @details
///     - This function allows updating the PCIe link speed by downgrading or upgrading.
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
///         + `nullptr == hDevice`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pendingAction`
ze_result_t ZE_APICALL zesIntelDevicePciLinkSpeedUpdateExp(
    zes_device_handle_t hDevice,       ///< [in] handle of the device
    ze_bool_t downgradeUpgrade,        ///< [in] boolean value to decide whether to perform PCIe downgrade(true) or upgrade(false)
    zes_device_action_t *pendingAction ///< [out] Pending action
);

///////////////////////////////////////////////////////////////////////////////
#ifndef ZES_INTEL_DRIVER_RESCAN_DEVICES_EXP_NAME
/// @brief Driver device rescan extension name
#define ZES_INTEL_DRIVER_RESCAN_DEVICES_EXP_NAME "ZES_intel_experimental_driver_rescan_devices"
#endif // ZES_INTEL_DRIVER_RESCAN_DEVICES_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Driver device rescan extension Version(s)
typedef enum _zes_intel_driver_rescan_devices_exp_version_t {
    ZES_INTEL_DRIVER_RESCAN_DEVICES_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_DRIVER_RESCAN_DEVICES_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_DRIVER_RESCAN_DEVICES_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_driver_rescan_devices_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Rescan devices after BDF changes
///
/// @details
///     - This function scans for devices that may have changed their PCI BDF
///       (Domain:Bus:Device:Function) address after events like device reset or hot-plug.
///     - The driver will update internal cached BDF information for devices that moved.
///     - Device handles remain valid after this call - applications do not need to re-enumerate.
///     - This is a synchronous operation that may take several milliseconds.
///     - The application must not call any other sysman telemetry or query APIs
///       concurrently with this call. While the rescan is in progress the driver
///       is updating its cached BDF-dependent state, so the device objects are in
///       a transient/inconsistent state and any concurrent sysman API may return
///       stale or incorrect data.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_UNKNOWN
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
///     - ::ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///         + One or more devices were unplugged and are no longer available
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hDriver`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pCount`
ze_result_t ZE_APICALL zesIntelDriverRescanDevicesExp(
    zes_driver_handle_t hDriver,   ///< [in] handle of the driver instance
    uint32_t *pCount,              ///< [in,out] pointer to the number of devices.
                                   ///< if count is zero, then the driver shall update the value with the
                                   ///< total number of devices available.
                                   ///< if count is greater than the number of devices available, then the
                                   ///< driver shall update the value with the correct number of devices available.
    zes_device_handle_t *phDevices ///< [in,out][optional][range(0, *pCount)] array of handle of devices.
                                   ///< if count is less than the number of devices available, then driver
                                   ///< shall only retrieve that number of device handles.
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
#ifndef ZES_INTEL_FREQ_THROTTLE_REASON_EXP_NAME
/// @brief Frequency throttle reason extension name
#define ZES_INTEL_FREQ_THROTTLE_REASON_EXP_NAME "ZES_intel_experimental_frequency_throttle_reason"
#endif // ZES_INTEL_FREQ_THROTTLE_REASON_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Frequency throttle reason extension Version(s)
typedef enum _zes_intel_freq_throttle_reason_exp_version_t {
    ZES_INTEL_FREQ_THROTTLE_REASON_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_FREQ_THROTTLE_REASON_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_FREQ_THROTTLE_REASON_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_freq_throttle_reason_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Frequency Detailed Throttle Reasons Extension Version(s)
typedef enum _zes_intel_freq_throttle_detailed_reason_exp_version_t {
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_freq_throttle_detailed_reason_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Frequency Detailed Throttle Reasons
typedef uint64_t zes_intel_freq_throttle_detailed_reason_exp_flags_t;
typedef enum _zes_intel_freq_throttle_detailed_reason_exp_flag_t {
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL1 = ZE_BIT(0),    ///< frequency throttled due to CARD PL1 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL2 = ZE_BIT(1),    ///< frequency throttled due to CARD PL2 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_CARD_PL4 = ZE_BIT(2),    ///< frequency throttled due to CARD PL4 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL1 = ZE_BIT(3), ///< frequency throttled due to PACKAGE PL1 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL2 = ZE_BIT(4), ///< frequency throttled due to PACKAGE PL2 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_PACKAGE_PL4 = ZE_BIT(5), ///< frequency throttled due to PACKAGE PL4 power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_ICCMAX = ZE_BIT(6),      ///< frequency throttled due to ICC max power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_POWER_FAST_VMODE = ZE_BIT(7),  ///< frequency throttled due to fast Vmode power
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_MEMORY = ZE_BIT(8),    ///< frequency throttled due to memory thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_PROCHOT = ZE_BIT(9),   ///< frequency throttled due to Prochot thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_SOC = ZE_BIT(10),      ///< frequency throttled due to SoC thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_SOC_AVG = ZE_BIT(11),  ///< frequency throttled due to SoC average thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_THERMAL_VR = ZE_BIT(12),       ///< frequency throttled due to VR thermal
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_VOLTAGE_P0_FREQ = ZE_BIT(13),  ///< frequency throttled due to P0 frequency
    ZES_INTEL_FREQ_THROTTLE_DETAILED_REASON_EXP_FLAG_FORCE_UINT32 = 0x7fffffff      ///< Value marking end of ZES_INTEL_FREQ_THROTTLE_REASON_DETAILED_FLAG_* ENUMs
} zes_intel_freq_throttle_detailed_reason_exp_flag_t;

#define ZES_INTEL_FREQ_THROTTLE_REASON_EXP_FLAG_UTILIZATION_LIMITED ZE_BIT(10) // Frequency utilization limit reason flag used when no specific detailed reason is available

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
#ifndef ZES_INTEL_MEMORY_PAGE_OFFLINE_EXP_NAME
/// @brief  Memory offline extension name
#define ZES_INTEL_MEMORY_PAGE_OFFLINE_EXP_NAME "ZES_intel_memory_page_offline"
#endif // ZES_INTEL_MEMORY_PAGE_OFFLINE_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Memory offline extension Version(s)
typedef enum _zes_intel_memory_page_offline_exp_version_t {
    ZES_INTEL_MEMORY_PAGE_OFFLINE_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_MEMORY_PAGE_OFFLINE_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_MEMORY_PAGE_OFFLINE_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_memory_page_offline_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Memory Page status
typedef enum _zes_intel_mem_page_status_exp_t {
    ZES_INTEL_MEM_PAGE_STATUS_EXP_OFFLINE = 1,
    ZES_INTEL_MEM_PAGE_STATUS_EXP_PENDING_OFFLINE = 2,
    ZES_INTEL_MEM_PAGE_STATUS_EXP_FORCE_UINT32 = 0x7fffffff
} zes_intel_mem_page_status_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Memory Page information structure
typedef struct _zes_intel_mem_page_info_exp_t {
    zes_structure_type_ext_t stype; ///< [in] type of this structure
    void *pNext;                    ///< [in,out][optional] pointer to extension-specific  structure
    uint64_t pageAddress;           ///< [out] Physical address of the memory page
    uint32_t pageSize;              ///< [out] Size of the page in bytes
} zes_intel_mem_page_info_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Get Memory Page Offline
///
/// @details
///     - This function returns the memory page offline state.
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
///     - ::ZE_RESULT_ERROR_SURVIVABILITY_MODE_DETECTED
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hDevice`
ze_result_t ZE_APICALL zesIntelDeviceMemoryGetPageOfflineStateExp(
    zes_device_handle_t hDevice,                    ///< [in] handle of the device
    zes_intel_mem_page_status_exp_t pageStatus,     ///< [in] Status of the Memory Pages to be queried
    uint32_t *pCount,                               ///< [in,out] pointer to the number of memory pages which are already offlined or pending to be offlined.
                                                    ///< if count is zero, then the driver shall update the value with the
                                                    ///< total number of memory pages in the given status.
                                                    ///< if count is non-zero, then driver shall only retrieve that number
                                                    ///< of memory pages in the given status.
    zes_intel_mem_page_info_exp_t *pPageOfflineInfo ///< [in,out][optional] array of memory page information structure.
                                                    ///< if count is less than the number of memory pages in the given status, then
                                                    ///< driver shall only retrieve that number of memory pages in the given status.
);
#ifndef ZES_INTEL_DEVICE_STATE_PENDING_ACTION_EXP_NAME
/// @brief Device state extension name
#define ZES_INTEL_DEVICE_STATE_PENDING_ACTION_EXP_NAME "ZES_intel_device_state_pending_action_exp"
#endif // ZES_INTEL_DEVICE_STATE_PENDING_ACTION_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Device state extension Version(s)
typedef enum _zes_intel_device_state_pending_action_exp_version_t {
    ZES_INTEL_DEVICE_STATE_PENDING_ACTION_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_DEVICE_STATE_PENDING_ACTION_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_DEVICE_STATE_PENDING_ACTION_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_device_state_pending_action_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Extension to provide wedged device recovery action
///
/// @details
///     - This structure can be passed in the 'pNext' of zes_device_state_t
///     - Provides information about pending actions required for device recovery
typedef struct _zes_intel_device_state_pending_action_exp_t {
    zes_structure_type_ext_t stype;     ///< [in] type of this structure
    const void *pNext;                  ///< [in][optional] must be null or a pointer to an extension-specific
                                        ///< structure (i.e. contains stype and pNext).
    zes_pending_action_t pendingAction; ///< [out] Indicates the pending action required for device recovery.
                                        ///< When device is wedged, will be set to ZES_PENDING_ACTION_PENDING_COLD_RESET
                                        ///< For example, When device is wedged this will be set to ZES_PENDING_ACTION_PENDING_COLD_RESET
} zes_intel_device_state_pending_action_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Intel experimental extension to the standard ::zes_device_state_ext_flag_t
///
/// @details
///     - These flags extend the standard device state flags (bits 0-3 defined by
///       ::zes_device_state_ext_flag_t)
#define ZES_INTEL_DEVICE_STATE_EXP_FLAG_GPU_LOST ZE_BIT(4)          ///< The GPU is lost: the device PCI path is inaccessible
#define ZES_INTEL_DEVICE_STATE_EXP_FLAG_DRIVER_NOT_LOADED ZE_BIT(5) ///< No kernel driver is bound to the device

///////////////////////////////////////////////////////////////////////////////
#ifndef ZES_INTEL_MEMORY_PAGE_OFFLINE_PROPERTY_EXP_NAME
/// @brief  Memory Page Offline Property extension name
#define ZES_INTEL_MEMORY_PAGE_OFFLINE_PROPERTY_EXP_NAME "ZES_intel_memory_page_offline_property"
#endif // ZES_INTEL_MEMORY_PAGE_OFFLINE_PROPERTY_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Memory Page Offline Property extension Version(s)
typedef enum _zes_intel_mem_page_offline_properties_exp_version_t {
    ZES_INTEL_MEM_PAGE_OFFLINE_PROPERTIES_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),     ///< version 1.0
    ZES_INTEL_MEM_PAGE_OFFLINE_PROPERTIES_EXP_VERSION_CURRENT = ZE_MAKE_VERSION(1, 0), ///< latest known version
    ZES_INTEL_MEM_PAGE_OFFLINE_PROPERTIES_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_mem_page_offline_properties_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Memory Page Offline Properties structure
typedef struct _zes_intel_mem_page_offline_properties_exp_t {
    zes_structure_type_ext_t stype; ///< [in] type of this structure
    void *pNext;                    ///< [in,out][optional] must be null or a pointer to an extension-specific
    uint32_t maxOfflinePages;       ///< [out] Maximum number of pages that can be offlined.
                                    ///< Returns 0 if page offline is not supported.
} zes_intel_mem_page_offline_properties_exp_t;

///////////////////////////////////////////////////////////////////////////////
#ifndef ZES_INTEL_DRIVER_INFO_LOGS_EXP_NAME
/// @brief Driver info logs extension name
#define ZES_INTEL_DRIVER_INFO_LOGS_EXP_NAME "ZES_intel_experimental_driver_info_logs"
#endif // ZES_INTEL_DRIVER_INFO_LOGS_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Driver info logs extension Version(s)
typedef enum _zes_intel_driver_info_logs_exp_version_t {
    ZES_INTEL_DRIVER_INFO_LOGS_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),                          ///< version 1.0, no longer implemented
    ZES_INTEL_DRIVER_INFO_LOGS_EXP_VERSION_2_0 = ZE_MAKE_VERSION(2, 0),                          ///< version 2.0, collection instances. Not backward compatible with version 1.0.
    ZES_INTEL_DRIVER_INFO_LOGS_EXP_VERSION_CURRENT = ZES_INTEL_DRIVER_INFO_LOGS_EXP_VERSION_2_0, ///< latest known version
    ZES_INTEL_DRIVER_INFO_LOGS_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_driver_info_logs_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle to an info log instance
typedef struct _zes_intel_info_log_handle_t *zes_intel_info_log_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Handle to an info log collection instance
typedef struct _zes_intel_info_log_instance_handle_t *zes_intel_info_log_instance_handle_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Get handles for Info Logs
///
/// @details
///     - This function retrieves the list of available info logs.
///     - The caller should first call this function with count pointer set to 0 to retrieve the total number of available logs.
///     - Subsequent calls with a non-zero count will return the info log handles.
///     - This API is NOT thread-safe. It must be called from a single thread or process.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
///     - ::ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY
///     - ::ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hDriver`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pCount`
ze_result_t ZE_APICALL zesIntelDriverEnumInfoLogsExp(
    zes_driver_handle_t hDriver,            ///< [in] handle of the driver
    uint32_t *pCount,                       ///< [in,out] pointer to the number of info logs.
                                            ///< if count is zero, then the driver shall update the value with the
                                            ///< total number of available info logs.
                                            ///< if count is non-zero, then driver shall only retrieve that number
                                            ///< of info logs.
    zes_intel_info_log_handle_t *phInfoLogs ///< [in][out][optional] array of info log handles.
                                            ///< if count is less than the number of available logs, then
                                            ///< driver shall only retrieve that number of logs.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Info log type
typedef enum _zes_intel_info_log_type_exp_t {
    ZES_INTEL_INFO_LOG_TYPE_EXP_DEVICE = 0, ///< Device info log
    ZES_INTEL_INFO_LOG_TYPE_EXP_FORCE_UINT32 = 0x7fffffff
} zes_intel_info_log_type_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Info Log format
typedef enum _zes_intel_info_log_format_exp_t {
    ZES_INTEL_INFO_LOG_FORMAT_CPER = 0,
    ZES_INTEL_INFO_LOG_FORMAT_FORCE_UINT32 = 0x7fffffff
} zes_intel_info_log_format_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Info log record types
///
/// @details
///     - The numeric values do not express an order of severity and must not be compared for magnitude.
///     - A value the application does not recognize must be treated as
///       ::ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_UNKNOWN.
typedef enum _zes_intel_info_log_record_type_exp_t {
    ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_UNKNOWN = 0,           ///< The type of the record could not be determined.
    ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_INFORMATIONAL = 1,     ///< The record does not report an error.
    ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_CORRECTED = 2,   ///< The error was corrected by the device.
    ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_RECOVERABLE = 3, ///< The error was not corrected and is not fatal.
    ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_ERROR_FATAL = 4,       ///< The error was not corrected and is fatal.
    ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_FORCE_UINT32 = 0x7fffffff
} zes_intel_info_log_record_type_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Info log properties structure
typedef struct _zes_intel_info_log_properties_exp_t {
    zes_structure_type_ext_t stype;                ///< [in] type of this structure. Must be ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_PROPERTIES_EXP
    void *pNext;                                   ///< [in,out][optional] pointer to extension-specific structure
    zes_intel_info_log_type_exp_t infoLogType;     ///< [out] Type of the info log
    zes_intel_info_log_format_exp_t infoLogFormat; ///< [out] Format of the info log.
    ze_bool_t isNamedInstancedCollectionSupported; ///< [out] true if this info log supports named collection instances, i.e.
                                                   ///< ::zesIntelInfoLogCreateInstanceExp accepts a non-null pInstanceName.
    ze_bool_t isPeekSupported;                     ///< [out] true if records can be read without being consumed, i.e.
                                                   ///< ::zesIntelInfoLogInstancePeekWithMetadataExp is supported
} zes_intel_info_log_properties_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Info log collection instance descriptor
///
/// @details
///     - Each member is optional. A nullptr member means "use the driver default".
///     - On input a non-null member points to the requested value; on output the driver updates the
///       pointed to value with the value which was actually applied, which may be rounded.
typedef struct _zes_intel_info_log_instance_exp_desc_t {
    zes_structure_type_ext_t stype; ///< [in] type of this structure. Must be ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_INSTANCE_EXP_DESC
    void *pNext;                    ///< [in,out][optional] pointer to extension-specific structure
    uint32_t *pBufferSize;          ///< [in,out][optional] pointer to the total size, in kilobytes, of the collection
                                    ///< buffer of the instance, aggregated across all of its per-CPU buffers. On input
                                    ///< the requested total size, which the driver splits evenly across the per-CPU
                                    ///< buffers; zero keeps the current size. On output the total size which was
                                    ///< actually applied, which may be rounded up, or zero if it could not be
                                    ///< determined.
} zes_intel_info_log_instance_exp_desc_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Status of a read from an info log collection instance
typedef struct _zes_intel_info_log_read_status_exp_t {
    zes_structure_type_ext_t stype;      ///< [in] type of this structure. Must be ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_READ_STATUS_EXP
    void *pNext;                         ///< [in,out][optional] pointer to extension-specific structure
    uint32_t droppedRecordCount;         ///< [out] Number of records dropped because the collection buffer overflowed, since
                                         ///< the previous read or peek on this instance. Reported as 0 when
                                         ///< isDroppedRecordCountValid is false.
    ze_bool_t isDroppedRecordCountValid; ///< [out] true when the driver could read every dropped record counter of this
                                         ///< instance, so droppedRecordCount is the complete count for the interval. false when
                                         ///< the driver could not determine the count: records may still have been dropped, and
                                         ///< a count the driver could not report is reported by a later call which can read the
                                         ///< counters.
    ze_bool_t hasDataToRead;             ///< [out] true when the collection instance holds records this call did not return,
                                         ///< for example because timeout elapsed or because pBuffer or pDescriptors was too
                                         ///< small. This value is a hint, and is only meaningful when the call returned
                                         ///< ::ZE_RESULT_SUCCESS; a failed call reports the reason through its return value.
} zes_intel_info_log_read_status_exp_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Per-record metadata returned when reading or peeking info log records
typedef struct _zes_intel_info_log_metadata_exp {
    zes_structure_type_ext_t stype;                  ///< [in] must be ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_METADATA_EXP
    void *pNext;                                     ///< [in,out][optional]
    zes_pci_address_t address;                       ///< [out] Device BDF (domain:bus:device.function)
    zes_uuid_t uuid;                                 ///< [out] Device UUID (fru_id from trace event)
    uint64_t timestamp;                              ///< [out] Event timestamp in nanoseconds. The reference point is implementation
                                                     ///< specific and only consistent across records of the same info log.
    uint32_t lengthOfData;                           ///< [out] CPER record byte length
    uint32_t offset;                                 ///< [out] Byte offset of this record in pBuffer
    zes_intel_info_log_record_type_exp_t recordType; ///< [out] Type of the information reported by the record, derived from the
                                                     ///< severity it was reported with. ::ZES_INTEL_INFO_LOG_RECORD_TYPE_EXP_UNKNOWN
                                                     ///< means the severity was not known.
} zes_intel_info_log_metadata_exp;

///////////////////////////////////////////////////////////////////////////////
/// @brief Get Info Log Properties
///
/// @details
///     - This function retrieves the properties of an info log handle.
///     - This API is NOT thread-safe. It must be called from a single thread or process.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
///     - ::ZE_RESULT_ERROR_NOT_AVAILABLE
///     - ::ZE_RESULT_ERROR_UNKNOWN
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hInfoLog`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pProperties`
ze_result_t ZE_APICALL zesIntelInfoLogGetPropertiesExp(
    zes_intel_info_log_handle_t hInfoLog,            ///< [in] handle of the info log
    zes_intel_info_log_properties_exp_t *pProperties ///< [in,out] pointer to info log properties
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Create a collection instance for an info log
///
/// @details
///     - Creating a collection instance starts the collection of records into a buffer owned by
///       that instance. Collection continues until the instance is deleted with
///       ::zesIntelInfoLogInstanceDeleteExp.
///     - A named collection instance collects into a buffer which is not shared with other
///       collection instances, and requires
///       zes_intel_info_log_properties_exp_t.isNamedInstancedCollectionSupported.
///     - A given name may only be collected from by one collection instance at a time, across all
///       processes: while one instance holds a name, creating another instance with that name
///       returns ::ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE. The name is released when the owning
///       instance is deleted, or when the process owning it exits.
///     - A named buffer which already exists but is not held by any collection instance, for
///       example one provisioned outside of this driver, is collected from rather than rejected, and
///       is left in place when the collection instance is deleted.
///     - When pInstanceName is nullptr the records are collected from the default buffer, which may
///       be shared with other consumers and may carry data from other sources.
///     - The application must not call this function from simultaneous threads.
///     - The application must pass a valid hInfoLog, a non-null pDesc whose stype is
///       ZES_INTEL_STRUCTURE_TYPE_INFO_LOG_INSTANCE_EXP_DESC, and a non-null phInfoLogInstance.
///       The behaviour is undefined otherwise.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
///         + `nullptr != pInstanceName` and named collection instances are not supported
///     - ::ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE
///         + a collection instance with the same name already exists, in this or in another process
///     - ::ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS
///         + the caller may not claim the named buffer
///     - ::ZE_RESULT_ERROR_NOT_AVAILABLE
///         + the named buffer could not be located
///     - ::ZE_RESULT_ERROR_UNKNOWN
///         + the collection instance could not be created or configured
ze_result_t ZE_APICALL zesIntelInfoLogCreateInstanceExp(
    zes_intel_info_log_handle_t hInfoLog,                   ///< [in] handle of the info log
    const char *pInstanceName,                              ///< [in][optional] name of the collection instance to create.
                                                            ///< nullptr uses the default buffer of the info log.
    zes_intel_info_log_instance_exp_desc_t *pDesc,          ///< [in,out] descriptor of the collection instance to create
    zes_intel_info_log_instance_handle_t *phInfoLogInstance ///< [out] handle of the collection instance which was created
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Read collected info log records and their metadata
///
/// @details
///     - A call in which `*pSize` is zero or `*pRecordCount` is zero on input is a query call. A
///       query call reports the total size in bytes of the record data and the total number of
///       records found, writes to neither pBuffer nor pDescriptors, and consumes nothing. pBuffer
///       and pDescriptors may be nullptr on a query call.
///     - Records are consumed as they are returned, a record returned by one call is not returned
///       again.
///     - `timeout` bounds the search of the data held by the collection instance. This driver never
///       waits for records to arrive: it stops as soon as the data currently held by the instance
///       has been searched, even when the timeout has not elapsed.
///     - The application must initialize the stype member of every pDescriptors element and of
///       pReadStatus.
///     - The application must not call this function from simultaneous threads, and must not call it
///       simultaneously with ::zesIntelInfoLogInstancePeekWithMetadataExp on the same handle.
///     - The application must pass a valid hInfoLogInstance, a non-null pSize and a non-null
///       pRecordCount, and must pass a non-null pBuffer and pDescriptors whenever `*pSize` and
///       `*pRecordCount` are both non-zero. The behaviour is undefined otherwise.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///         + a record which did not fit in the remaining space of pBuffer is not consumed and is
///           returned by the next call.
///           zes_intel_info_log_read_status_exp_t.hasDataToRead reports that records remain.
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_NOT_AVAILABLE
///     - ::ZE_RESULT_ERROR_UNKNOWN
///         + the collection data could not be read
ze_result_t ZE_APICALL zesIntelInfoLogInstanceReadWithMetadataExp(
    zes_intel_info_log_instance_handle_t hInfoLogInstance, ///< [in] handle of the info log collection instance
    uint64_t timeout,                                      ///< [in] maximum time, in milliseconds, spent searching for records.
                                                           ///< 0 returns immediately without searching, UINT64_MAX searches all
                                                           ///< the data held by the instance.
    uint32_t *pSize,                                       ///< [in,out] on input, size of pBuffer in bytes. On output, bytes written,
                                                           ///< or total bytes found on a query call.
    uint8_t *pBuffer,                                      ///< [in,out][optional][range(0, *pSize)] destination for the record data
    uint32_t *pRecordCount,                                ///< [in,out] on input, number of elements in pDescriptors. On output,
                                                           ///< records written, or total records found on a query call.
    zes_intel_info_log_metadata_exp *pDescriptors,         ///< [in,out][optional][range(0, *pRecordCount)] per-record metadata
    zes_intel_info_log_read_status_exp_t *pReadStatus      ///< [in,out][optional] status of this call
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Read collected info log records and their metadata without consuming them
///
/// @details
///     - Equivalent to ::zesIntelInfoLogInstanceReadWithMetadataExp except that the records
///       returned are not consumed and remain available to subsequent calls to either function.
///     - Only supported when zes_intel_info_log_properties_exp_t.isPeekSupported is true.
///     - A record which a previous read left partially buffered is not visible to this function.
///     - The same argument requirements as ::zesIntelInfoLogInstanceReadWithMetadataExp apply.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
///         + peek is not supported for this info log
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_NOT_AVAILABLE
///     - ::ZE_RESULT_ERROR_UNKNOWN
ze_result_t ZE_APICALL zesIntelInfoLogInstancePeekWithMetadataExp(
    zes_intel_info_log_instance_handle_t hInfoLogInstance, ///< [in] handle of the info log collection instance
    uint64_t timeout,                                      ///< [in] maximum time, in milliseconds, spent searching for records
    uint32_t *pSize,                                       ///< [in,out] size of pBuffer in bytes in, bytes found out
    uint8_t *pBuffer,                                      ///< [in,out][optional][range(0, *pSize)] destination for the record data
    uint32_t *pRecordCount,                                ///< [in,out] elements in pDescriptors in, records found out
    zes_intel_info_log_metadata_exp *pDescriptors,         ///< [in,out][optional][range(0, *pRecordCount)] per-record metadata
    zes_intel_info_log_read_status_exp_t *pReadStatus      ///< [in,out][optional] status of this call
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Delete a collection instance of an info log
///
/// @details
///     - Stops collection into the instance and releases the resources it allocated.
///     - The application must ensure that no other function is using the handle when calling this
///       function, must not call it from simultaneous threads with the same handle, and must not
///       call it twice with the same handle. The behaviour is undefined otherwise.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_UNKNOWN
///         + the collection instance could not be fully torn down
ze_result_t ZE_APICALL zesIntelInfoLogInstanceDeleteExp(
    zes_intel_info_log_instance_handle_t hInfoLogInstance ///< [in][release] handle of the collection instance to delete
);

///////////////////////////////////////////////////////////////////////////////
#ifndef ZES_INTEL_DRIVER_EVENT_EXP_NAME
/// @brief Driver scoped event extension name
#define ZES_INTEL_DRIVER_EVENT_EXP_NAME "ZES_intel_experimental_driver_event"
#endif // ZES_INTEL_DRIVER_EVENT_EXP_NAME

///////////////////////////////////////////////////////////////////////////////
/// @brief Driver scoped event extension Version(s)
typedef enum _zes_intel_driver_event_exp_version_t {
    ZES_INTEL_DRIVER_EVENT_EXP_VERSION_1_0 = ZE_MAKE_VERSION(1, 0),                      ///< version 1.0
    ZES_INTEL_DRIVER_EVENT_EXP_VERSION_CURRENT = ZES_INTEL_DRIVER_EVENT_EXP_VERSION_1_0, ///< latest known version
    ZES_INTEL_DRIVER_EVENT_EXP_VERSION_FORCE_UINT32 = 0x7fffffff
} zes_intel_driver_event_exp_version_t;

///////////////////////////////////////////////////////////////////////////////
/// @brief Driver Scoped CPER Data Available Event
#define ZES_INTEL_CPER_DATA_AVAILABLE ZE_BIT(16)

///////////////////////////////////////////////////////////////////////////////
/// @brief Register driver scoped events to be notified about
///
/// @details
///     - This function registers the driver scoped events the application wants to
///       be notified about. Unlike ::zesDeviceEventRegister the registration is not
///       tied to a device: the underlying data source is shared by all devices of
///       the driver.
///     - Only the Intel experimental driver scoped event flags are accepted, i.e.
///       ::ZES_INTEL_CPER_DATA_AVAILABLE. Standard ::zes_event_type_flag_t values
///       must be registered per device with ::zesDeviceEventRegister.
///     - Unlike ::zesDeviceEventRegister, which adds to the events already registered
///       for a device, this function replaces the set of registered driver scoped
///       events. Calling it with `events` set to 0 therefore clears all previously
///       registered driver scoped events.
///     - Registered events are reported only by ::zesIntelDriverEventListenExp, in its
///       `pDriverEvents` argument. As the events are driver scoped they have no device
///       handle to be reported against, so ::zesDriverEventListen and
///       ::zesDriverEventListenEx never report them.
///     - Calling this function while another thread is blocked in a listen call updates
///       that call, so an event registered after a listen has started can still be
///       reported by it.
///     - ::ZES_INTEL_CPER_DATA_AVAILABLE requires at least one info log collection
///       instance to have been created with ::zesIntelInfoLogCreateInstanceExp *before*
///       the listen call is made. It is reported when any live collection instance has
///       data pending. The data itself is left in place and must be retrieved with
///       ::zesIntelInfoLogInstanceReadWithMetadataExp.
///     - The application may call this function from simultaneous threads. However,
///       listening for ::ZES_INTEL_CPER_DATA_AVAILABLE must be serialized with
///       ::zesIntelInfoLogInstanceReadWithMetadataExp on a single thread, as both
///       operate on the same underlying trace buffer reader.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
///     - ::ZE_RESULT_ERROR_UNKNOWN
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hDriver`
///     - ::ZE_RESULT_ERROR_INVALID_ENUMERATION
///         + `events` contains a flag which is not a driver scoped event
ze_result_t ZE_APICALL zesIntelDriverEventRegisterExp(
    zes_driver_handle_t hDriver,  ///< [in] handle of the driver instance
    zes_event_type_flags_t events ///< [in] list of driver scoped events to listen to. Must be 0 or a combination
                                  ///< of the Intel experimental driver scoped event flags.
);

///////////////////////////////////////////////////////////////////////////////
/// @brief Listen for device scoped and driver scoped events
///
/// @details
///     - This function extends ::zesDriverEventListenEx with the ability to report the
///       driver scoped events registered with ::zesIntelDriverEventRegisterExp.
///     - The `hDriver`, `timeout`, `count`, `phDevices`, `pNumDeviceEvents` and `pEvents`
///       arguments behave exactly as in ::zesDriverEventListenEx: `pEvents` holds `count`
///       entries, one per device handle, and `*pNumDeviceEvents` is an output only value.
///     - `pDriverEvents` is optional. When it is `nullptr` this function behaves exactly
///       like ::zesDriverEventListenEx and no driver scoped event source is listened to.
///     - When `pDriverEvents` is not `nullptr` it is cleared on entry and, on return,
///       contains the driver scoped events which occurred, i.e. 0 or a combination of the
///       Intel experimental driver scoped event flags.
///     - `*pNumDeviceEvents` accounts for device scoped events only and, as in
///       ::zesDriverEventListenEx, is set to 1 when one or more device scoped events occurred,
///       irrespective of the number of device handles they occurred for. It is not a count of
///       the device handles which had events, so the application must scan `pEvents` to find
///       them. A driver scoped event which occurs without any device scoped event therefore
///       returns `*pNumDeviceEvents` set to 0 and `*pDriverEvents` set to the events which
///       occurred, so an application must check both values.
///     - ::ZES_INTEL_CPER_DATA_AVAILABLE requires at least one info log collection instance to
///       have been created with ::zesIntelInfoLogCreateInstanceExp *before* this call is made.
///       The data itself is left in place and must be retrieved with
///       ::zesIntelInfoLogInstanceReadWithMetadataExp.
///     - The application should not call this function from simultaneous threads with the
///       same driver handle.
///
/// @returns
///     - ::ZE_RESULT_SUCCESS
///     - ::ZE_RESULT_ERROR_UNINITIALIZED
///     - ::ZE_RESULT_ERROR_DEVICE_LOST
///     - ::ZE_RESULT_ERROR_UNSUPPORTED_FEATURE
///     - ::ZE_RESULT_ERROR_UNKNOWN
///     - ::ZE_RESULT_ERROR_INVALID_NULL_HANDLE
///         + `nullptr == hDriver`
///     - ::ZE_RESULT_ERROR_INVALID_NULL_POINTER
///         + `nullptr == pNumDeviceEvents`
///         + `nullptr == pEvents`
///     - ::ZE_RESULT_ERROR_INVALID_ARGUMENT
///         + one of the handles in `phDevices` is not a valid device handle
ze_result_t ZE_APICALL zesIntelDriverEventListenExp(
    zes_driver_handle_t hDriver,          ///< [in] handle of the driver instance
    uint64_t timeout,                     ///< [in] if non-zero, then indicates the maximum time (in milliseconds) to
                                          ///< yield before returning ::ZE_RESULT_SUCCESS or ::ZE_RESULT_NOT_READY;
                                          ///< if zero, then will check status and return immediately;
                                          ///< if `UINT64_MAX`, then function will not return until events arrive.
    uint32_t count,                       ///< [in] Number of device handles in phDevices.
    zes_device_handle_t *phDevices,       ///< [in][range(0, count)] Device handles to listen to for events. Only
                                          ///< devices from the provided driver handle can be specified in this list.
    uint32_t *pNumDeviceEvents,           ///< [out] Set to 1 if one or more device scoped events occurred for any of
                                          ///< the device handles in `phDevices`, 0 otherwise. Not a count of the
                                          ///< device handles which had events, `pEvents` must be scanned to find them.
    zes_event_type_flags_t *pEvents,      ///< [in,out][range(0, count)] Returns events that occurred for each device
                                          ///< handle, in the same order as `phDevices`.
    zes_event_type_flags_t *pDriverEvents ///< [in,out][optional] Returns the driver scoped events which occurred, i.e.
                                          ///< 0 or a combination of the Intel experimental driver scoped event flags.
                                          ///< When `nullptr`, driver scoped events are not listened to.
);

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZES_INTEL_GPU_SYSMAN_H
