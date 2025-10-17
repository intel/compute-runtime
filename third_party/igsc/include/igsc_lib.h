/*
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2019-2024 Intel Corporation
 */

/**
 * @file igsc_lib.h
 * @brief Intel Graphics System Controller Library API
 */

#ifndef __IGSC_LIB_H__
#define __IGSC_LIB_H__

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL_HIDDEN */
#ifndef IN
#define IN
#endif /* IN */
#ifndef OUT
#define OUT
#endif /* OUT */

#if defined(_WIN32) || defined(_WIN64)
#ifndef IGSC_DLL
#define IGSC_EXPORT
#else /* IGSC_DLL */
#ifdef IGSC_DLL_EXPORTS
#define IGSC_EXPORT __declspec(dllexport)
#else
#define IGSC_EXPORT __declspec(dllimport)
#endif
#endif /* IGSC_DLL */
#else
#ifndef IGSC_DLL
#define IGSC_EXPORT
#else /* IGSC_DLL */
#ifdef IGSC_DLL_EXPORTS
#define IGSC_EXPORT __attribute__((__visibility__("default")))
#else
#define IGSC_EXPORT
#endif
#endif /* IGSC_DLL */
#endif
/** @endcond */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef BIT
#define BIT(x) (1U << (x))
#endif /* BIT */

#ifndef UNUSED_VAR
#define UNUSED_VAR(_v) (void)_v
#endif

/**
 * A file descriptor
 * @typedef igsc_handle_t
 * @n
 * Under Linux: file descriptor int fd = open(2)
 * @n
 * Under Windows: HANDLE handle = CreateFile()
 */
#ifdef __linux__
typedef int igsc_handle_t;
#define IGSC_INVALID_DEVICE_HANDLE (-1)
#else /* __linux__ */
typedef void *igsc_handle_t;
#define IGSC_INVALID_DEVICE_HANDLE ((void *)0)
#endif /* __linux__ */

/**
 * types of supported log levels
 */
enum igsc_log_level_type {
    IGSC_LOG_LEVEL_ERROR = 0, /**< Errors only */
    IGSC_LOG_LEVEL_DEBUG = 1, /**< Debug messages and errors */
    IGSC_LOG_LEVEL_TRACE = 2, /**< Trace, debug messages and errors */
    IGSC_LOG_LEVEL_MAX = 3,   /**< Upper boundary */
};

/**
 *  @brief Sets log level
 *
 *  @param log_level log level to set
 *
 *  @return void
 */
IGSC_EXPORT
void igsc_set_log_level(unsigned int log_level);

/**
 *  @brief Retrieves current log level
 *
 *  @return current log level
 */
IGSC_EXPORT
unsigned int igsc_get_log_level(void);

/**
 * types of supported update images
 */
enum igsc_image_type {
    IGSC_IMAGE_TYPE_UNKNOWN = 0, /**< Unknown image type */
    IGSC_IMAGE_TYPE_GFX_FW,      /**< GSC Firmware image */
    IGSC_IMAGE_TYPE_OPROM,       /**< OPROM CODA an DATA combined image */
    IGSC_IMAGE_TYPE_OPROM_CODE,  /**< OPROM code image */
    IGSC_IMAGE_TYPE_OPROM_DATA,  /**< OPROM data image */
    IGSC_IMAGE_TYPE_FW_DATA,     /**< firmware data image */
};

/**
 * Structure to store fw version data
 */
struct igsc_fw_version {
    char project[4]; /**< Project code name */
    uint16_t hotfix; /**< FW Hotfix Number */
    uint16_t build;  /**< FW Build Number */
};

/**
 * Structure to store ifr binary version data
 */
struct igsc_ifr_bin_version {
    uint16_t major;  /**< IFR Binary Major Number */
    uint16_t minor;  /**< IFR Binary Minor Number */
    uint16_t hotfix; /**< IFR Binary Hotfix Number */
    uint16_t build;  /**< IFR Binary Build Number */
};

/**
 * Structure to store psc version data
 */
struct igsc_psc_version {
    uint32_t date;        /**< PSC date */
    uint32_t cfg_version; /**< PSC configuration version */
};

#define IGSC_MAX_OEM_VERSION_LENGTH 256

/**
 * Structure to store oem version data
 */
struct igsc_oem_version {
    uint16_t length;                              /**< actual OEM version length */
    uint8_t version[IGSC_MAX_OEM_VERSION_LENGTH]; /**< buffer to store oem version */
};

/**
 * versions comparison results
 */
enum igsc_version_compare_result {
    IGSC_VERSION_ERROR = 0,          /**< An internal error during comparison */
    IGSC_VERSION_NOT_COMPATIBLE = 1, /**< cannot compare, the update image is for a different platform */
    IGSC_VERSION_NEWER = 2,          /**< update image version is newer than the one on the device */
    IGSC_VERSION_EQUAL = 3,          /**< update image version is equal to the one on the device */
    IGSC_VERSION_OLDER = 4,          /**< update image version is older than the one on the device */
};

/**
 * fwdata versions comparison results
 */
enum igsc_fwdata_version_compare_result {
    IGSC_FWDATA_VERSION_REJECT_VCN = 0,                    /**< VCN version is bigger than device VCN */
    IGSC_FWDATA_VERSION_REJECT_OEM_MANUF_DATA_VERSION = 1, /**< OEM manufacturing data version is not bigger than device OEM version or equal in ver2 comparison */
    IGSC_FWDATA_VERSION_REJECT_DIFFERENT_PROJECT = 2,      /**< major version is different from device major version */
    IGSC_FWDATA_VERSION_ACCEPT = 3,                        /**< update image VCN version is equal than the one on the device, and OEM is bigger */
    IGSC_FWDATA_VERSION_OLDER_VCN = 4,                     /**< update image VCN version is smaller than the one on the device */
    IGSC_FWDATA_VERSION_REJECT_WRONG_FORMAT = 5,           /**< the version format is the wrong one or incompatible */
    IGSC_FWDATA_VERSION_REJECT_ARB_SVN = 6,                /**< update image SVN version is smaller than the one on the device */
};

/**
 * Structure to store OEM manufacturing data version and data major VCN
 * for GSC in-field data firmware update image
 */
struct igsc_fwdata_version {
    uint32_t oem_manuf_data_version; /**< GSC in-field data firmware OEM manufacturing data version */
    uint16_t major_version;          /**< GSC in-field data firmware major version */
    uint16_t major_vcn;              /**< GSC in-field data firmware major VCN */
};

#define IGSC_FWDATA_FORMAT_VERSION_1 0x1
#define IGSC_FWDATA_FORMAT_VERSION_2 0x2

#define IGSC_FWDATA_FITB_VALID_MASK 0x1

/**
 * Structure to store versions
 * for GSC in-field data firmware update image (version 2)
 */
struct igsc_fwdata_version2 {
    uint32_t format_version;              /**< GSC in-field data firmware version format */
    uint32_t oem_manuf_data_version;      /**< GSC in-field data firmware OEM manufacturing data version */
    uint32_t oem_manuf_data_version_fitb; /**< GSC in-field data firmware OEM manufacturing data version from FITB */
    uint16_t major_version;               /**< GSC in-field data firmware major version */
    uint16_t major_vcn;                   /**< GSC in-field data firmware major VCN */
    uint32_t flags;                       /**< GSC in-field data firmware flags */
    uint32_t data_arb_svn;                /**< GSC in-field data firmware SVN */
    uint32_t data_arb_svn_fitb;           /**< GSC in-field data firmware SVN from FITB */
};

/**
 * OPROM partition version size in bytes
 */
#define IGSC_OPROM_VER_SIZE 8
/**
 * Structure to store OPROM version data
 */
struct igsc_oprom_version {
    uint8_t version[IGSC_OPROM_VER_SIZE]; /**< OPROM Version string */
};

/**
 * OPROM partition type
 */
enum igsc_oprom_type {
    IGSC_OPROM_NONE = 0,    /**< OPROM INVALID PARTITION */
    IGSC_OPROM_DATA = 0x01, /**< OPROM data (VBT) */
    IGSC_OPROM_CODE = 0x02, /**< OPROM code (VBIOS and GOP) */
};

/**
 * subsystem vendor and device id support by the OPROM image
 * as defined by PCI.
 */
struct igsc_oprom_device_info {
    uint16_t subsys_vendor_id; /**< subsystem vendor id */
    uint16_t subsys_device_id; /**< subsystem device id */
};

/**
 * vendor and device id, subsystem vendor and device id support by
 * the GSC in-field data firmware update image as defined by PCI.
 */
struct igsc_oprom_device_info_4ids {
    uint16_t vendor_id;        /**< vendor id */
    uint16_t device_id;        /**< device id */
    uint16_t subsys_vendor_id; /**< subsystem vendor id */
    uint16_t subsys_device_id; /**< subsystem device id */
};

/**
 * vendor and device id, subsystem vendor and device id support by
 * the GSC in-field data firmware update image as defined by PCI.
 */
struct igsc_fwdata_device_info {
    uint16_t vendor_id;        /**< vendor id */
    uint16_t device_id;        /**< device id */
    uint16_t subsys_vendor_id; /**< subsystem vendor id */
    uint16_t subsys_device_id; /**< subsystem device id */
};

/**
 * @struct igsc_oprom_image
 * opaque struct for oprom image handle
 */
struct igsc_oprom_image;

/**
 * @struct igsc_fwdata_image
 * opaque struct for fw data image handle
 */
struct igsc_fwdata_image;

/**
 * opaque structure representing device lookup context
 */
struct igsc_device_iterator;

/**
 * A device node path (Linux) or device instance path (Windows) Length
 */
#define IGSC_INFO_NAME_SIZE 256

/**
 * Structure to store GSC device info
 */
struct igsc_device_info {
    char name[IGSC_INFO_NAME_SIZE]; /**<  the device node path */

    uint16_t domain; /**< pci domain for GFX device */
    uint8_t bus;     /**< pci bus number for GFX device */
    uint8_t dev;     /**< device number on pci bus */
    uint8_t func;    /**< device function number */

    uint16_t device_id;        /**< gfx device id */
    uint16_t vendor_id;        /**< gfx device vendor id */
    uint16_t subsys_device_id; /**< gfx device subsystem device id */
    uint16_t subsys_vendor_id; /**< gfx device subsystem vendor id */
};

/**
 * @name IGSC_ERRORS
 *     The Library return codes
 * @addtogroup IGSC_ERRORS
 * @{
 */
#define IGSC_ERROR_BASE 0x0000U                             /**< Error Base */
#define IGSC_SUCCESS (IGSC_ERROR_BASE + 0)                  /**< Success */
#define IGSC_ERROR_INTERNAL (IGSC_ERROR_BASE + 1)           /**< Internal Error */
#define IGSC_ERROR_NOMEM (IGSC_ERROR_BASE + 2)              /**< Memory Allocation Failed */
#define IGSC_ERROR_INVALID_PARAMETER (IGSC_ERROR_BASE + 3)  /**< Invalid parameter was provided */
#define IGSC_ERROR_DEVICE_NOT_FOUND (IGSC_ERROR_BASE + 4)   /**< Requested device was not found */
#define IGSC_ERROR_BAD_IMAGE (IGSC_ERROR_BASE + 5)          /**< Provided image has wrong format */
#define IGSC_ERROR_PROTOCOL (IGSC_ERROR_BASE + 6)           /**< Error in the update protocol */
#define IGSC_ERROR_BUFFER_TOO_SMALL (IGSC_ERROR_BASE + 7)   /**< Provided buffer is too small */
#define IGSC_ERROR_INVALID_STATE (IGSC_ERROR_BASE + 8)      /**< Invalid library internal state */
#define IGSC_ERROR_NOT_SUPPORTED (IGSC_ERROR_BASE + 9)      /**< Unsupported request */
#define IGSC_ERROR_INCOMPATIBLE (IGSC_ERROR_BASE + 10)      /**< Incompatible request */
#define IGSC_ERROR_TIMEOUT (IGSC_ERROR_BASE + 11)           /**< The operation has timed out */
#define IGSC_ERROR_PERMISSION_DENIED (IGSC_ERROR_BASE + 12) /**< The process doesn't have access rights */
#define IGSC_ERROR_BUSY (IGSC_ERROR_BASE + 13)              /**< Device is currently busy, try again later */
/**
 * @}
 */

/**
 * Hardware configuration blob size
 */
#define IGSC_HW_CONFIG_BLOB_SIZE 48

/**
 * @brief structure to store hw configuration
 * @param format_version version of the hw config
 * @param blob hardware configuration data
 */
struct igsc_hw_config {
    uint32_t format_version;
    uint8_t blob[IGSC_HW_CONFIG_BLOB_SIZE];
};

/**
 * @brief structure to store device subsystem ids
 * @param ssvid subsystem vendor id
 * @param ssdid subsystem device id
 */
struct igsc_subsystem_ids {
    uint16_t ssvid;
    uint16_t ssdid;
};

/**
 * @def IGSC_MAX_IMAGE_SIZE
 * @brief Maximum firmware image size
 */
#define IGSC_MAX_IMAGE_SIZE (8 * 1024 * 1024) /* 8M */

struct igsc_lib_ctx;

/**
 * Structure to store GSC FU device data
 */
struct igsc_device_handle {
    struct igsc_lib_ctx *ctx; /**< Internal library context */
};

/**
 * @addtogroup firmware_status
 * @{
 */

/**
 *  @brief Return the last firmware status code.
 *
 *  @param handle A handle to the device.
 *
 *  @return last firmware status code.
 */
IGSC_EXPORT
uint32_t igsc_get_last_firmware_status(IN struct igsc_device_handle *handle);

/**
 *  @brief Return the firmware status message corresponding to the code.
 *
 *  @param firmware_status code of firmware status
 *
 *  @return firmware status message corresponding to the code.
 */
IGSC_EXPORT
const char *igsc_translate_firmware_status(IN uint32_t firmware_status);

#define IGSC_MAX_FW_STATUS_INDEX 5

/**
 *  @brief Read firmware status register
 *
 *  @param handle A handle to the device.
 *  @param fwsts_index index of the firmware status register (0..IGSC_MAX_FW_STATUS_INDEX)
 *  @param fwsts_value returned firmware status values
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_read_fw_status_reg(IN struct igsc_device_handle *handle,
                            IN uint32_t fwsts_index,
                            OUT uint32_t *fwsts_value);

/**
 *  @brief Callback function template for printing igsc log messages.
 *
 *  @param log_level log level of the error message.
 *  @param fmt log message format
 *  @param ... variadic parameters
 */
typedef void (*igsc_log_func_t)(enum igsc_log_level_type log_level, const char *fmt, ...);

/**
 *  @brief Sets log callback function.
 *          This interface is not thread-aware,
 *          Changes here may lead to crashes in multi-thread app
 *          when the thread setting callback exists without setting this
 *          call-back function to NULL while other thread from same app continues to run.
 *
 *  @param log_callback_f pointer to the callback function for igsc library log messages.
 *          passing NULL to this will disable logging callback function.
 *  @return void.
 */
IGSC_EXPORT
void igsc_set_log_callback_func(IN igsc_log_func_t log_callback_f);

/**
 *  @brief Retrieves log callback function pointer
 *
 *  @return log callback function pointer
 */
IGSC_EXPORT
igsc_log_func_t igsc_get_log_callback_func(void);

/**
 *  @brief Initializes a GSC Firmware Update device.
 *
 *  @param handle A handle to the device. All subsequent calls to the lib's
 *         functions must be with this handle.
 *  @param device_path A path to the device
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_init_by_device(IN OUT struct igsc_device_handle *handle,
                               IN const char *device_path);

/**
 *  @brief Initializes a GSC Firmware Update device.
 *
 *  @param handle A handle to the device. All subsequent calls to the lib's
 *         functions must be with this handle.
 *  @param dev_handle An open device handle
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
int igsc_device_init_by_handle(IN OUT struct igsc_device_handle *handle,
                               IN igsc_handle_t dev_handle);

/**
 *  @brief Initializes a GSC Firmware Update device.
 *
 *  @param handle A handle to the device. All subsequent calls to the lib's
 *         functions must be with this handle.
 *  @param dev_info A device info structure
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_init_by_device_info(IN OUT struct igsc_device_handle *handle,
                                    IN const struct igsc_device_info *dev_info);

/**
 *  @brief Retrieve device information from the system
 *
 *  @param handle An initialized handle to the device.
 *  @param dev_info A device info structure
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_get_device_info(IN struct igsc_device_handle *handle,
                                OUT struct igsc_device_info *dev_info);

/**
 *  @brief Update device information from the firmware
 *         The subsystem device id and the subsystem vendor id,
 *         reported by the PCI system, may be different from the ones
 *         reported by the firmware and so the device information
 *         should be updated by with the values received from the firmware.
 *
 *  @param handle An initialized handle to the device.
 *  @param dev_info A device info structure
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_update_device_info(IN struct igsc_device_handle *handle,
                                   OUT struct igsc_device_info *dev_info);

/**
 *  @brief Closes a GSC Firmware Update device.
 *
 *  @param handle A handle to the device.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_close(IN OUT struct igsc_device_handle *handle);

/**
 *  @brief Retrieves the GSC Firmware Version from the device.
 *
 *  @param handle A handle to the device.
 *  @param version The memory to store obtained firmware version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_fw_version(IN struct igsc_device_handle *handle,
                           OUT struct igsc_fw_version *version);

/**
 *  @brief Retrieves the Firmware Version from the provided
 *  firmware update image.
 *
 *  @param buffer A pointer to the buffer with the firmware update image.
 *  @param buffer_len Length of the buffer with the firmware update image.
 *  @param version The memory to store the obtained firmware version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_fw_version(IN const uint8_t *buffer,
                          IN uint32_t buffer_len,
                          OUT struct igsc_fw_version *version);

/**
 *  @brief Retrieves the hw configuration from the device
 *
 *  @param handle A handle to the device.
 *  @param hw_config The memory to store obtained the hardware configuration
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 *  @return IGSC_ERROR_NOT_SUPPORTED if feature is not supported
 */
IGSC_EXPORT
int igsc_device_hw_config(IN struct igsc_device_handle *handle,
                          OUT struct igsc_hw_config *hw_config);

/**
 *  @brief Retrieves the subsystem ids (vid/did) from the device
 *
 *  @param handle A handle to the device.
 *  @param ssids The memory to store obtained subsystem ids
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 *  @return IGSC_ERROR_NOT_SUPPORTED if feature is not supported
 */
IGSC_EXPORT
int igsc_device_subsystem_ids(IN struct igsc_device_handle *handle,
                              OUT struct igsc_subsystem_ids *ssids);

/**
 *  @brief Retrieves the hw configurations from the provided
 *  firmware update image.
 *
 *  @param buffer A pointer to the buffer with the firmware update image.
 *  @param buffer_len Length of the buffer with the firmware update image.
 *  @param hw_config The memory to store obtained the hardware configuration
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_hw_config(IN const uint8_t *buffer,
                         IN uint32_t buffer_len,
                         OUT struct igsc_hw_config *hw_config);

/**
 *  @brief Check whether image hardware configuration compatible with
 *  device hardware configuration.
 *
 *  @param image_hw_config Stores the image hardware configuration.
 *  @param device_hw_config Stores the device hardware configuration.
 *
 *  @return IGSC_SUCCESS if image hardware configuration compatible with device.
 */
IGSC_EXPORT
int igsc_hw_config_compatible(IN const struct igsc_hw_config *image_hw_config,
                              IN const struct igsc_hw_config *device_hw_config);
/**
 *  @brief express hw configuration in a string
 *
 *  @param hw_config Stores the hardware configuration.
 *  @param buf to store the hw configuration in a printable null terminated string
 *  @param length length of supplied buffer
 *
 *  @return number of bytes in buffer excluding null terminator or < 0 on error
 */
IGSC_EXPORT
int igsc_hw_config_to_string(IN const struct igsc_hw_config *hw_config,
                             IN char *buf, IN size_t length);

/**
 *  @brief Callback function template for monitor firmware update progress.
 *
 *  @param sent Number of bytes sent to the firmware.
 *  @param total Total number of bytes in firmware update image.
 *  @param ctx Context provided by caller.
 */
typedef void (*igsc_progress_func_t)(uint32_t sent, uint32_t total, void *ctx);

/**
 *  @brief Perform the firmware update from the provided firmware update image.
 *
 *  @param handle A handle to the device.
 *  @param buffer A pointer to the buffer with the firmware update image.
 *  @param buffer_len Length of the buffer with the firmware update image.
 *  @param progress_f Pointer to the callback function for firmware update
 *         progress monitor.
 *  @param ctx Context passed to progress_f function.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT int
igsc_device_fw_update(IN struct igsc_device_handle *handle,
                      IN const uint8_t *buffer,
                      IN const uint32_t buffer_len,
                      IN igsc_progress_func_t progress_f,
                      IN void *ctx);

/* flags with which the update should be performed */
struct igsc_fw_update_flags {
    uint32_t force_update : 1;
    uint32_t reserved : 31;
};

/**
 *  @brief Perform the firmware update with flags from the provided firmware update image.
 *
 *  @param handle A handle to the device.
 *  @param buffer A pointer to the buffer with the firmware update image.
 *  @param buffer_len Length of the buffer with the firmware update image.
 *  @param progress_f Pointer to the callback function for firmware update
 *         progress monitor.
 *  @param ctx Context passed to progress_f function.
 *  @param flags flags with which the update should be performed
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT int
igsc_device_fw_update_ex(IN struct igsc_device_handle *handle,
                         IN const uint8_t *buffer,
                         IN const uint32_t buffer_len,
                         IN igsc_progress_func_t progress_f,
                         IN void *ctx,
                         IN struct igsc_fw_update_flags flags);

/**
 *  @brief Perform Intel Accelerator Fabric Platform Specific
 *         Configuration (PSC) update from the provided update data
 *         image.
 *
 *  @param handle A handle to the device.
 *  @param buffer A pointer to the buffer with the PSC data update image.
 *  @param buffer_len Length of the buffer with the PSC data update image.
 *  @param progress_f Pointer to the callback function for data update
 *         progress monitor.
 *  @param ctx Context passed to progress_f function.
 *
 *  @return
 *  * IGSC_SUCCESS if successful
 *  * otherwise error code.
 */
IGSC_EXPORT int
igsc_iaf_psc_update(IN struct igsc_device_handle *handle,
                    IN const uint8_t *buffer,
                    IN const uint32_t buffer_len,
                    IN igsc_progress_func_t progress_f,
                    IN void *ctx);

/**
 *  @brief Perform the GSC firmware in-field data update from the provided firmware update image.
 *
 *  @param handle A handle to the device.
 *  @param buffer A pointer to the buffer with the firmware in-field data update image.
 *  @param buffer_len Length of the buffer with the firmware in-field data update image.
 *  @param progress_f Pointer to the callback function for firmware update
 *         progress monitor.
 *  @param ctx Context passed to progress_f function.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT int
igsc_device_fwdata_update(IN struct igsc_device_handle *handle,
                          IN const uint8_t *buffer,
                          IN const uint32_t buffer_len,
                          IN igsc_progress_func_t progress_f,
                          IN void *ctx);

/**
 *  @brief Perform the GSC firmware in-field data update from the provided firmware update image.
 *
 *  @param handle A handle to the device.
 *  @param img A pointer to the parsed firmware data image structure.
 *  @param progress_f Pointer to the callback function for firmware update
 *         progress monitor.
 *  @param ctx Context passed to progress_f function.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT int
igsc_device_fwdata_image_update(IN struct igsc_device_handle *handle,
                                IN struct igsc_fwdata_image *img,
                                IN igsc_progress_func_t progress_f,
                                IN void *ctx);

/**
 *  @brief initializes firmware data image handle from the supplied firmware data update image.
 *
 *  @param img firmware data image handle allocated by the function.
 *  @param buffer A pointer to the buffer with the firmware data update image.
 *  @param buffer_len Length of the buffer with the firmware data update image.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_fwdata_init(IN OUT struct igsc_fwdata_image **img,
                           IN const uint8_t *buffer,
                           IN uint32_t buffer_len);

/**
 *  @brief Retrieves the GSC in-field data Firmware Version from the device.
 *
 *  @param handle A handle to the device.
 *  @param version The memory to store obtained firmware version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_fwdata_version(IN struct igsc_device_handle *handle,
                               OUT struct igsc_fwdata_version *version);

/**
 *  @brief Retrieves the GSC in-field data Firmware Version from the device
 *         With ability to return FW Data version in second version format.
 *
 *  @param handle A handle to the device.
 *  @param version The memory to store obtained firmware version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_fwdata_version2(IN struct igsc_device_handle *handle,
                                OUT struct igsc_fwdata_version2 *version);

/**
 *  @brief Retrieves the GSC in-field data Firmware version from the supplied GSC in-field data Firmware update image.
 *
 *  @param img GSC in-field data Firmware image handle
 *  @param version The memory to store the obtained GSC in-field data Firmware version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_fwdata_version(IN struct igsc_fwdata_image *img,
                              OUT struct igsc_fwdata_version *version);

/**
 *  @brief Retrieves the GSC in-field data Firmware version from the supplied GSC in-field data Firmware update image.
 *         With ability to return FW Data version in second version format.
 *
 *  @param img GSC in-field data Firmware image handle
 *  @param version The memory to store the obtained GSC in-field data Firmware version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_fwdata_version2(IN struct igsc_fwdata_image *img,
                               OUT struct igsc_fwdata_version2 *version);

/**
 *  @brief Compares input GSC in-field data firmware update version to the flash one
 *         With ability to compare FW Data version in second version format.
 *
 *  @param image_ver pointer to the GSC in-field data firmware update image version
 *  @param device_ver pointer to the device GSC data firmware version
 *
 *  @return
 *  * IGSC_FWDATA_VERSION_REJECT_VCN                    if image VCN version is bigger than device VCN
 *  * IGSC_FWDATA_VERSION_REJECT_OEM_MANUF_DATA_VERSION if OEM manufacturing data version is not bigger than device OEM version
 *  * IGSC_FWDATA_VERSION_REJECT_DIFFERENT_PROJECT      if major version is different from device major version
 *  * IGSC_FWDATA_VERSION_ACCEPT                        if VCN version is equal to the device's one, and OEM is bigger
 *  * IGSC_FWDATA_VERSION_OLDER_VCN                     if VCN version is smaller than the one on the device
 */
IGSC_EXPORT
uint8_t igsc_fwdata_version_compare(IN struct igsc_fwdata_version *image_ver,
                                    IN struct igsc_fwdata_version *device_ver);

/**
 *  @brief Compares input GSC in-field data firmware update version to the flash one
 *
 *  @param image_ver pointer to the GSC in-field data firmware update image version
 *  @param device_ver pointer to the device GSC data firmware version
 *
 *  @return
 *  * IGSC_FWDATA_VERSION_REJECT_VCN                    if image VCN version is bigger than device VCN
 *  * IGSC_FWDATA_VERSION_REJECT_OEM_MANUF_DATA_VERSION if OEM manufacturing data version is not bigger than device OEM version or equal in ver2 comparison
 *  * IGSC_FWDATA_VERSION_REJECT_DIFFERENT_PROJECT      if major version is different from device major version
 *  * IGSC_FWDATA_VERSION_ACCEPT                        if VCN version is equal to the device's one, and OEM is bigger
 *  * IGSC_FWDATA_VERSION_OLDER_VCN                     if VCN version is smaller than the one on the device
 *  * IGSC_FWDATA_VERSION_REJECT_WRONG_FORMAT           if version format is the wrong one or incompatible
 *  * IGSC_FWDATA_VERSION_REJECT_ARB_SVN                if update image SVN version is smaller than the one on the device
 */
IGSC_EXPORT
uint8_t igsc_fwdata_version_compare2(IN struct igsc_fwdata_version2 *image_ver,
                                     IN struct igsc_fwdata_version2 *device_ver);

/**
 *  @brief Retrieves a count of of different devices supported
 *  by the GSC in-field data firmware update image associated with the handle.
 *
 *  @param img GSC in-field data firmware image handle
 *  @param count the number of devices
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_fwdata_count_devices(IN struct igsc_fwdata_image *img,
                                    OUT uint32_t *count);

/**
 *  @brief Retrieves a list of supported devices
 *  by the GSC in-field data firmware update image associated with the handle.
 *  The caller supplies allocated buffer `devices` of
 *  `count` size. The function returns `count` filled
 *  with actually returned devices.
 *
 *  @param img GSC in-field data firmware image handle
 *  @param devices list of devices supported by the GSC in-field data firmware image
 *  @param count in the number of devices allocated
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_fwdata_supported_devices(IN struct igsc_fwdata_image *img,
                                        OUT struct igsc_fwdata_device_info *devices,
                                        IN OUT uint32_t *count);
/**
 *  @brief check if GSC in-field data firmware image can be applied on the device.
 *
 *  @param img GSC in-field data firmware image handle
 *  @param device physical device info
 *
 *  @return
 *    * IGSC_SUCCESS if device is on the list of supported devices.
 *    * IGSC_ERROR_DEVICE_NOT_FOUND otherwise.
 */
IGSC_EXPORT
int igsc_image_fwdata_match_device(IN struct igsc_fwdata_image *img,
                                   IN struct igsc_device_info *device);
/**
 *  @brief reset the iterator over supported devices
 *
 *  @param img GSC in-field data firmware image handle
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_fwdata_iterator_reset(IN struct igsc_fwdata_image *img);

/**
 *  @brief progress the supported device iterator
 *  and return the GSC in-field data firmware device info
 *
 *  @param img GSC in-field data firmware image handle
 *  @param device GSC in-field data firmware device information.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_fwdata_iterator_next(IN struct igsc_fwdata_image *img,
                                    OUT struct igsc_fwdata_device_info *device);

/**
 *  @brief release the fwdata image handle
 *
 *  @param img fwdata image handle
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_fwdata_release(IN struct igsc_fwdata_image *img);

/**
 *  @brief Compares input fw version to the flash one
 *
 *  @param image_ver pointer to the update image version
 *  @param device_ver pointer to the device version
 *
 *  @return
 *  * IGSC_VERSION_NOT_COMPATIBLE if update image is for a different platform
 *  * IGSC_VERSION_NEWER          if update image version is newer than the one on the device
 *  * IGSC_VERSION_EQUAL          if update image version is equal to the one on the device
 *  * IGSC_VERSION_OLDER          if update image version is older than the one on the device
 *  * IGSC_VERSION_ERROR          if NULL parameters were provided
 */
IGSC_EXPORT
uint8_t igsc_fw_version_compare(IN struct igsc_fw_version *image_ver,
                                IN struct igsc_fw_version *device_ver);

/**
 *  @brief Retrieves the GSC OPROM version from the device.
 *
 *  @param handle A handle to the device.
 *  @param oprom_type An OPROM type requested @see enum igsc_oprom_type
 *  @param version The memory to store obtained OPROM version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_oprom_version(IN struct igsc_device_handle *handle,
                              IN uint32_t oprom_type,
                              OUT struct igsc_oprom_version *version);

/**
 *  @brief Perform the OPROM update from the provided image.
 *
 *  @param handle A handle to the device.
 *  @param oprom_type OPROM part to update @see igsc_oprom_type
 *  @param img A pointer to the parsed oprom image structure.
 *  @param progress_f Pointer to the callback function for OPROM update
 *         progress monitor.
 *  @param ctx Context passed to progress_f function.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_oprom_update(IN struct igsc_device_handle *handle,
                             IN uint32_t oprom_type,
                             IN struct igsc_oprom_image *img,
                             IN igsc_progress_func_t progress_f,
                             IN void *ctx);
/**
 * @addtogroup oprom
 * @{
 */

/**
 *  @brief initializes OPROM image handle from the supplied OPROM update image.
 *
 *  @param img OPROM image handle allocated by the function.
 *  @param buffer A pointer to the buffer with the OPROM update image.
 *  @param buffer_len Length of the buffer with the OPROM update image.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_init(IN OUT struct igsc_oprom_image **img,
                          IN const uint8_t *buffer,
                          IN uint32_t buffer_len);

/**
 *  @brief Retrieves the OPROM version from the supplied OPROM update image.
 *
 *  @param img OPROM image handle
 *  @param type OPROM image type @see enum igsc_oprom_type
 *  @param version The memory to store the obtained OPROM version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_version(IN struct igsc_oprom_image *img,
                             IN enum igsc_oprom_type type,
                             OUT struct igsc_oprom_version *version);

/**
 *  @brief Retrieves the OPROM type from the provided OPROM update image.
 *
 *  @param img OPROM image handle
 *  @param oprom_type The variable to store obtained OPROM image type
 *  @see enum igsc_oprom_type
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_type(IN struct igsc_oprom_image *img,
                          IN uint32_t *oprom_type);

/**
 *  @brief Retrieves a count of of different devices supported
 *  by the OPROM update image associated with the handle.
 *
 *  @param img OPROM image handle
 *  @param count the number of devices
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_count_devices(IN struct igsc_oprom_image *img,
                                   OUT uint32_t *count);

/**
 *  @brief Retrieves a list of supported devices
 *  by the OPROM update image associated with the handle.
 *  The caller supplies allocated buffer `devices` of
 *  `count` size. The function returns `count` filled
 *  with actually returned devices.
 *
 *  @param img OPROM image handle
 *  @param devices list of devices supported by the OPROM image
 *  @param count in the number of devices allocated
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_supported_devices(IN struct igsc_oprom_image *img,
                                       OUT struct igsc_oprom_device_info *devices,
                                       IN OUT uint32_t *count);
/**
 *  @brief check if oprom image can be applied on the device.
 *
 *  @param img OPROM image handle
 *  @param request_type type of oprom device to match
 *  @param device physical device info
 *
 *  @return
 *    * IGSC_SUCCESS if device is on the list of supported devices.
 *    * IGSC_ERROR_DEVICE_NOT_FOUND otherwise.
 */
IGSC_EXPORT
int igsc_image_oprom_match_device(IN struct igsc_oprom_image *img,
                                  IN enum igsc_oprom_type request_type,
                                  IN struct igsc_device_info *device);
/**
 *  @brief reset the iterator over supported devices
 *
 *  @param img OPROM image handle
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_iterator_reset(IN struct igsc_oprom_image *img);

/**
 *  @brief progress the supported device iterator
 *  and return the oprom device info
 *
 *  @param img OPROM image handle
 *  @param device OPROM device information.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_iterator_next(IN struct igsc_oprom_image *img,
                                   OUT struct igsc_oprom_device_info *device);

/**
 *  @brief Retrieves a count of of different devices supported
 *  by the OPROM update image associated with the handle,
 *  based on image type.
 *
 *  @param img OPROM image handle
 *  @param request_type type of oprom device, enum igsc_oprom_type
 *  @param count the number of devices
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_count_devices_typed(IN struct igsc_oprom_image *img,
                                         IN uint32_t request_type,
                                         OUT uint32_t *count);

/**
 *  @brief Retrieves a list of supported devices based on image type
 *  by the OPROM update image associated with the handle.
 *  The caller supplies allocated buffer `devices` of
 *  `count` size. The function returns `count` filled
 *  with actually returned devices.
 *
 *  @param img OPROM image handle
 *  @param request_type type of oprom device, enum igsc_oprom_type
 *  @param devices list of devices supported by the OPROM image
 *  @param count in the number of devices allocated
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_supported_devices_typed(IN struct igsc_oprom_image *img,
                                             IN uint32_t request_type,
                                             OUT struct igsc_oprom_device_info_4ids *devices,
                                             IN OUT uint32_t *count);
/**
 *  @brief reset the iterator over supported devices based on image type
 *
 *  @param img OPROM image handle
 *  @param request_type type of oprom device, enum igsc_oprom_type
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_iterator_reset_typed(IN struct igsc_oprom_image *img,
                                          IN uint32_t request_type);

/**
 *  @brief progress the supported device iterator
 *  and return the oprom device info, based on image type
 *
 *  @param img OPROM image handle
 *  @param request_type type of oprom device, enum igsc_oprom_type
 *  @param device OPROM device information.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_iterator_next_typed(IN struct igsc_oprom_image *img,
                                         IN uint32_t request_type,
                                         OUT struct igsc_oprom_device_info_4ids *device);

/**
 *  @brief returns whether the oprom image has 4ids device extension
 *
 *  @param img OPROM image handle
 *  @param request_type type of oprom device
 *  @param has_4ids_extension whether the oprom image has 4ids device extension
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_has_4ids_extension(IN struct igsc_oprom_image *img,
                                        IN uint32_t request_type,
                                        OUT bool *has_4ids_extension);

/**
 *  @brief returns whether the oprom image has 2ids device extension
 *
 *  @param img OPROM image handle
 *  @param has_2ids_extension whether the oprom image has 2ids device extension
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_has_2ids_extension(IN struct igsc_oprom_image *img,
                                        OUT bool *has_2ids_extension);

/**
 *  @brief returns whether the oprom code config has devId enforcement bit set
 *
 *  @param hw_config device hardware configuration.
 *  @param devid_enforced true when devId is enforced, false otherwise
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_code_devid_enforced(IN struct igsc_hw_config *hw_config,
                                         OUT bool *devid_enforced);

/**
 *  @brief release the OPROM image handle
 *
 *  @param img OPROM image handle
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_oprom_release(IN struct igsc_oprom_image *img);

/**
 *  @brief Compares input oprom version to the flash one
 *
 *  @param image_ver pointer to the update image OPROM version
 *  @param device_ver pointer to the device OPROM version
 *
 *  @return
 *  * IGSC_VERSION_NOT_COMPATIBLE if update image is for a different platform
 *  * IGSC_VERSION_NEWER          if update image version is newer than the one on the device
 *  * IGSC_VERSION_EQUAL          if update image version is equal to the one on the device
 *  * IGSC_VERSION_OLDER          if update image version is older than the one on the device
 *  * IGSC_VERSION_ERROR          if NULL parameters were provided
 */
IGSC_EXPORT
uint8_t igsc_oprom_version_compare(const struct igsc_oprom_version *image_ver,
                                   const struct igsc_oprom_version *device_ver);
/**
 *  @brief Determine the type of the provided image.
 *
 *  @param buffer A pointer to the buffer with the image.
 *  @param buffer_len Length of the buffer with the image.
 *  @param type Type of the image (enum igsc_image_type).
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_image_get_type(IN const uint8_t *buffer,
                        IN const uint32_t buffer_len,
                        OUT uint8_t *type);
/**
 * @}
 */

/**
 * @addtogroup enumeration
 * @{
 */

/**
 *  @brief Create iterator for devices capable of FW update.
 *
 *  @param iter pointer to return the iterator pointer
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_iterator_create(struct igsc_device_iterator **iter);

/**
 *  @brief Obtain next devices capable of FW update.
 *
 *  @param iter pointer to iterator.
 *  @param info pointer for device information.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_iterator_next(struct igsc_device_iterator *iter,
                              struct igsc_device_info *info);

/**
 *  @brief Free iterator for devices capable of FW update.
 *
 *  @param iter pointer to iterator
 *
 *  @return void
 */
IGSC_EXPORT
void igsc_device_iterator_destroy(struct igsc_device_iterator *iter);
/**
 * @}
 */

/**
 * @addtogroup ifr
 * @{
 */

/**
 * ifr tiles masks
 */
enum igsc_ifr_tiles {
    IGSC_IFR_TILE_0 = 0x0001,
    IGSC_IFR_TILE_1 = 0x0002,
};

/**
 * ifr supported test masks
 */
enum igsc_supported_ifr_tests {
    IGSC_IFR_SUPPORTED_TEST_SCAN = 0x00000001,
    IGSC_IFR_SUPPORTED_TEST_ARRAY = 0x00000002,
};

/**
 * ifr repairs masks
 */
enum igsc_ifr_repairs {
    IGSC_IFR_REPAIR_DSS_EN = 0x00000001,
    IGSC_IFR_REPAIR_ARRAY = 0x00000002,
};

/**
 * @name IGSC_IFR_RUN_TEST_STATUSES
 *     The IFR Run Test Command Statuses
 * @addtogroup IGSC_IFR_RUN_TEST_STATUSES
 * @{
 */
enum ifr_test_run_status {
    IFR_TEST_STATUS_SUCCESS = 0,          /**< Test passed successfully */
    IFR_TEST_STATUS_PASSED_WITH_REPAIR,   /**< Test passed, recoverable error found and repaired. No subslice swap needed */
    IFR_TEST_STATUS_PASSED_WITH_RECOVERY, /**< Test passed, recoverable error found and repaired. Subslice swap needed. */
    IFR_TEST_STATUS_SUBSLICE_FAILURE,     /**< Test completed, unrecoverable error found (Subslice failure and no spare Subslice available). */
    IFR_TEST_STATUS_NON_SUBSLICE_FAILURE, /**< Test completed, unrecoverable error found (non-Subslice failure). */
    IFR_TEST_STATUS_ERROR,                /**< Test error */
};

/**
 *  @brief Retrieves the status of GSC IFR device.
 *
 *  @param handle A handle to the device.
 *  @param result Test result code
 *  @param supported_tests Bitmask holding the tests supported on the platform.
 *  @param ifr_applied Bitmask holding the in field repairs was applied during boot.
 *  @param tiles_num Number of tiles on the specific SOC.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_ifr_get_status(IN struct igsc_device_handle *handle,
                        OUT uint8_t *result,
                        OUT uint32_t *supported_tests,
                        OUT uint32_t *ifr_applied,
                        OUT uint8_t *tiles_num);

/**
 *  @brief Runs IFR test on GSC IFR device.
 *
 *  @param handle A handle to the device.
 *  @param test_type Requested test to run
 *  @param result Test result code
 *  @param tiles Tiles on which to run the test
 *  @param run_status Test run status
 *  @param error_code The error code of the test that was run (0 - no error)
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_ifr_run_test(IN struct igsc_device_handle *handle,
                      IN uint8_t test_type,
                      IN uint8_t tiles,
                      OUT uint8_t *result,
                      OUT uint8_t *run_status,
                      OUT uint32_t *error_code);
/**
 * @}
 */

/**
 * @addtogroup gsfp
 * @{
 */

/**
 * gfsp number of memory errors per tile
 */
struct igsc_gfsp_tile_mem_err {
    uint32_t corr_err;   /**<  Correctable memory errors on this boot and tile */
    uint32_t uncorr_err; /**<  Uncorrectable memory errors on this boot and tile */
};

/**
 * gfsp number of memory errors on the card
 */
struct igsc_gfsp_mem_err {
    uint32_t num_of_tiles;                  /**< Number of entries in errors array (number of available entries
                                                 when passed to the function and number of filled entries when returned) */
    struct igsc_gfsp_tile_mem_err errors[]; /**< array of memory errors structs for each tile */
};

/**
 *  @brief Gets number of tiles
 *
 *  @param handle A handle to the device.
 *  @param max_num_of_tiles maximum number of tiles
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_gfsp_count_tiles(IN struct igsc_device_handle *handle,
                          OUT uint32_t *max_num_of_tiles);

/**
 *  @brief Gets GFSP number of memory errors
 *
 *  @param handle A handle to the device.
 *  @param tiles structure to store errors
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_gfsp_memory_errors(IN struct igsc_device_handle *handle,
                            IN OUT struct igsc_gfsp_mem_err *tiles);

/**
 * IFR pending reset values definition
 */
enum igsc_ifr_pending_reset {
    IGSC_IFR_PENDING_RESET_NONE = 0,    /**< 0 - No reset needed */
    IGSC_IFR_PENDING_RESET_SHALLOW = 1, /**< 1 - Need to perform a shallow reset */
    IGSC_IFR_PENDING_RESET_DEEP = 2,    /**< 2 - Need to perform a deep reset */
};

/**
 * IFR array and scan test status bit masks
 */
enum igsc_ifr_array_scan_test_status_mask {
    IGSC_ARRAY_SCAN_STATUS_TEST_EXECUTION_MASK = BIT(0), /**< 0 - Test executed, 1 - Test not executed */
    IGSC_ARRAY_SCAN_STATUS_TEST_RESULT_MASK = BIT(1),    /**< 0 - Test finished successfully, 1 - Error occurred during test execution */
    IGSC_ARRAY_SCAN_STATUS_FOUND_HW_ERROR_MASK = BIT(2), /**< 0 - HW error not found, 1 - HW error found*/
    IGSC_ARRAY_SCAN_STATUS_HW_REPAIR_MASK = BIT(3),      /**< 0 - HW error will be fully repaired or no HW error found, 1 - HW error will not be fully repaired */
};

enum igsc_ifr_array_scan_extended_status {
    IGSC_IFR_EXT_STS_PASSED = 0,                         /**< Test passed successfully, no repairs needed */
    IGSC_IFR_EXT_STS_SHALLOW_RST_PENDING = 1,            /**< Shallow reset already pending from previous test, aborting test */
    IGSC_IFR_EXT_STS_DEEP_RST_PENDING = 2,               /**< Deep reset already pending from previous test, aborting test */
    IGSC_IFR_EXT_STS_NO_REPAIR_NEEDED = 3,               /**< Test passed, recoverable error found, no repair needed */
    IGSC_IFR_EXT_STS_REPAIRED_ARRAY = 4,                 /**< Test passed, recoverable error found and repaired using array repairs */
    IGSC_IFR_EXT_STS_REPAIRED_SUBSLICE = 5,              /**< Test passed, recoverable error found and repaired using Subslice swaps */
    IGSC_IFR_EXT_STS_REPAIRED_ARRAY_SUBSLICE = 6,        /**< Test passed, recoverable error found and repaired using array repairs and Subslice swaps */
    IGSC_IFR_EXT_STS_REPAIRED_ARRAY_FAULTY_SUBSLICE = 7, /**< Test passed, recoverable error found and repaired using array repairs and faulty spare Subslice */
    IGSC_IFR_EXT_STS_REPAIR_NOT_SUPPORTED = 8,           /**< Test completed, unrecoverable error found, part doesn't support in field repair */
    IGSC_IFR_EXT_STS_NO_RESORCES = 9,                    /**< Test completed, unrecoverable error found, not enough repair resources available */
    IGSC_IFR_EXT_STS_NON_SUBSLICE_IN_ARRAY = 10,         /**< Test completed, unrecoverable error found, non-Subslice failure in Array test */
    IGSC_IFR_EXT_STS_NON_SUBSLICE_IN_SCAN = 11,          /**< Test completed, unrecoverable error found, non-Subslice failure in Scan test */
    IGSC_IFR_EXT_STS_TEST_ERROR = 12,                    /**< Test error */
};

/**
 *  @brief Runs IFR Array and Scan tests on GSC IFR device.
 *
 *  @param handle A handle to the device.
 *  @param status Test run status, @see enum igsc_ifr_array_scan_test_status_mask
 *  @param extended_status Test run extended status, @see enum igsc_ifr_array_scan_extended_status
 *  @param pending_reset Whether a reset is pending, @see enum igsc_ifr_pending_reset
 *  @param error_code The error code of the test (0 - no error)
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_ifr_run_array_scan_test(IN struct igsc_device_handle *handle,
                                 OUT uint32_t *status,
                                 OUT uint32_t *extended_status,
                                 OUT uint32_t *pending_reset,
                                 OUT uint32_t *error_code);

/**
 *  @brief Runs IFR memory Post Package Repair (PPR) test on GSC IFR device.
 *
 *  @param handle A handle to the device.
 *  @param status Test run status,0 - Test is available and will be run after a reset
 *                                Other values - error
 *  @param pending_reset Whether a reset is pending, @see enum igsc_ifr_pending_reset
 *  @param error_code The error code of the test (0 - no error)
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_ifr_run_mem_ppr_test(IN struct igsc_device_handle *handle,
                              OUT uint32_t *status,
                              OUT uint32_t *pending_reset,
                              OUT uint32_t *error_code);

/**
 * IFR supported tests masks
 */
enum igsc_ifr_supported_tests_masks {
    IGSC_IFR_SUPPORTED_TESTS_ARRAY_AND_SCAN = BIT(0), /**< 1 - Array and Scan test */
    IGSC_IFR_SUPPORTED_TESTS_MEMORY_PPR = BIT(1),     /**< 2 - Memory PPR */
};

/**
 * IFR hw capabilities masks
 */
enum igsc_ifr_hw_capabilities_masks {
    IGSC_IRF_HW_CAPABILITY_IN_FIELD_REPAIR = BIT(0),     /**< 1: both in field tests and in field repairs are supported. */
                                                         /**< 0: only in field tests are supported */
    IGSC_IRF_HW_CAPABILITY_FULL_EU_MODE_SWITCH = BIT(1), /**< 1: Full EU mode switch is supported */
};

/**
 * IFR previous errors masks
 */
enum igsc_ifr_previous_errors_masks {
    IGSC_IFR_PREV_ERROR_DSS_ERR_ARR_STS_PKT = BIT(0),          /**< DSS Engine error in an array test status packet */
    IGSC_IFR_PREV_ERROR_NON_DSS_ERR_ARR_STS_PKT = BIT(1),      /**< Non DSS Engine error in an array test status packet */
    IGSC_IFR_PREV_ERROR_DSS_REPAIRABLE_PKT = BIT(2),           /**< DSS Repairable repair packet in an array test */
    IGSC_IFR_PREV_ERROR_DSS_UNREPAIRABLE_PKT = BIT(3),         /**< DSS Unrepairable repair packet in an array test */
    IGSC_IFR_PREV_ERROR_NON_DSS_REPAIRABLE_PKT = BIT(4),       /**< Non DSS Repairable repair packet in an array test */
    IGSC_IFR_PREV_ERROR_NON_DSS_UNREPAIRABLE_PKT = BIT(5),     /**< Non DSS Unrepairable repair packet in an array test */
    IGSC_IFR_PREV_ERROR_DSS_ERR_SCAN_STS_PKT = BIT(6),         /**< DSS failure in a scan test packet */
    IGSC_IFR_PREV_ERROR_NON_DSS_ERR_SCAN_STS_PKT = BIT(7),     /**< Non DSS failure in a scan test packet */
    IGSC_IFR_PREV_ERROR_NOT_ENOUGH_SPARE_DSS = BIT(8),         /**< Not enough spare DSS available */
    IGSC_IFR_PREV_ERROR_MIS_DSS_STS_PKT_ON_ARR = BIT(9),       /**< Missing DSS status packet on array test */
    IGSC_IFR_PREV_ERROR_MIS_NON_DSS_STS_PKT_ON_ARR = BIT(10),  /**< Missing non DSS status packet on array test */
    IGSC_IFR_PREV_ERROR_MIS_DSS_STS_PKT_ON_SCAN = BIT(11),     /**< Missing DSS status packet on scan test */
    IGSC_IFR_PREV_ERROR_MIS_NON_DSS_STS_PKT_ON_SCAN = BIT(12), /**< Missing non DSS status packet on scan test */
    IGSC_IFR_PREV_ERROR_DSS_ENG_DONE_CLR_IN_ARR = BIT(13),     /**< DSS Engine Done clear bit in array test status packet */
    IGSC_IFR_PREV_ERROR_NON_DSS_ENG_DONE_CLR_IN_ARR = BIT(14), /**< Non DSS Engine Done clear bit in array test status packet */
    IGSC_IFR_PREV_ERROR_UNEXPECTED = BIT(31),                  /**< Unexpected test failure */
};

/**
 * IFR repairs masks
 */
enum igsc_ifr_repairs_mask {
    IGSC_IFR_REPAIRS_MASK_DSS_EN_REPAIR = BIT(0), /**< DSS enable repair applied */
    IGSC_IFR_REPAIRS_MASK_ARRAY_REPAIR = BIT(1),  /**< Array repair applied */
    IGSC_IFR_REPAIRS_MASK_FAILURE = BIT(2),       /**< Repair failure occurred */
};

/**
 *  @brief Retrieves the status of GSC IFR device.
 *
 *  @param handle A handle to the device.
 *  @param supported_tests Bitmap holding the tests supported on the platform,
                           @see enum igsc_ifr_supported_tests_masks
 *  @param hw_capabilities Bitmap holdinf hw capabilities on the platform,
                           @see enum igsc_ifr_hw_capabilities_masks
 *  @param ifr_applied Bitmap holding the in-field-repairs applied during boot,
                           @see enum igsc_ifr_repairs_mask
 *  @param prev_errors Bitmap holding which errors were seen on this SOC in previous tests,
                           @see enum igsc_ifr_previous_errors_masks
 *  @param pending_reset Whether a reset is pending after running the test,
                           @see enum igsc_ifr_pending_reset
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_ifr_get_status_ext(IN struct igsc_device_handle *handle,
                            OUT uint32_t *supported_tests,
                            OUT uint32_t *hw_capabilities,
                            OUT uint32_t *ifr_applied,
                            OUT uint32_t *prev_errors,
                            OUT uint32_t *pending_reset);

/**
 *  @brief Counts the IFR supported tiles.
 *
 *  @param handle A handle to the device.
 *  @param supported_tiles Number of supported tiles
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_ifr_count_tiles(IN struct igsc_device_handle *handle,
                         OUT uint16_t *supported_tiles);

/**
 *  @brief Retrieves the IFR repair info.
 *
 *  @param handle A handle to the device.
 *  @param tile_idx The index of the tile the info is requested from
 *  @param used_array_repair_entries Number of array repair entries used by FW
 *  @param available_array_repair_entries Number of available array repair entries
 *  @param failed_dss  Number of failed DSS
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_ifr_get_tile_repair_info(IN struct igsc_device_handle *handle,
                                  IN uint16_t tile_idx,
                                  OUT uint16_t *used_array_repair_entries,
                                  OUT uint16_t *available_array_repair_entries,
                                  OUT uint16_t *failed_dss);
/**
 *  @brief Set ECC Configuration
 *
 *  @param handle A handle to the device.
 *  @param req_ecc_state Requested ECC State
 *  @param cur_ecc_state Current ECC State after command execution
 *  @param pen_ecc_state Pending ECC State after command execution
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_ecc_config_set(IN struct igsc_device_handle *handle,
                        IN uint8_t req_ecc_state,
                        OUT uint8_t *cur_ecc_state,
                        OUT uint8_t *pen_ecc_state);

/**
 *  @brief Get ECC Configuration
 *
 *  @param handle A handle to the device.
 *  @param cur_ecc_state Current ECC State
 *  @param pen_ecc_state Pending ECC State
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_ecc_config_get(IN struct igsc_device_handle *handle,
                        OUT uint8_t *cur_ecc_state,
                        OUT uint8_t *pen_ecc_state);

/**
 *  @brief Retrieves the OEM Version from the device.
 *
 *  @param handle A handle to the device.
 *  @param version The memory to store obtained OEM version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_oem_version(IN struct igsc_device_handle *handle,
                            OUT struct igsc_oem_version *version);

/**
 *  @brief Retrieves the IFR Binary Version from the device.
 *
 *  @param handle A handle to the device.
 *  @param version The memory to store obtained IFR binary version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_ifr_bin_version(IN struct igsc_device_handle *handle,
                                OUT struct igsc_ifr_bin_version *version);

/**
 *  @brief Retrieves the PSC Version from the device.
 *
 *  @param handle A handle to the device.
 *  @param version The memory to store obtained PSC version.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_psc_version(IN struct igsc_device_handle *handle,
                            OUT struct igsc_psc_version *version);

enum igsc_gfsp_health_indicators {
    IGSC_HEALTH_INDICATOR_HEALTHY = 0,
    IGSC_HEALTH_INDICATOR_DEGRADED = 1,
    IGSC_HEALTH_INDICATOR_CRITICAL = 2,
    IGSC_HEALTH_INDICATOR_REPLACE = 3
};

/**
 *  @brief Gets memory health indicator
 *
 *  @param handle A handle to the device.
 *  @param health_indicator contains pointer to @enum igsc_gfsp_health_indicators
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_gfsp_get_health_indicator(IN struct igsc_device_handle *handle,
                                   OUT uint8_t *health_indicator);

/**
 *  @brief Send generic GFSP command and receive response
 *
 *  @param handle A handle to the device.
 *  @param gfsp_cmd command id
 *  @param in_buffer pointer to the input buffer
 *  @param in_buffer_size input buffer size
 *  @param out_buffer pointer to the output buffer
 *  @param out_buffer_size output buffer size
 *  @param actual_out_buffer_size pointer to the actual data size returned in the output buffer
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_gfsp_heci_cmd(struct igsc_device_handle *handle, uint32_t gfsp_cmd,
                       uint8_t *in_buffer, size_t in_buffer_size,
                       uint8_t *out_buffer, size_t out_buffer_size,
                       size_t *actual_out_buffer_size);

/**
 * Late Binding flags
 */
enum csc_late_binding_flags {
    CSC_LATE_BINDING_FLAGS_IS_PERSISTENT_MASK = 0x1,
};

/**
 * Late Binding payload type
 */
enum csc_late_binding_type {
    CSC_LATE_BINDING_TYPE_INVALID = 0,
    CSC_LATE_BINDING_TYPE_FAN_TABLE,
    CSC_LATE_BINDING_TYPE_VR_CONFIG
};

/**
 * Late Binding payload status
 */
enum csc_late_binding_status {
    CSC_LATE_BINDING_STATUS_SUCCESS = 0,
    CSC_LATE_BINDING_STATUS_4ID_MISMATCH = 1,
    CSC_LATE_BINDING_STATUS_ARB_FAILURE = 2,
    CSC_LATE_BINDING_STATUS_GENERAL_ERROR = 3,
    CSC_LATE_BINDING_STATUS_INVALID_PARAMS = 4,
    CSC_LATE_BINDING_STATUS_INVALID_SIGNATURE = 5,
    CSC_LATE_BINDING_STATUS_INVALID_PAYLOAD = 6,
    CSC_LATE_BINDING_STATUS_TIMEOUT = 7,
};

/**
 *  @brief Sends Late Binding HECI command
 *
 *  @param handle       A handle to the device.
 *  @param type         Late Binding payload type @enum csc_late_binding_type
 *  @param flags        Late Binding flags to be sent to the firmware enum csc_late_binding_flags
 *  @param payload      Late Binding data to be sent to the firmware
 *  @param payload_size Size of the payload data
 *  @param status       Late Binding payload status @enum csc_late_binding_status
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_update_late_binding_config(IN struct igsc_device_handle *handle,
                                           IN uint32_t type,  /* enum csc_late_binding_type */
                                           IN uint32_t flags, /* enum csc_late_binding_flags */
                                           IN uint8_t *payload, IN size_t payload_size,
                                           OUT uint32_t *status); /* enum csc_late_binding_status */
/**
 *  @brief Sends ARB SVN Commit HECI command
 *
 *  @param handle   A handle to the device.
 *  @param fw_error An error returned by firmware in case of failure, can be NULL if not needed
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_commit_arb_svn(IN struct igsc_device_handle *handle, uint8_t *fw_error);

/**
 *  @brief Retrieves Minimal allowed ARB SVN
 *
 *  @param handle          A handle to the device.
 *  @param min_allowed_svn buffer for minimal allowed ARB SVN value
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_device_get_min_allowed_arb_svn(IN struct igsc_device_handle *handle,
                                        OUT uint8_t *min_allowed_svn);

/**
 * @}
 */

/**
 *  memory PPR status structures
 */

/**
 * PPR test status bit masks
 */
enum igsc_ppr_test_status_mask {
    IGSC_PPR_STATUS_TEST_EXECUTED_MASK = 0x1,
    IGSC_PPR_STATUS_TEST_SUCCESS_MASK = 0x2,
    IGSC_PPR_STATUS_FOUND_HW_ERROR_MASK = 0x4,
    IGSC_PPR_STATUS_HW_ERROR_REPAIRED_MASK = 0x8,
};

/**
 * Device PPR status structure
 */
struct igsc_device_mbist_ppr_status {
    uint32_t mbist_test_status;           /**< 0 – Pass, Any set bit represents that MBIST on the matching channel has failed */
    uint32_t num_of_ppr_fuses_used_by_fw; /**< Number of PPR fuses used by the firmware */
    uint32_t num_of_remaining_ppr_fuses;  /**< Number of remaining PPR fuses */
};

/**
 * PPR status structure
 */
struct igsc_ppr_status {
    uint8_t boot_time_memory_correction_pending; /**< 0 - No pending boot time memory correction, */
                                                 /**< 1 - Pending boot time memory correction     */
    uint8_t ppr_mode;                            /**< 0 – PPR enabled, 1 – PPR disabled, 2 – PPR test mode, */
                                                 /**< 3 – PPR auto run on next boot */
    uint8_t test_run_status;                     /**< test status @see enum igsc_ppr_test_status_mask */
    uint8_t reserved;
    uint32_t ras_ppr_applied;                                      /**< 0 - ppr not applied, 1 - ppr applied, 2 - ppr exhausted */
    uint32_t mbist_completed;                                      /**< 0 - Not Applied, Any set bit represents mbist completed */
    uint32_t num_devices;                                          /**< real number of devices in the array (on Xe_HP SDV, PVC <= 8) */
    struct igsc_device_mbist_ppr_status device_mbist_ppr_status[]; /**< Array of PPR statuses per device */
};

/**
 *  @brief Retrieves GFSP number of memory PPR devices
 *
 *  @param handle A handle to the device.
 *  @param device_count pointer to number of memory PPR devices, the number is returned by the FW
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_memory_ppr_devices(IN struct igsc_device_handle *handle,
                            OUT uint32_t *device_count);

/**
 *  @brief Retrieves GFSP memory PPR status structure data
 *
 *  @param handle A handle to the device.
 *  @param ppr_status pointer to PPR status structure, which contains num_devices field
 *         representing the number of allocated items in the device_mbist_ppr_status[] array.
 *
 *  @return IGSC_SUCCESS if successful, otherwise error code.
 */
IGSC_EXPORT
int igsc_memory_ppr_status(IN struct igsc_device_handle *handle,
                           OUT struct igsc_ppr_status *ppr_status);

/**
 * @}
 */
#ifdef __cplusplus
}
#endif
#endif /* __IGSC_LIB_H__ */