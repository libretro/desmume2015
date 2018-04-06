LOCAL_PATH := $(call my-dir)

CORE_DIR := $(LOCAL_PATH)/../..

JIT             :=
DESMUME_JIT     := 0
DESMUME_JIT_ARM := 0

ifeq ($(TARGET_ARCH),arm)
  DESMUME_JIT_ARM := 1
  JIT             := -DHAVE_JIT
endif

ifneq (,$(filter $(TARGET_ARCH),x86 x86_64))
  DESMUME_JIT := 1
  JIT         := -DHAVE_JIT
endif

include $(CORE_DIR)/../Makefile.common

COREFLAGS :=  $(JIT) $(INCDIR) -D__LIBRETRO__ -Wno-write-strings -DANDROID -DFRONTEND_SUPPORTS_RGB565

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
  COREFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE       := retro
LOCAL_SRC_FILES    := $(SOURCES_CXX) $(SOURCES_C)
LOCAL_CXXFLAGS     := $(COREFLAGS)
LOCAL_CFLAGS       := $(COREFLAGS)
LOCAL_LDFLAGS      := -Wl,-version-script=$(CORE_DIR)/libretro/link.T
LOCAL_LDLIBS       := -lz
LOCAL_CPP_FEATURES := exceptions
include $(BUILD_SHARED_LIBRARY)
