#include <stdarg.h>
#include <libretro.h>

#if defined(VITA)
  int _newlib_heap_size_user = 128 * 1024 * 1024;
#endif

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

#define LAYOUTS_MAX 9

retro_log_printf_t log_cb = NULL;
static retro_video_refresh_t video_cb = NULL;
static retro_input_poll_t poll_cb = NULL;
static retro_input_state_t input_cb = NULL;
retro_audio_sample_batch_t audio_batch_cb = NULL;
retro_environment_t environ_cb = NULL;

volatile int execute = 0;

static int delay_timer = 0;
static bool quick_switch_enable = false;
static bool mouse_enable = false;
static int pointer_device_l = 0;
static int pointer_device_r = 0;
static int analog_stick_deadzone;
static int analog_stick_acceleration = 2048;
static int analog_stick_acceleration_modifier = 0;
static int microphone_force_enable = 0;
static int nds_screen_gap = 0;
static int hybrid_layout_scale = 1;
static bool hybrid_layout_showbothscreens = true;
static bool hybrid_cursor_always_smallscreen = true;
static uint16_t pointer_colour = 0xFFFF;

static uint16_t *screen_buf;

extern GPUSubsystem *GPU;

int currFrameCounter;

unsigned GPU_LR_FRAMEBUFFER_NATIVE_WIDTH  = 256;
unsigned GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT = 192;
unsigned scale = 1;

#define NDS_MAX_SCREEN_GAP               100

int current_layout = LAYOUT_TOP_BOTTOM;

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

static bool touchEnabled;

static unsigned host_get_language(void)
{
   static const u8 langconv[]={ // libretro to NDS
      NDS_FW_LANG_ENG,
      NDS_FW_LANG_JAP,
      NDS_FW_LANG_FRE,
      NDS_FW_LANG_SPA,
      NDS_FW_LANG_GER,
      NDS_FW_LANG_ITA
   };

   unsigned lang = RETRO_LANGUAGE_ENGLISH;
   environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &lang);
   if (lang >= 6) lang = RETRO_LANGUAGE_ENGLISH;
   return langconv[lang];
}

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
   for(int i = 0; i < (5 * scale) ; i ++)
      aOut[aPitchInPix * i] = pointer_colour;
}

static void DrawPointerLineSmall(uint16_t* aOut, uint32_t aPitchInPix, int factor)
{
   for(int i = 0; i < (factor * scale) ; i ++)
      aOut[aPitchInPix * i] = pointer_colour;
}

static void DrawPointer(uint16_t* aOut, uint32_t aPitchInPix)
{
   if(FramesWithPointer-- < 0)
      return;

   TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), TouchX);
   TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), TouchY);

   if(TouchX >   (5 * scale)) DrawPointerLine(&aOut[TouchY * aPitchInPix + TouchX - (5 * scale) ], 1);
   if(TouchX < (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH - (5 * scale) )) DrawPointerLine(&aOut[TouchY * aPitchInPix + TouchX + 1], 1);
   if(TouchY >   (5 * scale)) DrawPointerLine(&aOut[(TouchY - (5 * scale) ) * aPitchInPix + TouchX], aPitchInPix);
   if(TouchY < (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-(5 * scale) )) DrawPointerLine(&aOut[(TouchY + 1) * aPitchInPix + TouchX], aPitchInPix);
}

static void DrawPointerHybrid(uint16_t* aOut, uint32_t aPitchInPix, bool large)
{
   if(FramesWithPointer-- < 0)
      return;
	
	unsigned height,width;
	unsigned DrawX, DrawY;
   if(!large)
   {  
	int addgap;
	if(nds_screen_gap >= hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3)
	   addgap = hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3-1;
   	else
		addgap = nds_screen_gap;
	aOut += hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3 + addgap)*hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3);
	height = hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3;
	int awidth = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3;
		width = hybrid_layout_scale*awidth;
		DrawX = Saturate(0, (width-1), TouchX);
		DrawY = Saturate(0, (height-1), TouchY);
   }
   else{
	   height = hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
		width = hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
		DrawX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), TouchX);
		DrawY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), TouchY);
   }   
   int factor;
   if(large)
   {
	   factor = 5*hybrid_layout_scale;
	   if(hybrid_layout_scale == 3)
	   {
		   DrawX = 3*DrawX;
		   DrawY = 3*DrawY;
	   }
   }
   else if(hybrid_layout_scale == 3)
	   factor = 6;
   else
	   factor = 3; 
   if(DrawX >   (factor * scale)) DrawPointerLineSmall(&aOut[DrawY * aPitchInPix + DrawX - (factor * scale) ], 1, factor);
   if(DrawX < (width - (factor * scale) )) DrawPointerLineSmall(&aOut[DrawY * aPitchInPix + DrawX + 1], 1, factor);
   if(DrawY >   (factor * scale)) DrawPointerLineSmall(&aOut[(DrawY - (factor * scale) ) * aPitchInPix + DrawX], aPitchInPix, factor);
   if(DrawY < (height-(factor * scale) )) DrawPointerLineSmall(&aOut[(DrawY + 1) * aPitchInPix + DrawX], aPitchInPix, factor);
}



static retro_pixel_format colorMode;
static uint32_t frameSkip;
static uint32_t frameIndex;

#define CONVERT_COLOR(color) (((color & 0x001f) << 11) | ((color & 0x03e0) << 1) | ((color & 0x0200) >> 4) | ((color & 0x7c00) >> 10))

bool Resample_Screen(int w1, int h1, bool shrink, const uint16_t *old, uint16_t *ret)
    {
		int w2, h2, x2, y2 ;
		if(shrink)
		{
			w2 = w1/3;
			h2 = h1/3;
		}
		else
		{
			w2 = w1*3;
			h2 = h1*3;
		}
			
		for (int i=0;i<h2;i++) 
		{
			for (int j=0;j<w2;j++) 
			{
				if(shrink){
					x2 = j*3;
					y2 = i*3;
				}
				else{
					x2 = j/3;
					y2 = i/3;
				}
				ret[(i*w2)+j] = old[(y2*w1)+x2] ;
			}                
		}                
        return true;
    }

static void BlankScreenSmallSection(uint16_t *pt1, const uint16_t *pt2){
	//Ensures above the hybrid screens is blank - If someone changes screen layout, stuff will be leftover otherwise
	unsigned i;
	pt1 += hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
	while( pt1 < pt2)
	{
		int awidth = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3;
		memset(pt1, 0, hybrid_layout_scale*awidth*sizeof(uint16_t));
		pt1 += hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3 + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH);
	}
}

static void SwapScreen(uint16_t *dst, const uint16_t *src, uint32_t pitch)
{
   unsigned i, j;
   uint32_t skip = pitch - GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;

   for(i = 0; i < GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT; i ++)
   {
      for(j = 0; j < GPU_LR_FRAMEBUFFER_NATIVE_WIDTH; j ++)
      {
         uint16_t col = *src++;
         *dst++ = CONVERT_COLOR(col);
      }
      dst += skip;
   }
}

static void SwapScreenLarge(uint16_t *dst, const uint16_t *src, uint32_t pitch)
{
	/*
	This method uses Nearest Neighbour to resize the primary screen to 3 times its original width and 3 times its original height.
	It is a lot faster than the previous method. If we want to apply some different method of scaling this needs to change.
	*/
	unsigned i, j, k;
	uint32_t skip = pitch - hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
		
	unsigned heightlimit = hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
	for(i = 0; i < heightlimit; i ++)
   {
	  if( i%hybrid_layout_scale != 0)
		  src -= GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
      for(j = 0; j < GPU_LR_FRAMEBUFFER_NATIVE_WIDTH; j ++)
      {
		 uint16_t col = *src++;
		 for(k = 0; k < hybrid_layout_scale; ++k)
			*dst++ = CONVERT_COLOR(col);
      }
      dst += skip;
   }
}

static void SwapScreenSmall(uint16_t *dst, const uint16_t *src, uint32_t pitch, bool first, bool draw)
{
   unsigned i, j;
	int addgap;
	if(nds_screen_gap >= hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3)
		addgap = hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3 - 1;
	else
		addgap = nds_screen_gap;
	//If it is the bottom screen, start drawing lower down.
	if(!first)
	{
		dst += hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3)*hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3);
		//Make Sure The Screen Gap is Empty
		for(i=0; i< addgap; ++i)
		{
			memset(dst, 0, hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3*sizeof(uint16_t));
			dst += hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3 + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH);
		}
	}
	
	if(hybrid_layout_scale != 3)
	{
		//Shrink to 1/3 the width and 1/3 the height
		uint16_t *resampl;
		resampl = (uint16_t*)malloc(GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/9*sizeof(uint16_t));
		Resample_Screen(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH, GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT, true, src, resampl);
		
		for(i=0; i<GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3; ++i)
		{
			if(draw)
			{
				for(j=0; j<GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3; ++j)
					*dst++ = CONVERT_COLOR(resampl[i*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3)+j]);
			}
			else
			{
				memset(dst, 0, GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3*sizeof(uint16_t));
				dst += GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3;
			}
			dst += GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
		}
		free(resampl);
	}
	else
	{
		uint32_t skip = pitch - GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
		for(i=0; i<GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT; ++i)
		{
			if(draw)
			{
				for(j=0; j<GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1; ++j)
				{	
					uint16_t col = *src++;
					*dst++ = CONVERT_COLOR(col);
				}
			}
			else
			{
				memset(dst, 0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1)*sizeof(uint16_t));
				dst += GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1;
			}
			//Cuts off last pixel in width, because 3 does not divide native_width evenly. This prevents overwriting some of the main screen
			*src++; *dst++;
			dst += skip;
		}
	}
	//Make Sure underneath the Screens is Empty. Fixes leftovers when changing screen layout
	if(!first){
		int endheight = hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3 - addgap;
		for(i=0; i<endheight; ++i){
			memset(dst, 0, hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3*sizeof(uint16_t));
			dst += hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3 + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH);
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
#ifdef GIT_VERSION
   info->library_version = "git" GIT_VERSION;
#else
   info->library_version = "SVN";
#endif
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
      case LAYOUT_HYBRID_TOP_ONLY:
	  {
		  int awidth = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3;
         if (src)
         {
            layout->dst    = src;
			int addgap;
			if(nds_screen_gap >= hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3)
				addgap = hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3-1;
			else
				addgap = nds_screen_gap;
			layout->dst2   = (uint16_t*)(src + hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3)*( hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/6 - addgap/2) - hybrid_layout_scale*awidth);
         }
         layout->width  = hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + awidth);
         layout->height = hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + awidth);
		 //What should these touch values be?
         layout->touch_x= GPU_LR_FRAMEBUFFER_NATIVE_WIDTH;
         layout->touch_y= 0;

         layout->draw_screen1  = true;
         layout->draw_screen2  = false;
	  }
         break;
      case LAYOUT_HYBRID_BOTTOM_ONLY:
		{
			int awidth = GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3;
         if (src)
         {
			int addgap;
			if(nds_screen_gap >= GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3)
				addgap = GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/3-1;
			else
				addgap = nds_screen_gap;
            layout->dst   = (uint16_t*)(src + hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/3)*( hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/6 - addgap/2) - hybrid_layout_scale*awidth);
			layout->dst2  = src;
         }
         layout->width  = hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + awidth);
         layout->height = hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
         layout->pitch  = hybrid_layout_scale*(GPU_LR_FRAMEBUFFER_NATIVE_WIDTH + awidth);
		 //What should these touch values be?
         layout->touch_x= 0;
         layout->touch_y= hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
		}

         layout->draw_screen1  = false;
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

         switch (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH)
         {
            case 256:
               scale = 1;
               break;
            case 512:
               scale = 2;
               break;
            case 768:
               scale = 3;
               break;
            case 1024:
               scale = 4;
               break;
            case 1280:
               scale = 5;
               break;
            case 1536:
               scale = 6;
               break;
            case 1792:
               scale = 7;
               break;
            case 2048:
               scale = 8;
               break;
            case 2304:
               scale = 9;
               break;
            case 2560:
               scale = 10;
               break;
         }
      }
		//This needs to be on first boot only as it affects the screen_buf size, unless want to realloc
	     var.key = "desmume_hybrid_layout_scale";

		if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			if ((atoi(var.value)) != hybrid_layout_scale)
			{
				hybrid_layout_scale = atoi(var.value);
				if (hybrid_layout_scale != 1 && hybrid_layout_scale != 3)
					hybrid_layout_scale = 1;
			}
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
	   else if(!strcmp(var.value, "hybrid/top"))
	   {
		  new_layout_id = LAYOUT_HYBRID_TOP_ONLY;
		  quick_switch_enable = true;
	   }
	   else if(!strcmp(var.value, "hybrid/bottom"))
	   {
		  new_layout_id = LAYOUT_HYBRID_BOTTOM_ONLY;
		  quick_switch_enable = true;
	   }
       else if (!strcmp(var.value, "quick switch"))
       {
          new_layout_id = LAYOUT_TOP_ONLY;
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
        if (!strcmp(var.value, "enabled"))
            mouse_enable = true;
      else if (!strcmp(var.value, "disabled"))
            mouse_enable = false;
    }
   else
      mouse_enable = false;

    var.key = "desmume_pointer_device_l";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (!strcmp(var.value, "emulated"))
            pointer_device_l = 1;
        else if(!strcmp(var.value, "absolute"))
            pointer_device_l = 2;
		else if (!strcmp(var.value, "pressed"))
			pointer_device_l = 3;
        else
            pointer_device_l=0;
    }
   else
      pointer_device_l=0;

    var.key = "desmume_pointer_device_r";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        if (!strcmp(var.value, "emulated"))
            pointer_device_r = 1;
        else if(!strcmp(var.value, "absolute"))
            pointer_device_r = 2;
		else if (!strcmp(var.value, "pressed"))
			pointer_device_r = 3;
        else
            pointer_device_r=0;
    }
   else
      pointer_device_r=0;

    var.key = "desmume_pointer_device_deadzone";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      analog_stick_deadzone = (int)(atoi(var.value));

    var.key = "desmume_pointer_type";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        touchEnabled = var.value && (!strcmp(var.value, "touch"));
    }

    var.key = "desmume_frameskip";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
        frameSkip = var.value ? strtol(var.value, 0, 10) : 0;
   else
      frameSkip = 0;

    var.key = "desmume_firmware_language";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    {
        static const struct { const char* name; int id; } languages[] =
        {
            { "Auto", -1 },
            { "Japanese", 0 },
            { "English", 1 },
            { "French", 2 },
            { "German", 3 },
            { "Italian", 4 },
            { "Spanish", 5 }
        };

        for (int i = 0; i < 7; i ++)
        {
            if (!strcmp(languages[i].name, var.value))
            {
                firmwareLanguage = languages[i].id;
                if (firmwareLanguage == -1) firmwareLanguage = host_get_language();
                break;
            }
        }
    }
   else
      firmwareLanguage = 1;


   var.key = "desmume_gfx_edgemark";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
        if (!strcmp(var.value, "enabled"))
         CommonSettings.GFX3D_EdgeMark = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.GFX3D_EdgeMark = false;
   }
   else
      CommonSettings.GFX3D_EdgeMark = true;

   var.key = "desmume_gfx_linehack";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.GFX3D_LineHack = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.GFX3D_LineHack = false;
   }
   else
      CommonSettings.GFX3D_LineHack = true;

   var.key = "desmume_gfx_txthack";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.GFX3D_TXTHack = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.GFX3D_TXTHack = false;
   }
   else
      CommonSettings.GFX3D_TXTHack = false;

   var.key = "desmume_mic_force_enable";

   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         microphone_force_enable = 1;
      else if(!strcmp(var.value, "disabled"))
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
      if (!strcmp(var.value, "enabled"))
         CommonSettings.StylusJitter = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.StylusJitter = false;
   }
   else
      CommonSettings.StylusJitter = false;

   var.key = "desmume_load_to_memory";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.loadToMemory = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.loadToMemory = false;
   }
   else
      CommonSettings.loadToMemory = false;

   var.key = "desmume_advanced_timing";

    if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         CommonSettings.advanced_timing = true;
      else if (!strcmp(var.value, "disabled"))
         CommonSettings.advanced_timing = false;
   }
   else
      CommonSettings.advanced_timing = true;
   
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
   
  var.key = "desmume_hybrid_showboth_screens";
   
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         hybrid_layout_showbothscreens = true;
      else if(!strcmp(var.value, "disabled"))
         hybrid_layout_showbothscreens = false;
   }
   else
      hybrid_layout_showbothscreens = true;
  
    var.key = "desmume_hybrid_cursor_always_smallscreen";
   
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strcmp(var.value, "enabled"))
         hybrid_cursor_always_smallscreen = true;
      else if(!strcmp(var.value, "disabled"))
         hybrid_cursor_always_smallscreen = false;
   }
   else
      hybrid_cursor_always_smallscreen = true;
  

   var.key = "desmume_pointer_colour";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
	  if(!strcmp(var.value, "white"))
		 pointer_colour = 0xFFFF;
      else if (!strcmp(var.value, "black"))
         pointer_colour = 0x0000;
	  else if(!strcmp(var.value, "red"))
		 pointer_colour = 0xF800;
	  else if(!strcmp(var.value, "yellow"))
		 pointer_colour = 0xFFE0;
	  else if(!strcmp(var.value, "blue"))
		 pointer_colour = 0x001F;
	  else
		 pointer_colour = 0xFFFF;
   }
   else{
	   pointer_colour = 0xFFFF;
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
      { "desmume_internal_resolution", "Internal resolution (restart); 256x192|512x384|768x576|1024x768|1280x960|1536x1152|1792x1344|2048x1536|2304x1728|2560x1920" },
      { "desmume_num_cores", "CPU cores; 1|2|3|4" },
#ifdef HAVE_JIT
#if defined(IOS) || defined(ANDROID)
      { "desmume_cpu_mode", "CPU mode; interpreter|jit" },
#else
      { "desmume_cpu_mode", "CPU mode; jit|interpreter" },
#endif
      { "desmume_jit_block_size", "JIT block size; 12|11|10|9|8|7|6|5|4|3|2|1|0|100|99|98|97|96|95|94|93|92|91|90|89|88|87|86|85|84|83|82|81|80|79|78|77|76|75|74|73|72|71|70|69|68|67|66|65|64|63|62|61|60|59|58|57|56|55|54|53|52|51|50|49|48|47|46|45|44|43|42|41|40|39|38|37|36|35|34|33|32|31|30|29|28|27|26|25|24|23|22|21|20|19|18|17|16|15|14|13" },
#else
      { "desmume_cpu_mode", "CPU mode; interpreter" },
#endif
      { "desmume_screens_layout", "Screen layout; top/bottom|bottom/top|left/right|right/left|top only|bottom only|quick switch|hybrid/top|hybrid/bottom" },
	  { "desmume_hybrid_layout_scale", "Hybrid layout scale (restart); 1|3"},
	  { "desmume_hybrid_showboth_screens", "Hybrid layout show both screens; enabled|disabled"},
	  { "desmume_hybrid_cursor_always_smallscreen", "Hybrid layout cursor always on small screen; enabled|disabled"},
      { "desmume_pointer_mouse", "Enable mouse/pointer; enabled|disabled" },
      { "desmume_pointer_type", "Pointer type; mouse|touch" },
	  { "desmume_pointer_colour", "Pointer Colour; white|black|red|blue|yellow"},
      { "desmume_pointer_device_l", "Pointer mode l-analog; none|emulated|absolute|pressed" },
      { "desmume_pointer_device_r", "Pointer mode r-analog; none|emulated|absolute|pressed" },
      { "desmume_pointer_device_deadzone", "Emulated pointer deadzone percent; 15|20|25|30|35|0|5|10" },
      { "desmume_pointer_device_acceleration_mod", "Emulated pointer acceleration modifier percent; 0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100" },
      { "desmume_pointer_stylus_pressure", "Emulated stylus pressure modifier percent; 50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100|0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|" },
      { "desmume_pointer_stylus_jitter", "Enable emulated stylus jitter; disabled|enabled"},
      { "desmume_load_to_memory", "Load Game into Memory (restart); disabled|enabled" },
      { "desmume_advanced_timing", "Enable Advanced Bus-Level Timing; enabled|disabled" },
      { "desmume_firmware_language", "Firmware language; Auto|English|Japanese|French|German|Italian|Spanish" },
      { "desmume_frameskip", "Frameskip; 0|1|2|3|4|5|6|7|8|9" },
      { "desmume_screens_gap", "Screen Gap; 0|5|64|90|0|1|2|3|4|5|6|7|8|9|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|32|33|34|35|36|37|38|39|40|41|42|43|44|45|46|47|48|49|50|51|52|53|54|55|56|57|58|59|60|61|62|63|64|65|66|67|68|69|70|71|72|73|74|75|76|77|78|79|80|81|82|83|84|85|86|87|88|89|90|91|92|93|94|95|96|97|98|99|100" },
      { "desmume_gfx_edgemark", "Enable Edgemark; enabled|disabled" },
      { "desmume_gfx_linehack", "Enable Line Hack; enabled|disabled" },
      { "desmume_gfx_txthack", "Enable TXT Hack; disabled|enabled"},
      { "desmume_mic_force_enable", "Force Microphone Enable; disabled|enabled" },
      { "desmume_mic_mode", "Microphone Simulation Settings; internal|sample|random|physical" },
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

   if(pointer_device_l != 0 || pointer_device_r != 0)  // 1=emulated pointer, 2=absolute pointer, 3=absolute pointer constantly pressed
   {
        int16_t analogX_l = 0;
        int16_t analogY_l = 0;           
        int16_t analogX_r = 0;
        int16_t analogY_r = 0;
        int16_t analogX = 0;
        int16_t analogY = 0;        
        int16_t analogXpointer = 0;
        int16_t analogYpointer = 0;
        
        //emulated pointer on one or both sticks
        //just prioritize the stick that has a higher radius
        if((pointer_device_l == 1) || (pointer_device_r == 1))
        {
            double radius = 0;
            double angle = 0;
            float final_acceleration = analog_stick_acceleration * (1.0 + (float)analog_stick_acceleration_modifier / 100.0);
            
            if((pointer_device_l == 1) && (pointer_device_r == 1))
            {
                analogX_l = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) /  final_acceleration;
                analogY_l = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / final_acceleration;
                analogX_r = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) /  final_acceleration;
                analogY_r = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / final_acceleration;
                
                double radius_l = sqrt(analogX_l * analogX_l + analogY_l * analogY_l);
                double radius_r = sqrt(analogX_r * analogX_r + analogY_r * analogY_r);

                if(radius_l > radius_r)
                {
                    radius = radius_l;
                    angle = atan2(analogY_l, analogX_l);
                    analogX = analogX_l;
                    analogY = analogY_l;
                }
                else
                {
                    radius = radius_r;
                    angle = atan2(analogY_r, analogX_r);
                    analogX = analogX_r;
                    analogY = analogY_r;
                }
            }
            
            else if(pointer_device_l == 1)             
            {
                analogX = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / final_acceleration;
                analogY = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / final_acceleration;
                radius = sqrt(analogX * analogX + analogY * analogY);
                angle = atan2(analogY, analogX);
            }
            else
            {
                analogX = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / final_acceleration;
                analogY = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / final_acceleration;
                radius = sqrt(analogX * analogX + analogY * analogY);
                angle = atan2(analogY, analogX);
            }    
     
            // Convert cartesian coordinate analog stick to polar coordinates
            double max = (float)0x8000/analog_stick_acceleration;

            //log_cb(RETRO_LOG_DEBUG, "%d %d.\n", analogX,analogY);
            //log_cb(RETRO_LOG_DEBUG, "%d %d.\n", radius,analog_stick_deadzone);
            if(radius > (float)analog_stick_deadzone*max/100)
            {
                // Re-scale analog stick range to negate deadzone (makes slow movements possible)
                radius = (radius - (float)analog_stick_deadzone*max/100)*((float)max/(max - (float)analog_stick_deadzone*max/100));

                // Convert back to cartesian coordinates
                analogXpointer = (int32_t)round(radius * cos(angle));
                analogYpointer = (int32_t)round(radius * sin(angle));
                
                TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), TouchX + analogXpointer);
                TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), TouchY + analogYpointer);

            
            }
            
        }

        //absolute pointer -- doesn't run if emulated pointer > deadzone 
        if(((pointer_device_l == 2) || (pointer_device_l == 3) || (pointer_device_r == 2) || (pointer_device_r == 3)) && !(analogXpointer || analogYpointer))
        {
            if(((pointer_device_l == 2) || (pointer_device_l == 3)) && ((pointer_device_r == 2) || (pointer_device_r == 3))) //both sticks set to absolute or pressed
            {
                
                if(pointer_device_l == 3) //left analog is always pressed
                {
                    int16_t analogXpress = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
                    int16_t analogYpress = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
                    
                    double radius = sqrt(analogXpress * analogXpress + analogYpress * analogYpress);
                    
                    //check if analog exceeds deadzone
                    if (radius > (float)analog_stick_deadzone*0x8000/100)
                    {
                        have_touch = 1;
                        
                        //scale analog position to ellipse enclosing framebuffer rectangle
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogXpress / (float)0x8000; 
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogYpress / (float)0x8000;

                    }
                    else if (pointer_device_r == 2) //use the other stick as absolute
                    {
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / (float)0x8000; 
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / (float)0x8000; 
                    }                        
                }
                
                else if(pointer_device_r == 3) // right analog is always pressed
                {
                    int16_t analogXpress = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
                    int16_t analogYpress = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
                    
                    double radius = sqrt(analogXpress * analogXpress + analogYpress * analogYpress);
                    
                    if (radius > (float)analog_stick_deadzone*0x8000/100)
                    {
                        have_touch = 1;
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogXpress / (float)0x8000; 
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogYpress / (float)0x8000;

                    } 
                    else if (pointer_device_l == 2)
                    {
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X) / (float)0x8000; 
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y) / (float)0x8000; 
                    }                        
         
                }
                else //right analog takes priority when both set to absolute
                {
                    analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X) / (float)0x8000; 
                    analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y) / (float)0x8000; 
                }
                            
                //set absolute analog position offset to center of screen
                TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), analogX + ((GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1) / 2));
                TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), analogY + ((GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1) / 2));
            }
 
            else if((pointer_device_l == 2) || (pointer_device_l == 3))
            {
                if(pointer_device_l == 2)
                {
                    analogX = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X); 
                    analogY = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
                    analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogX / (float)0x8000; 
                    analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogY / (float)0x8000;
                    
                    TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), analogX + ((GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1) / 2));
                    TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), analogY + ((GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1) / 2));                 

                    
                }
                if(pointer_device_l == 3)
                {
                    int16_t analogXpress = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
                    int16_t analogYpress = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
                    double radius = sqrt(analogXpress * analogXpress + analogYpress * analogYpress);   
                    if (radius > (float)analog_stick_deadzone*(float)0x8000/100)
                    {
                        have_touch = 1;
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogXpress / (float)0x8000; 
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogYpress / (float)0x8000;
                        
                        TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), analogX + ((GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1) / 2));
                        TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), analogY + ((GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1) / 2));                 
                    }
                }
            }

            else
            {
                if(pointer_device_r == 2)
                {
                    analogX = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X); 
                    analogY = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
                    analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogX / (float)0x8000; 
                    analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogY / (float)0x8000;
                    
                    TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), analogX + ((GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1) / 2));
                    TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), analogY + ((GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1) / 2));                        
                }
                
                if(pointer_device_r == 3)
                {
                    int16_t analogXpress = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
                    int16_t analogYpress = input_cb(0, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
                    double radius = sqrt(analogXpress * analogXpress + analogYpress * analogYpress);   
                    if (radius > (float)analog_stick_deadzone*(float)0x8000/100)
                    {
                        have_touch = 1;
                        analogX = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH/2*analogXpress / (float)0x8000; 
                        analogY = sqrt(2)*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT/2*analogYpress / (float)0x8000;
                        
                        TouchX = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1), analogX + ((GPU_LR_FRAMEBUFFER_NATIVE_WIDTH-1) / 2));
                        TouchY = Saturate(0, (GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1), analogY + ((GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT-1) / 2));                 
                        
                    }
                }            
            }
        }    
     
        //log_cb(RETRO_LOG_DEBUG, "%d %d.\n", GPU_LR_FRAMEBUFFER_NATIVE_WIDTH,GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);
        //log_cb(RETRO_LOG_DEBUG, "%d %d.\n", analogX,analogY);

        have_touch = have_touch || input_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2);

        FramesWithPointer = (analogX || analogY) ? FramesWithPointerBase : FramesWithPointer;
    
   }

   if(mouse_enable)
   {
      // TOUCH: Mouse
      if(!touchEnabled)
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
   {
	   //Hybrid layout requires "rescaling" of coordinates. No idea how this works on actual touch screen - tested on PC with mousepad and emulated gamepad
	if(current_layout == LAYOUT_HYBRID_TOP_ONLY || (current_layout == LAYOUT_HYBRID_BOTTOM_ONLY))
	{
		if( (current_layout == LAYOUT_HYBRID_BOTTOM_ONLY) && hybrid_layout_scale == 1 && ( !hybrid_cursor_always_smallscreen || !hybrid_layout_showbothscreens))
			NDS_setTouchPos(TouchX, TouchY, scale);
		else
			NDS_setTouchPos(TouchX*3/hybrid_layout_scale, TouchY*3/hybrid_layout_scale, scale);
	}
	else
		NDS_setTouchPos(TouchX, TouchY, scale);
   }
   else
      NDS_releaseTouch();
  

   // BUTTONS
   //NDS_beginProcessingInput();

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
      switch (current_layout)
      {
         case LAYOUT_TOP_ONLY:
            current_layout = LAYOUT_BOTTOM_ONLY;
            break;
         case LAYOUT_BOTTOM_ONLY:
            current_layout = LAYOUT_TOP_ONLY;
            break;
		case LAYOUT_HYBRID_TOP_ONLY:
			current_layout = LAYOUT_HYBRID_BOTTOM_ONLY;
			{
				//Need to swap around DST variables
				uint16_t*swap = layout.dst;
				layout.dst = layout.dst2;
				layout.dst2 = swap;
				//Need to reset Touch position to 0 with these conditions or it causes problems with mouse
				if(hybrid_layout_scale == 1 && (!hybrid_layout_showbothscreens || !hybrid_cursor_always_smallscreen))
				{   
					TouchX = 0;
					TouchY = 0;
				}
			}
			break;
		case LAYOUT_HYBRID_BOTTOM_ONLY:
			current_layout = LAYOUT_HYBRID_TOP_ONLY;
			{
				uint16_t*swap = layout.dst;
				layout.dst = layout.dst2;
				layout.dst2 = swap;
				//Need to reset Touch position to 0 with these conditions are it causes problems with mouse
				if(hybrid_layout_scale == 1 && (!hybrid_layout_showbothscreens || !hybrid_cursor_always_smallscreen))
				{
					TouchX = 0;
					TouchY = 0;
				}
					
			}
			break;
      }
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

   NDS_exec();
   SPU_Emulate_user();

   if (!skipped)
   {
	  if (current_layout == LAYOUT_HYBRID_TOP_ONLY || current_layout == LAYOUT_HYBRID_BOTTOM_ONLY)
	  {
		u16 *screen = GPU->GetCustomFramebuffer();
		if (current_layout == LAYOUT_HYBRID_TOP_ONLY)
		{
			if(hybrid_layout_scale == 3)
				SwapScreenLarge(layout.dst,  screen, layout.pitch);
			else
				SwapScreen (layout.dst,  screen, layout.pitch);
			BlankScreenSmallSection(layout.dst, layout.dst2);
			SwapScreenSmall(layout.dst2, screen, layout.pitch, true, hybrid_layout_showbothscreens);
		}
		else if (current_layout == LAYOUT_HYBRID_BOTTOM_ONLY)
			SwapScreenSmall(layout.dst, screen, layout.pitch, true, true);
	 
		screen = GPU->GetCustomFramebuffer() + (GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT);
		if (current_layout == LAYOUT_HYBRID_BOTTOM_ONLY)
		{
			if(hybrid_layout_scale == 3)
				SwapScreenLarge(layout.dst2,  screen, layout.pitch);
			else
				SwapScreen (layout.dst2,  screen, layout.pitch);
			BlankScreenSmallSection(layout.dst2, layout.dst);
			SwapScreenSmall (layout.dst, screen, layout.pitch, false , hybrid_layout_showbothscreens);
			//Keep the Touch Cursor on the Small Screen, even if the bottom is the primary screen? Make this configurable by user? (Needs work to get working with hybrid_layout_scale==3 and layout_hybrid_bottom_only)
			if(hybrid_cursor_always_smallscreen && hybrid_layout_showbothscreens)
				DrawPointerHybrid (layout.dst, layout.pitch, false);
			else
				DrawPointerHybrid (layout.dst2, layout.pitch, true);
		}
		else
		{
			SwapScreenSmall (layout.dst2, screen, layout.pitch, false, true);
			DrawPointerHybrid (layout.dst2, layout.pitch, false);
		}
	  }
	  //This is for every layout except Hybrid - same as before
	  else
	  {
		u16 *screen = GPU->GetCustomFramebuffer();
		if (layout.draw_screen1)
			SwapScreen (layout.dst,  screen, layout.pitch);
		if (layout.draw_screen2)
		{
			screen = GPU->GetCustomFramebuffer() + GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT;
			SwapScreen (layout.dst2, screen, layout.pitch);
			DrawPointer(layout.dst2, layout.pitch);
		}
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
   if (!game || colorMode != RETRO_PIXEL_FORMAT_RGB565)
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
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2,     "Tap Stylus" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3,     "Quick Screen Switch" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START,  "Start" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT,  "Select" },

      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   execute = NDS_LoadROM(game->path);

   if (execute == -1)
      return false;

   screen_buf = (uint16_t*)malloc(hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_WIDTH * (hybrid_layout_scale*GPU_LR_FRAMEBUFFER_NATIVE_HEIGHT + NDS_MAX_SCREEN_GAP) * 2 * sizeof(uint16_t));

   return true;
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
    execute    = 0;
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

#if defined(PSP)
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
