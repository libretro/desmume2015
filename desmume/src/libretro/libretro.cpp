#include <stdarg.h>
#include "libretro.h"

#include "cheatSystem.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "debug.h"
#include "render3D.h"
#include "rasterize.h"
#include "saves.h"
#include "firmware.h"
#include "GPU.h"
#include "SPU.h"
#include "emufile.h"
#include "common.h"

#define LAYOUTS_MAX 7

retro_log_printf_t log_cb = NULL;
static retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
retro_audio_sample_batch_t audio_batch_cb = NULL;
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

static uint16_t *screen_buf;

int currFrameCounter;

unsigned GPU_LR_FRAMEBUFFER_NATIVE_WIDTH  = 256;
unsigned GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT = 192;

#define NDS_MAX_SCREEN_GAP               100

#define LAYOUT_TOP_BOTTOM                 0
#define LAYOUT_BOTTOM_TOP                 1
#define LAYOUT_LEFT_RIGHT                 2
#define LAYOUT_RIGHT_LEFT                 3
#define LAYOUT_TOP_ONLY                   4
#define LAYOUT_BOTTOM_ONLY                5
#define LAYOUT_QUICK_SWITCH               6

static int current_layout = LAYOUT_TOP_BOTTOM;


struct LayoutData
{
   uint16_t *dst;
   uint16_t *dst2;
   uint32_t touch_x;
   uint32_t touch_y;
   uint32_t width;
   uint32_t height;
   uint32_t pitch;
   bool draw_screen1;
   bool draw_screen2;
};

static bool absolutePointer;

static inline int32_t Saturate(int32_t min, int32_t max, int32_t aValue)
{
   return std::max(min, std::min(max, aValue));
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

   TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), TouchX);
   TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), TouchY);

   if(TouchX >   5) DrawPointerLine(&aOut[TouchY * aPitchInPix + TouchX - 5], 1);
   if(TouchX < (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-5)) DrawPointerLine(&aOut[TouchY * aPitchInPix + TouchX + 1], 1);
   if(TouchY >   5) DrawPointerLine(&aOut[(TouchY - 5) * aPitchInPix + TouchX], aPitchInPix);
   if(TouchY < (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-5)) DrawPointerLine(&aOut[(TouchY + 1) * aPitchInPix + TouchX], aPitchInPix);
}

static retro_pixel_format colorMode;
static uint32_t frameSkip;
static uint32_t frameIndex;


#define CONVERT_COLOR(color) (((color & 0x001f) << 11) | ((color & 0x03e0) << 1) | ((color & 0x0200) >> 4) | ((color & 0x7c00) >> 10))

static void SwapScreen(uint16_t *dst, const uint16_t *src, uint32_t pitch)
{
   unsigned i, j;
   for(i = 0; i < GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT; i ++)
      for(j = 0; j < GPU_LR_FRAMEBUFFER_NATIVE_WIDTH; j ++)
      {
         uint16_t col = *src++;
         *dst++ = CONVERT_COLOR(col);
      }
}

namespace
{
    uint32_t firmwareLanguage;
}

void retro_get_system_info(struct retro_system_info *info)
{
   info->library_name = "DeSmuME";
   info->library_version = "SVN";
   info->valid_extensions = "nds|bin";
   info->need_fullpath = true;   
   info->block_extract = false;
}

static void get_layout_params(unsigned id, uint16_t *src, LayoutData *layout)
{
   if (!layout)
      return;

   switch (id)
   {
      case LAYOUT_TOP_BOTTOM:
         if (src)
         {
            layout->dst    = src;
            layout->dst2   = (uint16_t*)(src + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT + nds_screen_gap));
         }
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * 2 + nds_screen_gap;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_x= 0;
         layout->touch_y= GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;

         layout->draw_screen1  = true;
         layout->draw_screen2  = true;
         break;
      case LAYOUT_BOTTOM_TOP:
         if (src)
         {
            layout->dst   = (uint16_t*)(src + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT + nds_screen_gap));
            layout->dst2  = src;
         }
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT * 2 + nds_screen_gap;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_x= 0;
         layout->touch_y= GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;

         layout->draw_screen1  = true;
         layout->draw_screen2  = true;
         break;
      case LAYOUT_LEFT_RIGHT:
         if (src)
         {
            layout->dst    = src;
            layout->dst2   = (uint16_t*)(src + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + nds_screen_gap);
         }
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * 2 + nds_screen_gap;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * 2 + nds_screen_gap;
         layout->touch_x= GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_y= 0;

         layout->draw_screen1  = true;
         layout->draw_screen2  = true;
         break;
      case LAYOUT_RIGHT_LEFT:
         if (src)
         {
            layout->dst   = (uint16_t*)(src + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + nds_screen_gap);
            layout->dst2  = src;
         }
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * 2 + nds_screen_gap;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * 2 + nds_screen_gap;
         layout->touch_x= 0;
         layout->touch_y= 0;

         layout->draw_screen1  = true;
         layout->draw_screen2  = true;
         break;
      case LAYOUT_TOP_ONLY:
         if (src)
         {
            layout->dst    = src;
            layout->dst2   = (uint16_t*)(src + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);
         }
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_x= 0;
         layout->touch_y= GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;

         layout->draw_screen1 = true;
         break;
      case LAYOUT_BOTTOM_ONLY:
         if (src)
         {
            layout->dst    = (uint16_t*)(src + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);
            layout->dst2   = src;
         }
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_x= 0;
         layout->touch_y= GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;

         layout->draw_screen2 = true;
         break;
      case LAYOUT_QUICK_SWITCH:
         if (src)
         {
            layout->dst    = src;
            layout->dst2   = (uint16_t*)(src + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);
         }
         layout->width  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->height = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_x= 0;
         layout->touch_y= GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;

         break;
   }
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   struct LayoutData layout;
   get_layout_params(current_layout, NULL, &layout);

   info->geometry.base_width   = layout.width;
   info->geometry.base_height  = layout.height;
   info->geometry.max_width    = layout.width * 2;
   info->geometry.max_height   = layout.height;
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
         current_layout = LAYOUT_BOTTOM_ONLY;
           current_screen = 2;
       }
       else
       {
         current_layout = LAYOUT_TOP_ONLY;
           current_screen = 1;
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

static void check_variables(bool first_boot)
{
    struct retro_variable var = {0};

   if (first_boot)
   {
      var.key = "desmume_internal_resolution";

      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
         char *pch;
         char str[100];
         snprintf(str, sizeof(str), "%s", var.value);

         pch = strtok(str, "x");
         if (pch)
            GPU_LR_FRAMEBUFFER_NATIVE_WIDTH = strtoul(pch, NULL, 0);
         pch = strtok(NULL, "x");
         if (pch)
            GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT = strtoul(pch, NULL, 0);
      }
   }
    
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
      static int old_layout_id      = -1;
      unsigned new_layout_id        = 0;

      quick_switch_enable = false;

        if (!strcmp(var.value, "top/bottom"))
         new_layout_id = LAYOUT_TOP_BOTTOM;
      else if (!strcmp(var.value, "bottom/top"))
         new_layout_id = LAYOUT_BOTTOM_TOP;
      else if (!strcmp(var.value, "left/right"))
         new_layout_id = LAYOUT_LEFT_RIGHT;
      else if (!strcmp(var.value, "right/left"))
         new_layout_id = LAYOUT_RIGHT_LEFT;
      else if (!strcmp(var.value, "top only"))
         new_layout_id = LAYOUT_TOP_ONLY;
      else if (!strcmp(var.value, "bottom only"))
         new_layout_id = LAYOUT_BOTTOM_ONLY;
      else if (!strcmp(var.value, "quick switch"))
      {
         new_layout_id = LAYOUT_QUICK_SWITCH;
         quick_switch_enable = true;
      }

      if (old_layout_id != new_layout_id)
      {
         old_layout_id = new_layout_id;
         current_layout = new_layout_id;
      }
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

   var.key = "desmume_gfx_txthack";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enable"))
         CommonSettings.GFX3D_TXTHack = true;
      else if (!strcmp(var.value, "disable"))
         CommonSettings.GFX3D_TXTHack = false;
   }
   else
      CommonSettings.GFX3D_TXTHack = false;

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

   var.key = "desmume_pointer_stylus_pressure";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      CommonSettings.StylusPressure = atoi(var.value);
   else
      CommonSettings.StylusPressure = 50;

   var.key = "desmume_pointer_stylus_jitter";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enable"))
         CommonSettings.StylusJitter = true;
      else if (!strcmp(var.value, "disable"))
         CommonSettings.StylusJitter = false;
   }
   else
      CommonSettings.StylusJitter = false;

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
      if ((atoi(var.value)) != nds_screen_gap)
      {
         nds_screen_gap = atoi(var.value);
         if (nds_screen_gap > 100)
            nds_screen_gap = 100;
      }
   }
}

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
      { "desmume_internal_resolution", "Internal resolution (restart); 256x192|512x384|768x576|1024x768|1280x960|1536x1152|1792x1344|2048x1536|320x240|320x480|360x480|400x400|512x224|512x448|640x224|640x448|640x480|800x600|960x720|1024x768|1280x800|1280x960|1600x1200|1920x1080" },
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
      { "desmume_pointer_stylus_pressure", "Emulated stylus pressure modifier percent; 50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|" },
      { "desmume_pointer_stylus_jitter", "Enable emulated stylus jitter; disable|enable"},
      { "desmume_load_to_memory", "Load Game into Memory (restart); disable|enable" },
      { "desmume_advanced_timing", "Enable Advanced Bus-Level Timing; enable|disable" },
      { "desmume_firmware_language", "Firmware language; English|Japanese|French|German|Italian|Spanish" },
      { "desmume_frameskip", "Frameskip; 0|1|2|3|4|5|6|7|8|9" },
      { "desmume_screens_gap", "Screen Gap; 0|5|64|90|0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100" },    
      { "desmume_gfx_edgemark", "Enable Edgemark; enable|disable" },
      { "desmume_gfx_linehack", "Enable Line Hack; enable|disable" },
      { "desmume_gfx_txthack", "Enable TXT Hack; disable|enable"},
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

    check_variables(true);

    // Init DeSmuME
    struct NDS_fw_config_data fw_config;
    NDS_FillDefaultFirmwareConfigData(&fw_config);
    fw_config.language = firmwareLanguage;


    //addonsChangePak(NDS_ADDON_NONE);
    NDS_Init();
    SPU_ChangeSoundCore(0, 735 * 2);
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

extern unsigned retro_audio_frames;

void retro_run (void)
{
   struct LayoutData layout;
   bool updated                  = false;    
   bool have_touch               = false;


   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated)
   {
      check_variables(false);
      struct retro_system_av_info new_av_info;
      retro_get_system_av_info(&new_av_info);

      environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &new_av_info);        
   }

   poll_cb();
   get_layout_params(current_layout, screen_buf, &layout);

   if(pointer_device != 0)
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
      log_cb(RETRO_LOG_DEBUG, "%d %d.\n", GPU_LR_FRAMEBUFFER_NATIVE_WIDTH,GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);
      log_cb(RETRO_LOG_DEBUG, "%d %d.\n", analogX,analogY);

      have_touch = have_touch || input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2); 

      TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), TouchX + analogX);
      TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), TouchY + analogY);

      FramesWithPointer = (analogX || analogY) ? FramesWithPointerBase : FramesWithPointer;

   }

   if(mouse_enable)
   {
      // TOUCH: Mouse
      if(!absolutePointer)
      {
         const int16_t mouseX = input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X);
         const int16_t mouseY = input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y);
         have_touch           = have_touch || input_cb(1, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT);

         TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), TouchX + mouseX);
         TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), TouchY + mouseY);
         FramesWithPointer = (mouseX || mouseY) ? FramesWithPointerBase : FramesWithPointer;
      }
      // TOUCH: Pointer
      else if(input_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED))
      {
         const float X_FACTOR = ((float)layout.width / 65536.0f);
         const float Y_FACTOR = ((float)layout.height / 65536.0f);

         float x = (input_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X) + 32768.0f) * X_FACTOR;
         float y = (input_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y) + 32768.0f) * Y_FACTOR;

         if ((x >= layout.touch_x) && (x < layout.touch_x + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH) &&
               (y >= layout.touch_y) && (y < layout.touch_y + GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT))
         {
            have_touch = true;

            TouchX = x - layout.touch_x;
            TouchY = y - layout.touch_y;
         }
      }
   }

   if(have_touch)
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

   retro_audio_frames = 0;

   // RUN
   frameIndex ++;
   bool skipped = frameIndex <= frameSkip;

   if (skipped)
      NDS_SkipNextFrame();

   NDS_exec<false>();
   SPU_Emulate_user();

   if (!skipped)
   {
      if (current_layout == LAYOUT_QUICK_SWITCH)
      {
         switch (current_screen)
         {
            case 1:
               layout.draw_screen1 = true;
               break;
            case 2:
               layout.draw_screen2 = true;
               break;
         }
      }

      if (layout.draw_screen1)
         SwapScreen (layout.dst,  &GPU_screen[0], layout.pitch);
      if (layout.draw_screen2)
      {
         SwapScreen (layout.dst2, &GPU_screen[GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT], layout.pitch);
         DrawPointer(layout.dst2, layout.pitch);
      }
   }

   video_cb(skipped ? 0 : screen_buf, layout.width, layout.height, layout.pitch * 2);

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
    savestate_save(&state);
    
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

   screen_buf = (uint16_t*)malloc(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT + NDS_MAX_SCREEN_GAP) * 2 * sizeof(uint16_t));

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
    if (screen_buf)
       free(screen_buf);
    screen_buf = NULL;
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
