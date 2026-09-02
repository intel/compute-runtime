/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/api/info_log/sysman_info_log.h"
#include "level_zero/sysman/source/api/info_log/sysman_info_log_instance.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/tools/source/sysman/sysman.h"
#include "level_zero/zes_intel_gpu_sysman.h"

namespace L0 {

ze_result_t ZE_APICALL zesIntelDevicePciLinkSpeedUpdateExp(zes_device_handle_t hDevice, ze_bool_t downgradeUpgrade, zes_device_action_t *pendingAction) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDevice::pciLinkSpeedUpdate(hDevice, downgradeUpgrade, pendingAction);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelDeviceMemoryGetPageOfflineStateExp(zes_device_handle_t hDevice, zes_intel_mem_page_status_exp_t pageStatus, uint32_t *pCount, zes_intel_mem_page_info_exp_t *pPageOfflineInfo) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDevice::memoryGetPageOfflineStateExp(hDevice, pageStatus, pCount, pPageOfflineInfo);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelDriverRescanDevicesExp(zes_driver_handle_t hDriver, uint32_t *pCount, zes_device_handle_t *phDevices) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDriverHandle::fromHandle(hDriver)->getDeviceRescan(pCount, phDevices);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelDriverEnumInfoLogsExp(zes_driver_handle_t hDriver, uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDriverHandle::fromHandle(hDriver)->enumInfoLogs(pCount, phInfoLogs);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelDriverEventRegisterExp(zes_driver_handle_t hDriver, zes_event_type_flags_t events) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDriverHandle::fromHandle(hDriver)->driverEventRegister(events);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelDriverEventListenExp(zes_driver_handle_t hDriver, uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents, zes_event_type_flags_t *pDriverEvents) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::SysmanDriverHandle::fromHandle(hDriver)->sysmanDriverEventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents, pDriverEvents);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelInfoLogGetPropertiesExp(zes_intel_info_log_handle_t hInfoLog, zes_intel_info_log_properties_exp_t *pInfoLogProperties) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::InfoLog::fromHandle(hInfoLog)->infoLogGetProperties(pInfoLogProperties);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelInfoLogCreateInstanceExp(zes_intel_info_log_handle_t hInfoLog,
                                                        const char *pInstanceName,
                                                        zes_intel_info_log_instance_exp_desc_t *pDesc,
                                                        zes_intel_info_log_instance_handle_t *phInfoLogInstance) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::InfoLog::fromHandle(hInfoLog)->infoLogCreateInstance(pInstanceName, pDesc, phInfoLogInstance);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelInfoLogInstanceReadWithMetadataExp(zes_intel_info_log_instance_handle_t hInfoLogInstance,
                                                                  uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                                                  uint32_t *pRecordCount,
                                                                  zes_intel_info_log_metadata_exp *pDescriptors,
                                                                  zes_intel_info_log_read_status_exp_t *pReadStatus) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::InfoLogInstance::fromHandle(hInfoLogInstance)->readWithMetadata(timeout, pSize, pBuffer, pRecordCount, pDescriptors, pReadStatus);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelInfoLogInstancePeekWithMetadataExp(zes_intel_info_log_instance_handle_t hInfoLogInstance,
                                                                  uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                                                  uint32_t *pRecordCount,
                                                                  zes_intel_info_log_metadata_exp *pDescriptors,
                                                                  zes_intel_info_log_read_status_exp_t *pReadStatus) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::InfoLogInstance::fromHandle(hInfoLogInstance)->peekWithMetadata(timeout, pSize, pBuffer, pRecordCount, pDescriptors, pReadStatus);
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

ze_result_t ZE_APICALL zesIntelInfoLogInstanceDeleteExp(zes_intel_info_log_instance_handle_t hInfoLogInstance) {
    if (L0::sysmanInitFromCore) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    } else if (L0::Sysman::sysmanOnlyInit) {
        return L0::Sysman::InfoLogInstance::fromHandle(hInfoLogInstance)->destroy();
    } else {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
}

} // namespace L0

extern "C" {

ze_result_t ZE_APICALL zesIntelDevicePciLinkSpeedUpdateExp(zes_device_handle_t hDevice, ze_bool_t downgradeUpgrade, zes_device_action_t *pendingAction) {
    return L0::zesIntelDevicePciLinkSpeedUpdateExp(hDevice, downgradeUpgrade, pendingAction);
};

ze_result_t ZE_APICALL zesIntelDeviceMemoryGetPageOfflineStateExp(zes_device_handle_t hDevice, zes_intel_mem_page_status_exp_t pageStatus, uint32_t *pCount, zes_intel_mem_page_info_exp_t *pPageOfflineInfo) {
    return L0::zesIntelDeviceMemoryGetPageOfflineStateExp(hDevice, pageStatus, pCount, pPageOfflineInfo);
}

ze_result_t ZE_APICALL zesIntelDriverRescanDevicesExp(zes_driver_handle_t hDriver, uint32_t *pCount, zes_device_handle_t *phDevices) {
    return L0::zesIntelDriverRescanDevicesExp(hDriver, pCount, phDevices);
}

ze_result_t ZE_APICALL zesIntelDriverEnumInfoLogsExp(zes_driver_handle_t hDriver, uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) {
    return L0::zesIntelDriverEnumInfoLogsExp(hDriver, pCount, phInfoLogs);
}

ze_result_t ZE_APICALL zesIntelDriverEventRegisterExp(zes_driver_handle_t hDriver, zes_event_type_flags_t events) {
    return L0::zesIntelDriverEventRegisterExp(hDriver, events);
}

ze_result_t ZE_APICALL zesIntelDriverEventListenExp(zes_driver_handle_t hDriver, uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents, zes_event_type_flags_t *pDriverEvents) {
    return L0::zesIntelDriverEventListenExp(hDriver, timeout, count, phDevices, pNumDeviceEvents, pEvents, pDriverEvents);
}

ze_result_t ZE_APICALL zesIntelInfoLogGetPropertiesExp(zes_intel_info_log_handle_t hInfoLog, zes_intel_info_log_properties_exp_t *pInfoLogProperties) {
    return L0::zesIntelInfoLogGetPropertiesExp(hInfoLog, pInfoLogProperties);
}

ze_result_t ZE_APICALL zesIntelInfoLogCreateInstanceExp(zes_intel_info_log_handle_t hInfoLog,
                                                        const char *pInstanceName,
                                                        zes_intel_info_log_instance_exp_desc_t *pDesc,
                                                        zes_intel_info_log_instance_handle_t *phInfoLogInstance) {
    return L0::zesIntelInfoLogCreateInstanceExp(hInfoLog, pInstanceName, pDesc, phInfoLogInstance);
}

ze_result_t ZE_APICALL zesIntelInfoLogInstanceReadWithMetadataExp(zes_intel_info_log_instance_handle_t hInfoLogInstance,
                                                                  uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                                                  uint32_t *pRecordCount,
                                                                  zes_intel_info_log_metadata_exp *pDescriptors,
                                                                  zes_intel_info_log_read_status_exp_t *pReadStatus) {
    return L0::zesIntelInfoLogInstanceReadWithMetadataExp(hInfoLogInstance, timeout, pSize, pBuffer, pRecordCount, pDescriptors, pReadStatus);
}

ze_result_t ZE_APICALL zesIntelInfoLogInstancePeekWithMetadataExp(zes_intel_info_log_instance_handle_t hInfoLogInstance,
                                                                  uint64_t timeout, uint32_t *pSize, uint8_t *pBuffer,
                                                                  uint32_t *pRecordCount,
                                                                  zes_intel_info_log_metadata_exp *pDescriptors,
                                                                  zes_intel_info_log_read_status_exp_t *pReadStatus) {
    return L0::zesIntelInfoLogInstancePeekWithMetadataExp(hInfoLogInstance, timeout, pSize, pBuffer, pRecordCount, pDescriptors, pReadStatus);
}

ze_result_t ZE_APICALL zesIntelInfoLogInstanceDeleteExp(zes_intel_info_log_instance_handle_t hInfoLogInstance) {
    return L0::zesIntelInfoLogInstanceDeleteExp(hInfoLogInstance);
}

} // extern "C"
