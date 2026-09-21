set(SECRET_ENC  "${CMAKE_SOURCE_DIR}/discord_social_sdk.zip.enc")
set(SECRET_DEST "${CMAKE_SOURCE_DIR}/_external/discordsdk-src/_external")
set(SECRET_TMP  "${CMAKE_BINARY_DIR}/discord_social_sdk.zip")

if(NOT DEFINED ENV{DRIVER_SIM_DISCORD_SDK_KEY})
  message(FATAL_ERROR "DRIVER_SIM_DISCORD_SDK_KEY environment variable is not set")
endif()

find_program(OPENSSL_EXE openssl REQUIRED)

execute_process(
  COMMAND ${OPENSSL_EXE} enc -d -aes-256-cbc -pbkdf2 -iter 600000
          -in ${SECRET_ENC} -out ${SECRET_TMP} -pass env:DRIVER_SIM_DISCORD_SDK_KEY
  RESULT_VARIABLE dec_rc
  ERROR_VARIABLE  dec_err
)
if(NOT dec_rc EQUAL 0)
  message(FATAL_ERROR "Decryption failed (wrong key?): ${dec_err}")
endif()

file(ARCHIVE_EXTRACT INPUT ${SECRET_TMP} DESTINATION ${SECRET_DEST})
file(REMOVE ${SECRET_TMP})

# Re-run configure automatically if the encrypted file changes
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${SECRET_ENC})