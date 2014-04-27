HAVE_GRIFFIN    := 0

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

SRC_DIR = ../..

ifeq ($(TARGET_ARCH),arm)
LOCAL_CXXFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
DESMUME_JIT_ARM := 1
JIT += -D__RETRO_ARM__
JIT += -DHAVE_JIT
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CXXFLAGS +=  -DANDROID_X86
DESMUME_JIT := 1
JIT += -DHAVE_JIT
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CXXFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

SOURCES += \
	$(SRC_DIR)/armcpu.cpp \
	$(SRC_DIR)/arm_instructions.cpp \
	$(SRC_DIR)/bios.cpp \
	$(SRC_DIR)/cp15.cpp \
	$(SRC_DIR)/common.cpp \
	$(SRC_DIR)/debug.cpp \
	$(SRC_DIR)/Disassembler.cpp \
	$(SRC_DIR)/emufile.cpp \
	$(SRC_DIR)/encrypt.cpp \
	$(SRC_DIR)/FIFO.cpp \
	$(SRC_DIR)/firmware.cpp \
	$(SRC_DIR)/GPU.cpp \
	$(SRC_DIR)/mc.cpp \
	$(SRC_DIR)/path.cpp \
	$(SRC_DIR)/readwrite.cpp \
	$(SRC_DIR)/wifi.cpp \
	$(SRC_DIR)/MMU.cpp \
	$(SRC_DIR)/NDSSystem.cpp \
	$(SRC_DIR)/ROMReader.cpp \
	$(SRC_DIR)/render3D.cpp \
	$(SRC_DIR)/rtc.cpp \
	$(SRC_DIR)/saves.cpp \
	$(SRC_DIR)/slot1.cpp \
	$(SRC_DIR)/slot2.cpp \
	$(SRC_DIR)/SPU.cpp \
	$(SRC_DIR)/matrix.cpp \
	$(SRC_DIR)/gfx3d.cpp \
	$(SRC_DIR)/thumb_instructions.cpp \
	$(SRC_DIR)/movie.cpp \
	$(SRC_DIR)/utils/advanscene.cpp \
	$(SRC_DIR)/utils/datetime.cpp \
   $(SRC_DIR)/utils/guid.cpp \
	$(SRC_DIR)/utils/emufat.cpp \
	$(SRC_DIR)/utils/fsnitro.cpp \
	$(SRC_DIR)/utils/md5.cpp \
	$(SRC_DIR)/utils/xstring.cpp \
	$(SRC_DIR)/utils/decrypt/crc.cpp \
	$(SRC_DIR)/utils/decrypt/decrypt.cpp \
	$(SRC_DIR)/utils/decrypt/header.cpp \
	$(SRC_DIR)/utils/task.cpp \
   $(SRC_DIR)/utils/vfat.cpp \
	$(SRC_DIR)/utils/dlditool.cpp \
	$(SRC_DIR)/utils/libfat/cache.cpp \
	$(SRC_DIR)/utils/libfat/directory.cpp \
	$(SRC_DIR)/utils/libfat/disc.cpp \
	$(SRC_DIR)/utils/libfat/fatdir.cpp \
	$(SRC_DIR)/utils/libfat/fatfile.cpp \
	$(SRC_DIR)/utils/libfat/filetime.cpp \
	$(SRC_DIR)/utils/libfat/file_allocation_table.cpp \
	$(SRC_DIR)/utils/libfat/libfat.cpp \
	$(SRC_DIR)/utils/libfat/libfat_public_api.cpp \
	$(SRC_DIR)/utils/libfat/lock.cpp \
	$(SRC_DIR)/utils/libfat/partition.cpp \
	$(SRC_DIR)/utils/tinyxml/tinystr.cpp \
	$(SRC_DIR)/utils/tinyxml/tinyxml.cpp \
	$(SRC_DIR)/utils/tinyxml/tinyxmlerror.cpp \
	$(SRC_DIR)/utils/tinyxml/tinyxmlparser.cpp \
	$(SRC_DIR)/addons/slot2_auto.cpp \
	$(SRC_DIR)/addons/slot2_mpcf.cpp \
	$(SRC_DIR)/addons/slot2_paddle.cpp \
	$(SRC_DIR)/addons/slot2_gbagame.cpp \
	$(SRC_DIR)/addons/slot2_none.cpp \
	$(SRC_DIR)/addons/slot2_rumblepak.cpp \
	$(SRC_DIR)/addons/slot2_guitarGrip.cpp \
	$(SRC_DIR)/addons/slot2_expMemory.cpp \
	$(SRC_DIR)/addons/slot2_piano.cpp \
	$(SRC_DIR)/addons/slot2_passme.cpp \
	$(SRC_DIR)/addons/slot1_none.cpp \
	$(SRC_DIR)/addons/slot1_r4.cpp \
	$(SRC_DIR)/addons/slot1comp_mc.cpp \
	$(SRC_DIR)/addons/slot1comp_rom.cpp \
	$(SRC_DIR)/addons/slot1comp_protocol.cpp \
	$(SRC_DIR)/addons/slot1_retail_mcrom.cpp \
	$(SRC_DIR)/addons/slot1_retail_mcrom_debug.cpp \
	$(SRC_DIR)/addons/slot1_retail_nand.cpp \
	$(SRC_DIR)/addons/slot1_retail_auto.cpp \
	$(SRC_DIR)/cheatSystem.cpp \
	$(SRC_DIR)/texcache.cpp \
	$(SRC_DIR)/rasterize.cpp \
	$(SRC_DIR)/metaspu/metaspu.cpp \
	$(SRC_DIR)/version.cpp \
	$(SRC_DIR)/mic.cpp \
	$(SRC_DIR)/GPU_osd_stub.cpp \
	$(SRC_DIR)/driver.cpp \
	$(SRC_DIR)/fs-linux.cpp

ifeq ($(DESMUME_JIT_ARM),1)
SOURCES += \
   $(SRC_DIR)/libretro/arm_arm/arm_gen.cpp \
   $(SRC_DIR)/libretro/arm_arm/arm_jit.cpp
endif

ifeq ($(DESMUME_JIT),1)
SOURCES += \
	$(SRC_DIR)/arm_jit.cpp \
   $(SRC_DIR)/utils/AsmJit/core/assembler.cpp \
   $(SRC_DIR)/utils/AsmJit/core/assert.cpp \
   $(SRC_DIR)/utils/AsmJit/core/buffer.cpp \
   $(SRC_DIR)/utils/AsmJit/core/compiler.cpp \
   $(SRC_DIR)/utils/AsmJit/core/compilercontext.cpp \
   $(SRC_DIR)/utils/AsmJit/core/compilerfunc.cpp \
   $(SRC_DIR)/utils/AsmJit/core/compileritem.cpp \
   $(SRC_DIR)/utils/AsmJit/core/context.cpp \
   $(SRC_DIR)/utils/AsmJit/core/cpuinfo.cpp \
   $(SRC_DIR)/utils/AsmJit/core/defs.cpp \
   $(SRC_DIR)/utils/AsmJit/core/func.cpp \
   $(SRC_DIR)/utils/AsmJit/core/logger.cpp \
   $(SRC_DIR)/utils/AsmJit/core/memorymanager.cpp \
   $(SRC_DIR)/utils/AsmJit/core/memorymarker.cpp \
   $(SRC_DIR)/utils/AsmJit/core/operand.cpp \
   $(SRC_DIR)/utils/AsmJit/core/stringbuilder.cpp \
   $(SRC_DIR)/utils/AsmJit/core/stringutil.cpp \
   $(SRC_DIR)/utils/AsmJit/core/virtualmemory.cpp \
   $(SRC_DIR)/utils/AsmJit/core/zonememory.cpp \
   $(SRC_DIR)/utils/AsmJit/x86/x86assembler.cpp \
   $(SRC_DIR)/utils/AsmJit/x86/x86compiler.cpp \
   $(SRC_DIR)/utils/AsmJit/x86/x86compilercontext.cpp \
   $(SRC_DIR)/utils/AsmJit/x86/x86compilerfunc.cpp \
   $(SRC_DIR)/utils/AsmJit/x86/x86compileritem.cpp \
   $(SRC_DIR)/utils/AsmJit/x86/x86cpuinfo.cpp \
   $(SRC_DIR)/utils/AsmJit/x86/x86defs.cpp \
   $(SRC_DIR)/utils/AsmJit/x86/x86func.cpp \
   $(SRC_DIR)/utils/AsmJit/x86/x86operand.cpp \
   $(SRC_DIR)/utils/AsmJit/x86/x86util.cpp
endif

LOCAL_MODULE    := libretro

LOCAL_SRC_FILES := $(SOURCES) $(SRC_DIR)/libretro/libretro.cpp $(SRC_DIR)/utils/ConvertUTF.c
GLOBAL_DEFINES :=  $(JIT) -DHAVE_LIBZ -fexceptions

LOCAL_CXXFLAGS += -O3 -DLSB_FIRST -D__LIBRETRO__ -Wno-write-strings -DANDROID -DFRONTEND_SUPPORTS_RGB565 $(GLOBAL_DEFINES)
LOCAL_CFLAGS = -O3 -DLSB_FIRST -D__LIBRETRO__ -Wno-psabi -Wno-write-strings -DANDROID -DFRONTEND_SUPPORTS_RGB565 $(GLOBAL_DEFINES)

LOCAL_C_INCLUDES = -I$(SRC_DIR)/libretro/zlib -iquote $(SRC_DIR) -iquote $(SRC_DIR)/libretro

LOCAL_LDLIBS += -lz

include $(BUILD_SHARED_LIBRARY)
