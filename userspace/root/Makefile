
ROOT_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
ROOT_DIR := $(ROOT_DIR:/=)

rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))

DISK_ROOT_FILES := $(call rwildcard,$(ROOT_DIR),*)

