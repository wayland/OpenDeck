DEFINES += $(CORE_MCU_DEFINES) $(PROJECT_MCU_DEFINES)

DEFINES += \
TEST \
USE_LOGGER \
GLOG_CUSTOM_PREFIX_SUPPORT

TEST_DEFINES := 1

DEFINES += CORE_MCU_STUB
DEFINES := $(filter-out PROJECT_MCU_DATABASE_INIT_DATA%,$(DEFINES))