#include <stdarg.h>
#include "libretro.h"

#include "cheatSystem.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "debug.h"
#include "sndsdl.h"
#include "render3D.h"
#include "rasterize.h"
#include "saves.h"
#include "firmware.h"
#include "GPU.h"
#include "emufile.h"
#include "common.h"

retro_log_printf_t log_cb = NULL;
static retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
static retro_audio_sample_batch_t audio_batch_cb = NULL;
retro_environment_t environ_cb = NULL;

volatile bool execute = false;

static int delay_timer = 0;
static int current_screen = 1;
static bool quick_switch_enable = false;
static bool mouse_enable = false;
static int pointer_device = 0;
static int analog_stick_deadzone;
static int analog_stick_acceleration = 2048;
static int analog_stick_acceleration_modifier = 0;
static int microphone_force_enable = 0;
static int nds_screen_gap = 0;

namespace /* INPUT */
{
    static bool absolutePointer;

    template<int32_t MIN, int32_t MAX>
    static int32_t Saturate(int32_t aValue)
    {
        return std::max(MIN, std::min(MAX, aValue));
    }

    static int32_t TouchX;
    static int32_t TouchY;

    static const uint32_t FramesWithPointerBase = 60 * 10;
    static int32_t FramesWithPointer;

    static void DrawPointerLine(uint16_t* aOut, uint32_t aPitchInPix)
    {
        for(int i = 0; i < 5; i ++)
            aOut[aPitchInPix * i] = 0xFFFF;
    }

    static void DrawPointer(uint16_t* aOut, uint32_t aPitchInPix)
    {
        if(FramesWithPointer-- < 0)
            return;

        TouchX = Saturate<0, 255>(TouchX);
        TouchY = Saturate<0, 191>(TouchY);

        if(TouchX >   5) DrawPointerLine(&aOut[TouchY * aPitchInPix + TouchX - 5], 1);
        if(TouchX < 251) DrawPointerLine(&aOut[TouchY * aPitchInPix + TouchX + 1], 1);
        if(TouchY >   5) DrawPointerLine(&aOut[(TouchY - 5) * aPitchInPix + TouchX], aPitchInPix);
        if(TouchY < 187) DrawPointerLine(&aOut[(TouchY + 1) * aPitchInPix + TouchX], aPitchInPix);
    }
}


namespace /* VIDEO */
{
    const int nds_max_screen_gap = 100;

    static uint16_t screenSwap[GFX3D_FRAMEBUFFER_WIDTH * (GFX3D_FRAMEBUFFER_HEIGHT + nds_max_screen_gap) * 2];
    static retro_pixel_format colorMode;
    static uint32_t frameSkip;
    static uint32_t frameIndex;

    struct LayoutData
    {
        const char* name;
        uint16_t* screens[2];
        uint32_t touchScreenX;
        uint32_t touchScreenY;
        uint32_t width;
        uint32_t height;
        uint32_t pitchInPix;
    };

    static LayoutData layouts[] =
    {
        { "top/bottom", { &screenSwap[0], &screenSwap[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT] }, 0, GFX3D_FRAMEBUFFER_HEIGHT, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT * 2, GFX3D_FRAMEBUFFER_WIDTH},
        { "bottom/top", { &screenSwap[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT], &screenSwap[0] }, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, (GFX3D_FRAMEBUFFER_HEIGHT * 2), GFX3D_FRAMEBUFFER_WIDTH },
        { "left/right", { &screenSwap[0], &screenSwap[GFX3D_FRAMEBUFFER_WIDTH] }, GFX3D_FRAMEBUFFER_WIDTH, 0, (GFX3D_FRAMEBUFFER_WIDTH * 2), GFX3D_FRAMEBUFFER_HEIGHT, (GFX3D_FRAMEBUFFER_WIDTH * 2) },
        { "right/left", { &screenSwap[GFX3D_FRAMEBUFFER_WIDTH], &screenSwap[0] }, 0, 0, (GFX3D_FRAMEBUFFER_WIDTH * 2), GFX3D_FRAMEBUFFER_HEIGHT, (GFX3D_FRAMEBUFFER_WIDTH * 2) },
        { "top only", { &screenSwap[0], &screenSwap[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT] }, 0, GFX3D_FRAMEBUFFER_HEIGHT, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GFX3D_FRAMEBUFFER_WIDTH },
        { "bottom only", { &screenSwap[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT], &screenSwap[0] }, 0, GFX3D_FRAMEBUFFER_HEIGHT, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GFX3D_FRAMEBUFFER_WIDTH },
        { "quick switch", { &screenSwap[0], &screenSwap[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT] }, 0, GFX3D_FRAMEBUFFER_HEIGHT, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GFX3D_FRAMEBUFFER_WIDTH },
        { 0, 0, 0, 0 }
    };

    static LayoutData* screenLayout = &layouts[0];

    static void SwapScreen(void *dst, const void *src, uint32_t pitch, bool render_fullscreen)
    {
        const uint32_t *_src = (const uint32_t*)src;
        uint32_t width = render_fullscreen ? GFX3D_FRAMEBUFFER_WIDTH : (GFX3D_FRAMEBUFFER_WIDTH / 2);
        
        for(int i = 0; i < GFX3D_FRAMEBUFFER_HEIGHT; i ++)
        {
            uint32_t *_dst = (uint32_t*)dst + (i * pitch);

            for(int j = 0; j < width; j ++)
            {
               const uint32_t p = *_src++;            
               *_dst++ = (((p >> 10) & 0x001F001F001F001FULL)) | (((p >> 5) & 0x001F001F001F001FULL) << 6) | (((p >> 0) & 0x001F001F001F001FULL) << 11);
            }
        }
    }

    void SetupScreens(const char* aLayout)
    {
        screenLayout = &layouts[0];

        for(int i = 0; aLayout && layouts[i].name; i ++)
            if(!strcmp(aLayout, layouts[i].name))
                screenLayout = &layouts[i];
    }

    void SwapScreens(bool render_fullscreen)
    {
       SwapScreen(screenLayout->screens[0], (uint16_t*)&GPU_screen[0], screenLayout->pitchInPix / (render_fullscreen ? 1 : 2), false);
       SwapScreen(screenLayout->screens[1], (uint16_t*)&GPU_screen[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT * (render_fullscreen ? 1 : 2)], screenLayout->pitchInPix / (render_fullscreen ? 1 : 2), false);
       DrawPointer(screenLayout->screens[1], screenLayout->pitchInPix);
    }
    
   void UpdateScreenLayout()
   {
      if (nds_screen_gap > 100)
         nds_screen_gap = 100;

      if (screenLayout->name == "top/bottom") {
         screenLayout->screens[1] = &screenSwap[GFX3D_FRAMEBUFFER_WIDTH * (GFX3D_FRAMEBUFFER_HEIGHT + nds_screen_gap)];
         screenLayout->height = GFX3D_FRAMEBUFFER_HEIGHT * 2 + nds_screen_gap;
      } else if (screenLayout->name == "bottom/top") {
         screenLayout->screens[0] = &screenSwap[GFX3D_FRAMEBUFFER_WIDTH * (GFX3D_FRAMEBUFFER_HEIGHT + nds_screen_gap)];
         screenLayout->height = GFX3D_FRAMEBUFFER_HEIGHT * 2 + nds_screen_gap;
      } else if (screenLayout->name == "left/right") {
         screenLayout->screens[0] = &screenSwap[0];
         screenLayout->screens[1] = &screenSwap[GFX3D_FRAMEBUFFER_WIDTH + nds_screen_gap];
         screenLayout->width = GFX3D_FRAMEBUFFER_WIDTH * 2 + nds_screen_gap;
         screenLayout->pitchInPix = GFX3D_FRAMEBUFFER_WIDTH * 2 + nds_screen_gap;
      } else if (screenLayout->name == "right/left") {
         screenLayout->screens[0] = &screenSwap[GFX3D_FRAMEBUFFER_WIDTH + nds_screen_gap];
         screenLayout->width = GFX3D_FRAMEBUFFER_WIDTH * 2 + nds_screen_gap;
         screenLayout->pitchInPix = GFX3D_FRAMEBUFFER_WIDTH * 2 + nds_screen_gap;
      }
   }
	
}

namespace
{
    uint32_t firmwareLanguage;
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "DeSmuME";
   info->library_version = "svn";
   info->valid_extensions = "nds|bin";
   info->need_fullpath = true;   
   info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    info->geometry.base_width = screenLayout->width;
    info->geometry.base_height = screenLayout->height;
    info->geometry.max_width = screenLayout->width*2;
    info->geometry.max_height = screenLayout->height;
    info->geometry.aspect_ratio = 0.0;
    info->timing.fps = 60.0;
    info->timing.sample_rate = 44100.0;
}


static void QuickSwap(void)
{
	if(quick_switch_enable)
	{
	   if(current_screen == 1)
	   {
		   SetupScreens("bottom only");
		   current_screen=2;
	   }
	   else
	   {
		   SetupScreens("top only");
		   current_screen=1;
	   }
	}
}

static void MicrophoneToggle(void)
{
   if (NDS_getFinalUserInput().mic.micButtonPressed)
      NDS_setMic(false);
   else
      NDS_setMic(true);
}

static void check_variables(void)
{
	struct retro_variable var = {0};
	
	var.key = "desmume_num_cores";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		CommonSettings.num_cores = var.value ? strtol(var.value, 0, 10) : 1;
   else
      CommonSettings.num_cores = 1;
	
	var.key = "desmume_cpu_mode";
	var.value = 0;
	
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		if (!strcmp(var.value, "jit"))
			CommonSettings.use_jit = true;
		else if (!strcmp(var.value, "interpreter"))
			CommonSettings.use_jit = false;
	}
   else
   {
#ifdef HAVE_JIT
      CommonSettings.use_jit = true;
#else
      CommonSettings.use_jit = false;
#endif
   }

#ifdef HAVE_JIT
	var.key = "desmume_jit_block_size";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		CommonSettings.jit_max_block_size = var.value ? strtol(var.value, 0, 10) : 100;
   else
		CommonSettings.jit_max_block_size = 100;
#endif
	
	var.key = "desmume_screens_layout";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{	
		if (!strcmp(var.value, "quick switch"))
			quick_switch_enable = true;
		else 
			quick_switch_enable = false;
		SetupScreens(var.value);
	}	
   else
      quick_switch_enable = false;
	

	var.key = "desmume_pointer_mouse";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		if (!strcmp(var.value, "enable"))
			mouse_enable = true;
      else if (!strcmp(var.value, "disable"))
			mouse_enable = false;
	}
   else
      mouse_enable = false;

	var.key = "desmume_pointer_device";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		if (!strcmp(var.value, "l-stick"))
			pointer_device = 1;
		else if(!strcmp(var.value, "r-stick"))
			pointer_device = 2;
		else 
			pointer_device=0;
	}
   else
      pointer_device=0;
		
	var.key = "desmume_pointer_device_deadzone";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      analog_stick_deadzone = (int)(atoi(var.value));
		
	var.key = "desmume_pointer_type";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		absolutePointer = var.value && (!strcmp(var.value, "absolute"));
	}
	
	var.key = "desmume_frameskip";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		frameSkip = var.value ? strtol(var.value, 0, 10) : 0;
   else
      frameSkip = 0;
	 
	var.key = "desmume_firmware_language";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		static const struct { const char* name; uint32_t id; } languages[6] = 
		{
			{ "Japanese", 0 },
			{ "English", 1 },
			{ "French", 2 },
			{ "German", 3 },
			{ "Italian", 4 },
			{ "Spanish", 5 }
		};
		
		for (int i = 0; i < 6; i ++)
		{
			if (!strcmp(languages[i].name, var.value))
			{
				firmwareLanguage = languages[i].id;
				break;
			}
		}		
	}
   else
      firmwareLanguage = 1;


   var.key = "desmume_gfx_edgemark";
   
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
		if (!strcmp(var.value, "enable"))
         CommonSettings.GFX3D_EdgeMark = true;
      else if (!strcmp(var.value, "disable"))
         CommonSettings.GFX3D_EdgeMark = false;
   }
   else
      CommonSettings.GFX3D_EdgeMark = true;

   var.key = "desmume_gfx_linehack";
   
    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enable"))
         CommonSettings.GFX3D_LineHack = true;
      else if (!strcmp(var.value, "disable"))
         CommonSettings.GFX3D_LineHack = false;
   }
   else
      CommonSettings.GFX3D_LineHack = true;


   var.key = "desmume_gfx_depth_comparison_threshold";
   
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      CommonSettings.GFX3D_Zelda_Shadow_Depth_Hack = atoi(var.value);
   }
   else
      CommonSettings.GFX3D_Zelda_Shadow_Depth_Hack = 0;

   var.key = "desmume_mic_force_enable";
   
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "yes"))
         microphone_force_enable = 1;
      else if(!strcmp(var.value, "no"))
         microphone_force_enable = 0;
   }
   else
      NDS_setMic(false);
   
   var.key = "desmume_mic_mode";
   
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "internal"))
         CommonSettings.micMode = TCommonSettings::InternalNoise;
      else if(!strcmp(var.value, "sample"))
         CommonSettings.micMode = TCommonSettings::Sample;
      else if(!strcmp(var.value, "random"))
         CommonSettings.micMode = TCommonSettings::Random;
      else if(!strcmp(var.value, "physical"))
         CommonSettings.micMode = TCommonSettings::Physical;
   }
   else
      CommonSettings.micMode = TCommonSettings::InternalNoise;
   
   var.key = "desmume_pointer_device_acceleration_mod";
   
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      analog_stick_acceleration_modifier = atoi(var.value);
   else
      analog_stick_acceleration_modifier = 0;

   var.key = "desmume_load_to_memory";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enable"))
         CommonSettings.loadToMemory = true;
      else if (!strcmp(var.value, "disable"))
         CommonSettings.loadToMemory = false;
   }
   else
      CommonSettings.loadToMemory = false;

   var.key = "desmume_advanced_timing";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enable"))
         CommonSettings.advanced_timing = true;
      else if (!strcmp(var.value, "disable"))
         CommonSettings.advanced_timing = false;
   }
   else
      CommonSettings.advanced_timing = true;

   var.key = "desmume_spu_interpolation";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "Linear"))
         CommonSettings.spuInterpolationMode = SPUInterpolation_Linear;
      else if (!strcmp(var.value, "Cosine"))
         CommonSettings.spuInterpolationMode = SPUInterpolation_Cosine;
      else if (!strcmp(var.value, "None"))
         CommonSettings.spuInterpolationMode = SPUInterpolation_None;
   }
   else
      CommonSettings.spuInterpolationMode = SPUInterpolation_Linear;

   var.key = "desmume_spu_sync_mode";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "DualSPU"))
         CommonSettings.SPU_sync_mode = 0;
      else if (!strcmp(var.value, "Synchronous"))
         CommonSettings.SPU_sync_mode = 1;
   }
   else
      CommonSettings.SPU_sync_mode = 1;

   var.key = "desmume_spu_sync_method";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "N"))
         CommonSettings.SPU_sync_method = 0;
      else if (!strcmp(var.value, "Z"))
         CommonSettings.SPU_sync_method = 1;
      else if (!strcmp(var.value, "P"))
         CommonSettings.SPU_sync_method = 2;
   }
   else
      CommonSettings.SPU_sync_method = 1;
      
   var.key = "desmume_screens_gap";
   
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      nds_screen_gap = atoi(var.value);
      UpdateScreenLayout();
   }
}

void frontend_process_samples(u32 frames, const s16* data)
{
    audio_batch_cb(data, frames);
}

SoundInterface_struct* SNDCoreList[] =
{
    NULL
};

#ifndef GPU3D_NULL
#define GPU3D_NULL 0
#endif

#ifdef HAVE_OPENGL
#define GPU3D_OPENGL_3_2 1
#define GPU3D_SWRAST     2
#define GPU3D_OPENGL_OLD 3
#else
#define GPU3D_SWRAST     1
#endif

GPU3DInterface* core3DList[] =
{
   &gpu3DNull,
#ifdef HAVE_OPENGL
   &gpu3Dgl_3_2,
#endif
    &gpu3DRasterize,
#ifdef HAVE_OPENGL
    &gpu3DglOld,
#endif
    NULL
};

//

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb)   { }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_cb = cb; }

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   static const retro_variable values[] =
   {
      { "desmume_num_cores", "CPU cores; 1|2|3|4" },
#ifdef HAVE_JIT
      { "desmume_cpu_mode", "CPU mode; jit|interpreter" },	
      { "desmume_jit_block_size", "JIT block size; 100|99|98|97|96|95|94|93|92|91|90|89|88|87|86|85|84|83|82|81|80|79|78|77|76|75|74|73|72|71|70|69|68|67|66|65|64|63|62|61|60|59|58|57|56|55|54|53|52|51|50|49|48|47|46|45|44|43|42|41|40|39|38|37|36|35|34|33|32|31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13|12|11|10|9|8|7|6|5|4|3|2|1|0" },
#else
      { "desmume_cpu_mode", "CPU mode; interpreter" },
#endif
      { "desmume_screens_layout", "Screen layout; top/bottom|bottom/top|left/right|right/left|top only|bottom only|quick switch" },
      { "desmume_pointer_mouse", "Enable mouse/pointer; enable|disable" },
      { "desmume_pointer_type", "Mouse/pointer mode; relative|absolute" },
      { "desmume_pointer_device", "Pointer emulation; none|l-stick|r-stick" },
      { "desmume_pointer_device_deadzone", "Emulated pointer deadzone percent; 15|20|25|30|0|5|10" },	  
      { "desmume_pointer_device_acceleration_mod", "Emulated pointer acceleration modifier percent; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100" },	
      { "desmume_load_to_memory", "Load Game into Memory (restart); disable|enable" },
      { "desmume_advanced_timing", "Enable Advanced Bus-Level Timing; enable|disable" },
      { "desmume_firmware_language", "Firmware language; English|Japanese|French|German|Italian|Spanish" },
      { "desmume_frameskip", "Frameskip; 0|1|2|3|4|5|6|7|8|9" },
      { "desmume_screens_gap", "Screen Gap; 0|5|64|90|0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100" },	
      { "desmume_gfx_edgemark", "Enable Edgemark; enable|disable" },
      { "desmume_gfx_linehack", "Enable Line Hack; enable|disable" },
      { "desmume_gfx_depth_comparison_threshold", "Depth Comparison Threshold; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100" },
      { "desmume_mic_force_enable", "Force Microphone Enable; no|yes" },
      { "desmume_mic_mode", "Microphone Simulation Settings; internal|sample|random|physical" },
      { "desmume_spu_sync_mode", "SPU Synchronization Mode; Synchronous|DualSPU" },
      { "desmume_spu_sync_method", "SPU Synchronization Method; Z|P|N" },
      { "desmume_spu_interpolation", "SPU Interpolation Mode; Linear|Cosine|None" },
      { 0, 0 }
   };

   environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)values);
}


//====================== Message box
#define MSG_ARG \
	char msg_buf[1024] = {0}; \
	{ \
		va_list args; \
		va_start (args, fmt); \
		vsprintf (msg_buf, fmt, args); \
		va_end (args); \
	}

void msgWndInfo(const char *fmt, ...)
{
	MSG_ARG;
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s.\n", msg_buf);
}

bool msgWndConfirm(const char *fmt, ...)
{
	MSG_ARG;
   if (log_cb)
      log_cb(RETRO_LOG_INFO, "%s.\n", msg_buf);
   return true;
}

void msgWndError(const char *fmt, ...)
{
	MSG_ARG;
   if (log_cb)
      log_cb(RETRO_LOG_ERROR, "%s.\n", msg_buf);
}

void msgWndWarn(const char *fmt, ...)
{
	MSG_ARG;
   if (log_cb)
      log_cb(RETRO_LOG_WARN, "%s.\n", msg_buf);
}

msgBoxInterface msgBoxWnd = {
	msgWndInfo,
	msgWndConfirm,
	msgWndError,
	msgWndWarn,
};
//====================== Dialogs end

static void check_system_specs(void)
{
   unsigned level = 15;
   environ_cb(RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL, &level);
}

static void Change3DCoreWithFallback(int newCore)
{
   if( (newCore < 0)
#ifdef HAVE_OPENGL
         || (newCore > GPU3D_OPENGL_OLD)
#endif
         )
      newCore = GPU3D_SWRAST;
   
   printf("Attempting change to 3d core to: %s\n",core3DList[newCore]->name);

#ifdef HAVE_OPENGL
   if(newCore == GPU3D_OPENGL_OLD)
      goto TRY_OGL_OLD;
#endif

   if(newCore == GPU3D_SWRAST)
      goto TRY_SWRAST;

   if(newCore == GPU3D_NULL)
   {
      NDS_3D_ChangeCore(GPU3D_NULL);
      cur3DCore = GPU3D_NULL;
      goto DONE;
   }

#ifdef HAVE_OPENGL
   if(!NDS_3D_ChangeCore(GPU3D_OPENGL_3_2))
   {
      printf("falling back to 3d core: %s\n",core3DList[GPU3D_OPENGL_OLD]->name);
      cur3DCore = GPU3D_OPENGL_3_2;
      goto TRY_OGL_OLD;
   }
#endif
   goto DONE;

#ifdef HAVE_OPENGL
TRY_OGL_OLD:
   if(!NDS_3D_ChangeCore(GPU3D_OPENGL_OLD))
   {
      printf("falling back to 3d core: %s\n",core3DList[GPU3D_SWRAST]->name);
      cur3DCore = GPU3D_OPENGL_OLD;
      goto TRY_SWRAST;
   }
   goto DONE;

#endif
TRY_SWRAST:
   cur3DCore = GPU3D_SWRAST;
   NDS_3D_ChangeCore(GPU3D_SWRAST);

DONE:
   (void)0;
}

void retro_init (void)
{
   struct retro_log_callback log;
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
      log_cb = log.log;
   else
      log_cb = NULL;

    colorMode = RETRO_PIXEL_FORMAT_RGB565;
    if(!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &colorMode))
       return;

    check_variables();

    // Init DeSmuME
    struct NDS_fw_config_data fw_config;
    NDS_FillDefaultFirmwareConfigData(&fw_config);
    fw_config.language = firmwareLanguage;


    //addonsChangePak(NDS_ADDON_NONE);
    NDS_Init();
    NDS_CreateDummyFirmware(&fw_config);

    Change3DCoreWithFallback(GPU3D_SWRAST);

    backup_setManualBackupType(MC_TYPE_AUTODETECT);

    msgbox = &msgBoxWnd;
   check_system_specs();
}

void retro_deinit(void)
{
    NDS_DeInit();

#ifdef PERF_TEST
   rarch_perf_log();
#endif
}

void retro_reset (void)
{
    NDS_Reset();
}

void retro_run (void)
{
    // Settings
    bool updated = false;	
    bool render_fullscreen = false;
    
	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
	{
      check_variables();
		struct retro_system_av_info new_av_info;
		retro_get_system_av_info(&new_av_info);

		environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &new_av_info);		
	}

    poll_cb();

    bool haveTouch = false;
	
	if(pointer_device!=0)
	{
		int16_t analogX = 0;
		int16_t analogY = 0;
      
      float final_acceleration = analog_stick_acceleration * (1.0 + (float)analog_stick_acceleration_modifier / 100.0);
		
		if(pointer_device == 1)
		{
      analogX = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / final_acceleration;
			analogY = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / final_acceleration;
		} 
		else if(pointer_device == 2)
		{
			analogX = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / final_acceleration;
			analogY = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / final_acceleration;
		}
		else
		{
			analogX = 0;
			analogY = 0;
		}
		

		// Convert cartesian coordinate analog stick to polar coordinates
		double radius = sqrt(analogX * analogX + analogY * analogY);
		double angle = atan2(analogY, analogX);		
		double max = (float)0x8000/analog_stick_acceleration;
		
		//log_cb(RETRO_LOG_DEBUG, "%d %d.\n", analogX,analogY);
		//log_cb(RETRO_LOG_DEBUG, "%d %d.\n", radius,analog_stick_deadzone);
		if (radius > (float)analog_stick_deadzone*max/100)
		{
			// Re-scale analog stick range to negate deadzone (makes slow movements possible)
			radius = (radius - (float)analog_stick_deadzone*max/100)*((float)max/(max - (float)analog_stick_deadzone*max/100));
			
			// Convert back to cartesian coordinates
			analogX = (int32_t)round(radius * cos(angle));
			analogY = (int32_t)round(radius * sin(angle));
		}
		else
		{
			analogX = 0;
			analogY = 0;
		}		
		//log_cb(RETRO_LOG_DEBUG, "%d %d.\n", analogX,analogY);
		
		haveTouch = haveTouch || input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2); 
		
		TouchX = Saturate<0, 255>(TouchX + analogX);
		TouchY = Saturate<0, 191>(TouchY + analogY);
		
		FramesWithPointer = (analogX || analogY) ? FramesWithPointerBase : FramesWithPointer;
		
	}

	if(mouse_enable)
    {
		// TOUCH: Mouse
		if(!absolutePointer)
		{
			const int16_t mouseX = input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
			const int16_t mouseY = input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
			haveTouch = haveTouch || input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);
			
			TouchX = Saturate<0, 255>(TouchX + mouseX);
			TouchY = Saturate<0, 191>(TouchY + mouseY);
			FramesWithPointer = (mouseX || mouseY) ? FramesWithPointerBase : FramesWithPointer;
		}
		// TOUCH: Pointer
		else if(input_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED))
		{
			const float X_FACTOR = ((float)screenLayout->width / 65536.0f);
			const float Y_FACTOR = ((float)screenLayout->height / 65536.0f);
		
			float x = (input_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) + 32768.0f) * X_FACTOR;
			float y = (input_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) + 32768.0f) * Y_FACTOR;

			if (x >= screenLayout->touchScreenX && x < screenLayout->touchScreenX + GFX3D_FRAMEBUFFER_WIDTH &&
				y >= screenLayout->touchScreenY && y < screenLayout->touchScreenY + GFX3D_FRAMEBUFFER_HEIGHT)
			{
				haveTouch = true;

				TouchX = x - screenLayout->touchScreenX;
				TouchY = y - screenLayout->touchScreenY;
			}
		}
	}
	
	
	
    // TOUCH: Final        
    if(haveTouch)
        NDS_setTouchPos(TouchX, TouchY);
    else
        NDS_releaseTouch();

    // BUTTONS
    NDS_beginProcessingInput();
    
   NDS_setPad(
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L),
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R),
      0, // debug
      input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2) //Lid
   );
   
  if (!microphone_force_enable)
  {
    if(input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3))
      NDS_setMic(true);
    else if(!input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3))
      NDS_setMic(false);
  }
  else
    NDS_setMic(true);
  
	
	if(input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3) && quick_switch_enable && delay_timer == 0)
	{
		QuickSwap();
		delay_timer++;
	}
	
	if(delay_timer != 0)
	{
		delay_timer++;
		if(delay_timer == 30)
			delay_timer = 0;
	}
	
	
    NDS_endProcessingInput();

    // RUN
    frameIndex ++;
    bool skipped = frameIndex <= frameSkip;

    if (skipped)
       NDS_SkipNextFrame();

    NDS_exec<false>();
    
    // VIDEO: Swap screen colors and pass on
    if (!skipped)
        SwapScreens(render_fullscreen);

    video_cb(skipped ? 0 : screenSwap, screenLayout->width, screenLayout->height, screenLayout->pitchInPix * (render_fullscreen ? 1 : 2));

    frameIndex = skipped ? frameIndex : 0;
}

size_t retro_serialize_size (void)
{
    // HACK: Usually around 10 MB but can vary frame to frame!
    return 1024 * 1024 * 12;
}

bool retro_serialize(void *data, size_t size)
{
    EMUFILE_MEMORY state;
    savestate_save(&state, 0);
    
    if(state.size() <= size)
    {
        memcpy(data, state.buf(), state.size());
        return true;
    }
    
    return false;
}

bool retro_unserialize(const void * data, size_t size)
{
    EMUFILE_MEMORY state(const_cast<void*>(data), size);
    return savestate_load(&state);
}

bool retro_load_game(const struct retro_game_info *game)
{
   if (colorMode != RETRO_PIXEL_FORMAT_RGB565)
      return false;

   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,   "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,     "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,   "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT,  "Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X,      "X" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y,      "Y" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B,      "B" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A,      "A" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L,      "L" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2,     "Lid Close/Open" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3,     "Toggle Microphone" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R,      "R" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,     "Quick Screen Switch" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Select" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Reset" },

      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   execute = NDS_LoadROM(game->path);
   return execute;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
#if 0
    if(game_type == RETRO_GAME_TYPE_SUPER_GAME_BOY && num_info == 2)
    {
        strncpy(GBAgameName, info[1].path, sizeof(GBAgameName));
        addonsChangePak(NDS_ADDON_GBAGAME);
        
        return retro_load_game(&info[0]);
    }
#endif
    return false;
}

void retro_unload_game (void)
{
    NDS_FreeROM();
    execute = false;
}

// Stubs
void retro_set_controller_port_device(unsigned in_port, unsigned device) { }
void *retro_get_memory_data(unsigned type) { return 0; }
size_t retro_get_memory_size(unsigned type) { return 0; }
unsigned retro_api_version(void) { return RETRO_API_VERSION; }

extern CHEATS *cheats;

void retro_cheat_reset(void)
{
   if (cheats)
      cheats->clear();
}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   char ds_code[1024];
   char desc[1024];
   strcpy(ds_code, code);
   strcpy(desc, "N/A");

   if (!cheats)
      return;

   if (cheats->add_AR(ds_code, desc, 1) != TRUE)
   {
      /* Couldn't add Action Replay code */
   }
}

unsigned retro_get_region (void) { return RETRO_REGION_NTSC; }

#ifdef PSP
int ftruncate(int fd, off_t length)
{
   int ret;
   SceOff oldpos;
   if (!__PSP_IS_FD_VALID(fd)) {
      errno = EBADF;
      return -1;
   }

   switch(__psp_descriptormap[fd]->type)
   {
      case __PSP_DESCRIPTOR_TYPE_FILE:
         if (__psp_descriptormap[fd]->filename != NULL) {
            if (!(__psp_descriptormap[fd]->flags & (O_WRONLY | O_RDWR)))
               break;
            return truncate(__psp_descriptormap[fd]->filename, length);
            /* ANSI sez ftruncate doesn't move the file pointer */
         }
         break;
      case __PSP_DESCRIPTOR_TYPE_TTY:
      case __PSP_DESCRIPTOR_TYPE_PIPE:
      case __PSP_DESCRIPTOR_TYPE_SOCKET:
      default:
         break;
   }

   errno = EINVAL;
   return -1;
}
#endif
