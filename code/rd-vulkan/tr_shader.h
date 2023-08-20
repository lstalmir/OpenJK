#ifndef TR_SHADER_H_
#define TR_SHADER_H_

#if defined( __cplusplus )
#	include "qcommon/q_math.h"

#	pragma pack( push, 4 )

struct image_s;

// contains all types used for c/c++ - hlsl interop
namespace tr_shader {

	// define hlsl scalar types for C++ interop
	typedef uint32_t uint;
	typedef uint32_t qboolean32;

	// define hlsl vector types for C++ interop
	typedef struct {
		union {
			struct { // vector-like components
				float x;
			};
			struct { // color-like components
				float r;
			};
			float m[1];
		};
		float &operator[]( int i ) { return m[i]; }
		float operator[]( int i ) const { return m[i]; }
	} float1;

	typedef struct {
		union {
			struct { // vector-like components
				float x;
				float y;
			};
			struct { // color-like components
				float r;
				float g;
			};
			float m[2];
		};
		float &operator[]( int i ) { return m[i]; }
		float operator[]( int i ) const { return m[i]; }
	} float2;

	typedef struct {
		union {
			struct { // vector-like components
				float x;
				float y;
				float z;
			};
			struct { // color-like components
				float r;
				float g;
				float b;
			};
			float m[3];
		};
		float &operator[]( int i ) { return m[i]; }
		float operator[]( int i ) const { return m[i]; }
	} float3;

	typedef struct {
		union {
			struct { // vector-like components
				float x;
				float y;
				float z;
				float w;
			};
			struct { // color-like components
				float r;
				float g;
				float b;
				float a;
			};
			float m[4];
		};
		float &operator[]( int i ) { return m[i]; }
		float operator[]( int i ) const { return m[i]; }
	} float4;

	typedef struct {
		union {
			struct { // vector-like components
				int32_t x;
				int32_t y;
				int32_t z;
				int32_t w;
			};
			struct { // color-like components
				int32_t r;
				int32_t g;
				int32_t b;
				int32_t a;
			};
			int32_t m[4];
		};
		int32_t &operator[]( int i ) { return m[i]; }
		int32_t operator[]( int i ) const { return m[i]; }
	} int4;

	// define hlsl matrix types for C/C++ interop.
	typedef struct {
		union {
			struct { // matrix-like components
				float m00;
				float m01;
				float2 __m_unused0;
				float m10;
				float m11;
				float2 __m_unused1;
			};
			struct { // vector-like components
				float2 r0;
				float2 __r_unused0;
				float2 r1;
				float2 __r_unused1;
			};
			float m[4][2];
		};
	} float2x2;

	typedef float4 float4x3[3];
	typedef float4 float4x4[4];

	typedef struct {
		union {
			struct { // vector-like components
				byte x;
				byte y;
				byte z;
				byte w;
			};
			struct { // color-like components
				byte r;
				byte g;
				byte b;
				byte a;
			};
			byte m[4];
			uint32_t u32;
		};
		byte &operator[]( int i ) { return m[i]; }
		byte operator[]( int i ) const { return m[i]; }
		explicit operator uint32_t() const { return u32; }
		explicit operator uint32_t &() { return u32; }
		auto &operator=( uint32_t value ) {
			u32 = value;
			return *this;
		}
		auto &operator=( const byte value[4] ) {
			x = value[0];
			y = value[1];
			z = value[2];
			w = value[3];
			return *this;
		}
	} byte4;

// enum definition macro that handles both C/C++ and HLSL.
#	define TR_ENUM( name, ... )             \
		typedef enum {                       \
			##__VA_ARGS__,                   \
			name##_FORCE_DWORD_ = 0x7fffffff \
		} name

#	define TR_CppOnly( expr ) expr		  // evaluates to expr if code is compiled as C/C++.
#	define TR_HlslOnly( expr )			  // evaluates to expr if code is compiled as HLSL.
#	define TR_CppOrHlsl( cpp, hlsl ) cpp // evaluates to cpp if code is compiled as C/C++ or to hlsl if compiled as HLSL.

#	define TR_DescriptorSpace( n ) (n)
#	define TR_InputSemantic( name )

#else // TR_HLSL
// define special boolean type for HLSL/C++ interop (bool is reserved).
typedef bool qboolean32;

// enum definition macro that handles both C/C++ and HLSL.
#	define TR_ENUM( name, ... ) \
		typedef uint name;       \
		static const name __VA_ARGS__

#	define TR_CppOnly( expr )			   // evaluates to expr if code is compiled as C/C++.
#	define TR_HlslOnly( expr ) expr	   // evaluates to expr if code is compiled as HLSL.
#	define TR_CppOrHlsl( cpp, hlsl ) hlsl // evaluates to cpp if code is compiled as C/C++ or to hlsl if compiled as HLSL.

#	define TR_DescriptorSpace( n ) space##n
#	define TR_InputSemantic( name ) : name

#endif // TR_HLSL

// caps
#define NUM_TEXTURE_BUNDLES 2
#define TR_MAX_TEXMODS 4
#define TR_MAX_LIGHTMAPS 4
#define TR_MAX_FOGS 256
#define TR_MAX_SUB_BSP 32
#define TR_FUNCTABLE_SIZE 1024
#define TR_FUNCTABLE_SIZE2 10
#define TR_FUNCTABLE_MASK ( TR_FUNCTABLE_SIZE - 1 )
#define TR_MAX_SHADER_DEFORMS 3

#ifndef __TR_TYPES_H
// renderfx flags
#	define RF_MORELIGHT 0x00001	// allways have some light (viewmodel, some items)
#	define RF_THIRD_PERSON 0x00002 // don't draw through eyes, only mirrors (player bodies, chat sprites)
#	define RF_FIRST_PERSON 0x00004 // only draw through eyes (view weapon, damage blood blob)
#	define RF_DEPTHHACK 0x00008	// for view weapon Z crunching
#	define RF_NODEPTH 0x00010		// No depth at all (seeing through walls)

#	define RF_VOLUMETRIC 0x00020 // fake volumetric shading

#	define RF_NOSHADOW 0x00040 // don't add stencil shadows

#	define RF_LIGHTING_ORIGIN 0x00080 // use refEntity->lightingOrigin instead of refEntity->origin
									   // for lighting.  This allows entities to sink into the floor
									   // with their origin going solid, and allows all parts of a
									   // player to get the same lighting
#	define RF_SHADOW_PLANE 0x00100	   // use refEntity->shadowPlane
#	define RF_WRAP_FRAMES 0x00200	   // mod the model frames by the maxframes to allow continuous
									   // animation without needing to know the frame count
#	define RF_CAP_FRAMES 0x00400	   // cap the model frames by the maxframes for one shot anims

#	define RF_ALPHA_FADE 0x00800 // hacks blend mode and uses whatever the set alpha is.
#	define RF_PULSATE 0x01000	  // for things like a dropped saber, where we want to add an extra visual clue
#	define RF_RGB_TINT 0x02000	  // overrides ent RGB color to the specified color

#	define RF_FORKED 0x04000  // override lightning to have forks
#	define RF_TAPERED 0x08000 // lightning tapers
#	define RF_GROW 0x10000	   // lightning grows from start to end during its life

#	define RF_SETANIMINDEX 0x20000 // use backEnd.currentEntity->e.skinNum for R_BindAnimatedImage

#	define RF_DISINTEGRATE1 0x40000 // does a procedural hole-ripping thing.
#	define RF_DISINTEGRATE2 0x80000 // does a procedural hole-ripping thing with scaling at the ripping point

#	define RF_G2MINLOD 0x100000 // force Lowest lod on g2

#	define RF_SHADOW_ONLY 0x200000 // add surfs for shadowing but don't draw them normally -rww

#	define RF_DISTORTION 0x400000 // area distortion effect -rww

#	define RF_FORCE_ENT_ALPHA 0x800000 // override shader alpha settings
#endif

#define TR_GLOBALS_SPACE TR_DescriptorSpace( 0 )
#define TR_SAMPLERS_SPACE TR_DescriptorSpace( 1 )
#define TR_SHADER_SPACE TR_DescriptorSpace( 2 )
#define TR_MODEL_SPACE TR_DescriptorSpace( 3 )
#define TR_TEXTURE_SPACE_0 TR_DescriptorSpace( 4 )
#define TR_TEXTURE_SPACE_1 TR_DescriptorSpace( 5 )
#define TR_VIEW_SPACE TR_DescriptorSpace( 6 )
#define TR_G2_BONES_SPACE TR_DescriptorSpace( 7 )
#define TR_CUSTOM_SPACE_0 TR_DescriptorSpace( 8 )
#define TR_CUSTOM_SPACE_1 TR_DescriptorSpace( 9 )
#define TR_NUM_SPACES 10

#define TR_SHADER_SPEC_MD3			0x1
#define TR_SHADER_SPEC_GLM			0x2
#define TR_SHADER_SPEC_GLA			0x4
#define TR_SHADER_SPEC_NOTEXMODS	0x8

#define TR_SHADER_SPEC_ATEST_GT_0	0x10
#define TR_SHADER_SPEC_ATEST_LT_80	0x20
#define TR_SHADER_SPEC_ATEST_GE_80	0x40
#define TR_SHADER_SPEC_ATEST_GE_C0	0x80
#define TR_SHADER_SPEC_ATEST_BITS	0xF0


	TR_ENUM( genFunc_t,
		GF_NONE = 0,
		GF_SIN = 1,
		GF_SQUARE = 2,
		GF_TRIANGLE = 3,
		GF_SAWTOOTH = 4,
		GF_INVERSE_SAWTOOTH = 5,
		GF_NOISE = 6,
		GF_RAND = 7 );

	TR_ENUM( deform_t,
		DEFORM_NONE = 0,
		DEFORM_WAVE = 1,
		DEFORM_NORMALS = 2,
		DEFORM_BULGE = 3,
		DEFORM_MOVE = 4,
		DEFORM_PROJECTION_SHADOW = 5,
		DEFORM_AUTOSPRITE = 6,
		DEFORM_AUTOSPRITE2 = 7,
		DEFORM_TEXT0 = 8,
		DEFORM_TEXT1 = 9,
		DEFORM_TEXT2 = 10,
		DEFORM_TEXT3 = 11,
		DEFORM_TEXT4 = 12,
		DEFORM_TEXT5 = 13,
		DEFORM_TEXT6 = 14,
		DEFORM_TEXT7 = 15 );

	TR_ENUM( alphaGen_t,
		AGEN_IDENTITY = 0,
		AGEN_SKIP = 1,
		AGEN_ENTITY = 2,
		AGEN_ONE_MINUS_ENTITY = 3,
		AGEN_VERTEX = 4,
		AGEN_ONE_MINUS_VERTEX = 5,
		AGEN_LIGHTING_SPECULAR = 6,
		AGEN_WAVEFORM = 7,
		AGEN_PORTAL = 8,
		AGEN_BLEND = 9,
		AGEN_CONST = 10,
		AGEN_DOT = 11,
		AGEN_ONE_MINUS_DOT = 12 );

	TR_ENUM( colorGen_t,
		CGEN_BAD = 0,
		CGEN_IDENTITY_LIGHTING = 1,
		CGEN_IDENTITY = 2,
		CGEN_SKIP = 3,
		CGEN_ENTITY = 4,
		CGEN_ONE_MINUS_ENTITY = 5,
		CGEN_EXACT_VERTEX = 6,
		CGEN_VERTEX = 7,
		CGEN_ONE_MINUS_VERTEX = 8,
		CGEN_WAVEFORM = 9,
		CGEN_LIGHTING_DIFFUSE = 10,
		CGEN_LIGHTING_DIFFUSE_ENTITY = 11,
		CGEN_FOG = 12,
		CGEN_CONST = 13,
		CGEN_LIGHTMAPSTYLE = 14 );

	TR_ENUM( texCoordGen_t,
		TCGEN_BAD = 0,
		TCGEN_IDENTITY = 1,
		TCGEN_LIGHTMAP = 2,
		TCGEN_LIGHTMAP1 = 3,
		TCGEN_LIGHTMAP2 = 4,
		TCGEN_LIGHTMAP3 = 5,
		TCGEN_TEXTURE = 6,
		TCGEN_ENVIRONMENT_MAPPED = 7,
		TCGEN_FOG = 8,
		TCGEN_VECTOR = 9 );

	TR_ENUM( texMod_t,
		TMOD_NONE = 0,
		TMOD_TRANSFORM = 1,
		TMOD_TURBULENT = 2,
		TMOD_SCROLL = 3,
		TMOD_SCALE = 4,
		TMOD_STRETCH = 5,
		TMOD_ROTATE = 6,
		TMOD_ENTITY_TRANSLATE = 7 );

	TR_ENUM( acff_t,
		ACFF_NONE = 0,
		ACFF_MODULATE_RGB = 1,
		ACFF_MODULATE_RGBA = 2,
		ACFF_MODULATE_ALPHA = 3 );


	typedef struct vertex_s {
		float4 position									TR_InputSemantic( POSITION );
		float4 normal									TR_InputSemantic( NORMAL );
		float2 texCoord0								TR_InputSemantic( TEXCOORD0 );
#if defined( __cplusplus )
		union {
			struct {
#endif
				float2 texCoord1						TR_InputSemantic( TEXCOORD1 );
				float2 texCoord2						TR_InputSemantic( TEXCOORD2 );
				float2 texCoord3						TR_InputSemantic( TEXCOORD3 );
				float2 texCoord4						TR_InputSemantic( TEXCOORD4 );
#if defined( __cplusplus )
			};
			float2 lightmaps[TR_MAX_LIGHTMAPS];
		};
#endif
		TR_CppOrHlsl( byte4, float4 ) vertexColor		TR_InputSemantic( COLOR );
		TR_CppOrHlsl( byte, float ) vertexAlpha			TR_InputSemantic( ALPHA );
		TR_CppOrHlsl( byte, uint ) vertexDlightBits		TR_InputSemantic( DLIGHT );

#if defined( __cplusplus )
		// store vertex input attributes with the vertex type
		inline static const VkVertexInputAttributeDescription m_scAttributes[] = {
			{ 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },	 // position
			{ 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16 }, // normal
			{ 2, 0, VK_FORMAT_R32G32_SFLOAT, 32 },		 // texCoord0
			{ 3, 0, VK_FORMAT_R32G32_SFLOAT, 40 },		 // texCoord1
			{ 4, 0, VK_FORMAT_R32G32_SFLOAT, 48 },		 // texCoord2
			{ 5, 0, VK_FORMAT_R32G32_SFLOAT, 56 },		 // texCoord3
			{ 6, 0, VK_FORMAT_R32G32_SFLOAT, 64 },		 // texCoord4
			{ 7, 0, VK_FORMAT_B8G8R8A8_UNORM, 72 },		 // vertexColor
			{ 8, 0, VK_FORMAT_R8_UNORM, 76 },			 // vertexAlpha
			{ 9, 0, VK_FORMAT_R8_UNORM, 77 }			 // vertexDlightBits
		};

		inline static const VkVertexInputBindingDescription m_scBinding = {
			0, 80, VK_VERTEX_INPUT_RATE_VERTEX
		};
#endif
	} vertex_t;
	
	typedef struct oldVertex_s {
		float4 position									TR_InputSemantic( POSITION1 );
		float4 normal									TR_InputSemantic( NORMAL1 );
		float2 texCoord0								TR_InputSemantic( TEXCOORD5 );
#if defined( __cplusplus )
		union {
			struct {
#endif
				float2 texCoord1						TR_InputSemantic( TEXCOORD6 );
				float2 texCoord2						TR_InputSemantic( TEXCOORD7 );
				float2 texCoord3						TR_InputSemantic( TEXCOORD8 );
				float2 texCoord4						TR_InputSemantic( TEXCOORD9 );
#if defined( __cplusplus )
			};
			float2 lightmaps[TR_MAX_LIGHTMAPS];
		};
#endif
		TR_CppOrHlsl( byte4, float4 ) vertexColor		TR_InputSemantic( COLOR1 );
		TR_CppOrHlsl( byte, float ) vertexAlpha			TR_InputSemantic( ALPHA1 );
		TR_CppOrHlsl( byte, uint ) vertexDlightBits		TR_InputSemantic( DLIGHT1 );

#if defined( __cplusplus )
		// store vertex input attributes with the vertex type
		inline static const VkVertexInputAttributeDescription m_scAttributes[] = {
			{ 10, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0 },	 // position
			{ 11, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 16 },	 // normal
			{ 12, 1, VK_FORMAT_R32G32_SFLOAT, 32 },			 // texCoord0
			{ 13, 1, VK_FORMAT_R32G32_SFLOAT, 40 },			 // texCoord1
			{ 14, 1, VK_FORMAT_R32G32_SFLOAT, 48 },			 // texCoord2
			{ 15, 1, VK_FORMAT_R32G32_SFLOAT, 56 },			 // texCoord3
			{ 16, 1, VK_FORMAT_R32G32_SFLOAT, 64 },			 // texCoord4
			{ 17, 1, VK_FORMAT_B8G8R8A8_UNORM, 72 },		 // vertexColor
			{ 18, 1, VK_FORMAT_R8_UNORM, 76 },				 // vertexAlpha
			{ 19, 1, VK_FORMAT_R8_UNORM, 77 }				 // vertexDlightBits
		};

		inline static const VkVertexInputBindingDescription m_scBinding = {
			1, 80, VK_VERTEX_INPUT_RATE_VERTEX
		};
#endif
	} oldVertex_t;

	
	typedef struct {
		float4x3 mat;
	} mdxaBone_t;

	typedef struct {
		float3 origin;	   // in world coordinates
		float __unused0;
		float4x3 axis;	   // orientation in world
		float3 viewOrigin; // viewParms->or.origin in local coordinates
		float __unused1;
		float4x4 modelMatrix;
	} orientationr_t;

	// plane_t structure
	typedef struct cplane_s {
		float3 normal;
		float dist;
		// byte					type;			// for fast side tests: 0,1,2 = axial, 3 = nonaxial
		// byte					signbits;		// signx + (signy<<1) + (signz<<2), used as lookup during collision
	} cplane_t;

	typedef struct {
		float4 sunDirection;
		float4 sunAmbient;
		float4 sunLight;
	} sunParms_t;

	typedef struct {
		float atmosphereDensity;
		float atmosphereDistance;
		float atmosphereFalloff;
		float __unused0;
	} skyParms_t;

	typedef struct {
		float3 color;
		float depthForOpaque;
	} fogParms_t;

	typedef struct {
		fogParms_t parms;

		int originalBrushNumber;
		float3 bounds[2];

		float4 surface;

		uint colorInt; // in packed byte format
		float tcScale; // texture coordinate vector scales

		// for clipping distance in fog when outside
		qboolean32 hasSurface;
	} fog_t;

	typedef struct {
		float4 ambientLight[TR_MAX_LIGHTMAPS];
		float4 directLight[TR_MAX_LIGHTMAPS];
		// byte					styles[MAXLIGHTMAPS];
		// byte					latLong[2];
	} mgrid_t;

	typedef struct {
		orientationr_t ori;
		orientationr_t world;
		float4x4 projectionMatrix;
		cplane_t portalPlane; // clip anything behind this if mirroring
		cplane_t frustum[5];
		float4 visBounds[2];
		float3 pvsOrigin;	  // may be different than or.origin for portals
		qboolean32 isPortal;		  // true if this view is through a portal
		qboolean32 isMirror;  // the portal is a mirror, invert the face culling
		int viewportX, viewportY, viewportWidth, viewportHeight;
		float fovX, fovY;
		float zFar;
	} viewParms_t;

	typedef struct stageVars_s {
		float4 color;
		float2 texcoords[NUM_TEXTURE_BUNDLES];
	} stageVars_t;

	typedef struct {
		genFunc_t func;

		float base;
		float amplitude;
		float phase;
		float frequency;

		float3 __unused0;
	} waveForm_t;

	typedef struct {
		float3 moveVector;

		deform_t deformation; // vertex coordinate modification type
		waveForm_t deformationWave;
		float deformationSpread;

		float bulgeWidth;
		float bulgeHeight;
		float bulgeSpeed;
	} deformStage_t;

	typedef struct {
		// used for TMOD_TURBULENT and TMOD_STRETCH
		waveForm_t wave;

		// used for TMOD_TRANSFORM
		float2x2 mat;	  // s' = s * m[0][0] + t * m[1][0] + trans[0]
		float2 translate; // t' = s * m[0][1] + t * m[0][1] + trans[1]

		texMod_t type;

		// used for TMOD_SCALE
		//	float			scale[2];			// s *= scale[0]
		// t *= scale[1]

		// used for TMOD_SCROLL
		//	float			scroll[2];			// s' = s + scroll[0] * time
		// t' = t + scroll[1] * time

		// used for TMOD_ROTATE
		// + = clockwise
		// - = counterclockwise
		// float			rotateSpeed;

		float __unused0;
	} texModInfo_t;

	typedef struct textureBundle_s {
		float4			tcGenVectors[2];
		texCoordGen_t	tcGen;

		uint			numTexMods;
		uint			numImageAnimations;
		float			imageAnimationSpeed;

		texModInfo_t	texMods[TR_MAX_TEXMODS];

		qboolean32		isLightmap;
		qboolean32		oneShotAnimMap;
		qboolean32		vertexLightmap;
		qboolean32		isVideoMap;
	} textureBundle_t;

	typedef struct shaderStage_s {
		textureBundle_t bundle[NUM_TEXTURE_BUNDLES];

		waveForm_t rgbWave;
		waveForm_t alphaWave;

		colorGen_t rgbGen;
		alphaGen_t alphaGen;

		uint constantColor;
		float portalRange;

		acff_t adjustColorsForFog;

		int fogNum;

		float __unused0;

		int numDeforms;
		deformStage_t deforms[TR_MAX_SHADER_DEFORMS];

	} shaderStage_t;

	typedef struct {
		int renderfx;
		int frame;

		float shaderTime; // subtracted from refdef time to control effect start times

		// texturing
		int skinNum; // inline skin index

		// most recent data
		float3 lightingOrigin;
		float shadowPlane;

		float4 axis[3]; // rotation vectors
		qboolean32 nonNormalizedAxes;
		float3 origin;

		// previous data for frame interpolation
		float3 oldorigin;
		float backlerp;
		int oldframe;

		// misc
		uint shaderRGBA;		 // colors used by colorSrc=vertex shaders
		float2 shaderTexCoord; // texture coordinates used by tcMod=vertex modifiers
		
		// extra sprite information
		float radius;

		float rotation;
		float endTime;
		float saberLength;

	} trRefEntity_t;

	typedef struct {
		orientationr_t ori;

		float2 texCoordScrollSpeed;
		uint color; // packed RGBA color

		// dynamic lighting information
		uint dlightBits;

		float4 ambientLight;
		float4 directedLight;
		float3 lightDir;
		uint ambientLightInt;

		trRefEntity_t e;

		qboolean32 entity2D;

	} model_t;

	typedef struct {
		int numfogs;
		int globalFog;
		int startLightMapIndex;
		int _unused0;

		float4 lightGridOrigin;
		float4 lightGridSize;
		float4 lightGridInverseSize;
		int4 lightGridBounds;
		mgrid_t lightGridData;
		uint lightGridArray;
		int numGridArrayElements;

		int numClusters;
		int clusterBytes;

	} world_t;

	typedef struct {
		int visCount;	// incremented every time a new vis cluster is entered
		int frameCount; // incremented every frame
		int sceneCount; // incremented every scene
		int viewCount;	// incremented every view (twice a scene if portaled)
					   // and every R_MarkFragments call

		int frameSceneNum; // zeroed at RE_BeginFrame

		float floatTime;

		float rangedFog;
		float distanceCull;

		float4x3 viewaxis;

		world_t world;

		float identityLight;   // 1.0 / ( 1 << overbrightBits )
		int identityLightByte; // identityLight * 255
		int overbrightBits;	   // r_overbrightBits->integer, but set to 0 if no hw gamma

		int viewCluster;

		int sunSurfaceLight; // from the sky shader for this level
		float3 __unused0;
		sunParms_t sunParms;
		skyParms_t skyParms;

		int numFogs;
		int numModels;
		int numBSPModels;
		int numSkins;

	} trGlobals_t;

	// function tables are initialized once and don't ever change
	// put them in a separate buffer that is not backed on the CPU to save memory
	typedef struct {
#if defined( __cplusplus )
		float sinTable[TR_FUNCTABLE_SIZE];
		float squareTable[TR_FUNCTABLE_SIZE];
		float triangleTable[TR_FUNCTABLE_SIZE];
		float sawToothTable[TR_FUNCTABLE_SIZE];
		float inverseSawToothTable[TR_FUNCTABLE_SIZE];
#else
		float4 sinTable[TR_FUNCTABLE_SIZE / 4];
		float4 squareTable[TR_FUNCTABLE_SIZE / 4];
		float4 triangleTable[TR_FUNCTABLE_SIZE / 4];
		float4 sawToothTable[TR_FUNCTABLE_SIZE / 4];
		float4 inverseSawToothTable[TR_FUNCTABLE_SIZE / 4];
#endif
	} trFuncTables_t;

#if defined( __cplusplus )
} // namespace tr_shader

#	pragma pack( pop )
#endif

#endif // TR_SHADER_H_
