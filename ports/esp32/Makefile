# Makefile for MicroPython on ESP32.
#
# This is a simple, convenience wrapper around idf.py (which uses cmake).

include ../../py/verbose.mk

# Select the board to build for:
ifdef BOARD_DIR
# Custom board path - remove trailing slash and get the final component of
# the path as the board name.
BOARD ?= $(notdir $(BOARD_DIR:/=))
else
# If not given on the command line, then default to ESP32_GENERIC.
BOARD ?= ESP32_GENERIC
BOARD_DIR ?= boards/$(BOARD)
endif

ifeq ($(wildcard $(BOARD_DIR)/.),)
ifeq ($(findstring boards/GENERIC,$(BOARD_DIR)),boards/GENERIC)
$(warning The GENERIC* boards have been renamed to ESP32_GENERIC*)
endif
$(error Invalid BOARD specified: $(BOARD_DIR))
endif

# If the build directory is not given, make it reflect the board name (and
# optionally the board variant).
ifneq ($(BOARD_VARIANT),)
BUILD ?= build-$(BOARD)-$(BOARD_VARIANT)
else
BUILD ?= build-$(BOARD)
endif

# Device serial settings.
ifneq ($(PORT),)
PORT_ARG := -p $(PORT)
endif
ifneq ($(BAUD),)
BAUD_ARG := -b $(BAUD)
endif
DEVICE += $(PORT_ARG) $(BAUD_ARG)

PYTHON ?= python3
export IDF_COMPONENT_CACHE_PATH ?= $(abspath ../../toolchain/espressif/component_cache)

.PHONY: all clean deploy erase submodules FORCE

CMAKE_ARGS =
BUILD_INPUT_STATE := $(BUILD)/.calsci-build-inputs.json
BUILD_INPUT_SIG_CMD = $(PYTHON) check_build_signature.py --state-file "$(BUILD_INPUT_STATE)" --build-dir "$(BUILD)" --board "$(BOARD)" --board-variant "$(BOARD_VARIANT)" --user-c-modules "$(USER_C_MODULES)" --frozen-manifest "$(FROZEN_MANIFEST)" --preview-version-2 "$(MICROPY_PREVIEW_VERSION_2)"

ifdef USER_C_MODULES
	CMAKE_ARGS += -DUSER_C_MODULES=${USER_C_MODULES}
endif

IDFPY_FLAGS += -D MICROPY_BOARD=$(BOARD) -D MICROPY_BOARD_DIR="$(abspath $(BOARD_DIR))" $(CMAKE_ARGS)

ifdef FROZEN_MANIFEST
       IDFPY_FLAGS += -D MICROPY_FROZEN_MANIFEST=$(FROZEN_MANIFEST)
endif

ifdef BOARD_VARIANT
	IDFPY_FLAGS += -D MICROPY_BOARD_VARIANT=$(BOARD_VARIANT)
endif

ifdef MICROPY_PREVIEW_VERSION_2
	IDFPY_FLAGS += -D MICROPY_PREVIEW_VERSION_2=1
endif

ifeq ($(BUILD_VERBOSE),1)
	IDFPY_FLAGS += --verbose
endif

HELP_BUILD_ERROR ?= "See \033[1;31mhttps://github.com/micropython/micropython/wiki/Build-Troubleshooting\033[0m"

define RUN_IDF_PY
	$(Q)idf.py $(IDFPY_FLAGS) -B $(BUILD) $(1)
endef

define RUN_FULLCLEAN
	if idf.py $(IDFPY_FLAGS) -B $(BUILD) fullclean; then \
		:; \
	else \
		echo "idf.py fullclean failed for $(BUILD); removing the build directory directly."; \
		rm -rf "$(BUILD)"; \
	fi
endef

all:
	@if $(BUILD_INPUT_SIG_CMD) check; then \
		:; \
	else \
		status=$$?; \
		if [ $$status -eq 2 ]; then \
			echo "Build inputs changed for $(BUILD); running fullclean to remove stale generated files."; \
			$(call RUN_FULLCLEAN); \
		else \
			exit $$status; \
		fi; \
	fi
	$(Q)idf.py $(IDFPY_FLAGS) -B $(BUILD) build || (echo -e $(HELP_BUILD_ERROR); false)
	$(Q)$(PYTHON) makeimg.py \
		$(BUILD)/sdkconfig \
		$(BUILD)/bootloader/bootloader.bin \
		$(BUILD)/partition_table/partition-table.bin \
		$(BUILD)/micropython.bin \
		$(BUILD)/firmware.bin \
		$(BUILD)/micropython.uf2
	$(Q)$(BUILD_INPUT_SIG_CMD) write

$(BUILD)/bootloader/bootloader.bin $(BUILD)/partition_table/partition-table.bin $(BUILD)/micropython.bin: FORCE

clean:
	$(call RUN_FULLCLEAN)

deploy:
	$(call RUN_IDF_PY,$(DEVICE) flash)

erase:
	$(call RUN_IDF_PY,$(DEVICE) erase-flash)

monitor:
	$(call RUN_IDF_PY,$(DEVICE) monitor)

size:
	$(call RUN_IDF_PY,size $(SIZE_FLAGS))

size-components:
	$(call RUN_IDF_PY,size-components $(SIZE_FLAGS))

size-files:
	$(call RUN_IDF_PY,size-files)

# Run idf.py with the UPDATE_SUBMODULES flag to update
# necessary submodules for this board.
#
# This is done in a dedicated build directory as some CMake cache values are not
# set correctly if not all submodules are loaded yet.
submodules:
	$(Q)idf.py $(IDFPY_FLAGS) -B $(BUILD)/submodules -D UPDATE_SUBMODULES=1 reconfigure
