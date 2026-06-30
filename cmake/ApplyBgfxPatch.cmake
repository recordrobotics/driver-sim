set(PATCH_FILE "${SOURCE_DIR}/.shaderc-rt-format.patch")
set(STAMP_FILE "${SOURCE_DIR}/.shaderc-rt-format.applied")

# Download only once.
if(NOT EXISTS "${PATCH_FILE}")
    message(STATUS "Downloading bgfx patch...")

    file(DOWNLOAD
        "${PATCH_URL}"
        "${PATCH_FILE}"
        STATUS status
        SHOW_PROGRESS
    )

    list(GET status 0 code)
    if(NOT code EQUAL 0)
        list(GET status 1 msg)
        message(FATAL_ERROR "Failed to download patch: ${msg}")
    endif()
endif()

# Apply only once.
if(NOT EXISTS "${STAMP_FILE}")
    message(STATUS "Applying bgfx patch...")

    execute_process(
        COMMAND git apply "${PATCH_FILE}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed to apply bgfx patch.")
    endif()

    file(TOUCH "${STAMP_FILE}")
endif()