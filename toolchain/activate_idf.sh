#!/usr/bin/env bash

WAS_SOURCED=0
if [ "${BASH_SOURCE[0]}" != "$0" ]; then
    WAS_SOURCED=1
fi

find_idf_python() {
    local candidate

    for candidate in "${TOOLS_DIR}"/python_env/*/bin/python; do
        if [ -x "${candidate}" ]; then
            printf '%s\n' "${candidate}"
            return 0
        fi
    done

    return 1
}

check_tool_dir() {
    local tool_name="${1}"
    if [ ! -d "${TOOLS_DIR}/tools/${tool_name}" ]; then
        echo "ERROR: Missing required ESP-IDF tool at ${TOOLS_DIR}/tools/${tool_name}" >&2
        return 1
    fi
}

sync_idf_targets() {
    local env_file="${TOOLS_DIR}/idf-env.json"

    if [ -f "${env_file}" ] &&
        grep -Fq "\"path\": \"${IDF_DIR}\"" "${env_file}" &&
        grep -Fq '"esp32s3"' "${env_file}"; then
        return 0
    fi

    echo "Refreshing ESP-IDF tool metadata for ${IDF_DIR}"
    "${IDF_PYTHON_BIN}" "${IDF_DIR}/tools/idf_tools.py" install --targets=esp32s3
}

build_dir_is_stale() {
    local cache_file="${BUILD_DIR}/CMakeCache.txt"
    local project_description="${BUILD_DIR}/project_description.json"

    if [ ! -d "${BUILD_DIR}" ]; then
        return 1
    fi

    if [ -f "${cache_file}" ] &&
        ! grep -Fq "${ESP32_PORT_DIR}" "${cache_file}"; then
        return 0
    fi

    if [ -f "${project_description}" ] &&
        ! grep -Fq "\"project_path\":       \"${ESP32_PORT_DIR}\"" "${project_description}"; then
        return 0
    fi

    return 1
}

remove_stale_build_backups() {
    local stale_dir

    for stale_dir in "${BUILD_DIR}".stale-*; do
        if [ ! -e "${stale_dir}" ]; then
            continue
        fi

        echo "Removing old stale build backup ${stale_dir}"
        rm -rf "${stale_dir}"
    done
}

remove_stale_build_dir() {
    echo "Removing stale build directory ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
}

main() {
    SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
    FIRMWARE_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
    IDF_DIR="${SCRIPT_DIR}/esp-idf"
    TOOLS_DIR="${SCRIPT_DIR}/espressif"
    ESP32_PORT_DIR="${FIRMWARE_DIR}/ports/esp32"
    BUILD_DIR="${ESP32_PORT_DIR}/build-ESP32_GENERIC_S3-SPIRAM_OCT"
    USER_C_MODULES_PATH="../../../c_modules/micropython.cmake"
    BUILD_MODE="${1:-build}"
    MAKE_JOBS="${MAKE_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

    if [ ! -f "${IDF_DIR}/export.sh" ]; then
        echo "ERROR: ESP-IDF export.sh not found at ${IDF_DIR}/export.sh" >&2
        return 1
    fi

    if [ ! -d "${TOOLS_DIR}" ]; then
        echo "ERROR: ESP-IDF tools directory not found at ${TOOLS_DIR}" >&2
        return 1
    fi

    IDF_PYTHON_BIN="$(find_idf_python)" || {
        echo "ERROR: ESP-IDF Python environment not found under ${TOOLS_DIR}/python_env" >&2
        return 1
    }

    check_tool_dir xtensa-esp-elf || return 1
    check_tool_dir xtensa-esp-elf-gdb || return 1
    check_tool_dir riscv32-esp-elf || return 1
    check_tool_dir esp32ulp-elf || return 1
    check_tool_dir openocd-esp32 || return 1

    export IDF_TOOLS_PATH="${TOOLS_DIR}"
    export IDF_COMPONENT_CACHE_PATH="${TOOLS_DIR}/component_cache"
    export CALSCI_FIRMWARE_DIR="${FIRMWARE_DIR}"
    export CALSCI_BUILD_DIR="${BUILD_DIR}"
    export CALSCI_USER_C_MODULES="${USER_C_MODULES_PATH}"
    export IDF_TOOLS_INSTALL_CMD="${IDF_PYTHON_BIN} ${IDF_DIR}/tools/idf_tools.py install --targets=esp32s3"
    export IDF_TOOLS_EXPORT_CMD="${IDF_PYTHON_BIN} ${IDF_DIR}/tools/idf_tools.py export"

    mkdir -p "${IDF_COMPONENT_CACHE_PATH}"

    if ! sync_idf_targets; then
        echo "ERROR: Failed to sync ESP-IDF tool metadata" >&2
        return 1
    fi

    # shellcheck disable=SC1091
    if ! . "${IDF_DIR}/export.sh"; then
        echo "ERROR: Activation script failed" >&2
        return 1
    fi

    echo "ESP-IDF toolchain activated from ${IDF_DIR}"
    echo "ESP-IDF tools path: ${IDF_TOOLS_PATH}"
    echo "CalSci firmware root: ${CALSCI_FIRMWARE_DIR}"
    echo "Target build dir: ${CALSCI_BUILD_DIR}"

    if [ "${BUILD_MODE}" = "activate-only" ]; then
        return 0
    fi

    remove_stale_build_backups || return 1

    if build_dir_is_stale; then
        remove_stale_build_dir || return 1
    fi

    if ! (
        cd "${ESP32_PORT_DIR}"
        make \
            BOARD=ESP32_GENERIC_S3 \
            BOARD_VARIANT=SPIRAM_OCT \
            BUILD=build-ESP32_GENERIC_S3-SPIRAM_OCT \
            USER_C_MODULES="${USER_C_MODULES_PATH}" \
            -j"${MAKE_JOBS}"
    ); then
        echo "ERROR: Firmware build failed" >&2
        return 1
    fi

    echo "Build completed in ${BUILD_DIR}"
    echo "Firmware bin: ${BUILD_DIR}/firmware.bin"
    echo "Bootloader bin: ${BUILD_DIR}/bootloader/bootloader.bin"
    echo "Partition table bin: ${BUILD_DIR}/partition_table/partition-table.bin"
}

main "$@"
status=$?

if [ "${WAS_SOURCED}" -eq 1 ]; then
    return "${status}"
fi

exit "${status}"
