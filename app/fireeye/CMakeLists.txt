# ##############################################################################
# FireEye - Electric Bicycle Charging Safety Monitor
# CMakeLists.txt
# ##############################################################################

if(CONFIG_FIREYEYE)

  # Application name
  set(APP_NAME fireeye)

  # Source files
  set(CSRCS
    src/fireeye_main.c
    src/fireeye_filter.c
    src/fireeye_threshold.c
    src/fireeye_fsm.c
  )

  # Register application
  nuttx_add_application(
    NAME ${APP_NAME}
    STACKSIZE ${CONFIG_FIREYEYE_STACKSIZE}
    PRIORITY ${CONFIG_FIREYEYE_PRIORITY}
    SRCS ${CSRCS}
  )

endif()
