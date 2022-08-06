/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#pragma once

#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "../Ratl/vector_vs.h"
#include "../Ratl/stack_vs.h"
#include "tr_common.h"
#include "tr_public.h"
#include "mdx_format.h"
#include "qvk.h"

#include "tr_shader.h"

extern refimport_t ri;

#ifndef TR_COMPACT_INDICES
#define TR_COMPACT_INDICES 0
#endif
#if TR_COMPACT_INDICES
typedef uint16_t trIndex_t;
static constexpr VkIndexType g_scIndexType = VK_INDEX_TYPE_UINT16;
#else
typedef uint32_t trIndex_t;
static constexpr VkIndexType g_scIndexType = VK_INDEX_TYPE_UINT32;
#endif


// 13 bits
// can't be increased without changing bit packing for drawsurfs
// see QSORT_SHADERNUM_SHIFT
#define SHADERNUM_BITS	13
#define MAX_SHADERS		(1<<SHADERNUM_BITS)


typedef struct dlight_s {
	vec3_t	origin;
	vec3_t	color;				// range from 0.0 to 1.0, should be color normalized
	float	radius;

	vec3_t	transformed;		// origin in local coordinate system
} dlight_t;

typedef struct buffer_s {
	int					size;

	memtag_t			memtag;

	VkBuffer			buf;

	VmaAllocation		allocation;
	VmaAllocationInfo	allocationInfo;

	VkMemoryPropertyFlags memoryPropertyFlags;

} buffer_t;

typedef struct image_s {
	char				imgName[MAX_QPATH];		// game path, including extension
	int					frameUsed;				// for texture usage in frame statistics
	word				width, height;			// source image
	
	memtag_t			memtag;

	VkImage				tex;
	VkImageView			texview;

	VmaAllocation		allocation;

	VkFormat			internalFormat;
	VkImageAspectFlags	allAspectFlags;
	VkImageTiling		tiling;
	VkImageUsageFlags	usage;

	VkSamplerAddressMode wrapClampMode;

	VkImageLayout		layout;
	VkAccessFlags		access;

	bool				mipmap;

	bool				allowPicmip;
	short				iLastLevelUsedOn;

	int					iLastFrameUsedOn;

	VkDescriptorSet		descriptorSet;

} image_t;


#define TR_MAX_FRAMEBUFFER_IMAGES 4

typedef struct {
	image_t					*i;
	bool					external;			// true for images that are not owned by this frame buffer
	VkImageLayout			layout;
} frameBufferImage_t;

typedef struct {
	frameBufferImage_t		images[TR_MAX_FRAMEBUFFER_IMAGES];
	int						numImages;

	int						depthBufferIndex;

	int						width;
	int						height;

	VkRenderPass			renderPass;
	VkFramebuffer			buf;

	VkClearValue			clearValues[TR_MAX_FRAMEBUFFER_IMAGES];

} frameBuffer_t;

typedef struct {
	buffer_t	b;

	int			vertexOffset;
	int			indexOffset;

	int			numVertexes;
	int			numIndexes;

} vertexBuffer_t;


// a Vulkan pipeline layout handlw with additional info
typedef struct {
	VkPipelineLayout	handle;

	int					numDescriptorSets;

} pipelineLayout_t;

// a Vulkan pipeline handle with additional info
typedef struct {
	VkPipeline			handle;

	pipelineLayout_t	*layout;
	int					stateBits;

} pipelineState_t;


// a trRefEntity_t has all the information passed in by
// the client game, as well as some locally derived info
typedef struct {
	refEntity_t	e;

	float		axisLength;		// compensate for non-normalized axis

	qboolean	needDlights;	// true for bmodels that touch a dlight
	qboolean	lightingCalculated;
	vec3_t		lightDir;		// normalized direction towards light
	vec3_t		ambientLight;	// color normalized to 0-255
	int			ambientLightInt;	// 32 bit rgba packed
	vec3_t		directedLight;
	int			dlightBits;

	vertexBuffer_t		*vertexBuffer;
	
	VkDescriptorSet		modelDescriptorSet;
	buffer_t			*modelBuffer;

	tr_shader::model_t	model;

} trRefEntity_t;


// trRefdef_t holds everything that comes in refdef_t,
// as well as the locally generated scene information
typedef struct {
	int			x, y, width, height;
	float		fov_x, fov_y;
	vec3_t		vieworg;
	vec3_t		viewaxis[3];		// transformation matrix

	int			time;				// time in milliseconds for shader effects and other time dependent rendering issues
	int			frametime;
	int			rdflags;			// RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte		areamask[MAX_MAP_AREA_BYTES];
	qboolean	areamaskModified;	// qtrue if areamask changed since last scene

	float		floatTime;			// tr.refdef.time / 1000.0

	// text messages for deform text shaders
	//	char		text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];

	int			num_entities;
	trRefEntity_t	*entities;

	int			num_dlights;
	struct dlight_s	*dlights;

	int			numPolys;
	struct srfPoly_s	*polys;

	int			numDrawSurfs;
	struct drawSurf_s	*drawSurfs;

	qboolean	doLAGoggles;

	int			fogIndex;	//what fog brush the vieworg is in

} trRefdef_t;

typedef struct {
	vec3_t		origin;			// in world coordinates
	vec3_t		axis[3];		// orientation in world
	vec3_t		viewOrigin;		// viewParms->or.origin in local coordinates
	float		modelMatrix[16];
} orientationr_t;


//===============================================================================

typedef enum {
	SS_BAD,
	SS_PORTAL,			// mirrors, portals, viewscreens
	SS_ENVIRONMENT,		// sky box
	SS_OPAQUE,			// opaque

	SS_DECAL,			// scorch marks, etc.
	SS_SEE_THROUGH,		// ladders, grates, grills that may have small blended edges
			// in addition to alpha test
	SS_BANNER,

	SS_INSIDE,			// inside body parts (i.e. heart)
	SS_MID_INSIDE,
	SS_MIDDLE,
	SS_MID_OUTSIDE,
	SS_OUTSIDE,			// outside body parts (i.e. ribs)

	SS_FOG,

	SS_UNDERWATER,		// for items that should be drawn in front of the water plane

	SS_BLEND0,			// regular transparency and filters
	SS_BLEND1,			// generally only used for additive type effects
	SS_BLEND2,
	SS_BLEND3,

	SS_BLEND6,
	SS_STENCIL_SHADOW,
	SS_ALMOST_NEAREST,	// gun smoke puffs

	SS_NEAREST			// blood blobs
} shaderSort_t;


#define MAX_SHADER_STAGES 8

using tr_shader::genFunc_t;
using tr_shader::deform_t;
using tr_shader::alphaGen_t;
using tr_shader::colorGen_t;
using tr_shader::texCoordGen_t;
using tr_shader::texMod_t;

using tr_shader::fog_t;
using tr_shader::fogParms_t;
using tr_shader::sunParms_t;
using tr_shader::skyParms_t;

typedef enum {
	ACFF_NONE,
	ACFF_MODULATE_RGB,
	ACFF_MODULATE_RGBA,
	ACFF_MODULATE_ALPHA
} acff_t;

typedef enum {
	GLFOGOVERRIDE_NONE = 0,
	GLFOGOVERRIDE_BLACK,
	GLFOGOVERRIDE_WHITE,

	GLFOGOVERRIDE_MAX
} EGLFogOverride;

typedef struct {
	genFunc_t	func;

	float base;
	float amplitude;
	float phase;
	float frequency;
} waveForm_t;

#define TR_MAX_TEXMODS 4
#define	MAX_SHADER_DEFORMS	3
typedef struct {
	deform_t	deformation;			// vertex coordinate modification type

	vec3_t		moveVector;
	waveForm_t	deformationWave;
	float		deformationSpread;

	float		bulgeWidth;
	float		bulgeHeight;
	float		bulgeSpeed;
} deformStage_t;


typedef struct {
	texMod_t		type;

	// used for TMOD_TURBULENT and TMOD_STRETCH
	waveForm_t		wave;

	// used for TMOD_TRANSFORM
	float			matrix[2][2];		// s' = s * m[0][0] + t * m[1][0] + trans[0]
	float			translate[2];		// t' = s * m[0][1] + t * m[0][1] + trans[1]

	// used for TMOD_SCALE
	//	float			scale[2];			// s *= scale[0]
	// t *= scale[1]

	// used for TMOD_SCROLL
	//	float			scroll[2];			// s' = s + scroll[0] * time
	// t' = t + scroll[1] * time

	// used for TMOD_ROTATE
	// + = clockwise
	// - = counterclockwise
	//float			rotateSpeed;

} texModInfo_t;


#define SURFSPRITE_NONE			0
#define SURFSPRITE_VERTICAL		1
#define SURFSPRITE_ORIENTED		2
#define SURFSPRITE_EFFECT		3
#define SURFSPRITE_WEATHERFX	4
#define SURFSPRITE_FLATTENED	5

#define SURFSPRITE_FACING_NORMAL	0
#define SURFSPRITE_FACING_UP		1
#define SURFSPRITE_FACING_DOWN		2
#define SURFSPRITE_FACING_ANY		3

typedef struct surfaceSprite_s
{
	int				surfaceSpriteType;
	float			width, height, density, wind, windIdle, fadeDist, fadeMax, fadeScale;
	float			fxAlphaStart, fxAlphaEnd, fxDuration, vertSkew;
	vec2_t			variance, fxGrow;
	int				facing;		// Hangdown on vertical sprites, faceup on others.
} surfaceSprite_t;

typedef struct {
	image_t						*image;

	texCoordGen_t				tcGen;
	vec3_t						*tcGenVectors;

	texModInfo_t				*texMods;
	short						numTexMods;
	short						numImageAnimations;
	float						imageAnimationSpeed;

	bool						isLightmap;
	bool						oneShotAnimMap;
	bool						vertexLightmap;
	bool						isVideoMap;

	int							videoMapHandle;
} textureBundle_t;

typedef struct {
	bool						active;
	bool						isDetail;
	bool						isLightmap;
	byte						index;						// index of stage
	byte						lightmapStyle;
	uint32_t					stateBits;					// GLS_xxxx mask
	acff_t						adjustColorsForFog;
	EGLFogOverride				mGLFogColorOverride;
	surfaceSprite_t				*ss;
	bool						glow;						// Whether this object emits a glow or not.

	textureBundle_t				bundle[NUM_TEXTURE_BUNDLES];

	VkDescriptorSet				descriptorSet;

	tr_shader::shaderStage_t	shaderData;
	buffer_t					*shaderBuffer;

} shaderStage_t;

struct shaderCommands_s;

#define LIGHTMAP_2D			-4		// shader is for 2D rendering
#define LIGHTMAP_BY_VERTEX	-3		// pre-lit triangle models
#define LIGHTMAP_WHITEIMAGE	-2
#define	LIGHTMAP_NONE		-1

typedef enum {
	CT_FRONT_SIDED,
	CT_BACK_SIDED,
	CT_TWO_SIDED
} cullType_t;

typedef enum {
	FP_NONE,		// surface is translucent and will just be adjusted properly
	FP_EQUAL,		// surface is opaque but possibly alpha tested
	FP_LE			// surface is trnaslucent, but still needs a fog pass (fog surface)
} fogPass_t;


typedef struct shader_s {
	char		name[MAX_QPATH];		// game path, including extension
	int			lightmapIndex[MAXLIGHTMAPS];	// for a shader to match, both name and lightmapIndex must match
	byte		styles[MAXLIGHTMAPS];

	int			spec;

	int			index;					// this shader == tr.shaders[index]
	int			sortedIndex;			// this shader == tr.sortedShaders[sortedIndex]

	float		sort;					// lower numbered shaders draw before higher numbered

	int			surfaceFlags;			// if explicitlyDefined, this will have SURF_* flags
	int			contentFlags;

	bool		defaultShader;			// we want to return index 0 if the shader failed to
				// load for some reason, but R_FindShader should
				// still keep a name allocated for it, so if
				// something calls RE_RegisterShader again with
				// the same name, we don't try looking for it again
	bool		explicitlyDefined;		// found in a .shader file
	bool		entityMergable;			// merge across entites optimizable (smoke, blood)

	skyParms_t	*sky;
	fogParms_t	*fogParms;

	int			multitextureEnv;		// 0, GL_MODULATE, GL_ADD (FIXME: put in stage)

	cullType_t	cullType;				// CT_FRONT_SIDED, CT_BACK_SIDED, or CT_TWO_SIDED
	bool		polygonOffset;			// set for decals and other items that must be offset
	bool		noMipMaps;				// for console fonts, 2D elements, etc.
	bool		noPicMip;				// for images that must always be full resolution
	bool		noTC;					// for images that don't want to be texture compressed (namely skies)

	fogPass_t	fogPass;				// draw a blended pass, possibly with depth test equals

	deformStage_t	*deforms[MAX_SHADER_DEFORMS];
	short		numDeforms;

	short		numUnfoggedPasses;
	shaderStage_t	*stages;

	float		timeOffset;                                 // current time offset for this shader

	float		portalRange;

	// True if this shader has a stage with glow in it (just an optimization).
	bool		hasGlow;

	struct	shader_s	*next;
} shader_t;


/*
Ghoul2 Insert Start
*/
// bogus little registration system for hit and location based damage files in hunk memory
/*
typedef struct
{
	byte	*loc;
	int		width;
	int		height;
	char	name[MAX_QPATH];
} hitMatReg_t;

#define MAX_HITMAT_ENTRIES 1000

extern hitMatReg_t		hitMatReg[MAX_HITMAT_ENTRIES];
*/
/*
Ghoul2 Insert End
*/

//=================================================================================

// skins allow models to be retextured without modifying the model file
typedef struct {
	char		name[MAX_QPATH];
	shader_t	*shader;
} skinSurface_t;

typedef struct skin_s {
	char		name[MAX_QPATH];		// game path, including extension
	int			numSurfaces;
	skinSurface_t	*surfaces[128];
} skin_t;


typedef struct {
	orientationr_t	ori;
	orientationr_t	world;
	vec3_t		pvsOrigin;			// may be different than or.origin for portals
	qboolean	isPortal;			// true if this view is through a portal
	qboolean	isMirror;			// the portal is a mirror, invert the face culling
	int			frameSceneNum;		// copied from tr.frameSceneNum
	int			frameCount;			// copied from tr.frameCount
	cplane_t	portalPlane;		// clip anything behind this if mirroring
	int			viewportX, viewportY, viewportWidth, viewportHeight;
	float		fovX, fovY;
	float		projectionMatrix[16];
	cplane_t	frustum[5];
	vec3_t		visBounds[2];
	float		zFar;

	tr_shader::viewParms_t	shaderData;

	buffer_t				*buffer;
	VkDescriptorSet			descriptorSet;
} viewParms_t;


/*
==============================================================================

SURFACES

==============================================================================
*/

// any changes in surfaceType must be mirrored in rb_surfaceTable[]
typedef enum {
	SF_BAD,
	SF_SKIP,				// ignore
	SF_FACE,
	SF_GRID,
	SF_TRIANGLES,
	SF_POLY,
	SF_MD3,
	/*
	Ghoul2 Insert Start
	*/
	SF_MDX,
	/*
	Ghoul2 Insert End
	*/

	SF_FLARE,
	SF_ENTITY,				// beams, rails, lightning, etc that can be determined by entity
	SF_DISPLAY_LIST,

	SF_NUM_SURFACE_TYPES,
	SF_MAX = 0xffffffff			// ensures that sizeof( surfaceType_t ) == sizeof( int )
} surfaceType_t;

typedef struct drawSurf_s {
	unsigned			sort;			// bit combination for fast compares
	surfaceType_t		*surface;		// any of surface*_t
} drawSurf_t;

#define	MAX_FACE_POINTS		64

#define	MAX_PATCH_SIZE		32			// max dimensions of a patch mesh in map file
#define	MAX_GRID_SIZE		65			// max dimensions of a grid mesh in memory

// when cgame directly specifies a polygon, it becomes a srfPoly_t
// as soon as it is called
typedef struct srfPoly_s {
	surfaceType_t	surfaceType;
	qhandle_t		hShader;
	int				fogIndex;
	int				numVerts;
	polyVert_t		*verts;
} srfPoly_t;

typedef struct srfDisplayList_s {
	surfaceType_t	surfaceType;
	int				listNum;
} srfDisplayList_t;

typedef struct srfFlare_s {
	surfaceType_t	surfaceType;
	vec3_t			origin;
	vec3_t			normal;
	vec3_t			color;
} srfFlare_t;

typedef struct srfGridMesh_s {
	surfaceType_t	surfaceType;

	// dynamic lighting information
	int				dlightBits;

	// culling information
	vec3_t			meshBounds[2];
	vec3_t			localOrigin;
	float			meshRadius;

	// lod information, which may be different
	// than the culling information to allow for
	// groups of curves that LOD as a unit
	vec3_t			lodOrigin;
	float			lodRadius;

	// vertexes
	int				width, height;
	float			*widthLodError;
	float			*heightLodError;
	drawVert_t		verts[1];		// variable sized
} srfGridMesh_t;


#define	VERTEXSIZE			(6+(MAXLIGHTMAPS*3))
#define VERTEX_LM			5
#define	VERTEX_COLOR		(5+(MAXLIGHTMAPS*2))


#define	VERTEX_FINAL_COLOR	(5+(MAXLIGHTMAPS*3))

typedef struct {
	surfaceType_t	surfaceType;
	cplane_t	plane;

	vertexBuffer_t	*vertexBuffer;

	// dynamic lighting information
	int			dlightBits;
	
	// triangle definitions (no normals at points)
	int			numPoints;
	int			numIndices;
	int			ofsIndices;
	float		points[1][VERTEXSIZE];	// variable sized
				     // there is a variable length list of indices here also
} srfSurfaceFace_t;


// misc_models in maps are turned into direct geometry by q3map
typedef struct {
	surfaceType_t	surfaceType;
	
	// dynamic lighting information
	int				dlightBits;

	// culling information (FIXME: use this!)
	vec3_t			bounds[2];
	//	vec3_t			localOrigin;
	//	float			radius;

	cplane_t		plane;

	vertexBuffer_t	*vertexBuffer;

} srfTriangles_t;


typedef struct {
	surfaceType_t	surfaceType;

	const char		*name;

	vertexBuffer_t	*vertexBuffer;

	int				numShaders;
	md3Shader_t		*shaders;

	int				numVertexes;
	int				numIndexes;
	int				numFrames;

} trMD3Surface_t;


typedef struct {
	mdxmSurface_t	*header;
	vertexBuffer_t	*vertexBuffer;
} trMdxmSurface_t;


extern	void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])(void *);

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/


//
// in memory representation
//

#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2

typedef struct msurface_s {
	int					viewCount;		// if == tr.viewCount, already added
	struct shader_s		*shader;
	int					fogIndex;

	surfaceType_t		*data;			// any of srf*_t
} msurface_t;



#define	CONTENTS_NODE		-1

typedef struct mnode_s {
	// common with leaf and node
	int			contents;		// -1 for nodes, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current
	vec3_t		mins, maxs;		// for bounding box culling
	struct mnode_s	*parent;

	// node specific
	cplane_t	*plane;
	struct mnode_s	*children[2];

	// leaf specific
	int			cluster;
	int			area;

	msurface_t	**firstmarksurface;
	int			nummarksurfaces;
} mnode_t;

typedef struct {
	vec3_t		bounds[2];		// for culling
	msurface_t	*firstSurface;
	int			numSurfaces;
} bmodel_t;

typedef struct {
	byte		ambientLight[MAXLIGHTMAPS][3];
	byte		directLight[MAXLIGHTMAPS][3];
	byte		styles[MAXLIGHTMAPS];
	byte		latLong[2];
} mgrid_t;

typedef struct {
	char		name[MAX_QPATH];		// ie: maps/tim_dm2.bsp
	char		baseName[MAX_QPATH];	// ie: tim_dm2

	int			numShaders;
	dshader_t	*shaders;

	bmodel_t	*bmodels;

	int			numplanes;
	cplane_t	*planes;

	int			numnodes;		// includes leafs
	int			numDecisionNodes;
	mnode_t		*nodes;

	int			numsurfaces;
	msurface_t	*surfaces;

	int			nummarksurfaces;
	msurface_t	**marksurfaces;

	int			numfogs;
	fog_t		*fogs;
	int			globalFog;

	buffer_t	*fogsBuffer;

	int			startLightMapIndex;

	vec3_t		lightGridOrigin;
	vec3_t		lightGridSize;
	vec3_t		lightGridInverseSize;
	int			lightGridBounds[3];
	mgrid_t			*lightGridData;
	unsigned short	*lightGridArray;
	int			numGridArrayElements;

	int			numClusters;
	int			clusterBytes;

	const byte	*vis;			// may be passed in by CM_LoadMap to save space

	byte		*novis;			// clusterBytes of 0xff
} world_t;

//======================================================================

typedef enum {
	MOD_BAD,
	MOD_BRUSH,
	MOD_MESH,
	/*
	Ghoul2 Insert Start
	*/
	MOD_MDXM,
	MOD_MDXA
	/*
	Ghoul2 Insert End
	*/

} modtype_t;

typedef struct trMD3Model_s {
	int				flags;

	int				numFrames;
	md3Frame_t		*frames;

	int				numTags;
	md3Tag_t		*tags;

	int				numSurfaces;
	trMD3Surface_t	*surfaces;
	
	int				numSkins;
	
} trMD3Model_t;

typedef struct trMdxmModel_s {
	mdxmHeader_t	*header;
	trMdxmSurface_t *surfaces;
} trMdxmModel_t;

typedef struct model_s {
	char			name[MAX_QPATH];
	modtype_t		type;
	int				index;				// model = tr.models[model->mod_index]

	int				dataSize;			// just for listing purposes
	bmodel_t		*bmodel;			// only if type == MOD_BRUSH
	trMD3Model_t	*md3[MD3_MAX_LODS]; // only if type == MOD_MESH
					/*
					Ghoul2 Insert Start
					*/
	trMdxmModel_t	*mdxm; // only if type == MOD_GL2M which is a GHOUL II Mesh file NOT a GHOUL II animation file
	mdxaHeader_t	*mdxa;				// only if type == MOD_GL2A which is a GHOUL II Animation file
					/*
					Ghoul2 Insert End
					*/
	unsigned char	numLods;
	bool			bspInstance;			// model is a bsp instance
} model_t;


#define	MAX_MOD_KNOWN	1024

void		R_ModelInit (void);
model_t		*R_GetModelByHandle( qhandle_t hModel );
void		R_LerpTag( orientation_t *tag, qhandle_t handle, int startFrame, int endFrame,
		float frac, const char *tagName );
void		R_ModelBounds( qhandle_t handle, vec3_t mins, vec3_t maxs );

void		R_Modellist_f (void);

//====================================================
#define	MAX_DRAWIMAGES			2048
#define	MAX_LIGHTMAPS			256
#define	MAX_SKINS				512	//1024


#define	MAX_DRAWSURFS			0x10000
#define	DRAWSURF_MASK			(MAX_DRAWSURFS-1)

/*

the drawsurf sort data is packed into a single 32 bit value so it can be
compared quickly during the qsorting process

the bits are allocated as follows:

22 - 31	: sorted shader index
12 - 21	: entity index
3 - 7	: fog index
2		: used to be clipped flag
0 - 1	: dlightmap index
#define	QSORT_SHADERNUM_SHIFT	22
#define	QSORT_ENTITYNUM_SHIFT	12
#define	QSORT_FOGNUM_SHIFT		3

	TTimo - 1.32
31	:  used for alpha fading models (drawn last)
18-30 : sorted shader index
7-17  : entity index
2-6   : fog index
0-1   : dlightmap index
*/

#define	QSORT_FOGNUM_SHIFT		2
#define	QSORT_REFENTITYNUM_SHIFT	7
#define	QSORT_SHADERNUM_SHIFT	(QSORT_REFENTITYNUM_SHIFT+REFENTITYNUM_BITS)
// Note: 32nd bit is reserved for RF_ALPHA_FADE voodoo magic
// see R_AddEntitySurfaces tr.shiftedEntityNum
#if (QSORT_SHADERNUM_SHIFT+SHADERNUM_BITS) > 31
	#error "Need to update sorting, too many bits."
#endif

extern	int			gl_filter_min, gl_filter_max;

/*
** performanceCounters_t
*/
typedef struct {
	int		c_sphere_cull_patch_in, c_sphere_cull_patch_clip, c_sphere_cull_patch_out;
	int		c_box_cull_patch_in, c_box_cull_patch_clip, c_box_cull_patch_out;
	int		c_sphere_cull_md3_in, c_sphere_cull_md3_clip, c_sphere_cull_md3_out;
	int		c_box_cull_md3_in, c_box_cull_md3_clip, c_box_cull_md3_out;

	int		c_leafs;
	int		c_dlightSurfaces;
	int		c_dlightSurfacesCulled;
} frontEndCounters_t;


#define MAX_OUTIMAGES 4
#define MIN_UPLOADBUFFER_SIZE (65536*64*16) // 4MB

#define MAX_FRAME_CMDBUFFERS 3	//	0 - buffer upload cmd buffer
								//	1 - render commands
								//	2 - inter-frame commands - in case we have to submit anything between RE_BeginFrame and RE_EndFrame

typedef struct {
	buffer_t				*buffer;
	int						offset;
	int						age;
} uploadBuffer_t;

template<int CAPACITY_>
struct uploadBufferPool_t {
	static constexpr int CAPACITY = CAPACITY_;

	uploadBuffer_t					buffers[CAPACITY];
	ratl::stack_vs<int, CAPACITY>	freeBuffers;

	uint32_t						used[Q_max(1, CAPACITY / (8 * sizeof( uint32_t )))];

	uploadBufferPool_t() {
		int i;
		for( i = 0; i < CAPACITY; ++i ) {
			freeBuffers.push((CAPACITY - 1) - i);
		}
		memset( used, 0, sizeof( used ) );
		memset( buffers, 0, sizeof( buffers ) );
	}

	int alloc() {
		if( freeBuffers.empty() )
			return NULL;
		int index = freeBuffers.top();
		freeBuffers.pop();
		return index;
	}

	void free(int i) {
		freeBuffers.push(i);
	}

	bool full() const {
		return freeBuffers.empty();
	}

	bool is_used(int i) const {
		return (used[i >> 5] & (1 << (i & 0xF))) != 0;
	}

	uploadBuffer_t &operator[]( int i ) {
		return buffers[i];
	}
};

// the renderer front end should never modify vkstate_t
typedef struct {
	VkInstance				instance;
	VkPhysicalDevice		physicalDevice;
	VkDevice				device;

	VkQueue					queue;
	uint32_t				queueFamilyIndex;

	VmaAllocator			allocator;

	VkSurfaceKHR			surface;
	VkSwapchainKHR			swapchain;

	VkFormat				swapchainImageFormat;
	VkExtent2D				swapchainImageExtent;

	VkDescriptorPool		descriptorPool;

	VkPipelineCache			pipelineCache;

	uint32_t				imgcount;
	image_t					images[MAX_OUTIMAGES];

	VkCommandPool			cmdpool;
	VkCommandBuffer			cmdbuffers[MAX_OUTIMAGES * 3];

	VkFence					fences[MAX_OUTIMAGES];
	VkSemaphore				semaphores[MAX_OUTIMAGES];

	uploadBufferPool_t<32>	uploadBuffers;
	ratl::vector_vs<int, 16>	frameUploadBuffers[MAX_OUTIMAGES]; // upload buffers used in each frame

	int						resnum;
	uint32_t				imagenum;

	bool					wireframeRasterizationSupported;

	VkPhysicalDeviceProperties physicalDeviceProperties;
	char					physicalDeviceDriverVersion[32];

	PFN_vkSetDebugUtilsObjectNameEXT	pfnSetDebugObjectName;
	PFN_vkCmdBeginDebugUtilsLabelEXT	pfnBeginDebugUtilsLabel;
	PFN_vkCmdEndDebugUtilsLabelEXT		pfnEndDebugUtilsLabel;
	PFN_vkCreateDebugUtilsMessengerEXT	pfnCreateDebugMessenger;
	PFN_vkDestroyDebugUtilsMessengerEXT	pfnDestroyDebugMessenger;

	VkDebugUtilsMessengerEXT			debugMessenger;

	PFN_vkCmdPushDescriptorSetKHR		pfnPushDescriptorSet;

	// samplers
	VkSampler				wrapModeSampler;
	VkSampler				clampModeSampler;

	VkSampler				pointClampSampler;
	VkSampler				pointWrapSampler;
	VkSampler				linearClampSampler;
	VkSampler				linearWrapSampler;

	VkDescriptorSetLayout	commonDescriptorSetLayout;
	VkDescriptorSetLayout	samplerDescriptorSetLayout;
	VkDescriptorSetLayout	shaderDescriptorSetLayout;
	VkDescriptorSetLayout	modelDescriptorSetLayout;
	VkDescriptorSetLayout	textureDescriptorSetLayout;
	VkDescriptorSetLayout	viewDescriptorSetLayout;
	VkDescriptorSetLayout	ghoul2BonesDescriptorSetLayout;
	
	pipelineLayout_t		shadePipelineLayout;
	pipelineLayout_t		ghoul2ShadePipelineLayout;

	// Debug pipelines
	pipelineState_t			wireframePipeline;
	pipelineState_t			wireframeXRayPipeline;
	pipelineLayout_t		wireframePipelineLayout;

	// Handles to the Glow Effect resources.
	pipelineState_t			glowBlurPipeline;
	pipelineLayout_t		glowBlurPipelineLayout;

	pipelineState_t			glowCombinePipeline;
	pipelineLayout_t		glowCombinePipelineLayout;

} vkstate_t;


typedef struct {
	int		c_surfaces, c_shaders, c_vertexes, c_indexes, c_totalIndexes;
	float	c_overDraw;

	int		c_dlightVertexes;
	int		c_dlightIndexes;

	int		c_flareAdds;
	int		c_flareTests;
	int		c_flareRenders;

	int		msec;			// total msec for backend run
} backEndCounters_t;

// all state modified by the back end is seperated
// from the front end state
typedef struct {
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	orientationr_t	ori;
	backEndCounters_t	pc;
	qboolean	isHyperspace;
	trRefEntity_t	*currentEntity;
	qboolean	skyRenderedThisView;	// flag for drawing sun

	qboolean	projection2D;	// if qtrue, drawstretchpic doesn't need to change modes
	tr_shader::byte4	color2D;
	qboolean	vertexes2D;		// shader needs to be finished
	trRefEntity_t	entity2D;	// currentEntity will point at this when doing 2D rendering
} backEndState_t;

/*
** trGlobals_t
**
** Most renderer globals are defined here.
** backend functions should never modify any of these fields,
** but may read fields that aren't dynamically modified
** by the frontend.
*/
#define NUM_SCRATCH_IMAGES 16

typedef struct {
	qboolean				registered;		// cleared at shutdown, set at beginRegistration

	int						visCount;		// incremented every time a new vis cluster is entered
	int						frameCount;		// incremented every frame
	int						sceneCount;		// incremented every scene
	int						viewCount;		// incremented every view (twice a scene if portaled)
								// and every R_MarkFragments call

	int						frameSceneNum;	// zeroed at RE_BeginFrame

	qboolean				worldMapLoaded;
	world_t					*world;
	char					worldDir[MAX_QPATH];		// ie: maps/tim_dm2 (copy of world_t::name sans extension but still includes the path)

	const byte				*externalVisData;	// from RE_SetWorldVisData, shared with CM_Load

	image_t					*defaultImage;
	image_t					*scratchImage[NUM_SCRATCH_IMAGES];
	image_t					*fogImage;
	image_t					*noiseImage;
	image_t					*dlightImage;	// inverse-quare highlight for projective adding
	image_t					*whiteImage;			// full of 0xff
	image_t					*identityLightImage;	// full of tr.identityLightByte

	image_t					*screenImage; //reserve us a gl texnum to use with RF_DISTORTION

	image_t					*screenshotImage;

	buffer_t				*fogsBuffer;

	VkDescriptorSet			commonDescriptorSet;
	VkDescriptorSet			samplerDescriptorSet;

	frameBuffer_t			*postProcessFrameBuffer;
	
	// A rectangular texture representing the normally rendered scene.
	frameBuffer_t			*sceneFrameBuffer;
	
	// Image the glowing objects are rendered to. - AReis
	frameBuffer_t			*glowFrameBuffer;

	// Image used to downsample and blur scene to.	- AReis
	frameBuffer_t			*glowBlurFrameBuffer;

	shader_t				*defaultShader;
	shader_t				*shadowShader;
	shader_t				*distortionShader;
	shader_t				*projectionShadowShader;

	shader_t				*sunShader;

	int						numLightmaps;
	image_t					*lightmaps[MAX_LIGHTMAPS];

	trRefEntity_t			*currentEntity;
	trRefEntity_t			worldEntity;		// point currentEntity at this when rendering world
	int						currentEntityNum;
	unsigned				shiftedEntityNum;	// currentEntityNum << QSORT_REFENTITYNUM_SHIFT (possible with high bit set for RF_ALPHA_FADE)
	model_t					*currentModel;

	viewParms_t				viewParms;

	float					identityLight;		// 1.0 / ( 1 << overbrightBits )
	int						identityLightByte;	// identityLight * 255
	int						overbrightBits;		// r_overbrightBits->integer, but set to 0 if no hw gamma

	orientationr_t			ori;					// for current entity

	trRefdef_t				refdef;

	int						viewCluster;

	sunParms_t				sunParms;
	int						sunSurfaceLight;	// from the sky shader for this level


	frontEndCounters_t		pc;
	int						frontEndMsec;		// not in pc due to clearing issue

	float					rangedFog;
	float					distanceCull;

	// GPU-visible buffer with data stored in this strucutre
	buffer_t				*globalsBuffer;
	tr_shader::trGlobals_t	globals;

	buffer_t				*funcTablesBuffer;

	//
	// put large tables at the end, so most elements will be
	// within the +/32K indexed range on risc processors
	//
	model_t					*models[MAX_MOD_KNOWN];
	int						numModels;

	world_t					bspModels[MAX_SUB_BSP];
	int						numBSPModels;

	// shader indexes from other modules will be looked up in tr.shaders[]
	// shader indexes from drawsurfs will be looked up in sortedShaders[]
	// lower indexed sortedShaders must be rendered first (opaque surfaces before translucent)
	int						numShaders;
	shader_t				*shaders[MAX_SHADERS];
	shader_t				*sortedShaders[MAX_SHADERS];
	int						iNumDeniedShaders;	// used for error-messages only

	int						numSkins;
	skin_t					*skins[MAX_SKINS];
} trGlobals_t;

int		 R_Images_StartIteration(void);
image_t *R_Images_GetNextIteration(void);
void	 R_Images_Clear(void);
void	 R_Images_DeleteLightMaps(void);
void	 R_Images_DeleteImage(image_t *pImage);

int		 R_Buffers_StartIteration(void);
buffer_t *R_Buffers_GetNextIteration(void);
void	 R_Buffers_Clear(void);
void	 R_Buffers_DeleteBuffer(buffer_t *pBuffer);


extern backEndState_t	backEnd;
extern trGlobals_t	tr;
extern glconfig_t	glConfig;		// outside of TR since it shouldn't be cleared during ref re-init
extern vkstate_t	vkState;		// outside of TR since it shouldn't be cleared during ref re-init
extern window_t		window;


//
// cvars
//
extern cvar_t	*r_ignore;				// used for debugging anything
extern cvar_t	*r_verbose;				// used for verbose debug spew

extern cvar_t	*r_znear;				// near Z clip plane

extern cvar_t	*r_stencilbits;			// number of desired stencil bits
extern cvar_t	*r_depthbits;			// number of desired depth bits
extern cvar_t	*r_colorbits;			// number of desired color bits, only relevant for fullscreen
extern cvar_t	*r_stereo;				// desired pixelformat stereo flag
extern cvar_t	*r_texturebits;			// number of desired texture bits
				// 0 = use framebuffer depth
				// 16 = use 16-bit textures
				// 32 = use 32-bit textures
				// all else = error
extern cvar_t	*r_texturebitslm;		// number of desired lightmap texture bits

extern cvar_t	*r_measureOverdraw;		// enables stencil buffer overdraw measurement

extern cvar_t	*r_lodbias;				// push/pull LOD transitions
extern cvar_t	*r_lodscale;

extern cvar_t	*r_primitives;			// "0" = based on compiled vertex array existance
			     // "1" = glDrawElemet tristrips
			     // "2" = glDrawElements triangles
			     // "-1" = no drawing

extern cvar_t	*r_fastsky;				// controls whether sky should be cleared or drawn
extern cvar_t	*r_drawSun;				// controls drawing of sun quad
extern cvar_t	*r_dynamiclight;		// dynamic lights enabled/disabled
// rjr - removed for hacking extern cvar_t	*r_dlightBacks;			// dlight non-facing surfaces for continuity

extern	cvar_t	*r_norefresh;			// bypasses the ref rendering
extern	cvar_t	*r_drawentities;		// disable/enable entity rendering
extern	cvar_t	*r_drawworld;			// disable/enable world rendering
extern	cvar_t	*r_drawfog;				// disable/enable fog rendering
extern	cvar_t	*r_speeds;				// various levels of information display
extern  cvar_t	*r_detailTextures;		// enables/disables detail texturing stages

extern	cvar_t	*r_novis;				// disable/enable usage of PVS
extern	cvar_t	*r_nocull;
extern	cvar_t	*r_facePlaneCull;		// enables culling of planar surfaces with back side test
extern	cvar_t	*r_nocurves;
extern	cvar_t	*r_showcluster;

extern cvar_t	*r_dlightStyle;
extern cvar_t	*r_surfaceSprites;
extern cvar_t	*r_surfaceWeather;

extern cvar_t	*r_windSpeed;
extern cvar_t	*r_windAngle;
extern cvar_t	*r_windGust;
extern cvar_t	*r_windDampFactor;
extern cvar_t	*r_windPointForce;
extern cvar_t	*r_windPointX;
extern cvar_t	*r_windPointY;

extern cvar_t	*r_mode;				// video mode
extern cvar_t	*r_fullscreen;
extern cvar_t	*r_noborder;			// disable border in windowed mode
extern cvar_t	*r_centerWindow;		// override vid_x/ypos and center the window
extern cvar_t	*r_gamma;
extern cvar_t	*r_displayRefresh;		// optional display refresh option
extern cvar_t	*r_ignorehwgamma;		// overrides hardware gamma capabilities

extern cvar_t	*r_allowExtensions;				// global enable/disable of optional Vulkan extensions
extern cvar_t	*r_ext_compressed_textures;		// these control use of specific extensions
extern cvar_t	*r_ext_compressed_lightmaps;	// turns on compression of lightmaps, off by default
extern cvar_t	*r_ext_preferred_tc_method;
extern cvar_t	*r_ext_gamma_control;
extern cvar_t	*r_ext_texenv_op;
extern cvar_t	*r_ext_multitexture;
extern cvar_t	*r_ext_compiled_vertex_array;
extern cvar_t	*r_ext_texture_env_add;
extern cvar_t	*r_ext_texture_filter_anisotropic;

extern cvar_t	*r_DynamicGlow;
extern cvar_t	*r_DynamicGlowPasses;
extern cvar_t	*r_DynamicGlowDelta;
extern cvar_t	*r_DynamicGlowIntensity;
extern cvar_t	*r_DynamicGlowSoft;
extern cvar_t	*r_DynamicGlowWidth;
extern cvar_t	*r_DynamicGlowHeight;

extern	cvar_t	*r_nobind;						// turns off binding to appropriate textures
extern	cvar_t	*r_singleShader;				// make most world faces use default shader
extern	cvar_t	*r_colorMipLevels;				// development aid to see texture mip usage
extern	cvar_t	*r_picmip;						// controls picmip values
extern	cvar_t	*r_finish;
extern	cvar_t	*r_swapInterval;
extern	cvar_t	*r_textureMode;
extern	cvar_t	*r_offsetFactor;
extern	cvar_t	*r_offsetUnits;

extern	cvar_t	*r_fullbright;					// avoid lightmap pass
extern	cvar_t	*r_lightmap;					// render lightmaps only
extern	cvar_t	*r_vertexLight;					// vertex lighting mode for better performance

extern	cvar_t	*r_logFile;						// number of frames to emit GL logs
extern	cvar_t	*r_showtris;					// enables wireframe rendering of the world
extern	cvar_t	*r_showtriscolor;				// changes wireframe color
extern	cvar_t	*r_showsky;						// forces sky in front of all surfaces
extern	cvar_t	*r_shownormals;					// draws wireframe normals
extern	cvar_t	*r_clear;						// force screen clear every frame

extern	cvar_t	*r_shadows;						// controls shadows: 0 = none, 1 = blur, 2 = stencil, 3 = black planar projection
extern	cvar_t	*r_flares;						// light flares

extern	cvar_t	*r_intensity;

extern	cvar_t	*r_lockpvs;
extern	cvar_t	*r_noportals;
extern	cvar_t	*r_portalOnly;

extern	cvar_t	*r_subdivisions;
extern	cvar_t	*r_lodCurveError;
extern	cvar_t	*r_skipBackEnd;

extern	cvar_t	*r_ignoreGLErrors;

extern	cvar_t	*r_overBrightBits;
extern	cvar_t	*r_mapOverBrightBits;

extern	cvar_t	*r_debugSurface;
extern	cvar_t	*r_simpleMipMaps;

extern	cvar_t	*r_showImages;
extern	cvar_t	*r_debugSort;
extern	cvar_t	*r_debugStyle;

/*
Ghoul2 Insert Start
*/
extern	cvar_t	*r_noGhoul2;
/*
Ghoul2 Insert End
*/

extern	cvar_t	*r_environmentMapping;
//====================================================================

void R_SwapBuffers( int );

void R_RenderView( viewParms_t *parms );

void R_AddMD3Surfaces( trRefEntity_t *e );
void R_AddNullModelSurfaces( trRefEntity_t *e );
void R_AddBeamSurfaces( trRefEntity_t *e );
void R_AddRailSurfaces( trRefEntity_t *e, qboolean isUnderwater );
void R_AddLightningBoltSurfaces( trRefEntity_t *e );

void R_AddPolygonSurfaces( void );

void R_DecomposeSort( unsigned sort, int *entityNum, shader_t **shader,
		      int *fogNum, int *dlightMap );

void R_AddDrawSurf( const surfaceType_t *surface, const shader_t *shader, int fogIndex, int dlightMap );


#define	CULL_IN		0		// completely unclipped
#define	CULL_CLIP	1		// clipped by one or more planes
#define	CULL_OUT	2		// completely outside the clipping planes
void R_LocalNormalToWorld (const vec3_t local, vec3_t world);
void R_LocalPointToWorld (const vec3_t local, vec3_t world);
void R_WorldNormalToEntity (const vec3_t localVec, vec3_t world);
int R_CullLocalBox (const vec3_t bounds[2]);
int R_CullPointAndRadius( const vec3_t pt, float radius );
int R_CullLocalPointAndRadius( const vec3_t pt, float radius );

void R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms, orientationr_t *ori );

/*
** Vulkan wrapper/helper functions
*/
PFN_vkVoidFunction VK_GetProcAddress( const char *name, qboolean required = qfalse );
void VK_BeginFrame( void );
void VK_EndFrame( void );
void VK_TextureMode( const char *string );
uploadBuffer_t *VK_GetUploadBuffer( int uploadSize );
void VK_PrepareUploadBuffers( void );
void VK_UploadImage( image_t *im, const byte *pic, int width, int height, int mip );
void VK_UploadBuffer( buffer_t *buffer, const byte *data, int size, int offset );
void *VK_BeginUploadBuffer( buffer_t *buffer, int size, int offset );
void VK_EndUploadBuffer();
void VK_BeginUpload();
void VK_EndUpload();
VkCommandBuffer VK_GetUploadCommandBuffer();
void VK_SetImageLayout( image_t *im, VkImageLayout dstLayout, VkAccessFlags dstAccess );
void VK_SetImageLayout2( VkCommandBuffer cmdbuf, image_t *im, VkImageLayout dstLayout, VkAccessFlags dstAccess );
void VK_CopyImage( image_t *dst, image_t *src );
void VK_BindImage( image_t *image, int loc = 0 );
void VK_ClearColorImage( image_t *image, const VkClearColorValue *value );
void VK_ClearDepthStencilImage( image_t *image, const VkClearDepthStencilValue *value );
void VK_AllocateDescriptorSet( VkDescriptorSetLayout layout, VkDescriptorSet *set );
void VK_DeleteDescriptorSet( VkDescriptorSet set );
void VK_SetDebugObjectName( uint64_t object, VkObjectType type, const char *name );
int VK_AlignUniformBufferSize( int structureSize );
void R_BindDescriptorSet( int space, VkDescriptorSet descriptorSet );

template<typename T>
inline T VK_GetProcAddress( const char *name, qboolean required = qfalse ) {
	return ( T )VK_GetProcAddress( name, required );
}

template<typename T>
inline void VK_SetDebugObjectName( T object, VkObjectType type, const char *name ) {
	VK_SetDebugObjectName( (uint64_t)object, type, name );
}

void RB_BeginDebugRegion( const char *name, uint32_t color = 0xFFFFFFFF );
void RB_EndDebugRegion( void );

#define GLS_SRCBLEND_ZERO						0x00000001
#define GLS_SRCBLEND_ONE						0x00000002
#define GLS_SRCBLEND_DST_COLOR					0x00000003
#define GLS_SRCBLEND_ONE_MINUS_DST_COLOR		0x00000004
#define GLS_SRCBLEND_SRC_ALPHA					0x00000005
#define GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA		0x00000006
#define GLS_SRCBLEND_DST_ALPHA					0x00000007
#define GLS_SRCBLEND_ONE_MINUS_DST_ALPHA		0x00000008
#define GLS_SRCBLEND_ALPHA_SATURATE				0x00000009
#define		GLS_SRCBLEND_BITS					0x0000000f

#define GLS_DSTBLEND_ZERO						0x00000010
#define GLS_DSTBLEND_ONE						0x00000020
#define GLS_DSTBLEND_SRC_COLOR					0x00000030
#define GLS_DSTBLEND_ONE_MINUS_SRC_COLOR		0x00000040
#define GLS_DSTBLEND_SRC_ALPHA					0x00000050
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA		0x00000060
#define GLS_DSTBLEND_DST_ALPHA					0x00000070
#define GLS_DSTBLEND_ONE_MINUS_DST_ALPHA		0x00000080
#define		GLS_DSTBLEND_BITS					0x000000f0

#define GLS_DEPTHMASK_TRUE						0x00000100

#define GLS_POLYMODE_LINE						0x00001000

#define GLS_DEPTHTEST_DISABLE					0x00010000
#define GLS_DEPTHFUNC_EQUAL						0x00020000

#define GLS_CULL_NONE							0x00040000

#define GLS_INPUT_MD3							0x00100000
#define GLS_INPUT_GLM							0x00200000
#define GLS_INPUT_GLA							0x00300000
#define GLS_INPUT_BITS							0x00300000

#define GLS_ATEST_GT_0							0x10000000
#define GLS_ATEST_LT_80							0x20000000
#define GLS_ATEST_GE_80							0x40000000
#define GLS_ATEST_GE_C0							0x80000000
#define	GLS_ATEST_BITS							0xF0000000

#define GLS_DEFAULT			GLS_DEPTHMASK_TRUE
#define GLS_ALPHA			(GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)

#define GLS_CURRENT								0xFFFFFFFF

void	RE_StretchRaw (int x, int y, int w, int h, int cols, int rows, const byte *data, int iClient, qboolean bDirty );
void	RE_UploadCinematic (int cols, int rows, const byte *data, int client, qboolean dirty);
void	RE_GetScreenShot(byte *data, int w, int h);
byte*	RE_TempRawImage_ReadFromFile(const char *psLocalFilename, int *piWidth, int *piHeight, byte *pbReSampleBuffer, qboolean qbVertFlip);
void	RE_TempRawImage_CleanUp();

void		RE_BeginRegistration( glconfig_t *glconfig );
void		RE_LoadWorldMap( const char *mapname );
void		RE_SetWorldVisData( const byte *vis );
qhandle_t	RE_RegisterModel( const char *name );
qhandle_t	RE_RegisterSkin( const char *name );
int			RE_GetAnimationCFG(const char *psCFGFilename, char *psDest, int iDestSize);
void		RE_Shutdown( qboolean destroyWindow );

void		RE_RegisterMedia_LevelLoadBegin(const char *psMapName, ForceReload_e eForceReload, qboolean bAllowScreenDissolve);
void		RE_RegisterMedia_LevelLoadEnd(void);
int			RE_RegisterMedia_GetLevel(void);
qboolean	RE_RegisterModels_LevelLoadEnd(qboolean bDeleteEverythingNotUsedThisLevel = qfalse );
void		*RE_RegisterModels_Malloc( int iSize, void *pvDiskBufferIfJustLoaded, const char *psModelFileName, qboolean *pqbAlreadyFound, memtag_t eTag );
void		RE_RegisterModels_StoreShaderRequest(const char *psModelFileName, const char *psShaderName, const int *piShaderIndexPoke);
void		RE_RegisterModels_Info_f(void);
qboolean	RE_RegisterImages_LevelLoadEnd(void);
void		RE_RegisterImages_Info_f(void);
void		RE_RegisterBuffers_Info_f( void );


model_t		*R_AllocModel( void );

void		R_Init( void );
void		VK_InitSwapchain( void );

image_t		*R_FindImageFile( const char *name, qboolean mipmap, qboolean allowPicmip, qboolean allowTC, VkSamplerAddressMode wrapClampMode );

image_t		*R_CreateImage( const char *name, const byte *pic, int width, int height, VkFormat format, qboolean mipmap, qboolean allowPicmip, qboolean allowTC, VkSamplerAddressMode wrapClampMode );
image_t		*R_CreateTransientImage( const char *name, int width, int height, VkFormat format, VkSamplerAddressMode wrapClampMode );
image_t		*R_CreateReadbackImage( const char *name, int width, int height, VkFormat format );

void		R_ResizeImage( image_t *image, int width, int height );

buffer_t	*R_CreateBuffer( int size, VkBufferUsageFlags usage, VkMemoryPropertyFlags requiredFlags, memtag_t tag = TAG_HUNKALLOC );
vertexBuffer_t	*R_CreateVertexBuffer( int numVertexes, int numIndexes, int indexOffset = 0, memtag_t tag = TAG_HUNKALLOC );

qboolean	R_GetModeInfo( int *width, int *height, int mode );

void		R_SetColorMappings( void );
void		R_GammaCorrect( byte *buffer, int bufSize );

void	R_ImageList_f( void );
void	R_BufferList_f( void );
void	R_SkinList_f( void );
void	R_FontList_f( void );
void	R_ScreenShot_f( void );
void	R_ScreenShotTGA_f( void );

void	R_InitFogTable( void );
float	R_FogFactor( float s, float t );
void	R_InitImages( void );
void	R_DeleteTextures( void );
void	R_InitBuffers( void );
void	R_DeleteBuffers( void );
float	R_SumOfUsedImages( qboolean bUseFormat );
void	R_InitSkins( void );
skin_t	*R_GetSkinByHandle( qhandle_t hSkin );


//
// tr_shader.c
//
extern	const int	lightmapsNone[MAXLIGHTMAPS];
extern	const int	lightmaps2d[MAXLIGHTMAPS];
extern	const int	lightmapsVertex[MAXLIGHTMAPS];
extern	const int	lightmapsFullBright[MAXLIGHTMAPS];
extern	const byte	stylesDefault[MAXLIGHTMAPS];

qhandle_t		 RE_RegisterShader( const char *name );
qhandle_t		 RE_RegisterShaderNoMip( const char *name );

shader_t	*R_FindShader( const char *name, const int *lightmapIndex, const byte *styles, qboolean mipRawImage, int spec );
shader_t	*R_GetShaderByHandle( qhandle_t hShader );
void		R_InitShaders( void );
void		R_ShaderList_f( void );

//
// tr_spv.c
//
VkShaderModule	SPV_FindShaderModuleFile( const char *name );
VkShaderModule	SPV_CreateShaderModule( const uint32_t *code, int size );
void			SPV_InitPipelineCache( void );
void			SPV_InitGlowShaders( void );
void			SPV_InitWireframeShaders( void );
pipelineState_t *SPV_GetShadePipeline( int stateBits );

void			R_SetPipelineState( pipelineState_t *pipeline );


#define TR_MAX_DESCRIPTOR_SET_BINDING_COUNT 16

class CDescriptorSetLayoutBuilder {
public:
	int								bindingCount;
	VkDescriptorSetLayoutBinding	bindings[TR_MAX_DESCRIPTOR_SET_BINDING_COUNT];

public:
	CDescriptorSetLayoutBuilder();

	void reset();
	void addBinding( VkDescriptorType type );
	void addBinding( VkSampler &sampler );
	void build( VkDescriptorSetLayout *layout );
};

#define TR_MAX_DESCRIPTOR_SET_LAYOUT_COUNT 16
#define TR_MAX_PUSH_CONSTANT_RANGE_COUNT 5

class CPipelineLayoutBuilder {
public:
	int								descriptorSetLayoutCount;
	VkDescriptorSetLayout			descriptorSetLayouts[TR_MAX_DESCRIPTOR_SET_LAYOUT_COUNT];
	int								pushConstantRangeCount;
	VkPushConstantRange				pushConstantRanges[TR_MAX_PUSH_CONSTANT_RANGE_COUNT];

public:
	CPipelineLayoutBuilder();

	void reset();
	void addDescriptorSetLayout( VkDescriptorSetLayout layout );
	void addPushConstantRange( VkShaderStageFlags stages, int size, int offset = 0 );
	void build( pipelineLayout_t *layout );
};

#define TR_MAX_SHADER_STAGE_COUNT 5
#define TR_MAX_DYNAMIC_STATE_COUNT 10
#define TR_MAX_VERTEX_INPUT_ATTRIBUTE_COUNT 20
#define TR_MAX_VERTEX_INPUT_BINDING_COUNT 2

class CPipelineBuilder {
public:
	pipelineLayout_t						*layout;
	int										shaderStageCount;
	VkPipelineShaderStageCreateInfo			shaderStages[TR_MAX_SHADER_STAGE_COUNT];
	int32_t									shaderSpec;
	int										dynamicStateCount;
	VkDynamicState							dynamicStates[TR_MAX_DYNAMIC_STATE_COUNT];
	int										vertexAttributeCount;
	VkVertexInputAttributeDescription		vertexAttributes[TR_MAX_VERTEX_INPUT_ATTRIBUTE_COUNT];
	int										vertexBindingCount;
	VkVertexInputBindingDescription			vertexBindings[TR_MAX_VERTEX_INPUT_BINDING_COUNT];
	VkPipelineVertexInputStateCreateInfo	vertexInput;
	VkPipelineInputAssemblyStateCreateInfo	inputAssembly;
	VkPipelineMultisampleStateCreateInfo	multisample;
	VkPipelineDepthStencilStateCreateInfo	depthStencil;
	VkPipelineRasterizationStateCreateInfo	rasterization;
	VkPipelineColorBlendStateCreateInfo		colorBlend;
	VkPipelineDynamicStateCreateInfo		dynamic;
	VkGraphicsPipelineCreateInfo			pipelineCreateInfo;

public:
	CPipelineBuilder();
	~CPipelineBuilder();

	void reset( bool setDefaults );
	void setShader( VkShaderStageFlagBits stage, const uint32_t *code, int codeSize );
	void setDynamicState( VkDynamicState state );
	void addVertexAttribute( VkFormat format, int offset, int binding = 0 );
	void addVertexBinding( int stride, VkVertexInputRate rate );
	void build( pipelineState_t *pipeline );

	template<typename T, int codeSize>
	void setShader( VkShaderStageFlagBits stage, const T ( &code )[codeSize] ) {
		// code must be aligned at least to uint32_t size to be safely casted
		assert( ( (uintptr_t)( code ) & ( alignof( uint32_t ) - 1 ) ) == 0 );

		setShader( stage, reinterpret_cast<const uint32_t *>( code ), codeSize * sizeof( T ) );
	}

	template<typename T>
	void addVertexAttributes() {
		if (vertexAttributeCount + ARRAY_LEN(T::m_scAttributes) > TR_MAX_VERTEX_INPUT_ATTRIBUTE_COUNT) {
			Com_Error( ERR_FATAL, "CPipelineBuilder: max vertex attribute count limit reached\n" );
		}
		memcpy( vertexAttributes + vertexAttributeCount, T::m_scAttributes, sizeof( T::m_scAttributes ) );
		vertexAttributeCount += ARRAY_LEN( T::m_scAttributes );
	}

	template<typename T>
	void addVertexAttributesAndBinding() {
		if( vertexBindingCount + 1 > TR_MAX_VERTEX_INPUT_BINDING_COUNT ) {
			Com_Error( ERR_FATAL, "CPipelineBuilder: max vertex binding count limit reached\n" );
		}
		memcpy( vertexBindings + vertexBindingCount, &T::m_scBinding, sizeof( T::m_scBinding ) );
		vertexBindingCount++;

		addVertexAttributes<T>();
	}
};

class CFrameBufferBuilder {
public:
	int							width;
	int							height;
	int							depthBufferIndex;
	int							attachmentCount;
	VkAttachmentDescription		attachmentDescriptions[TR_MAX_FRAMEBUFFER_IMAGES];
	VkClearValue				clearValues[TR_MAX_FRAMEBUFFER_IMAGES];
	image_t						*externalImages[TR_MAX_FRAMEBUFFER_IMAGES];

public:
	CFrameBufferBuilder();

	void reset();
	void addColorAttachment( VkFormat format, bool clear = false, const VkClearColorValue &clearValue = { 0.f, 0.f, 0.f, 1.f } );
	void addColorAttachment( image_t *image, bool clear = false, const VkClearColorValue &clearValue = { 0.f, 0.f, 0.f, 1.f } );
	void addDepthStencilAttachment( VkFormat format, bool clear = false, const VkClearDepthStencilValue &clearValue = { 0.f, 0 } );
	void addDepthStencilAttachment( image_t *image, bool clear = false, const VkClearDepthStencilValue &clearValue = { 0.f, 0 } );
	void build( frameBuffer_t **frameBuffer );
};

#define TR_MAX_DESCRIPTOR_SET_UPDATE_SIZE 8

class CDescriptorSetWriter {
public:
	VkDescriptorSet				descriptorSet;
	int							writeCount;
	VkWriteDescriptorSet		writes[TR_MAX_DESCRIPTOR_SET_UPDATE_SIZE];
	int							bufferCount;
	VkDescriptorBufferInfo		buffers[TR_MAX_DESCRIPTOR_SET_UPDATE_SIZE];
	int							imageCount;
	VkDescriptorImageInfo		images[TR_MAX_DESCRIPTOR_SET_UPDATE_SIZE];

public:
	explicit CDescriptorSetWriter( VkDescriptorSet dstSet );

	void reset( VkDescriptorSet dstSet = VK_NULL_HANDLE );
	void writeBuffer( int binding, VkDescriptorType type, buffer_t *buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE );
	void writeImage( int binding, VkDescriptorType type, image_t *image, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
	void writeSampler( int binding, VkSampler sampler );
	void flush();
	void flush( VkCommandBuffer cmdbuf );
};

class CDynamicGeometryBuilder {
	struct vertexBufferList_t {
		vertexBuffer_t		*vertexBuffer;
		vertexBufferList_t	*next;
	};

	vertexBufferList_t	*root;
	vertexBufferList_t	*curr;

	int					triangleStripVertexes[2];
	int					triangleStripVertexCount;
	bool				triangleStrip;

	int					uploadIndexCount;
	int					uploadVertexCount;

public:
	int					vertexCount;
	int					vertexOffset;

	int					indexCount;
	int					indexOffset;

	int					drawStateBits;

	trIndex_t			indexes[SHADER_MAX_INDEXES];
	tr_shader::vertex_t vertexes[SHADER_MAX_VERTEXES];

public:
	void init();
	void reset();
	void checkOverflow( int numVertexes, int numIndexes );
	void beginGeometry();
	void beginTriangleStrip();
	void endTriangleStrip();
	int	 addVertex();
	void addTriangle( int, int, int );
	void endGeometry();
	void uploadGeometry();
	void setDrawStateBits( int stateBits );
};


/*
====================================================================

IMPLEMENTATION SPECIFIC FUNCTIONS

====================================================================
*/
static inline void GLimp_LogComment( char *comment ) {}


/*
====================================================================

TESSELATOR/SHADER DECLARATIONS

====================================================================
*/
typedef byte color4ub_t[4];

typedef struct stageVars
{
	color4ub_t	colors[SHADER_MAX_VERTEXES];
	vec2_t		texcoords[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
} stageVars_t;

#define	NUM_TEX_COORDS		(MAXLIGHTMAPS+1)

typedef struct drawCommand_s {
	int				numVertexBuffers;
	vertexBuffer_t	*vertexBuffers[TR_MAX_VERTEX_INPUT_BINDING_COUNT];

	int				vertexCount;
	int				vertexOffsets[TR_MAX_VERTEX_INPUT_BINDING_COUNT];

	int				indexCount;
	int				indexOffset;

	int				stateBits;

	VkDescriptorSet ghoul2BonesDescriptorSet;
} drawCommand_t;

struct shaderCommands_s
{
	shader_t	*shader;
	int			fogNum;

	int			dlightBits;	// or together of all vertexDlightBits

	// info extracted from current shader
	int			numPasses;
	void		(*currentStageIteratorFunc)( void );
	shaderStage_t	*xstages;

	int			registration;

	qboolean	SSInitializedWind;

	//rww - doing a fade, don't compute shader color/alpha overrides
	bool		fading;

	// vertex data
	int			numDraws;
	drawCommand_t	draws[SHADER_MAX_VERTEXES];
};

#ifdef _MSC_VER
typedef __declspec(align(16)) shaderCommands_s	shaderCommands_t;
#else
typedef shaderCommands_s	shaderCommands_t;
#endif

extern	shaderCommands_t	tess;

extern	color4ub_t	styleColors[MAX_LIGHT_STYLES];
extern	bool		styleUpdated[MAX_LIGHT_STYLES];

void RB_BeginSurface( shader_t *shader, int fogNum );
void RB_EndSurface();
drawCommand_t *RB_DrawSurface();
void RB_CheckOverflow();
#define RB_CHECKOVERFLOW() if (tess.numDraws + 1 >= SHADER_MAX_VERTEXES) {RB_CheckOverflow();}

void RB_StageIteratorGeneric( void );
void RB_StageIteratorSky( void );

void RB_AddQuadStamp( vec3_t origin, vec3_t left, vec3_t up, byte *color );
void RB_AddQuadStampExt( vec3_t origin, vec3_t left, vec3_t up, byte *color, float s1, float t1, float s2, float t2 );

void RB_ShowImages( void );

/*
============================================================

WORLD MAP

============================================================
*/

void R_AddBrushModelSurfaces( trRefEntity_t *e );
void R_AddWorldSurfaces( void );


/*
============================================================

LIGHTS

============================================================
*/

void R_DlightBmodel( bmodel_t *bmodel, qboolean NoLight );
void R_SetupEntityLighting( const trRefdef_t *refdef, trRefEntity_t *ent );
void R_TransformDlights( int count, dlight_t *dl, orientationr_t *ori );


/*
============================================================

SHADOWS

============================================================
*/

void RB_ShadowTessEnd( void );
void RB_ShadowFinish( void );
void RB_ProjectionShadowDeform( void );

/*
============================================================

SKIES

============================================================
*/

void R_BuildCloudData( shaderCommands_t *shader );
void R_InitSkyTexCoords( float cloudLayerHeight );
void R_DrawSkyBox( shaderCommands_t *shader );
void RB_DrawSun( void );
void RB_ClipSkyPolygons( shaderCommands_t *shader );

/*
============================================================

CURVE TESSELATION

============================================================
*/
srfGridMesh_t *R_SubdividePatchToGrid( int width, int height,
				       drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE] );
/*
Ghoul2 Insert Start
*/

float ProjectRadius( float r, vec3_t location );
/*
Ghoul2 Insert End
*/


/*
============================================================

MARKERS, POLYGON PROJECTION ON WORLD POLYGONS

============================================================
*/

int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
		     int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer );

/*
============================================================

SCENE GENERATION

============================================================
*/

void R_InitNextFrame( void );

void RE_ClearScene( void );
void RE_AddRefEntityToScene( const refEntity_t *ent );
void RE_AddPolyToScene( qhandle_t hShader , int numVerts, const polyVert_t *verts );
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b );
void RE_RenderScene( const refdef_t *fd );

qboolean RE_GetLighting( const vec3_t origin, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir );

// Only returns a four sided face and normal of the best face to break ( this is for glass right now )
void RE_GetBModelVerts( int bmodelIndex, vec3_t *verts, vec3_t normal );

/*
=============================================================

ANIMATED MODELS

=============================================================
*/

/*
Ghoul2 Insert Start
*/
class CBoneCache;

class CRenderableSurface
{
public:
#ifdef _G2_GORE
	int				ident;
#else
	const int		ident;			// ident of this surface - required so the materials renderer knows what sort of surface this refers to
#endif
 	CBoneCache 		*boneCache;		// pointer to transformed bone list for this surf
	trMdxmSurface_t	*surfaceData;	// pointer to surface data loaded into file - only used by client renderer DO NOT USE IN GAME SIDE - if there is a vid restart this will be out of wack on the game
#ifdef _G2_GORE
	float			*alternateTex;		// alternate texture coordinates.
	void			*goreChain;

	float			scale;
	float			fade;
	float			impactTime; // this is a number between 0 and 1 that dictates the progression of the bullet impact
#endif

#ifdef _G2_GORE
	CRenderableSurface& operator= ( const CRenderableSurface& src )
	{
		ident	 = src.ident;
		boneCache = src.boneCache;
		surfaceData = src.surfaceData;
		alternateTex = src.alternateTex;
		goreChain = src.goreChain;

		return *this;
	}
#endif

	CRenderableSurface():
		ident(SF_MDX),
		boneCache(0),
#ifdef _G2_GORE
		surfaceData(0),
		alternateTex(0),
		goreChain(0)
#else
		surfaceData(0)
#endif
	{}

	void Init()
	{
		boneCache=0;
		surfaceData=0;
#ifdef _G2_GORE
		ident = SF_MDX;
		alternateTex=0;
		goreChain=0;
#endif
	}
};

void R_AddGhoulSurfaces( trRefEntity_t *ent );
void RB_SurfaceGhoul( CRenderableSurface *surface );
/*
Ghoul2 Insert End
*/



/*
=============================================================
=============================================================
*/
void	R_TransformModelToClip( const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
				     vec4_t eye, vec4_t dst );
void	R_TransformClipToWindow( const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window );

void	RB_DeformTessGeometry( void );

void	RB_CalcScaleTexCoords( const float scale[2], float *dstTexCoords );
void	RB_CalcScrollTexCoords( const float scroll[2], float *dstTexCoords );
void	RB_CalcStretchTexCoords( const waveForm_t *wf, float *texCoords );
void	RB_CalcTransformTexCoords( const texModInfo_t *tmi, float *dstTexCoords );
void	RB_CalcRotateTexCoords( float rotSpeed, float *dstTexCoords );

void	RB_CalcEnvironmentTexCoords( float *dstTexCoords );
void	RB_CalcFogTexCoords( float *dstTexCoords );
void	RB_CalcTurbulentTexCoords( const waveForm_t *wf, float *dstTexCoords );

void	RB_CalcWaveColor( const waveForm_t *wf, unsigned char *dstColors );
void	RB_CalcColorFromEntity( unsigned char *dstColors );
void	RB_CalcColorFromOneMinusEntity( unsigned char *dstColors );
void	RB_CalcWaveAlpha( const waveForm_t *wf, unsigned char *dstColors );
void	RB_CalcSpecularAlpha( unsigned char *alphas );
void	RB_CalcAlphaFromEntity( unsigned char *dstColors );
void	RB_CalcAlphaFromOneMinusEntity( unsigned char *dstColors );
void	RB_CalcModulateColorsByFog( unsigned char *dstColors );
void	RB_CalcModulateAlphasByFog( unsigned char *dstColors );
void	RB_CalcModulateRGBAsByFog( unsigned char *dstColors );

void	RB_CalcDiffuseColor( unsigned char *colors );
void	RB_CalcDiffuseEntityColor( unsigned char *colors );
void	RB_CalcDisintegrateColors( unsigned char *colors, colorGen_t rgbGen );
void	RB_CalcDisintegrateVertDeform( void );
/*
=============================================================

RENDERER BACK END FUNCTIONS

=============================================================
*/

void RB_ExecuteRenderCommands( const void *data );

/*
=============================================================

RENDERER BACK END COMMAND QUEUE

=============================================================
*/

#define	MAX_RENDER_COMMANDS	0x40000

typedef struct {
	byte	cmds[MAX_RENDER_COMMANDS];
	int		used;
} renderCommandList_t;

typedef struct {
	int		commandId;
	float	color[4];
} setColorCommand_t;

typedef struct {
	int		commandId;
	int		buffer;
} drawBufferCommand_t;

typedef struct {
	int		commandId;
	image_t	*image;
	int		width;
	int		height;
	void	*data;
} subImageCommand_t;

typedef struct {
	int		commandId;
} swapBuffersCommand_t;

typedef struct {
	int		commandId;
	int		buffer;
} endFrameCommand_t;

typedef struct {
	int		commandId;
	shader_t	*shader;
	float	x, y;
	float	w, h;
	float	s1, t1;
	float	s2, t2;
} stretchPicCommand_t;

typedef struct {
	int		commandId;
	shader_t	*shader;
	float	x, y;
	float	w, h;
	float	s1, t1;
	float	s2, t2;
	float	a;
} rotatePicCommand_t;

typedef struct
{
	int			commandId;
} setModeCommand_t;

typedef struct
{
	int		commandId;
	float	x, y;
	float	w, h;
} scissorCommand_t;

typedef struct {
	int		commandId;
	trRefdef_t	refdef;
	viewParms_t	viewParms;
	drawSurf_t *drawSurfs;
	int		numDrawSurfs;
} drawSurfsCommand_t;

typedef enum {
	RC_END_OF_LIST,
	RC_SET_COLOR,
	RC_STRETCH_PIC,
	RC_SCISSOR,
	RC_ROTATE_PIC,
	RC_ROTATE_PIC2,
	RC_DRAW_SURFS,
	RC_DRAW_BUFFER,
	RC_SWAP_BUFFERS,
	RC_WORLD_EFFECTS,
} renderCommand_t;


// these are sort of arbitrary limits.
// the limits apply to the sum of all scenes in a frame --
// the main view, all the 3D icons, etc
#define	MAX_POLYS		2048
#define	MAX_POLYVERTS	( MAX_POLYS * 4 )

// all of the information needed by the back end must be
// contained in a backEndData_t.
typedef struct {
	VkCommandBuffer			cmdbuf;
	VkCommandBuffer			uploadCmdbuf;
	VkSemaphore				semaphore;			// signaled when the image is available

	image_t					*image;				// output texture
	int						imageArraySlice;	// output array slice (for stereo rendering)

	pipelineState_t			*pipelineState;

	VkDescriptorSet			descriptorSets[TR_NUM_SPACES];

	frameBuffer_t			*frameBuffer;		// last written frame buffer

	VkClearColorValue		defaultClearValue;

	CDynamicGeometryBuilder dynamicGeometryBuilder;

	drawSurf_t				drawSurfs[MAX_DRAWSURFS];
	dlight_t				dlights[MAX_DLIGHTS];
	trRefEntity_t			entities[MAX_REFENTITIES];
	srfPoly_t				polys[MAX_POLYS];
	polyVert_t				polyVerts[MAX_POLYVERTS];
	renderCommandList_t		commands;

} backEndData_t;

extern	backEndData_t	*backEndData;

void *R_GetCommandBuffer( int bytes );
void RB_ExecuteRenderCommands( const void *data );

void R_IssuePendingRenderCommands( void );

void R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs );

void RE_SetColor( const float *rgba );
void RE_StretchPic ( float x, float y, float w, float h,
		    float s1, float t1, float s2, float t2, qhandle_t hShader );
void RE_RotatePic ( float x, float y, float w, float h,
		   float s1, float t1, float s2, float t2,float a, qhandle_t hShader );
void RE_RotatePic2 ( float x, float y, float w, float h,
		    float s1, float t1, float s2, float t2,float a, qhandle_t hShader );
void RE_RenderWorldEffects(void);
void RE_LAGoggles( void );
void RE_Scissor ( float x, float y, float w, float h);
void RE_BeginFrame( stereoFrame_t stereoFrame );
void RE_EndFrame( int *frontEndMsec, int *backEndMsec );
qboolean	RE_ProcessDissolve(void);
qboolean	RE_InitDissolve(qboolean bForceCircularExtroWipe);


long generateHashValue( const char *fname );
void R_LoadImage( const char *name, byte **pic, int *width, int *height );
void		RE_InsertModelIntoHash(const char *name, model_t *mod);
qboolean R_FogParmsMatch( int fog1, int fog2 );

void R_DeleteFrameBuffer( frameBuffer_t *frameBuffer );
void R_BindFrameBuffer( frameBuffer_t *frameBuffer );
void R_ClearFrameBuffer( frameBuffer_t *frameBuffer );

/*
Ghoul2 Insert Start
*/

// tr_ghoul2.cpp
void		Create_Matrix(const float *angle, mdxaBone_t *matrix);
void		Multiply_3x4Matrix(mdxaBone_t *out,const mdxaBone_t *in2,const mdxaBone_t *in);
extern qboolean R_LoadMDXM (model_t *mod, void *buffer, const char *name, qboolean &bAlreadyCached );
extern qboolean R_LoadMDXA (model_t *mod, void *buffer, const char *name, qboolean &bAlreadyCached );
/*
Ghoul2 Insert End
*/

// tr_surfacesprites
void RB_DrawSurfaceSprites( shaderStage_t *stage, shaderCommands_t *input);
