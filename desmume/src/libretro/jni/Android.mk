HAVE_GRIFFIN    := 0

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

SRCDIR = ../../

ifeq ($(TARGET_ARCH),arm)
LOCAL_CXXFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
SOURCES += \
   $(SRCDIR)/libretro/arm_arm/arm_gen.cpp \
   $(SRCDIR)/libretro/arm_arm/arm_jit.cpp
JIT += -DHAVE_JIT -D__RETRO_ARM__
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CXXFLAGS +=  -DANDROID_X86
SOURCES += \
	$(SRCDIR)/arm_jit.cpp \
   $(SRCDIR)/utils/AsmJit/core/assembler.cpp \
   $(SRCDIR)/utils/AsmJit/core/assert.cpp \
   $(SRCDIR)/utils/AsmJit/core/buffer.cpp \
   $(SRCDIR)/utils/AsmJit/core/compiler.cpp \
   $(SRCDIR)/utils/AsmJit/core/compilercontext.cpp \
   $(SRCDIR)/utils/AsmJit/core/compilerfunc.cpp \
   $(SRCDIR)/utils/AsmJit/core/compileritem.cpp \
   $(SRCDIR)/utils/AsmJit/core/context.cpp \
   $(SRCDIR)/utils/AsmJit/core/cpuinfo.cpp \
   $(SRCDIR)/utils/AsmJit/core/defs.cpp \
   $(SRCDIR)/utils/AsmJit/core/func.cpp \
   $(SRCDIR)/utils/AsmJit/core/logger.cpp \
   $(SRCDIR)/utils/AsmJit/core/memorymanager.cpp \
   $(SRCDIR)/utils/AsmJit/core/memorymarker.cpp \
   $(SRCDIR)/utils/AsmJit/core/operand.cpp \
   $(SRCDIR)/utils/AsmJit/core/stringbuilder.cpp \
   $(SRCDIR)/utils/AsmJit/core/stringutil.cpp \
   $(SRCDIR)/utils/AsmJit/core/virtualmemory.cpp \
   $(SRCDIR)/utils/AsmJit/core/zonememory.cpp \
   $(SRCDIR)/utils/AsmJit/x86/x86assembler.cpp \
   $(SRCDIR)/utils/AsmJit/x86/x86compiler.cpp \
   $(SRCDIR)/utils/AsmJit/x86/x86compilercontext.cpp \
   $(SRCDIR)/utils/AsmJit/x86/x86compilerfunc.cpp \
   $(SRCDIR)/utils/AsmJit/x86/x86compileritem.cpp \
   $(SRCDIR)/utils/AsmJit/x86/x86cpuinfo.cpp \
   $(SRCDIR)/utils/AsmJit/x86/x86defs.cpp \
   $(SRCDIR)/utils/AsmJit/x86/x86func.cpp \
   $(SRCDIR)/utils/AsmJit/x86/x86operand.cpp \
   $(SRCDIR)/utils/AsmJit/x86/x86util.cpp
JIT += -DHAVE_JIT
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CXXFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif


SOURCES += \
	$(SRCDIR)/armcpu.cpp \
	$(SRCDIR)/arm_instructions.cpp \
	$(SRCDIR)/bios.cpp \
	$(SRCDIR)/cp15.cpp \
	$(SRCDIR)/common.cpp \
	$(SRCDIR)/debug.cpp \
	$(SRCDIR)/Disassembler.cpp \
	$(SRCDIR)/emufile.cpp \
	$(SRCDIR)/FIFO.cpp \
	$(SRCDIR)/firmware.cpp \
	$(SRCDIR)/GPU.cpp \
	$(SRCDIR)/mc.cpp \
	$(SRCDIR)/path.cpp \
	$(SRCDIR)/readwrite.cpp \
	$(SRCDIR)/wifi.cpp \
	$(SRCDIR)/MMU.cpp \
	$(SRCDIR)/NDSSystem.cpp \
	$(SRCDIR)/ROMReader.cpp \
	$(SRCDIR)/render3D.cpp \
	$(SRCDIR)/rtc.cpp \
	$(SRCDIR)/saves.cpp \
	$(SRCDIR)/slot1.cpp \
	$(SRCDIR)/SPU.cpp \
	$(SRCDIR)/matrix.cpp \
	$(SRCDIR)/gfx3d.cpp \
	$(SRCDIR)/thumb_instructions.cpp \
	$(SRCDIR)/movie.cpp \
	$(SRCDIR)/utils/datetime.cpp \
	$(SRCDIR)/utils/guid.cpp \
	$(SRCDIR)/utils/emufat.cpp \
	$(SRCDIR)/utils/md5.cpp \
	$(SRCDIR)/utils/xstring.cpp \
	$(SRCDIR)/utils/decrypt/crc.cpp \
	$(SRCDIR)/utils/decrypt/decrypt.cpp \
	$(SRCDIR)/utils/decrypt/header.cpp \
	$(SRCDIR)/utils/task.cpp \
	$(SRCDIR)/utils/vfat.cpp \
	$(SRCDIR)/utils/dlditool.cpp \
	$(SRCDIR)/utils/libfat/cache.cpp \
	$(SRCDIR)/utils/libfat/directory.cpp \
	$(SRCDIR)/utils/libfat/disc.cpp \
	$(SRCDIR)/utils/libfat/fatdir.cpp \
	$(SRCDIR)/utils/libfat/fatfile.cpp \
	$(SRCDIR)/utils/libfat/filetime.cpp \
	$(SRCDIR)/utils/libfat/file_allocation_table.cpp \
	$(SRCDIR)/utils/libfat/libfat.cpp \
	$(SRCDIR)/utils/libfat/libfat_public_api.cpp \
	$(SRCDIR)/utils/libfat/lock.cpp \
	$(SRCDIR)/utils/libfat/partition.cpp \
	$(SRCDIR)/utils/tinyxml/tinystr.cpp \
	$(SRCDIR)/utils/tinyxml/tinyxml.cpp \
	$(SRCDIR)/utils/tinyxml/tinyxmlerror.cpp \
	$(SRCDIR)/utils/tinyxml/tinyxmlparser.cpp \
	$(SRCDIR)/addons.cpp \
	$(SRCDIR)/addons/slot2_mpcf.cpp \
	$(SRCDIR)/addons/slot2_paddle.cpp \
	$(SRCDIR)/addons/slot2_gbagame.cpp \
	$(SRCDIR)/addons/slot2_none.cpp \
	$(SRCDIR)/addons/slot2_rumblepak.cpp \
	$(SRCDIR)/addons/slot2_guitarGrip.cpp \
	$(SRCDIR)/addons/slot2_expMemory.cpp \
	$(SRCDIR)/addons/slot2_piano.cpp \
	$(SRCDIR)/addons/slot1_none.cpp \
	$(SRCDIR)/addons/slot1_r4.cpp \
	$(SRCDIR)/addons/slot1_retail.cpp \
	$(SRCDIR)/addons/slot1_retail_nand.cpp \
	$(SRCDIR)/cheatSystem.cpp \
	$(SRCDIR)/texcache.cpp \
	$(SRCDIR)/rasterize.cpp \
	$(SRCDIR)/metaspu/metaspu.cpp \
	$(SRCDIR)/version.cpp \
	$(SRCDIR)/mic.cpp \
	$(SRCDIR)/GPU_osd_stub.cpp \
	$(SRCDIR)/driver.cpp \
	$(SRCDIR)/fs-linux.cpp

LOCAL_MODULE    := libretro

LOCAL_SRC_FILES := $(SOURCES) $(SRCDIR)/libretro/libretro.cpp $(SRCDIR)/utils/ConvertUTF.c
GLOBAL_DEFINES :=  $(JIT) -DHAVE_LIBZ -fexceptions

LOCAL_CXXFLAGS += -O3 -DLSB_FIRST -D__LIBRETRO__ -Wno-write-strings -DANDROID -DFRONTEND_SUPPORTS_RGB565 $(GLOBAL_DEFINES)
LOCAL_CFLAGS = -O3 -DLSB_FIRST -D__LIBRETRO__ -Wno-write-strings -DANDROID -DFRONTEND_SUPPORTS_RGB565 $(GLOBAL_DEFINES)

LOCAL_C_INCLUDES = -I$(SRCDIR)/libretro/zlib -iquote $(SRCDIR) -iquote $(SRCDIR)/libretro

LOCAL_LDLIBS += -lz

include $(BUILD_SHARED_LIBRARY)
