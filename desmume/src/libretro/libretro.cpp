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
#include "GPU_osd.h"

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

#ifdef X432R_CUSTOMRENDERER_ENABLED
static bool highres_enabled = false;
static int internal_res = 1;

namespace X432R
{
	#ifdef X432R_CUSTOMRENDERER_DEBUG
	bool debugModeEnabled = true;
	#endif
	
	CRITICAL_SECTION customFrontBufferSync;
#ifdef HAVE_OPENGL
//	static GLuint screenTexture[2] = {0};
	static GLuint screenTexture = 0;
	static GLuint hudTexture = 0;
#endif
	static u32 lastRenderMagnification = 1;
	
	HighResolutionFramebuffers backBuffer;
	static u32 frontBuffer[3][1024 * 768 * 2] = {0};
	static u32 masterBrightness[3][2] = {0};
	static bool isHighResolutionScreen[3][2] = {0};
	static u32 hudBuffer[GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT * 2] = {0};
	
	static volatile bool screenTextureUpdated = false;
	
	
	void ClearBuffers()
	{
		Lock lock(customFrontBufferSync);
		
		backBuffer.Clear();
		
		memset( frontBuffer, 0xFF, sizeof(frontBuffer) );
		memset( masterBrightness, 0, sizeof(masterBrightness) );
		memset( isHighResolutionScreen, 0, sizeof(isHighResolutionScreen) );
		
		screenTextureUpdated = false;
	}
	
	
	inline bool IsHighResolutionRendererSelected()
	{
		switch(cur3DCore)
		{
			case GPU3D_SWRAST_X2:
			case GPU3D_SWRAST_X3:
			case GPU3D_SWRAST_X4:
			case GPU3D_OPENGL_X2:
			case GPU3D_OPENGL_X3:
			case GPU3D_OPENGL_X4:
				return true;
		}
		
		return false;
	}
	
	inline bool IsSoftRasterzierSelected()
	{
		switch(cur3DCore)
		{
			case GPU3D_SWRAST:
			case GPU3D_SWRAST_X2:
			case GPU3D_SWRAST_X3:
			case GPU3D_SWRAST_X4:
				return true;
		}
		
		return false;
	}
	
   inline u32 GetCurrentRenderMagnification()
   {
      return internal_res;
   }
	
	
	static inline void Lock_forHighResolutionFrontBuffer()
	{
      /* TODO/FIXME - just omit for now */
		//EnterCriticalSection(&customFrontBufferSync);
	}
	
	static inline void Unlock_forHighResolutionFrontBuffer()
	{
      /* TODO/FIXME - just omit for now */
		//LeaveCriticalSection(&customFrontBufferSync);
	}
	
	#ifdef X432R_CUSTOMRENDERER_DEBUG
	void ShowDebugMessage(std::string message)
	{
		if( (osd == NULL) || !debugModeEnabled ) return;
		
		osd->setLineColor(0xFF, 0x80, 0);
		osd->addLine( message.c_str() );
		osd->setLineColor(0xFF, 0xFF, 0xFF);
	}
	#endif
	
	
	static inline void UpdateWindowCaptionFPS(const u32 fps, const u32 fps3d)
	{
	}
	
	
	//---------- MainThreadp ----------
	
	static void UpdateFrontBuffer()
	{
		static bool buffers_cleared = false;
		
		if( !IsHighResolutionRendererSelected() || !romloaded || finished || display_die )
		{
			if( !buffers_cleared )
			{
				ClearBuffers();
				
				buffers_cleared = true;
			}
			
			return;
		}
		
		buffers_cleared = false;
		
		Lock lock(customFrontBufferSync);		// XR[vð²¯Ä±ÌCX^XÌfXgN^ªÄÎêéÆ©®IÉbNªð³êé
		
		const u8 buffer_index = CommonSettings.single_core() ? 0 : clamp(newestDisplayBuffer, 0, 2);
		
		u32 * const front_buffer = frontBuffer[buffer_index];
		u32 * const master_brightness = masterBrightness[buffer_index];
		bool * const is_highreso_screen = isHighResolutionScreen[buffer_index];
		
		switch(cur3DCore)
		{
			case GPU3D_SWRAST_X2:
#ifdef HAVE_OPENGL
			case GPU3D_OPENGL_X2:
#endif
				backBuffer.UpdateFrontBufferAndDisplayCapture<2>(front_buffer, master_brightness, is_highreso_screen);
				break;
			
			case GPU3D_SWRAST_X3:
#ifdef HAVE_OPENGL
			case GPU3D_OPENGL_X3:
#endif
				backBuffer.UpdateFrontBufferAndDisplayCapture<3>(front_buffer, master_brightness, is_highreso_screen);
				break;
			
			case GPU3D_SWRAST_X4:
#ifdef HAVE_OPENGL
			case GPU3D_OPENGL_X4:
#endif
				backBuffer.UpdateFrontBufferAndDisplayCapture<4>(front_buffer, master_brightness, is_highreso_screen);
				break;
		}
		
		screenTextureUpdated = false;
	}
	
	static void ChangeRenderMagnification(u32 magnification)
	{
		if(magnification < 1)
			magnification = 1;
		
		else if(magnification > 4)
			magnification = 4;
		
		bool softrast = false;
		
		switch(cur3DCore)
		{
			case GPU3D_SWRAST:
			case GPU3D_SWRAST_X2:
			case GPU3D_SWRAST_X3:
			case GPU3D_SWRAST_X4:
				softrast = true;
				break;
		}
		
		switch(magnification)
		{
			case 2:
				cur3DCore = softrast ? GPU3D_SWRAST_X2 : GPU3D_OPENGL_X2;
				break;
			
			case 3:
				cur3DCore = softrast ? GPU3D_SWRAST_X3 : GPU3D_OPENGL_X3;
				break;
			
			case 4:
				cur3DCore = softrast ? GPU3D_SWRAST_X4 : GPU3D_OPENGL_X4;
				break;
			
			default:
				cur3DCore = softrast ? GPU3D_SWRAST : GPU3D_OPENGL_3_2;
				break;
		}
		
		Change3DCoreWithFallbackAndSave(cur3DCore);
	}
	
	
	//---------- DisplayThreadp ----------
	
	static inline bool IsHudVisible()
	{
		if(osd == NULL) return false;
		
		return
		(
			CommonSettings.hud.ShowInputDisplay ||
			CommonSettings.hud.ShowGraphicalInputDisplay ||
			CommonSettings.hud.FpsDisplay ||
			CommonSettings.hud.FrameCounterDisplay ||
			CommonSettings.hud.ShowLagFrameCounter ||
			CommonSettings.hud.ShowMicrophone ||
			CommonSettings.hud.ShowRTC ||
			( osd->GetLineCount() > 0 )
		);
	}
	
	
	#ifdef X432R_SMOOTHINGFILTER_TEST
	static const u8 maxSmoothingLevel = 12;
	static u8 smoothingLevel = 0;
	
	bool IsSmoothingFilterEnabled()
	{
		return (smoothingLevel > 0);
	}
	
	struct RGBA8888_CompareAlpha
	{
		bool operator()(const RGBA8888 &color1, const RGBA8888 &color2) const
		{
			return (color1.A < color2.A);
		}
	};
	
	static inline void SetColorIntensity(RGBA8888 *color)
	{
		#if 1
		static const float ntscCoefR = 0.2989f;
		static const float ntscCoefG = 0.5866f;
		static const float ntscCoefB = 0.1145f;
		
		color->A = (u8)( ( ntscCoefR * (float)color->R ) + ( ntscCoefG * (float)color->G ) + ( ntscCoefB * (float)color->B ) );
		#else
		color->A = (color->R + color->G + color->B) / 3;
		#endif
	}
	
	
	template <u32 RENDER_MAGNIFICATION, u8 SMOOTHING_LEVEL>
	static inline RGBA8888 DD_GetSmoothedPixelData1(const u32 x, const u32 y, const u32 * const source_buffer)
	{
		// 3x3 bilateralhL (ÌÈª»Ì½ß³KªzðgpµÈ¢)
		
		
		#if 1
		if( (x < 1) || ( x >= ( (GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION) - 1 ) ) || (y < 1) || ( y >= ( (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION) - 1 ) ) )
			return source_buffer[ (y * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION) + x ];
		
		const u32 y0 = (y - 1) * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 y1 = y * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 y2 = (y + 1) * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 x0 = x - 1;
		const u32 x2 = x + 1;
		#else
		const u32 y0 = (u32)std::max<s32>( (s32)y - 1, 0 ) * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 y1 = y * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 y2 = (u32)std::min<s32>( (s32)y + 1, GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION ) * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 x0 = (u32)std::max<s32>( (s32)x - 1, 0 );
		const u32 x2 = (u32)std::min<s32>( (s32)x + 1, GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION );
		#endif
		
		
/*		// sigma = 0.509
		static const float coef0 = 0.6f;			// 48
		static const float coef1 = 0.087f;			// 7
		static const float coef2 = 0.0126f;			// 1
		
		// sigma = 0.563
//		static const float coef0 = 0.5f;			// 25
//		static const float coef1 = 0.1034f;			// 5
//		static const float coef2 = 0.0213f;			// 1
		
		// sigma = 0.636
//		static const float coef0 = 0.4f;			// 12
//		static const float coef1 = 0.1162f;			// 3
//		static const float coef2 = 0.0337f;			// 1
		
		// sigma = 0.751
//		static const float coef0 = 0.3f;			// 12
//		static const float coef1 = 0.1238f;			// 5
//		static const float coef2 = 0.051f;			// 2
		
		// sigma = 0.849
//		static const float coef0 = 0.25f;			// 4
//		static const float coef1 = 0.125f;			// 2
//		static const float coef2 = 0.0625f;			// 1
*/		
		static const u32 gaussian_coef1[9] =
		{
			1,		7,		1,
			7,		48,		7,
			1,		7,		1
		};
		
		static const u32 gaussian_coef2[9] =
		{
			1,		3,		1,
			3,		12,		3,
			1,		3,		1
		};
		
		static const u32 gaussian_coef3[9] =
		{
			2,		5,		2,
			5,		12,		5,
			2,		5,		2
		};
		
		
		RGBA8888 color = source_buffer[y1 + x];
		
		RGBA8888 colors[9] =
		{
			source_buffer[y0 + x0],		source_buffer[y0 + x],		source_buffer[y0 + x2],
			source_buffer[y1 + x0],		color,						source_buffer[y1 + x2],
			source_buffer[y2 + x0],		source_buffer[y2 + x],		source_buffer[y2 + x2]
		};
		
		u32 total_r = 0;
		u32 total_g = 0;
		u32 total_b = 0;
		u32 total_coef = 0;
		u32 intensity_delta, coef;
		
		
		#define X432R_ADD_PIXELDATA(gaussian_coef) \
			for(u32 i = 0; i < 9; ++i) \
			{ \
				intensity_delta = ( 255 - abs(color.A - colors[i].A) ); \
				coef = gaussian_coef[i] * intensity_delta; \
				\
				total_r += ( colors[i].R * coef ); \
				total_g += ( colors[i].G * coef ); \
				total_b += ( colors[i].B * coef ); \
				\
				total_coef += coef; \
			}
		
		
		switch(SMOOTHING_LEVEL)
		{
			case 1:
				X432R_ADD_PIXELDATA(gaussian_coef1)
				break;
			
			case 2:
				X432R_ADD_PIXELDATA(gaussian_coef2)
				break;
			
			case 3:
				X432R_ADD_PIXELDATA(gaussian_coef3)
				break;
		}
		
		
		#undef X432R_ADD_PIXELDATA
		
		
		color.R = (u8)(total_r / total_coef);
		color.G = (u8)(total_g / total_coef);
		color.B = (u8)(total_b / total_coef);
		color.A = 0xFF;
		
		return color;
	}
	
	template <u32 RENDER_MAGNIFICATION, u8 SMOOTHING_LEVEL>
	static inline RGBA8888 DD_GetSmoothedPixelData2(const u32 x, const u32 y, const u32 * const source_buffer)
	{
		// 3x3 Trimmed Mean
		
		
		#if 1
		if( (x < 1) || ( x >= ( (GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION) - 1 ) ) || (y < 1) || ( y >= ( (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION) - 1 ) ) )
			return source_buffer[ (y * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION) + x ];
		
		const u32 y0 = (y - 1) * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 y1 = y * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 y2 = (y + 1) * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 x0 = x - 1;
		const u32 x2 = x + 1;
		#else
		const u32 y0 = (u32)std::max<s32>( (s32)y - 1, 0 ) * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 y1 = y * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 y2 = (u32)std::min<s32>( (s32)y + 1, GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION ) * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		const u32 x0 = (u32)std::max<s32>( (s32)x - 1, 0 );
		const u32 x2 = (u32)std::min<s32>( (s32)x + 1, GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION );
		#endif
		
		
		RGBA8888 color = source_buffer[y1 + x];
		
		RGBA8888 colors[9] =
		{
			source_buffer[y0 + x0],		source_buffer[y0 + x],		source_buffer[y0 + x2],
			source_buffer[y1 + x0],		color,						source_buffer[y1 + x2],
			source_buffer[y2 + x0],		source_buffer[y2 + x],		source_buffer[y2 + x2]
		};
		
		u32 total_r = 0;
		u32 total_g = 0;
		u32 total_b = 0;
		
		
		std::sort( colors, colors + 9, RGBA8888_CompareAlpha() );
		
		
		#define X432R_ADD_PIXELDATA(bein_index,end_index) \
			for(u32 i = bein_index; i <= end_index; ++i) \
			{ \
				total_r += colors[i].R; \
				total_g += colors[i].G; \
				total_b += colors[i].B; \
			}
		
		#define X432R_GET_WEIGHTED_MEAN(original_weight,data_count) \
			total_r += (color.R * original_weight); \
			total_g += (color.G * original_weight); \
			total_b += (color.B * original_weight); \
			color.R = total_r / (data_count + original_weight); \
			color.G = total_g / (data_count + original_weight); \
			color.B = total_b / (data_count + original_weight);
		
		#define X432R_GET_MEAN(data_count) \
			color.R = total_r / data_count; \
			color.G = total_g / data_count; \
			color.B = total_b / data_count;
		
		
		switch(SMOOTHING_LEVEL)
		{
			case 4:
			case 5:
			case 6:
				switch(color.A >> 6)				// ³f[^ÌPxÉ¶ÄgÍÍðÂÏ
				{
					case 0:							// 0`63
						X432R_ADD_PIXELDATA(2, 4)
						break;
					
					case 1:							// 64`191
					case 2:
						X432R_ADD_PIXELDATA(3, 5)
						break;
					
					case 3:							// 192`255
						X432R_ADD_PIXELDATA(4, 6)
						break;
				}
				break;
			
			case 7:
			case 8:
			case 9:
				switch(color.A / 85)
				{
					case 0:							// 0`84
						X432R_ADD_PIXELDATA(2, 4)
						break;
					
					case 1:							// 85`169
						X432R_ADD_PIXELDATA(3, 5)
						break;
					
					case 2:							// 170`255
					case 3:
						X432R_ADD_PIXELDATA(4, 6)
						break;
				}
				break;
			
			case 10:
			case 11:
			case 12:
				switch(color.A / 51)
				{
					case 0:							// 0`50
						X432R_ADD_PIXELDATA(1, 3)
						break;
					
					case 1:							// 51`101
						X432R_ADD_PIXELDATA(2, 4)
						break;
					
					case 2:							// 102`152
						X432R_ADD_PIXELDATA(3, 5)
						break;
					
					case 3:							// 153`203
						X432R_ADD_PIXELDATA(4, 6)
						break;
					
					case 4:							// 204`255
					case 5:
						X432R_ADD_PIXELDATA(5, 7)
						break;
				}
				break;
		}
		
		switch(SMOOTHING_LEVEL)
		{
			case 4:
			case 7:
			case 10:
				X432R_GET_WEIGHTED_MEAN(6, 3)
				break;
			
			case 5:
			case 8:
			case 11:
				X432R_GET_WEIGHTED_MEAN(3, 3)
				break;
			
			case 6:
			case 9:
			case 12:
				X432R_GET_MEAN(3)
				break;
		}
		
		
		#undef X432R_ADD_PIXELDATA
		#undef X432R_GET_WEIGHTED_MEAN
		#undef X432R_GET_MEAN
		
		
		color.A = 0xFF;
		
		return color;
	}
	#endif
	
	#ifndef X432R_SMOOTHINGFILTER_TEST
	template <u32 RENDER_MAGNIFICATION, u32 ROTATION_ANGLE, bool HIGHRESO, bool HUD_VISIBLE>
	static void DD_UpdateBackSurface(const u32 *source_buffer, const u32 master_brightness, const u32 screen_index)
	#else
	template <u32 RENDER_MAGNIFICATION, u32 ROTATION_ANGLE, bool HIGHRESO, u8 SMOOTHING_LEVEL, bool HUD_VISIBLE>
	static void DD_UpdateBackSurface(u32 *source_buffer, const u32 master_brightness, const u32 screen_index)
	#endif
	{
		const u32 dest_linepitch = ddraw.surfDescBack.lPitch / 4;		// ddraw.surfDescBack.lPitch / ( sizeof(u32) / sizeof(u8) )
		u32 * const destbuffer_begin = (u32 *)ddraw.surfDescBack.lpSurface;
		u32 *dest_buffer;
		
		source_buffer += (screen_index * GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION * RENDER_MAGNIFICATION);
		
		const u32 * const hud_buffer = hudBuffer + (screen_index * GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT);
		
		s32 dest_x, dest_y, dest_begin_x, dest_begin_y, dest_end_x, dest_end_y;
		s32 source_x, source_y, source_begin_x, source_begin_y;
		s32 downscaled_index;
		
		RGBA8888 color_rgba8888, hud_rgba8888;
		
		switch(ROTATION_ANGLE)
		{
			case 90:
//				dest_begin_x = screen_index ? (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION) : 0;
				dest_begin_x = screen_index ? 0 : (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION);
				dest_begin_y = 0;
				dest_end_x = dest_begin_x + (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION);
				dest_end_y = dest_begin_y + (GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION);
				
				source_begin_x = 0;
				source_begin_y = (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION) - 1;
				break;
				
			case 180:
				dest_begin_x = 0;
//				dest_begin_y = screen_index ? (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION) : 0;
				dest_begin_y = screen_index ? 0 : (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION);
				dest_end_x = GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
				dest_end_y = dest_begin_y + (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION);
				
				source_begin_x = (GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION) - 1;
				source_begin_y = (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION) - 1;
				break;
				
			case 270:
				dest_begin_x = screen_index ? (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION) : 0;
				dest_begin_y = 0;
				dest_end_x = dest_begin_x + (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION);
				dest_end_y = dest_begin_y + (GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION);
				
				source_begin_x = (GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION) - 1;
				source_begin_y = 0;
				break;
				
			default:
				dest_begin_x = 0;
				dest_begin_y = screen_index ? (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION) : 0;
				dest_end_x = GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
				dest_end_y = dest_begin_y + (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION);
				
				source_begin_x = 0;
				source_begin_y = 0;
				break;
		}
		
		source_x = source_begin_x;
		source_y = source_begin_y;
		
		
		#ifdef X432R_SMOOTHINGFILTER_TEST
		if( IsSmoothingFilterEnabled() )
		{
			RGBA8888 *buffer = (RGBA8888 *)source_buffer;
			
			for(u32 i = 0; i < GFX3D_FRAMEBUFFER_WIDTH * GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION * RENDER_MAGNIFICATION; ++i, ++buffer)
			{
				SetColorIntensity(buffer);
			}
		}
		#endif
		
		
		for(dest_y = dest_begin_y; dest_y < dest_end_y; ++dest_y)
		{
			dest_buffer = destbuffer_begin + (dest_y * dest_linepitch) + dest_begin_x;
			
			for(dest_x = dest_begin_x; dest_x < dest_end_x; ++dest_x, ++dest_buffer)
			{
				#ifndef X432R_SMOOTHINGFILTER_TEST
				if( !HIGHRESO || HUD_VISIBLE )
					downscaled_index = ( (source_y / RENDER_MAGNIFICATION) * GFX3D_FRAMEBUFFER_WIDTH ) + (source_x / RENDER_MAGNIFICATION);
				
				if(HIGHRESO)
					color_rgba8888 = RGBA8888::AlphaBlend( master_brightness, source_buffer[ (source_y * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION) + source_x ] );
				else
					color_rgba8888 = source_buffer[downscaled_index];
				#else
				if( ( !HIGHRESO && (SMOOTHING_LEVEL == 0) ) || HUD_VISIBLE )
					downscaled_index = ( (source_y / RENDER_MAGNIFICATION) * GFX3D_FRAMEBUFFER_WIDTH ) + (source_x / RENDER_MAGNIFICATION);
				
				if(SMOOTHING_LEVEL > 0)
				{
					if(SMOOTHING_LEVEL <= 3)
						color_rgba8888 = DD_GetSmoothedPixelData1<RENDER_MAGNIFICATION, SMOOTHING_LEVEL>(source_x, source_y, source_buffer);
					else
						color_rgba8888 = DD_GetSmoothedPixelData2<RENDER_MAGNIFICATION, SMOOTHING_LEVEL>(source_x, source_y, source_buffer);
				}
				
				else if(HIGHRESO)
					color_rgba8888 = source_buffer[ (source_y * GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION) + source_x ];
				
				else
					color_rgba8888 = source_buffer[downscaled_index];
				
				color_rgba8888.AlphaBlend(master_brightness);
				#endif
				
				if(HUD_VISIBLE)
					color_rgba8888.AlphaBlend( hud_buffer[downscaled_index] );
				
				*dest_buffer = color_rgba8888.Color;
				
				switch(ROTATION_ANGLE)
				{
					case 90:	--source_y;		break;
					case 180:	--source_x;		break;
					case 270:	++source_y;		break;
					default:	++source_x;		break;
				}
			}
			
			switch(ROTATION_ANGLE)
			{
				case 90:	++source_x;		source_y = source_begin_y;		break;
				case 180:	--source_y;		source_x = source_begin_x;		break;
				case 270:	--source_x;		source_y = source_begin_y;		break;
				default:	++source_y;		source_x = source_begin_x;		break;
			}
		}
	}
	
	static void DD_FillRect(LPDIRECTDRAWSURFACE7 surface, const u32 left, const u32 top, const u32 right, const u32 bottom)
	{
      /* FIXME - DirectDraw crap. */
		RECT rect;
		SetRect(&rect, left, top, right, bottom);
		
		DDBLTFX effect;
		memset( &effect, 0, sizeof(DDBLTFX) );
		effect.dwSize = sizeof(DDBLTFX);
		
		effect.dwFillColor = ScreenGapColor;		// 32bppÂ«ÌÝl¶
		
		surface->Blt(&rect, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &effect);
	}
	
	#ifndef X432R_SMOOTHINGFILTER_TEST
	template <u32 RENDER_MAGNIFICATION>
	#else
	template <u32 RENDER_MAGNIFICATION, u8 SMOOTHING_LEVEL>
	#endif
	static void DD_DoDisplay()
	{
      /* FIXME - Windows crap. */
		const HWND window_handle = MainWindow->getHWnd();
		const bool hud_visible = IsHudVisible();
		
		const u32 rotation_angle = (video.layout == 0) ? video.rotation : 0;
		const bool source_rect_rotation = (rotation_angle == 90) || (rotation_angle == 270);
		bool screen_swap = false;
		
		switch(video.swap)
		{
			case 1:
            screen_swap = true;
            break;
			case 2:
            screen_swap = (MainScreen.offset > 0);	
            break;
			case 3:
            screen_swap = (SubScreen.offset > 0);
            break;
		}
		
		if( !screenTextureUpdated || hud_visible )
		{
         /* TODO - FIXME - figure out what this stuff is */
			if(hud_visible)
			{
				osd->swapScreens = screen_swap;
				aggDraw.hud->attach( (u8*)hudBuffer, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT * 2, GFX3D_FRAMEBUFFER_WIDTH * 4 );
				aggDraw.hud->clear();
				DoDisplay_DrawHud();
			}
			
			if( !ddraw.lock() ) return;
			
			const u32 color_depth = ddraw.surfDescBack.ddpfPixelFormat.dwRGBBitCount;
			
			if(color_depth != 32)
			{
            /* FIXME - DirectDraw crap. */
				ddraw.unlock();
				
				if(color_depth != 0)
				{
					INFO("X432R: DirectDraw Output failed.\n");
					INFO("Unsupported color depth: %i bpp\n", color_depth);
				
					SetStyle( ( GetStyle() & ~DWS_DISPMETHODS ) | DWS_OPENGL );
					WritePrivateProfileInt("Video","Display Method", DISPMETHOD_OPENGL, IniName);
					ddraw.createSurfaces(window_handle);
				}
				
				return;
			}
			
			static u8 buffer_index = 0;
			
			if( !screenTextureUpdated )
			{
				Lock lock(customFrontBufferSync);
				
				screenTextureUpdated = true;
				
				buffer_index = CommonSettings.single_core() ? 0 : clamp(currDisplayBuffer, 0, 2);
			}
			
			#ifndef X432R_SMOOTHINGFILTER_TEST
			const u32 * const front_buffer = frontBuffer[buffer_index];
			#else
			u32 * const front_buffer = frontBuffer[buffer_index];
			#endif
			
			const u32 * const master_brightness = masterBrightness[buffer_index];
			const bool * const is_highreso_screen = isHighResolutionScreen[buffer_index];
			
			#ifndef X432R_SMOOTHINGFILTER_TEST
			#define X432R_CALL_UPDATEBACKSURFACE(rotation_angle,screen_index) \
			{ \
				if( is_highreso_screen[screen_index] ) \
				{ \
					if(hud_visible) \
						DD_UpdateBackSurface<RENDER_MAGNIFICATION, rotation_angle, true, true>(front_buffer, master_brightness[screen_index], screen_index); \
					else \
						DD_UpdateBackSurface<RENDER_MAGNIFICATION, rotation_angle, true, false>(front_buffer, master_brightness[screen_index], screen_index); \
				} \
				else \
				{ \
					if(hud_visible) \
						DD_UpdateBackSurface<RENDER_MAGNIFICATION, rotation_angle, false, true>(front_buffer, 0, screen_index); \
					else \
						DD_UpdateBackSurface<RENDER_MAGNIFICATION, rotation_angle, false, false>(front_buffer, 0, screen_index); \
				} \
			}
			#else
			#define X432R_CALL_UPDATEBACKSURFACE(rotation_angle,screen_index) \
			{ \
					if( is_highreso_screen[screen_index] ) \
					{ \
						if(hud_visible) \
							DD_UpdateBackSurface<RENDER_MAGNIFICATION, rotation_angle, true, SMOOTHING_LEVEL, true>(front_buffer, master_brightness[screen_index], screen_index); \
						else \
							DD_UpdateBackSurface<RENDER_MAGNIFICATION, rotation_angle, true, SMOOTHING_LEVEL, false>(front_buffer, master_brightness[screen_index], screen_index); \
					} \
					else \
					{ \
						if(hud_visible) \
							DD_UpdateBackSurface<RENDER_MAGNIFICATION, rotation_angle, false, SMOOTHING_LEVEL, true>(front_buffer, 0, screen_index); \
						else \
							DD_UpdateBackSurface<RENDER_MAGNIFICATION, rotation_angle, false, SMOOTHING_LEVEL, false>(front_buffer, 0, screen_index); \
				} \
			}
			#endif
			
			for(u32 i = 0; i < 2; ++i)
			{
				switch(rotation_angle)
				{
					case 90:	X432R_CALL_UPDATEBACKSURFACE(90, i);	break;
					case 180:	X432R_CALL_UPDATEBACKSURFACE(180, i);	break;
					case 270:	X432R_CALL_UPDATEBACKSURFACE(270, i);	break;
					default:	X432R_CALL_UPDATEBACKSURFACE(0, i);		break;
				}
			}
			
			#undef X432R_CALL_UPDATEBACKSURFACE
			
			if( !ddraw.unlock() ) return;
		}
		
		RECT screen_source_rect[2] =
		{
			{
				0,
				0,
				source_rect_rotation ?	(GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION)			: (GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION),
				source_rect_rotation ?	(GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION)			: (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION)
			},
			{
				source_rect_rotation ?	(GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION)			: 0,
				source_rect_rotation ?	0										: (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION),
				source_rect_rotation ?	(GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION * 2)		: (GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION),
				source_rect_rotation ?	(GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION)			: (GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION * 2)
			}
		};
		
		RECT *source_rect[2] =
		{
			&screen_source_rect[0],
			&screen_source_rect[1]
		};
		
		RECT *dest_rect[2] =
		{
			screen_swap ? &SubScreenRect : &MainScreenRect,
			screen_swap ? &MainScreenRect : &SubScreenRect
		};
		
		
		RECT window_rect;
		
		#if 1
      /* FIXME - Windows crap. */
		GetWindowRect(window_handle, &window_rect);
		#else
		u32 toolbar_height = MainWindowToolbar->GetHeight();
		
		GetClientRect(window_handle, &window_rect);
		window_rect.top += toolbar_height;
		window_rect.bottom += toolbar_height;
		#endif
		
		const u32 window_width = window_rect.right - window_rect.left;
		const u32 window_height = window_rect.bottom - window_rect.top;
		
		RECT screen_rect;
		GetNdsScreenRect(&screen_rect);
		
		if( IsZoomed(window_handle) )
		{
         /* FIXME - DirectDraw crap. */
			DD_FillRect(ddraw.surface.primary,	0,					0,						screen_rect.left,		window_height);			// left
			DD_FillRect(ddraw.surface.primary,	screen_rect.right,	0,						window_width,			window_height);			// right
			DD_FillRect(ddraw.surface.primary,	screen_rect.left,	0,						screen_rect.right,		screen_rect.top);		// top
			DD_FillRect(ddraw.surface.primary,	screen_rect.left,	screen_rect.bottom,		screen_rect.right,		window_height);			// bottom
		}
		
		if( (video.layout == 0) && (video.screengap > 0) )
		{
         /* FIXME - DirectDraw crap. */
			#if 1
			if(source_rect_rotation)
				DD_FillRect(ddraw.surface.primary, MainScreenRect.right, MainScreenRect.top, SubScreenRect.left, MainScreenRect.bottom);
			else
				DD_FillRect(ddraw.surface.primary, MainScreenRect.left, MainScreenRect.bottom, MainScreenRect.right, SubScreenRect.top);
			#else
			RECT gap_rect = GapRect;
			gap_rect.top += toolbar_height;
			gap_rect.bottom += toolbar_height;
			
			DD_FillRect(ddraw.surface.primary, gap_rect.left, gap_rect.top, gap_rect.right, gap_rect.bottom);
			#endif
		}
		
		if(video.layout == 2)
		{
         /* FIXME - DirectDraw crap. */
			const u32 screen_index = screen_swap ? 1 : 0;
			
			ddraw.blt( dest_rect[screen_index], source_rect[screen_index] );
		}
		else
		{
         /* FIXME - DirectDraw crap. */
			for(int i = 0; i < 2; ++i)
			{
				if( !ddraw.blt( dest_rect[i], source_rect[i] ) ) break;
			}
		}
	}
	
	
	template <u32 RENDER_MAGNIFICATION>
	static void DoDisplay()
	{
		X432R_STATIC_RENDER_MAGNIFICATION_CHECK();
		assert( (lastRenderMagnification >= 1) && (lastRenderMagnification <= 4) );
		
		
		static const u32 RENDER_WIDTH = GFX3D_FRAMEBUFFER_WIDTH * RENDER_MAGNIFICATION;
		static const u32 RENDER_HEIGHT = GFX3D_FRAMEBUFFER_HEIGHT * RENDER_MAGNIFICATION;
		static const u32 TEXTURE_WIDTH = RENDER_WIDTH;
		static const u32 TEXTURE_HEIGHT = RENDER_HEIGHT * 2;
		
		
		if( finished || display_die || (gpu3D == NULL) ) return;
		
		
		if(screenTextureUpdated)
		{
			if( displayPostponeType && !displayNoPostponeNext && ( displayPostponeType < 0 || ( timeGetTime() < displayPostponeUntil ) ) ) return;
			
			displayNoPostponeNext = false;
		}
		
		
#ifdef HAVE_LUA
		if( AnyLuaActive() )
		{
			if( g_thread_self() == display_thread )
				InvokeOnMainThread( ( void(*)(DWORD) )CallRegisteredLuaFunctions, LUACALL_AFTEREMULATIONGUI );
			
			else
				CallRegisteredLuaFunctions(LUACALL_AFTEREMULATIONGUI);
		}
#endif
		
		
      /* FIXME - Windows crap */
		const HWND window_handle = MainWindow->getHWnd();
		const u32 window_style = GetStyle();
		
		if( (window_style & DWS_DDRAW_HW) || (window_style & DWS_DDRAW_SW) )
		{
			DeleteScreenTexture();
#ifdef HAVE_OPENGL
			gldisplay.kill();
#endif
			
			#ifndef X432R_SMOOTHINGFILTER_TEST
			DD_DoDisplay<RENDER_MAGNIFICATION>();
			#else
         /* FIXME - DirectDraw crap. */
			switch(smoothingLevel)
			{
				case 1:
					DD_DoDisplay<RENDER_MAGNIFICATION, 1>();
					break;
				
				case 2:
					DD_DoDisplay<RENDER_MAGNIFICATION, 2>();
					break;
				
				case 3:
					DD_DoDisplay<RENDER_MAGNIFICATION, 3>();
					break;
				
				case 4:
					DD_DoDisplay<RENDER_MAGNIFICATION, 4>();
					break;
				
				case 5:
					DD_DoDisplay<RENDER_MAGNIFICATION, 5>();
					break;
				
				case 6:
					DD_DoDisplay<RENDER_MAGNIFICATION, 6>();
					break;
				
				case 7:
					DD_DoDisplay<RENDER_MAGNIFICATION, 7>();
					break;
				
				case 8:
					DD_DoDisplay<RENDER_MAGNIFICATION, 8>();
					break;
				
				case 9:
					DD_DoDisplay<RENDER_MAGNIFICATION, 9>();
					break;
				
				case 10:
					DD_DoDisplay<RENDER_MAGNIFICATION, 10>();
					break;
				
				case 11:
					DD_DoDisplay<RENDER_MAGNIFICATION, 11>();
					break;
				
				case 12:
					DD_DoDisplay<RENDER_MAGNIFICATION, 12>();
					break;
				
				default:
					DD_DoDisplay<RENDER_MAGNIFICATION, 0>();
					break;
			}
			#endif
			
			return;
		}
		
		
#ifdef HAVE_OPENGL
		//------ OGL_DoDisplay() ------
		
		if( !gldisplay.begin() )
         return;
		
		// ì¬Â\ÈÅåeNX`TCYð`FbN
		static int max_texture_size = 0;
		
		if(max_texture_size == 0)
		{
			glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
			INFO( "X432R: OpenGL supported texture size:%d (required:%d)\n", max_texture_size, (TEXTURE_WIDTH * 4) );
			
			if( max_texture_size < (TEXTURE_WIDTH * 4) )
			{
				gldisplay.end();
				
				INFO("X432R: High-Resolution Output failed.\n");
				Change3DCoreWithFallbackAndSave(GPU3D_OPENGL_3_2);
				return;
			}
			
			INFO("X432R: High-Resolution Output OK.\n");
		}
		
		
		RECT client_rect;
		GetClientRect(window_handle, &client_rect);
		
		const int window_width = client_rect.right - client_rect.left;
		const int window_height = client_rect.bottom - client_rect.top;
		
		
		// æÊ`æpeNX`¶¬
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		
		if(hudTexture == 0)
		{
			glGenTextures(1, &hudTexture);
			
			glBindTexture(GL_TEXTURE_2D, hudTexture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT * 2, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
		}
		
		if(screenTexture == 0)
			glGenTextures(1, &screenTexture);
		
		glBindTexture(GL_TEXTURE_2D, screenTexture);
		
		if(lastRenderMagnification != RENDER_MAGNIFICATION)
		{
			lastRenderMagnification = RENDER_MAGNIFICATION;
			
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEXTURE_WIDTH, TEXTURE_HEIGHT, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);		// f[^ð]¹¸ÉÌæ¾¯mÛ
		}
		
		static bool is_highreso_upper = false;
		static bool is_highreso_lower = false;
		static u32 upper_brightness = 0;
		static u32 lower_brightness = 0;
		
		if( !screenTextureUpdated )
		{
			Lock_forHighResolutionFrontBuffer();
			
			screenTextureUpdated = true;
			
			const u8 buffer_index = CommonSettings.single_core() ? 0 : clamp(currDisplayBuffer, 0, 2);
			const u32 * const front_buffer = frontBuffer[buffer_index];
			
			is_highreso_upper = isHighResolutionScreen[buffer_index][0];
			is_highreso_lower = isHighResolutionScreen[buffer_index][1];
			
			upper_brightness = is_highreso_upper ? masterBrightness[buffer_index][0] : 0;
			lower_brightness = is_highreso_lower ? masterBrightness[buffer_index][1] : 0;
			
			Unlock_forHighResolutionFrontBuffer();
			
			if(is_highreso_upper && is_highreso_lower)
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, front_buffer);
			else
			{
				if(is_highreso_upper)
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, RENDER_WIDTH, RENDER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, front_buffer);
				else
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, front_buffer);
				
				if(is_highreso_lower)
					glTexSubImage2D( GL_TEXTURE_2D, 0, 0, RENDER_HEIGHT, RENDER_WIDTH, RENDER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, front_buffer + (RENDER_WIDTH * RENDER_HEIGHT) );
				else
					glTexSubImage2D( GL_TEXTURE_2D, 0, 0, RENDER_HEIGHT, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_BGRA, GL_UNSIGNED_BYTE, front_buffer + (RENDER_WIDTH * RENDER_HEIGHT) );
			}
		}
		
		
		// eNX`ÀW¶¬
		static const float default_texvert[2][8] =
		{
			{
				0.0f,		0.0f,
				1.0f,		0.0f,
				1.0f,		0.5f,
				0.0f,		0.5f
			},
			{
				0.0f,		0.5f,
				1.0f,		0.5f,
				1.0f,		1.0f,
				0.0f,		1.0f
			}
		};
		
		static const float lowreso_texvert[2][8] =
		{
			{
				0.0f,								0.0f,
				GFX3D_FRAMEBUFFER_WIDTH / (float)TEXTURE_WIDTH,		0.0f,
				GFX3D_FRAMEBUFFER_WIDTH / (float)TEXTURE_WIDTH,		GFX3D_FRAMEBUFFER_HEIGHT / (float)TEXTURE_HEIGHT,
				0.0f,								GFX3D_FRAMEBUFFER_HEIGHT / (float)TEXTURE_HEIGHT
			},
			{
				0.0f,								0.5f,
				GFX3D_FRAMEBUFFER_WIDTH / (float)TEXTURE_WIDTH,		0.5f,
				GFX3D_FRAMEBUFFER_WIDTH / (float)TEXTURE_WIDTH,		0.5f + ( GFX3D_FRAMEBUFFER_HEIGHT / (float)TEXTURE_HEIGHT ),
				0.0f,								0.5f + ( GFX3D_FRAMEBUFFER_HEIGHT / (float)TEXTURE_HEIGHT )
			}
		};
		
		float texvert[2][8];
		float texvert_hud[2][8];
		
		bool screen_swap = false;
		bool hud_swap = false;
		u32 texture_index_offset = 0;
		u32 i;
		
		switch(video.swap)
		{
			case 1:		screen_swap = true;							break;
			case 2:		screen_swap = (MainScreen.offset > 0);		break;
			case 3:		screen_swap = (SubScreen.offset > 0);		break;
		}
		
		hud_swap = screen_swap;
		
		switch(video.rotation)
		{
			case 90:
				texture_index_offset = 6;
				screen_swap = !screen_swap;
				break;
				
			case 180:
				texture_index_offset = 4;
				screen_swap = !screen_swap;
				break;
				
			case 270:
				texture_index_offset = 2;
				break;
		}
		
		for(i = 0; i < 8; ++i)
		{
			u32 index = (i + texture_index_offset) % 8;
			
			texvert[0][i] = is_highreso_upper ? default_texvert[0][index] : lowreso_texvert[0][index];
			texvert[1][i] = is_highreso_lower ? default_texvert[1][index] : lowreso_texvert[1][index];
			
			texvert_hud[0][i] = default_texvert[0][index];
			texvert_hud[1][i] = default_texvert[1][index];
		}
		
		
		// |SÀWzñ¶¬
		const u32 upperscreen_index = screen_swap ? 1 : 0;
		const u32 lowerscreen_index = screen_swap ? 0 : 1;
		const RECT screen_rect[] = {MainScreenRect, SubScreenRect};
		
		for(i = 0; i < 2; ++i)
		{
			ScreenToClient(window_handle, (LPPOINT)&screen_rect[i].left);
			ScreenToClient(window_handle, (LPPOINT)&screen_rect[i].right);
		}
		
		const float polygon_vertices[2][8] =
		{
			{
				screen_rect[upperscreen_index].left,		screen_rect[upperscreen_index].top,
				screen_rect[upperscreen_index].right,		screen_rect[upperscreen_index].top,
				screen_rect[upperscreen_index].right,		screen_rect[upperscreen_index].bottom,
				screen_rect[upperscreen_index].left,		screen_rect[upperscreen_index].bottom
			},
			{
				screen_rect[lowerscreen_index].left,		screen_rect[lowerscreen_index].top,
				screen_rect[lowerscreen_index].right,		screen_rect[lowerscreen_index].top,
				screen_rect[lowerscreen_index].right,		screen_rect[lowerscreen_index].bottom,
				screen_rect[lowerscreen_index].left,		screen_rect[lowerscreen_index].bottom
			}
		};
		
		
		// `æJn
		const RGBA8888 clearcolor = (u32)ScreenGapColor;
#ifdef HAVE_OPENGL
		const GLuint texture_filter = ( GetStyle() & DWS_FILTER ) ? GL_LINEAR : GL_NEAREST;
#endif
		
		glDisable(GL_LIGHTING);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_STENCIL_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_ALPHA);
		
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		
		glViewport(0, 0, window_width, window_height);
		
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho( 0.0f, (float)window_width, (float)window_height, 0.0f, -100.0f, 100.0f );
		
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		
		glClearColor( (float)clearcolor.R / 255.0f, (float)clearcolor.G / 255.0f, (float)clearcolor.B / 255.0f, 1.0f );
		glClear(GL_COLOR_BUFFER_BIT);				// ViewportSÌðScreen GapÌFÅNA
		
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texture_filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texture_filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		
//		glColor4ub(0xFF, 0xFF, 0xFF, 0xFF);
		glVertexPointer(2, GL_FLOAT, 0, polygon_vertices);
		
		
		glTexCoordPointer(2, GL_FLOAT, 0, texvert);
		
		if(video.layout == 2)
			glDrawArrays(GL_QUADS, screen_swap ? 4 : 0, 4);
		else
			glDrawArrays(GL_QUADS, 0, 4 * 2);
		
		if( (upper_brightness != 0) || (lower_brightness != 0) )
		{
//			glDisable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, 0);
			glEnableClientState(GL_COLOR_ARRAY);
			
			#if 0
			u8 fade_colors[2][4][4];
			RGBA8888 brightness;
			u32 j;
			
			for(i = 0; i < 2; ++i)
			{
				brightness = (i == 0) ? upper_brightness : lower_brightness;
				
				for(j = 0; j < 4; ++j)
				{
					fade_colors[i][j][0] = brightness.R;
					fade_colors[i][j][1] = brightness.G;
					fade_colors[i][j][2] = brightness.B;
					fade_colors[i][j][3] = brightness.A;
				}
			}
			#else
			const u32 fade_colors[8] =
			{
				upper_brightness, upper_brightness, upper_brightness, upper_brightness,
				lower_brightness, lower_brightness, lower_brightness, lower_brightness
			};
			#endif
			
			glColorPointer(4, GL_UNSIGNED_BYTE, 0, fade_colors);
			glDrawArrays(GL_QUADS, 0, 4 * 2);
			
			glDisableClientState(GL_COLOR_ARRAY);
//			glEnable(GL_TEXTURE_2D);
		}
		
		if( IsHudVisible() )
		{
			osd->swapScreens = hud_swap;
			aggDraw.hud->attach( (u8*)hudBuffer, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT * 2, GFX3D_FRAMEBUFFER_WIDTH * 4 );
			aggDraw.hud->clear();
			DoDisplay_DrawHud();
			
			glBindTexture(GL_TEXTURE_2D, hudTexture);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT * 2, GL_BGRA, GL_UNSIGNED_BYTE, hudBuffer);
			
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
//			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
//			glColor4ub(0xFF, 0xFF, 0xFF, 0x7F);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			
			glTexCoordPointer(2, GL_FLOAT, 0, texvert_hud);
			glDrawArrays(GL_QUADS, 0, 4 * 2);
		}
		
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_BLEND);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		
		gldisplay.showPage();
		gldisplay.end();
#endif
	}
	
#ifdef HAVE_OPENGL
#if 0
	inline GLuint GetScreenTexture()
#else
	inline u32 GetScreenTexture()
#endif
	{
		if(screenTexture == 0)
			glGenTextures(1, &screenTexture);
		
		return screenTexture;
	}
	
	static inline void DeleteScreenTexture()
	{
		lastRenderMagnification = 1;
		
		if(screenTexture != 0)
		{
			glDeleteTextures(1, &screenTexture);
			screenTexture = 0;
		}
		
		if(hudTexture != 0)
		{
			glDeleteTextures(1, &hudTexture);
			hudTexture = 0;
		}
	}
#endif
}
#endif



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
	 
#ifdef X432R_CUSTOMRENDERER_ENABLED
	var.key = "desmume_high_res_renderer_enabled";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		if (!strcmp(var.value, "enable"))
			highres_enabled = true;
      else if (!strcmp(var.value, "disable"))
			highres_enabled = false;
	}
   else
      highres_enabled = false;
	
	var.key = "desmume_internal_res";

	if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
	{
		if (!strcmp(var.value, "2x"))
			internal_res = 2;
		else if(!strcmp(var.value, "3x"))
			internal_res = 3;
		else if(!strcmp(var.value, "4x"))
			internal_res = 4;			
		else 
         internal_res = 1;
	}		
   else
      internal_res = 1;
#endif
	
	
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

#ifdef X432R_CUSTOMRENDERER_ENABLED

#ifdef HAVE_OPENGL
#define GPU3D_OPENGL_X2 4
#define GPU3D_OPENGL_X3 5
#define GPU3D_OPENGL_X4 6

#define GPU3D_SWRAST_X2 7
#define GPU3D_SWRAST_X3 8
#define GPU3D_SWRAST_X4 9
#else
#define GPU3D_SWRAST_X2 4
#define GPU3D_SWRAST_X3 5
#define GPU3D_SWRAST_X4 6
#endif

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
#ifdef X432R_CUSTOMRENDERER_ENABLED
#ifdef HAVE_OPENGL
    &X432R::gpu3Dgl_X2,
    &X432R::gpu3Dgl_X3,
    &X432R::gpu3Dgl_X4,
#endif
    &X432R::gpu3DRasterize_X2,
    &X432R::gpu3DRasterize_X3,
    &X432R::gpu3DRasterize_X4,
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
#ifdef X432R_CUSTOMRENDERER_ENABLED
      { "desmume_high_res_renderer_enabled", "Enable high resolution renderer; disable|enable" },
      { "desmume_internal_res", "Internal resolution; 1x|2x|3x|4x" },
#endif	  
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
#ifdef X432R_CUSTOMRENDERER_ENABLED
   if( (newCore < 0) || (newCore > GPU3D_SWRAST_X4) )
      newCore = GPU3D_SWRAST;
#else
   if( (newCore < 0)
#ifdef HAVE_OPENGL
         || (newCore > GPU3D_OPENGL_OLD)
#endif
         )
      newCore = GPU3D_SWRAST;
#endif
   
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
#ifdef X432R_CUSTOMRENDERER_ENABLED
	switch(newCore)
	{
		case GPU3D_NULL:
			NDS_3D_ChangeCore(GPU3D_NULL);
			break;
#ifdef HAVE_OPENGL
		case GPU3D_OPENGL_X2:
		case GPU3D_OPENGL_X3:
		case GPU3D_OPENGL_X4:
#endif
		case GPU3D_SWRAST_X2:
		case GPU3D_SWRAST_X3:
		case GPU3D_SWRAST_X4:
         /* FIXME/TODO - do something here to do with 
          * scaling */
#ifdef HAVE_OPENGL
		case GPU3D_OPENGL_3_2:
			if( NDS_3D_ChangeCore(newCore) ) break;
			
			printf("falling back to 3d core: %s\n", core3DList[GPU3D_OPENGL_OLD]->name);
		
		case GPU3D_OPENGL_OLD:
			if( NDS_3D_ChangeCore(GPU3D_OPENGL_OLD) ) break;
			
			printf("falling back to 3d core: %s\n", core3DList[GPU3D_SWRAST]->name);
#endif
		default:
			NDS_3D_ChangeCore(GPU3D_SWRAST);
			break;
	}
#endif
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
