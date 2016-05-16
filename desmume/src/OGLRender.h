/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2016 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OGLRENDER_H
#define OGLRENDER_H

#include <queue>
#include <set>
#include <string>
#include "render3D.h"
#include "types.h"

#define OGLRENDER_MAX_MULTISAMPLES			16
#define OGLRENDER_VERT_INDEX_BUFFER_COUNT	(POLYLIST_SIZE * 6)

enum OGLVertexAttributeID
{
	OGLVertexAttributeID_Position	= 0,
	OGLVertexAttributeID_TexCoord0	= 8,
	OGLVertexAttributeID_Color		= 3,
};

enum OGLTextureUnitID
{
	// Main textures will always be on texture unit 0.
	OGLTextureUnitID_FinalColor = 1,
	OGLTextureUnitID_ToonTable,
	OGLTextureUnitID_GColor,
	OGLTextureUnitID_GDepth,
	OGLTextureUnitID_GPolyID,
	OGLTextureUnitID_FogAttr,
	OGLTextureUnitID_PolyStates
};

enum OGLBindingPointID
{
	OGLBindingPointID_RenderStates = 0
};

enum OGLErrorCode
{
	OGLERROR_NOERR = RENDER3DERROR_NOERR,
	
	OGLERROR_BEGINGL_FAILED,
	
	OGLERROR_FEATURE_UNSUPPORTED,
	OGLERROR_VBO_UNSUPPORTED,
	OGLERROR_PBO_UNSUPPORTED,
	OGLERROR_SHADER_UNSUPPORTED,
	OGLERROR_VAO_UNSUPPORTED,
	OGLERROR_FBO_UNSUPPORTED,
	OGLERROR_MULTISAMPLED_FBO_UNSUPPORTED,
	
	OGLERROR_VERTEX_SHADER_PROGRAM_LOAD_ERROR,
	OGLERROR_FRAGMENT_SHADER_PROGRAM_LOAD_ERROR,
	OGLERROR_SHADER_CREATE_ERROR,
	
	OGLERROR_FBO_CREATE_ERROR
};

union GLvec2
{
	struct { GLfloat x, y; };
	GLfloat v[2];
};

union GLvec3
{
	struct { GLfloat r, g, b; };
	struct { GLfloat x, y, z; };
	GLfloat v[3];
};

union GLvec4
{
	struct { GLfloat r, g, b, a; };
	struct { GLfloat x, y, z, w; };
	GLfloat v[4];
};

struct OGLVertex
{
	GLvec4 position;
	GLvec2 texCoord;
	GLvec3 color;
};

struct OGLRenderStates
{
	GLvec2 framebufferSize;
	GLint toonShadingMode;
	GLuint enableAlphaTest;
	GLuint enableAntialiasing;
	GLuint enableEdgeMarking;
	GLuint enableFogAlphaOnly;
	GLuint useWDepth;
	GLfloat alphaTestRef;
	GLfloat fogOffset;
	GLfloat fogStep;
	GLfloat pad_0; // This needs to be here to preserve alignment
	GLvec4 fogColor;
	GLvec4 fogDensity[32]; // Array of floats need to be padded as vec4
	GLvec4 edgeColor[8];
	GLvec4 toonColor[32];
};

struct OGLPolyStates
{
	union
	{
		struct { GLubyte enableTexture, enableFog, enableDepthWrite, setNewDepthForTranslucent; };
		GLubyte flags[4];
	};
	
	union
	{
		struct { GLubyte polyAlpha, polyMode, polyID, valuesPad[1]; };
		GLubyte values[4];
	};
	
	union
	{
		struct { GLubyte texSizeS, texSizeT, texParamPad[2]; };
		GLubyte texParam[4];
	};
};

struct OGLRenderRef
{	
	// OpenGL Feature Support
	GLint stateTexMirroredRepeat;
	
	// VBO
	GLuint vboGeometryVtxID;
	GLuint iboGeometryIndexID;
	GLuint vboPostprocessVtxID;
	GLuint iboPostprocessIndexID;
	
	// PBO
	GLuint pboRenderDataID;
	
	// UBO / TBO
	GLuint uboRenderStatesID;
	GLuint tboPolyStatesID;
	GLuint texPolyStatesID;
	
	// FBO
	GLuint texCIColorID;
	GLuint texCIDepthID;
	GLuint texCIFogAttrID;
	GLuint texCIPolyID;
	GLuint texCIDepthStencilID;
	
	GLuint texGColorID;
	GLuint texGDepthID;
	GLuint texGFogAttrID;
	GLuint texGPolyID;
	GLuint texGDepthStencilID;
	GLuint texPostprocessFogID;
	GLuint texFinalColorID;
	
	GLuint rboMSGColorID;
	GLuint rboMSGDepthID;
	GLuint rboMSGPolyID;
	GLuint rboMSGFogAttrID;
	GLuint rboMSGDepthStencilID;
	GLuint rboFramebufferRGBA6665ID;
	
	GLuint fboClearImageID;
	GLuint fboRenderID;
	GLuint fboPostprocessID;
	GLuint fboMSIntermediateRenderID;
	GLuint selectedRenderingFBO;
	
	// Shader states
	GLuint vertexGeometryShaderID;
	GLuint fragmentGeometryShaderID;
	GLuint programGeometryID;
	
	GLuint vertexEdgeMarkShaderID;
	GLuint vertexFogShaderID;
	GLuint vertexFramebufferOutputShaderID;
	GLuint fragmentEdgeMarkShaderID;
	GLuint fragmentFogShaderID;
	GLuint fragmentFramebufferRGBA6665OutputShaderID;
	GLuint fragmentFramebufferRGBA8888OutputShaderID;
	GLuint programEdgeMarkID;
	GLuint programFogID;
	GLuint programFramebufferRGBA6665OutputID;
	GLuint programFramebufferRGBA8888OutputID;
	
	GLint uniformFramebufferSize;
	GLint uniformStateToonShadingMode;
	GLint uniformStateEnableAlphaTest;
	GLint uniformStateEnableAntialiasing;
	GLint uniformStateEnableEdgeMarking;
	GLint uniformStateEnableFogAlphaOnly;
	GLint uniformStateUseWDepth;
	GLint uniformStateAlphaTestRef;
	GLint uniformStateEdgeColor;
	GLint uniformStateFogColor;
	GLint uniformStateFogDensity;
	GLint uniformStateFogOffset;
	GLint uniformStateFogStep;
	
	GLint uniformPolyTexScale;
	GLint uniformPolyMode;
	GLint uniformPolyEnableDepthWrite;
	GLint uniformPolySetNewDepthForTranslucent;
	GLint uniformPolyAlpha;
	GLint uniformPolyID;
	
	GLint uniformPolyEnableTexture;
	GLint uniformPolyEnableFog;
	
	GLint uniformPolyStateIndex;
	
	GLuint texToonTableID;
	
	// VAO
	GLuint vaoGeometryStatesID;
	GLuint vaoPostprocessStatesID;
	
	// Textures
	std::queue<GLuint> freeTextureIDs;
	
	// Client-side Buffers
	GLfloat *color4fBuffer;
	GLushort *vertIndexBuffer;
	CACHE_ALIGN GLuint workingCIDepthBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIDepthStencilBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIFogAttributesBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	CACHE_ALIGN GLuint workingCIPolyIDBuffer[GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT];
	
	// Vertex Attributes Pointers
	GLvoid *vtxPtrPosition;
	GLvoid *vtxPtrTexCoord;
	GLvoid *vtxPtrColor;
};

struct GFX3D_State;
struct VERTLIST;
struct POLYLIST;
struct INDEXLIST;
struct POLY;
class TexCacheItem;
class OpenGLRenderer;

extern GPU3DInterface gpu3Dgl;
extern GPU3DInterface gpu3DglOld;
extern GPU3DInterface gpu3Dgl_3_2;

extern const GLenum RenderDrawList[4];
extern CACHE_ALIGN const GLfloat divide5bitBy31_LUT[32];
extern const GLfloat PostprocessVtxBuffer[16];
extern const GLubyte PostprocessElementBuffer[6];

extern void texDeleteCallback(TexCacheItem *texItem, void *param1, void *param2);

//This is called by OGLRender whenever it initializes.
//Platforms, please be sure to set this up.
//return true if you successfully init.
extern bool (*oglrender_init)();

//This is called by OGLRender before it uses opengl.
//return true if youre OK with using opengl
extern bool (*oglrender_beginOpenGL)();

//This is called by OGLRender after it is done using opengl.
extern void (*oglrender_endOpenGL)();

//This is called by OGLRender whenever the framebuffer is resized.
extern bool (*oglrender_framebufferDidResizeCallback)(size_t w, size_t h);

// Helper functions for calling the above function pointers at the
// beginning and ending of OpenGL commands.
bool BEGINGL();
void ENDGL();

// These functions need to be assigned by ports that support using an
// OpenGL 3.2 Core Profile context. The OGLRender_3_2.cpp file includes
// the corresponding functions to assign to each function pointer.
//
// If any of these functions are unassigned, then one of the legacy OpenGL
// renderers will be used instead.
extern void (*OGLLoadEntryPoints_3_2_Func)();
extern void (*OGLCreateRenderer_3_2_Func)(OpenGLRenderer **rendererPtr);

FORCEINLINE u32 BGRA8888_32_To_RGBA6665_32(const u32 srcPix);
FORCEINLINE u32 BGRA8888_32Rev_To_RGBA6665_32Rev(const u32 srcPix);
bool IsVersionSupported(unsigned int checkVersionMajor, unsigned int checkVersionMinor, unsigned int checkVersionRevision);

#if defined(ENABLE_SSSE3)
class OpenGLRenderer : public Render3D_SSSE3
#elif defined(ENABLE_SSE2)
class OpenGLRenderer : public Render3D_SSE2
#else
class OpenGLRenderer : public Render3D
#endif
{
private:
	// Driver's OpenGL Version
	unsigned int versionMajor;
	unsigned int versionMinor;
	unsigned int versionRevision;
	
private:
	Render3DError _FlushFramebufferConvertOnCPU(const FragmentColor *__restrict srcFramebuffer, FragmentColor *__restrict dstFramebuffer, u16 *__restrict dstRGBA5551);
	
protected:
	// OpenGL-specific References
	OGLRenderRef *ref;
	
	// OpenGL Feature Support
	bool isVBOSupported;
	bool isPBOSupported;
	bool isFBOSupported;
	bool isMultisampledFBOSupported;
	bool isShaderSupported;
	bool isVAOSupported;
	bool willFlipFramebufferOnGPU;
	bool willConvertFramebufferOnGPU;
	
	// Textures
	TexCacheItem *currTexture;
	
	FragmentColor *_mappedFramebuffer;
	bool _pixelReadNeedsFinish;
	size_t _currentPolyIndex;
	std::vector<u8> _shadowPolyID;
	
	Render3DError FlushFramebuffer(const FragmentColor *__restrict srcFramebuffer, FragmentColor *__restrict dstFramebuffer, u16 *__restrict dstRGBA5551);
	
	// OpenGL-specific methods
	virtual Render3DError CreateVBOs() = 0;
	virtual void DestroyVBOs() = 0;
	virtual Render3DError CreatePBOs() = 0;
	virtual void DestroyPBOs() = 0;
	virtual Render3DError CreateFBOs() = 0;
	virtual void DestroyFBOs() = 0;
	virtual Render3DError CreateMultisampledFBO() = 0;
	virtual void DestroyMultisampledFBO() = 0;
	virtual Render3DError InitGeometryProgram(const std::string &vertexShaderProgram, const std::string &fragmentShaderProgram) = 0;
	virtual void DestroyGeometryProgram() = 0;
	virtual Render3DError CreateVAOs() = 0;
	virtual void DestroyVAOs() = 0;
	virtual Render3DError InitTextures() = 0;
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet) = 0;
	virtual Render3DError InitTables() = 0;
	virtual Render3DError InitPostprocessingPrograms(const std::string &edgeMarkVtxShader,
													 const std::string &edgeMarkFragShader,
													 const std::string &fogVtxShader,
													 const std::string &fogFragShader,
													 const std::string &framebufferOutputVtxShader,
													 const std::string &framebufferOutputRGBA6665FragShader,
													 const std::string &framebufferOutputRGBA8888FragShader) = 0;
	virtual Render3DError DestroyPostprocessingPrograms() = 0;
	virtual Render3DError InitEdgeMarkProgramBindings() = 0;
	virtual Render3DError InitEdgeMarkProgramShaderLocations() = 0;
	virtual Render3DError InitFogProgramBindings() = 0;
	virtual Render3DError InitFogProgramShaderLocations() = 0;
	virtual Render3DError InitFramebufferOutputProgramBindings() = 0;
	virtual Render3DError InitFramebufferOutputShaderLocations() = 0;
	
	virtual Render3DError LoadGeometryShaders(std::string &outVertexShaderProgram, std::string &outFragmentShaderProgram) = 0;
	virtual Render3DError InitGeometryProgramBindings() = 0;
	virtual Render3DError InitGeometryProgramShaderLocations() = 0;
	virtual Render3DError CreateToonTable() = 0;
	virtual Render3DError DestroyToonTable() = 0;
	virtual Render3DError UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer) = 0;
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet) = 0;
	virtual Render3DError ExpandFreeTextures() = 0;
	virtual Render3DError EnableVertexAttributes() = 0;
	virtual Render3DError DisableVertexAttributes() = 0;
	virtual Render3DError DownsampleFBO() = 0;
	virtual Render3DError ReadBackPixels() = 0;
	
	virtual void SetPolygonIndex(const size_t index) = 0;
	
public:
	OpenGLRenderer();
	virtual ~OpenGLRenderer();
	
	virtual Render3DError InitExtensions() = 0;
	virtual Render3DError DeleteTexture(const TexCacheItem *item) = 0;
	
	bool IsExtensionPresent(const std::set<std::string> *oglExtensionSet, const std::string extensionName) const;
	bool ValidateShaderCompile(GLuint theShader) const;
	bool ValidateShaderProgramLink(GLuint theProgram) const;
	void GetVersion(unsigned int *major, unsigned int *minor, unsigned int *revision) const;
	void SetVersion(unsigned int major, unsigned int minor, unsigned int revision);
	
	virtual FragmentColor* GetFramebuffer();
};

class OpenGLRenderer_1_2 : public OpenGLRenderer
{
protected:
	// OpenGL-specific methods
	virtual Render3DError CreateVBOs();
	virtual void DestroyVBOs();
	virtual Render3DError CreatePBOs();
	virtual void DestroyPBOs();
	virtual Render3DError CreateFBOs();
	virtual void DestroyFBOs();
	virtual Render3DError CreateMultisampledFBO();
	virtual void DestroyMultisampledFBO();
	virtual Render3DError CreateVAOs();
	virtual void DestroyVAOs();
	virtual Render3DError InitTextures();
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet);
	virtual Render3DError InitTables();
	
	virtual Render3DError InitGeometryProgram(const std::string &vertexShaderProgram, const std::string &fragmentShaderProgram);
	virtual Render3DError LoadGeometryShaders(std::string &outVertexShaderProgram, std::string &outFragmentShaderProgram);
	virtual Render3DError InitGeometryProgramBindings();
	virtual Render3DError InitGeometryProgramShaderLocations();
	virtual void DestroyGeometryProgram();
	virtual Render3DError InitPostprocessingPrograms(const std::string &edgeMarkVtxShader,
													 const std::string &edgeMarkFragShader,
													 const std::string &fogVtxShader,
													 const std::string &fogFragShader,
													 const std::string &framebufferOutputVtxShader,
													 const std::string &framebufferOutputRGBA6665FragShader,
													 const std::string &framebufferOutputRGBA8888FragShader);
	virtual Render3DError DestroyPostprocessingPrograms();
	virtual Render3DError InitEdgeMarkProgramBindings();
	virtual Render3DError InitEdgeMarkProgramShaderLocations();
	virtual Render3DError InitFogProgramBindings();
	virtual Render3DError InitFogProgramShaderLocations();
	virtual Render3DError InitFramebufferOutputProgramBindings();
	virtual Render3DError InitFramebufferOutputShaderLocations();
	
	virtual Render3DError CreateToonTable();
	virtual Render3DError DestroyToonTable();
	virtual Render3DError UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer);
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet);
	virtual Render3DError ExpandFreeTextures();
	virtual Render3DError EnableVertexAttributes();
	virtual Render3DError DisableVertexAttributes();
	virtual Render3DError DownsampleFBO();
	virtual Render3DError ReadBackPixels();
	
	// Base rendering methods
	virtual Render3DError BeginRender(const GFX3D &engine);
	virtual Render3DError RenderGeometry(const GFX3D_State &renderState, const POLYLIST *polyList, const INDEXLIST *indexList);
	virtual Render3DError EndRender(const u64 frameCount);
	
	virtual Render3DError ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer);
	virtual Render3DError ClearUsingValues(const FragmentColor &clearColor, const FragmentAttributes &clearAttributes) const;
	
	virtual void SetPolygonIndex(const size_t index);
	virtual Render3DError SetupPolygon(const POLY &thePoly);
	virtual Render3DError SetupTexture(const POLY &thePoly, bool enableTexturing);
	virtual Render3DError SetupViewport(const u32 viewportValue);
	
public:
	~OpenGLRenderer_1_2();
	
	virtual Render3DError InitExtensions();
	virtual Render3DError UpdateToonTable(const u16 *toonTableBuffer);
	virtual Render3DError Reset();
	virtual Render3DError RenderFinish();
	virtual Render3DError SetFramebufferSize(size_t w, size_t h);
	
	virtual Render3DError DeleteTexture(const TexCacheItem *item);
};

class OpenGLRenderer_1_3 : public OpenGLRenderer_1_2
{
protected:
	virtual Render3DError CreateToonTable();
	virtual Render3DError UploadClearImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 *__restrict polyIDBuffer);
	
public:
	virtual Render3DError UpdateToonTable(const u16 *toonTableBuffer);
	virtual Render3DError SetFramebufferSize(size_t w, size_t h);
};

class OpenGLRenderer_1_4 : public OpenGLRenderer_1_3
{
protected:
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet);
};

class OpenGLRenderer_1_5 : public OpenGLRenderer_1_4
{
protected:
	virtual Render3DError CreateVBOs();
	virtual void DestroyVBOs();
	virtual Render3DError CreateVAOs();
	
	virtual Render3DError EnableVertexAttributes();
	virtual Render3DError DisableVertexAttributes();
	virtual Render3DError BeginRender(const GFX3D &engine);
	
public:
	~OpenGLRenderer_1_5();
};

class OpenGLRenderer_2_0 : public OpenGLRenderer_1_5
{
protected:
	virtual Render3DError InitExtensions();
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet);
	virtual Render3DError InitPostprocessingPrograms(const std::string &edgeMarkVtxShader,
													 const std::string &edgeMarkFragShader,
													 const std::string &fogVtxShader,
													 const std::string &fogFragShader,
													 const std::string &framebufferOutputVtxShader,
													 const std::string &framebufferOutputRGBA6665FragShader,
													 const std::string &framebufferOutputRGBA8888FragShader);
	virtual Render3DError DestroyPostprocessingPrograms();
	virtual Render3DError InitEdgeMarkProgramBindings();
	virtual Render3DError InitEdgeMarkProgramShaderLocations();
	virtual Render3DError InitFogProgramBindings();
	virtual Render3DError InitFogProgramShaderLocations();
	virtual Render3DError InitFramebufferOutputProgramBindings();
	virtual Render3DError InitFramebufferOutputShaderLocations();
	
	virtual Render3DError EnableVertexAttributes();
	virtual Render3DError DisableVertexAttributes();
	
	virtual Render3DError BeginRender(const GFX3D &engine);
	virtual Render3DError RenderEdgeMarking(const u16 *colorTable, const bool useAntialias);
	virtual Render3DError RenderFog(const u8 *densityTable, const u32 color, const u32 offset, const u8 shift, const bool alphaOnly);
	
	virtual Render3DError SetupPolygon(const POLY &thePoly);
	virtual Render3DError SetupTexture(const POLY &thePoly, bool enableTexturing);
};

class OpenGLRenderer_2_1 : public OpenGLRenderer_2_0
{
protected:
	virtual Render3DError ReadBackPixels();
	
public:
	virtual Render3DError RenderFinish();
};

#endif
