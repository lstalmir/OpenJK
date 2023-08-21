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

// tr_init.c -- functions that are not called every frame

#include "../server/exe_headers.h"

#include "tr_local.h"
#include "../rd-common/tr_common.h"
#include "tr_stl.h"
#include "../rd-common/tr_font.h"
#include "tr_WorldEffects.h"

glconfig_t glConfig;
vkstate_t vkState;
window_t window;

static void GfxInfo_f( void );

cvar_t *r_api;

cvar_t *r_verbose;
cvar_t *r_ignore;

cvar_t *r_detailTextures;

cvar_t *r_znear;

cvar_t *r_skipBackEnd;

cvar_t *r_measureOverdraw;

cvar_t *r_fastsky;
cvar_t *r_drawSun;
cvar_t *r_dynamiclight;
// rjr - removed for hacking cvar_t	*r_dlightBacks;

cvar_t *r_lodbias;
cvar_t *r_lodscale;

cvar_t *r_norefresh;
cvar_t *r_drawentities;
cvar_t *r_drawworld;
cvar_t *r_drawfog;
cvar_t *r_speeds;
cvar_t *r_fullbright;
cvar_t *r_novis;
cvar_t *r_nocull;
cvar_t *r_facePlaneCull;
cvar_t *r_showcluster;
cvar_t *r_nocurves;

cvar_t *r_dlightStyle;
cvar_t *r_surfaceSprites;
cvar_t *r_surfaceWeather;

cvar_t *r_windSpeed;
cvar_t *r_windAngle;
cvar_t *r_windGust;
cvar_t *r_windDampFactor;
cvar_t *r_windPointForce;
cvar_t *r_windPointX;
cvar_t *r_windPointY;

cvar_t *r_allowExtensions;

cvar_t *r_ext_compressed_textures;
cvar_t *r_ext_compressed_lightmaps;
cvar_t *r_ext_preferred_tc_method;
cvar_t *r_ext_gamma_control;
cvar_t *r_ext_multitexture;
cvar_t *r_ext_compiled_vertex_array;
cvar_t *r_ext_texture_env_add;
cvar_t *r_ext_texture_filter_anisotropic;

cvar_t *r_DynamicGlow;
cvar_t *r_DynamicGlowPasses;
cvar_t *r_DynamicGlowDelta;
cvar_t *r_DynamicGlowIntensity;
cvar_t *r_DynamicGlowSoft;
cvar_t *r_DynamicGlowWidth;
cvar_t *r_DynamicGlowHeight;

cvar_t *r_ignoreGLErrors;
cvar_t *r_logFile;

cvar_t *r_primitives;
cvar_t *r_texturebits;
cvar_t *r_texturebitslm;

cvar_t *r_lightmap;
cvar_t *r_vertexLight;
cvar_t *r_shadows;
cvar_t *r_shadowRange;
cvar_t *r_flares;
cvar_t *r_atmosphere;
cvar_t *r_nobind;
cvar_t *r_singleShader;
cvar_t *r_colorMipLevels;
cvar_t *r_picmip;
cvar_t *r_showtris;
cvar_t *r_showtriscolor;
cvar_t *r_showsky;
cvar_t *r_shownormals;
cvar_t *r_finish;
cvar_t *r_clear;
cvar_t *r_textureMode;
cvar_t *r_offsetFactor;
cvar_t *r_offsetUnits;
cvar_t *r_gamma;
cvar_t *r_intensity;
cvar_t *r_lockpvs;
cvar_t *r_noportals;
cvar_t *r_portalOnly;

cvar_t *r_subdivisions;
cvar_t *r_lodCurveError;

cvar_t *r_overBrightBits;
cvar_t *r_mapOverBrightBits;

cvar_t *r_debugSurface;
cvar_t *r_simpleMipMaps;

cvar_t *r_showImages;

cvar_t *r_ambientScale;
cvar_t *r_directedScale;
cvar_t *r_debugLight;
cvar_t *r_debugSort;
cvar_t *r_debugStyle;

cvar_t *r_modelpoolmegs;

cvar_t *r_noGhoul2;
cvar_t *r_Ghoul2AnimSmooth;
cvar_t *r_Ghoul2UnSqash;
cvar_t *r_Ghoul2TimeBase = 0;
cvar_t *r_Ghoul2NoLerp;
cvar_t *r_Ghoul2NoBlend;
cvar_t *r_Ghoul2BlendMultiplier = 0;
cvar_t *r_Ghoul2UnSqashAfterSmooth;

cvar_t *broadsword;
cvar_t *broadsword_kickbones;
cvar_t *broadsword_kickorigin;
cvar_t *broadsword_playflop;
cvar_t *broadsword_dontstopanim;
cvar_t *broadsword_waitforshot;
cvar_t *broadsword_smallbbox;
cvar_t *broadsword_extra1;
cvar_t *broadsword_extra2;

cvar_t *broadsword_effcorr;
cvar_t *broadsword_ragtobase;
cvar_t *broadsword_dircap;

// More bullshit needed for the proper modular renderer --eez
cvar_t *sv_mapname;
cvar_t *sv_mapChecksum;
cvar_t *se_language; // JKA
#ifdef JK2_MODE
cvar_t *sp_language; // JK2
#endif
cvar_t *com_buildScript;

cvar_t *r_environmentMapping;
cvar_t *r_screenshotJpegQuality;


#define MAX_EXTENSIONS 16

typedef struct {
	// data read from the driver
	uint32_t					supportedExtensionCount;
	VkExtensionProperties		*supportedExtensions;

	// filled during initialization
	uint32_t					enabledExtensionCount;
	const char					*enabledExtensions[MAX_EXTENSIONS];
} vkinstance_initContext_t;

typedef struct {
	// data read from the driver
	VkPhysicalDevice			physicalDevice;
	VkPhysicalDeviceProperties	physicalDeviceProperties;
	VkPhysicalDeviceFeatures	supportedDeviceFeatures;
	uint32_t					supportedExtensionCount;
	VkExtensionProperties		*supportedExtensions;
	uint32_t					supportedDeviceQueueFamilyCount;
	VkQueueFamilyProperties		*supportedDeviceQueueFamilies;

	// filled during initialization
	VkPhysicalDeviceFeatures	enabledDeviceFeatures;
	uint32_t					enabledExtensionCount;
	const char					*enabledExtensions[MAX_EXTENSIONS];
} vkdevice_initContext_t;

/*
** custom debug messenger function that breaks on errors
*/
VKAPI_ATTR VkBool32 VKAPI_CALL VK_DebugMessage(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {
	if( messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT ) {
		ri.Printf( PRINT_DEVELOPER, "VK_DebugMessage: %s\n", pCallbackData->pMessage );
	}
	return VK_FALSE;
}


void RE_SetLightStyle( int style, int color );

void R_Splash() {
	VK_SetImageLayout( backEndData->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT );

	image_t *image = R_FindImageFile( "menu/splash", qfalse, qfalse, qfalse, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
	if( !image ) {
		// Can't find the splash image so just clear to black
		VkClearColorValue clearColorValue = { 0, 0, 0, 1 };
		VkImageSubresourceRange clearRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		vkCmdClearColorImage(
			backEndData->cmdbuf,
			backEndData->image->tex,
			backEndData->image->layout,
			&clearColorValue,
			1, &clearRange );
	}

	else {
		const int width = 640;
		const int height = 480;

		VkImageBlit blitRegion = {};

		blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blitRegion.srcSubresource.layerCount = 1;
		blitRegion.srcOffsets[1].x = image->width;
		blitRegion.srcOffsets[1].y = image->height;
		blitRegion.srcOffsets[1].z = 1;

		blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blitRegion.dstSubresource.layerCount = 1;
		blitRegion.dstOffsets[1].x = width;
		blitRegion.dstOffsets[1].y = height;
		blitRegion.dstOffsets[1].z = 1;

		VK_SetImageLayout( image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT );
		vkCmdBlitImage( backEndData->cmdbuf,
				image->tex,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				backEndData->image->tex,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &blitRegion,
				VK_FILTER_LINEAR );
	}
}

static const char *VK_GetVendorString( uint32_t vendorID ) {
	switch( vendorID ) {
	case 0x8086: return "Intel";
	case 0x10DE: return "NVIDIA";
	case 0x1022: return "AMD";
	case 0x5143: return "Qualcomm";
	case 0x13B5: return "ARM";
	case VK_VENDOR_ID_VIV: return "VIV";
	case VK_VENDOR_ID_VSI: return "VSI";
	case VK_VENDOR_ID_KAZAN: return "KAZAN";
	case VK_VENDOR_ID_CODEPLAY: return "CODEPLAY";
	case VK_VENDOR_ID_MESA: return "MESA";
	case VK_VENDOR_ID_POCL: return "POCL";
	}
	return "Unknown";
}

static void VK_InitTextureCompression( const vkdevice_initContext_t *ctx ) {
	bool textureCompressionSupported = false;

	// Check if block compression feature is supported.
	if( ctx->supportedDeviceFeatures.textureCompressionBC == VK_TRUE ) {
		textureCompressionSupported = true;
	}

	// Check if DXT1 and DXT5 texture formats are supported.
	else {
		VkFormatProperties dxt1FormatProperties, dxt5FormatProperties;
		vkGetPhysicalDeviceFormatProperties( ctx->physicalDevice, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, &dxt1FormatProperties );
		vkGetPhysicalDeviceFormatProperties( ctx->physicalDevice, VK_FORMAT_BC3_UNORM_BLOCK, &dxt5FormatProperties );

		// Required usage flags for compressed textures.
		const VkFormatFeatureFlags textureFeatureFlags = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

		bool dxt1FormatSupported = false;
		if( ( dxt1FormatProperties.optimalTilingFeatures & textureFeatureFlags ) == textureFeatureFlags ) {
			Com_Printf( "...VK_FORMAT_BC1_RGBA_UNORM_BLOCK format supported\n" );
			dxt1FormatSupported = true;
		}

		bool dxt5FormatSupported = false;
		if( ( dxt5FormatProperties.optimalTilingFeatures & textureFeatureFlags ) == textureFeatureFlags ) {
			Com_Printf( "...VK_FORMAT_BC3_UNORM_BLOCK format supported\n" );
			dxt5FormatSupported = true;
		}

		textureCompressionSupported = dxt1FormatSupported || dxt5FormatSupported;
	}

	if( !r_ext_compressed_textures->value ) {
		// Compressed textures are off
		glConfig.textureCompression = TC_NONE;
		Com_Printf( "...ignoring texture compression\n" );
	}
	else if( !textureCompressionSupported ) {
		// Requesting texture compression, but no method found
		glConfig.textureCompression = TC_NONE;
		Com_Printf( "...no supported texture compression method found\n" );
		Com_Printf( ".....ignoring texture compression\n" );
	}
	else {
		// Vulkan supports DXT texture compression.
		glConfig.textureCompression = TC_S3TC_DXT;
	}
}

/*
===============
VK_InitExtensions
===============
*/
#define REQUIRED qtrue

template<typename _initContext_t>
static bool VK_ExtensionSupported( const _initContext_t *ctx, const char *name ) {
	for( uint32_t i = 0; i < ctx->supportedExtensionCount; ++i ) {
		if( Q_stricmp( ctx->supportedExtensions[i].extensionName, name ) == 0 )
			return true;
	}
	return false;
}

template<typename _initContext_t>
static bool VK_ExtensionEnabled( _initContext_t *ctx, const char *name ) {
	for( uint32_t i = 0; i < ctx->enabledExtensionCount; ++i ) {
		if( Q_stricmp( ctx->enabledExtensions[i], name ) == 0 )
			return true;
	}
	return false;
}

template<typename _initContext_t>
static bool VK_EnableExtension( _initContext_t *ctx, const char *name, qboolean required = qfalse ) {
	bool extensionEnabled = false;
	if( VK_ExtensionSupported( ctx, name ) && ctx->enabledExtensionCount < MAX_EXTENSIONS ) {
		ctx->enabledExtensions[ctx->enabledExtensionCount] = name;
		ctx->enabledExtensionCount++;
		extensionEnabled = true;
	}
	if( !extensionEnabled && required ) {
		Com_Error( ERR_FATAL, "VK_EnableExtension: required extension %s not supported\n", name );
	}
	return extensionEnabled;
}

extern bool g_bDynamicGlowSupported;
static void VK_InitDeviceExtensions( vkdevice_initContext_t *ctx ) {
	bool enableOptionalExtensions = true;
	if( !r_allowExtensions->integer ) {
		enableOptionalExtensions = false;
	}

	Com_Printf( "Initializing Vulkan device extensions\n" );

	// required extensions
	VK_EnableExtension( ctx, VK_KHR_SWAPCHAIN_EXTENSION_NAME, REQUIRED );
	VK_EnableExtension( ctx, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, REQUIRED );

	// Select our tc scheme
	VK_InitTextureCompression( ctx );

	// samplerAnisotropy
	glConfig.maxTextureFilterAnisotropy = 0;
	if( ctx->supportedDeviceFeatures.samplerAnisotropy ) {
		glConfig.maxTextureFilterAnisotropy = ctx->physicalDeviceProperties.limits.maxSamplerAnisotropy;
		Com_Printf( "...VkPhysicalDeviceFeatures::samplerAnisotropy available\n" );

		if( r_ext_texture_filter_anisotropic->integer > 1 ) {
			Com_Printf( "...using VkPhysicalDeviceFeatures::samplerAnisotropy\n" );
			ctx->enabledDeviceFeatures.samplerAnisotropy = VK_TRUE;
		}
		else {
			Com_Printf( "...ignoring VkPhysicalDeviceFeatures::samplerAnisotropy\n" );
		}
		ri.Cvar_SetValue( "r_ext_texture_filter_anisotropic_avail", glConfig.maxTextureFilterAnisotropy );
		if( r_ext_texture_filter_anisotropic->value > glConfig.maxTextureFilterAnisotropy ) {
			ri.Cvar_SetValue( "r_ext_texture_filter_anisotropic_avail", glConfig.maxTextureFilterAnisotropy );
		}
	}
	else {
		Com_Printf( "...VkPhysicalDeviceFeatures::samplerAnisotropy not supported\n" );
		ri.Cvar_Set( "r_ext_texture_filter_anisotropic_avail", "0" );
	}

	// line rasterization
	if( ctx->supportedDeviceFeatures.fillModeNonSolid ) {
		Com_Printf( "...VkPhysicalDeviceFeatures::fillModeNonSolid available\n" );
		if( enableOptionalExtensions ) {
			Com_Printf( "...using VkPhysicalDeviceFeatures::fillModeNonSolid\n" );
			ctx->enabledDeviceFeatures.fillModeNonSolid = VK_TRUE;
		}
		else {
			Com_Printf( "...ignoring VkPhysicalDeviceFeatures::fillModeNonSolid\n" );
		}
	}
}

static void VK_InitInstanceExtensions( vkinstance_initContext_t *ctx ) {
	bool enableOptionalExtensions = true;
	if( !r_allowExtensions->integer ) {
		Com_Printf( "*** IGNORING OPTIONAL VULKAN EXTENSIONS ***\n" );
		enableOptionalExtensions = false;
	}

	Com_Printf( "Initializing Vulkan instance extensions\n" );

	// required extensions
	VK_EnableExtension( ctx, VK_KHR_SURFACE_EXTENSION_NAME, REQUIRED );
	VK_EnableExtension( ctx, SURFACE_EXTENSION_NAME, REQUIRED );

#if defined( _DEBUG )
	if( enableOptionalExtensions ) {
		// debug extensions
		VK_EnableExtension( ctx, VK_EXT_DEBUG_UTILS_EXTENSION_NAME );
	}
#endif
}

/***********************************************************************************************************/

PFN_vkVoidFunction VK_GetProcAddress( const char *name, qboolean required ) {
	PFN_vkVoidFunction func = NULL;

	if( vkState.device ) {
		// try to load the device function first as this pointer may be more optimal
		// than the one retrieved via vkGetInstanceProcAddr
		func = vkGetDeviceProcAddr( vkState.device, name );
	}

	if( !func ) {
		// if the function is not available at the device level, import it from the instance
		func = vkGetInstanceProcAddr( vkState.instance, name );
	}

	if( !func && required ) {
		Com_Error( ERR_FATAL, "VK_GetProcAddress: required Vulkan entry point %s not found\n", name );
	}

	return func;
}

/***********************************************************************************************************/

static void InitVulkanInstance( void ) {
	vkinstance_initContext_t *initCtx;
	VkResult res;

	// initialize the instance
	initCtx = ( vkinstance_initContext_t * )R_Malloc( sizeof( vkinstance_initContext_t ), TAG_ALL, qtrue );

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.apiVersion = VK_API_VERSION_1_1;

	// get number of supported instance extensions, allocate a buffer and read the extensions
	vkEnumerateInstanceExtensionProperties( NULL, &initCtx->supportedExtensionCount, NULL );
	initCtx->supportedExtensions = ( VkExtensionProperties * )R_Malloc( sizeof( VkExtensionProperties ) * initCtx->supportedExtensionCount, TAG_ALL );
	res = vkEnumerateInstanceExtensionProperties( NULL, &initCtx->supportedExtensionCount, initCtx->supportedExtensions );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to query supported Vulkan instance extensions (%d)\n", res );
	}

	// initialize extensions
	VK_InitInstanceExtensions( initCtx );

	// create the instance
	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledExtensionCount = initCtx->enabledExtensionCount;
	instanceCreateInfo.ppEnabledExtensionNames = initCtx->enabledExtensions;

	res = vkCreateInstance( &instanceCreateInfo, NULL, &vkState.instance );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create Vulkan instance (%d)\n", res );
	}

	if( VK_ExtensionEnabled( initCtx, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) ) {
		vkState.pfnSetDebugObjectName = VK_GetProcAddress<PFN_vkSetDebugUtilsObjectNameEXT>( "vkSetDebugUtilsObjectNameEXT", REQUIRED );
		vkState.pfnBeginDebugUtilsLabel = VK_GetProcAddress<PFN_vkCmdBeginDebugUtilsLabelEXT>( "vkCmdBeginDebugUtilsLabelEXT", REQUIRED );
		vkState.pfnEndDebugUtilsLabel = VK_GetProcAddress<PFN_vkCmdEndDebugUtilsLabelEXT>( "vkCmdEndDebugUtilsLabelEXT", REQUIRED );

		vkState.pfnCreateDebugMessenger = VK_GetProcAddress<PFN_vkCreateDebugUtilsMessengerEXT>( "vkCreateDebugUtilsMessengerEXT" );
		vkState.pfnDestroyDebugMessenger = VK_GetProcAddress<PFN_vkDestroyDebugUtilsMessengerEXT>( "vkDestroyDebugUtilsMessengerEXT" );

		#if 0
		if( vkState.pfnCreateDebugMessenger && vkState.pfnDestroyDebugMessenger ) {
			VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {};

			// create a debug messenger
			messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			messengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			messengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
			messengerCreateInfo.pfnUserCallback = VK_DebugMessage;

			vkState.pfnCreateDebugMessenger( vkState.instance, &messengerCreateInfo, NULL, &vkState.debugMessenger );
		}
		#endif
	}

	R_Free( initCtx->supportedExtensions );
	R_Free( initCtx );
}

static void InitVulkanSurface( void ) {
	VkResult res;

#if defined( _WIN32 )
	PFN_vkCreateWin32SurfaceKHR pfnCreateSurfaceKHR;
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};

	// get the address of the platform-specific function that creates the surface
	pfnCreateSurfaceKHR = VK_GetProcAddress<PFN_vkCreateWin32SurfaceKHR>( "vkCreateWin32SurfaceKHR", REQUIRED );

	// setup the surface create info
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = ( HINSTANCE )window.process;
	surfaceCreateInfo.hwnd = ( HWND )window.handle;

#elif defined( __linux__ )
	PFN_vkCreateXlibSurfaceKHR pfnCreateSurfaceKHR;
	VkXlibSurfaceCreateInfoKHR surfaceCreateInfo = {};

	// get the address of the platform-specific function that creates the surface
	pfnCreateSurfaceKHR = VK_GetProcAddress<PFN_vkCreateXlibSurfaceKHR>( "vkCreateXlibSurfaceKHR", REQUIRED );

	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	...
#endif

	// create the surface
	res = pfnCreateSurfaceKHR( vkState.instance, &surfaceCreateInfo, NULL, &vkState.surface );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create window surface (%d)\n", res );
	}
}

static void InitVulkanDevice( void ) {
	vkdevice_initContext_t *initCtx;
	VkResult res;

	// initialize the device
	initCtx = ( vkdevice_initContext_t * )R_Malloc( sizeof( vkdevice_initContext_t ), TAG_ALL, qtrue );
	initCtx->physicalDevice = vkState.physicalDevice;

	vkGetPhysicalDeviceFeatures( initCtx->physicalDevice, &initCtx->supportedDeviceFeatures );
	vkGetPhysicalDeviceProperties( initCtx->physicalDevice, &initCtx->physicalDeviceProperties );

	// get number of supported device queue families, allocate a buffer and read the properties
	vkGetPhysicalDeviceQueueFamilyProperties( initCtx->physicalDevice, &initCtx->supportedDeviceQueueFamilyCount, NULL );
	initCtx->supportedDeviceQueueFamilies = ( VkQueueFamilyProperties * )R_Malloc( sizeof( VkQueueFamilyProperties ) * initCtx->supportedDeviceQueueFamilyCount, TAG_ALL );
	vkGetPhysicalDeviceQueueFamilyProperties( initCtx->physicalDevice, &initCtx->supportedDeviceQueueFamilyCount, initCtx->supportedDeviceQueueFamilies );

	// get number of supported device extensions, allocate a buffer and read the extensions
	vkEnumerateDeviceExtensionProperties( initCtx->physicalDevice, NULL, &initCtx->supportedExtensionCount, NULL );
	initCtx->supportedExtensions = ( VkExtensionProperties * )R_Malloc( sizeof( VkExtensionProperties ) * initCtx->supportedExtensionCount, TAG_ALL );
	res = vkEnumerateDeviceExtensionProperties( initCtx->physicalDevice, NULL, &initCtx->supportedExtensionCount, initCtx->supportedExtensions );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to query supported Vulkan device extensions (%d)\n", res );
	}

	// initialize extensions
	VK_InitDeviceExtensions( initCtx );

	// setup the default command queue
	float queuePriority = 1.f;
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

	for( uint32_t i = 0; i < initCtx->supportedDeviceQueueFamilyCount; ++i ) {
		const VkQueueFamilyProperties *queueFamily = &initCtx->supportedDeviceQueueFamilies[i];

		VkBool32 queueSupportsPresentation = VK_FALSE;
		res = vkGetPhysicalDeviceSurfaceSupportKHR( initCtx->physicalDevice, i, vkState.surface, &queueSupportsPresentation );

		if( queueFamily->queueFlags & VK_QUEUE_GRAPHICS_BIT && res == VK_SUCCESS && queueSupportsPresentation ) {
			queueCreateInfo.queueFamilyIndex = i;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			break;
		}
	}

	if( queueCreateInfo.queueCount == 0 ) {
		Com_Error( ERR_FATAL, "InitVulkan: suitable command queue family not found\n" );
	}

	// create the device
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pEnabledFeatures = &initCtx->enabledDeviceFeatures;
	deviceCreateInfo.enabledExtensionCount = initCtx->enabledExtensionCount;
	deviceCreateInfo.ppEnabledExtensionNames = initCtx->enabledExtensions;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

	res = vkCreateDevice( vkState.physicalDevice, &deviceCreateInfo, NULL, &vkState.device );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create Vulkan device (%d)\n", res );
	}

	// get the command queue
	vkState.queueFamilyIndex = queueCreateInfo.queueFamilyIndex;
	vkGetDeviceQueue( vkState.device, vkState.queueFamilyIndex, 0, &vkState.queue );

	R_Free( initCtx->supportedExtensions );
	R_Free( initCtx->supportedDeviceQueueFamilies );
	R_Free( initCtx );
}

static void InitVulkanObjects( void ) {
	VkResult res;

	// create the command pool
	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolCreateInfo.queueFamilyIndex = vkState.queueFamilyIndex;

	res = vkCreateCommandPool( vkState.device, &commandPoolCreateInfo, NULL, &vkState.cmdpool );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create command pool (%d)\n", res );
	}

	// create the descriptor pool
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descriptorPoolCreateInfo.maxSets = 4096;

	VkDescriptorPoolSize descriptorPoolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2048 },
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 2048 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2048 }
	};

	descriptorPoolCreateInfo.poolSizeCount = ARRAY_LEN( descriptorPoolSizes );
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes;

	res = vkCreateDescriptorPool( vkState.device, &descriptorPoolCreateInfo, NULL, &vkState.descriptorPool );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create descriptor pool (%d)\n", res );
	}

	// create the allocator
	VmaAllocatorCreateInfo allocatorCreateInfo = {};
	allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_1;
	allocatorCreateInfo.instance = vkState.instance;
	allocatorCreateInfo.physicalDevice = vkState.physicalDevice;
	allocatorCreateInfo.device = vkState.device;

	res = vmaCreateAllocator( &allocatorCreateInfo, &vkState.allocator );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create device memory allocator (%d)\n", res );
	}
}

void InitVulkanDescriptorSetLayouts( void ) {
	CDescriptorSetLayoutBuilder builder;
	VkResult res;

	// create the immutable samplers
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.anisotropyEnable = ( glConfig.maxTextureFilterAnisotropy > 0 ) ? VK_TRUE : VK_FALSE;
	samplerCreateInfo.maxAnisotropy = ( glConfig.maxTextureFilterAnisotropy > 0 ) ? glConfig.maxTextureFilterAnisotropy : 1;
	samplerCreateInfo.maxLod = FLT_MAX;

	res = vkCreateSampler( vkState.device, &samplerCreateInfo, NULL, &vkState.wrapModeSampler );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create wrap mode sampler (%d)\n", res );
	}

	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	res = vkCreateSampler( vkState.device, &samplerCreateInfo, NULL, &vkState.clampModeSampler );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create clamp mode sampler (%d)\n", res );
	}

	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 1;

	res = vkCreateSampler( vkState.device, &samplerCreateInfo, NULL, &vkState.linearClampSampler );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create linear clamp sampler (%d)\n", res );
	}
	VK_SetDebugObjectName( vkState.linearClampSampler, VK_OBJECT_TYPE_SAMPLER, "linearClampSampler" );

	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

	res = vkCreateSampler( vkState.device, &samplerCreateInfo, NULL, &vkState.linearWrapSampler );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create linear wrap sampler (%d)\n", res );
	}
	VK_SetDebugObjectName( vkState.linearWrapSampler, VK_OBJECT_TYPE_SAMPLER, "linearWrapSampler" );

	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

	res = vkCreateSampler( vkState.device, &samplerCreateInfo, NULL, &vkState.pointWrapSampler );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create point wrap sampler (%d)\n", res );
	}
	VK_SetDebugObjectName( vkState.pointWrapSampler, VK_OBJECT_TYPE_SAMPLER, "pointWrapSampler" );

	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	res = vkCreateSampler( vkState.device, &samplerCreateInfo, NULL, &vkState.pointClampSampler );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create point clamp sampler (%d)\n", res );
	}
	VK_SetDebugObjectName( vkState.pointClampSampler, VK_OBJECT_TYPE_SAMPLER, "pointClampSampler" );

	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 1;
	samplerCreateInfo.minLod = 8;

	res = vkCreateSampler( vkState.device, &samplerCreateInfo, NULL, &vkState.skyFogColorSampler );
	if (res != VK_SUCCESS) {
		Com_Error( ERR_FATAL, "InitVulkan: failed to create sky fog color sampler (%d)\n", res );
	}
	VK_SetDebugObjectName( vkState.skyFogColorSampler, VK_OBJECT_TYPE_SAMPLER, "skyFogColorSampler" );

	// common descriptor set layout
	builder.reset();
	builder.addBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ); // tr
	builder.addBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ); // tr_funcs
	builder.addBinding( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ); // tr_lightGridData
	builder.addBinding( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ); // tr_lightGridArray
	builder.addBinding( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ); // tr_fogs
	builder.addBinding( VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );	 // tr_noise
	builder.build( &vkState.commonDescriptorSetLayout );
	VK_SetDebugObjectName( vkState.commonDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "commonDescriptorSetLayout" );

	// samplers descriptor set layout
	builder.reset();
	builder.addBinding( vkState.pointClampSampler );
	builder.addBinding( vkState.pointWrapSampler );
	builder.addBinding( vkState.linearClampSampler );
	builder.addBinding( vkState.linearWrapSampler );
	builder.addBinding( vkState.skyFogColorSampler );
	builder.build( &vkState.samplerDescriptorSetLayout );
	VK_SetDebugObjectName( vkState.samplerDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "samplerDescriptorSetLayout" );

	// shader descriptor set layout
	builder.reset();
	builder.addBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	builder.build( &vkState.shaderDescriptorSetLayout );
	VK_SetDebugObjectName( vkState.shaderDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "shaderDescriptorSetLayout" );

	// model descriptor set layout
	builder.build( &vkState.modelDescriptorSetLayout );
	VK_SetDebugObjectName( vkState.modelDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "modelDescriptorSetLayout" );

	// texture descriptor set layout
	builder.reset();
	builder.addBinding( VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE );
	builder.addBinding( VK_DESCRIPTOR_TYPE_SAMPLER );
	builder.build( &vkState.textureDescriptorSetLayout );
	VK_SetDebugObjectName( vkState.textureDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "textureDescriptorSetLayout" );

	// view descriptor set layout
	builder.reset();
	builder.addBinding( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER );
	builder.build( &vkState.viewDescriptorSetLayout );
	VK_SetDebugObjectName( vkState.viewDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "viewDescriptorSetLayout" );

	// ghoul2 bones descriptor set layout
	builder.reset();
	builder.addBinding( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER );
	builder.build( &vkState.ghoul2BonesDescriptorSetLayout );
	VK_SetDebugObjectName( vkState.ghoul2BonesDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "ghoul2BonesDescriptorSetLayout" );
}

void InitVulkanDescriptorSets( void ) {
	CDescriptorSetWriter writer( VK_NULL_HANDLE );

	// common descriptor set
	VK_AllocateDescriptorSet( vkState.commonDescriptorSetLayout, &tr.commonDescriptorSet );

	writer.reset( tr.commonDescriptorSet );
	writer.writeBuffer( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, tr.globalsBuffer );
	writer.writeBuffer( 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, tr.funcTablesBuffer );
	writer.writeBuffer( 4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, tr.fogsBuffer );
	writer.writeImage( 5, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, tr.noiseImage );
	writer.flush();

	// samplers descriptor set
	VK_AllocateDescriptorSet( vkState.samplerDescriptorSetLayout, &tr.samplerDescriptorSet );
	VK_SetDebugObjectName( tr.samplerDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "tr.samplerDescriptorSet" );

	// 2D entity model descriptor set
	VK_AllocateDescriptorSet( vkState.modelDescriptorSetLayout, &backEnd.entity2D.modelDescriptorSet );
	VK_SetDebugObjectName( backEnd.entity2D.modelDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "backEnd.entity2D.modelDescriptorSet" );

	writer.reset( backEnd.entity2D.modelDescriptorSet );
	writer.writeBuffer( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, backEnd.entity2D.modelBuffer );
	writer.flush();

	// view descriptor set
	VK_AllocateDescriptorSet( vkState.viewDescriptorSetLayout, &tr.viewParms.descriptorSet );
	VK_SetDebugObjectName( tr.viewParms.descriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET, "tr.viewParms.descriptorSet" );

	writer.reset( tr.viewParms.descriptorSet );
	writer.writeBuffer( 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, tr.viewParms.buffer );
	writer.flush();
}

void VK_InitSwapchain( void ) {
	VkResult res;
	VkSurfaceCapabilitiesKHR surfaceCapabilities;

	// get the surface capabilities
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR( vkState.physicalDevice, vkState.surface, &surfaceCapabilities );

	// swapchain can be created only on a non-minimized window
	if( surfaceCapabilities.currentExtent.width && surfaceCapabilities.currentExtent.height ) {
		VkSwapchainKHR oldSwapchain = vkState.swapchain;
		uint32_t imageCount = MAX_OUTIMAGES;
		VkImage images[MAX_OUTIMAGES];

		// create the new swapchain
		VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.surface = vkState.surface;
		swapchainCreateInfo.oldSwapchain = oldSwapchain;

		swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // fifo must be supported by every implementation
		swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
		
		swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
		swapchainCreateInfo.imageArrayLayers = 1; // todo: stereo support
		swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		swapchainCreateInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
		swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		swapchainCreateInfo.minImageCount = 2;

		res = vkCreateSwapchainKHR( vkState.device, &swapchainCreateInfo, NULL, &vkState.swapchain );
		if( res != VK_SUCCESS ) {
			Com_Error( ERR_FATAL, "VK_InitSwapchain: failed to create a new swapchain (%d)\n", res );
		}

		if( oldSwapchain ) {
			// destroy image resources associated with images belonging to the old swapchain
			// R_Images_DestroyImage can't be used because the image resource is owned by the swapchain
			for( int i = 0; i < vkState.imgcount; ++i ) {
				vkDestroyImageView( vkState.device, vkState.images[i].texview, NULL );
			}
			vkDestroySwapchainKHR( vkState.device, oldSwapchain, NULL );

			// clear the array so that it can be reused
			memset( vkState.images, 0, sizeof( vkState.images ) );
		}

		// get swapchain images
		res = vkGetSwapchainImagesKHR( vkState.device, vkState.swapchain, &imageCount, images );
		if( res != VK_SUCCESS ) {
			Com_Error( ERR_FATAL, "VK_InitSwapchain: failed to get swapchain images (%d)\n", res );
		}

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = swapchainCreateInfo.imageFormat;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.subresourceRange.levelCount = 1;

		for( int i = 0; i < imageCount; ++i ) {
			image_t *img = &vkState.images[i];
			img->tex = images[i];
			img->width = swapchainCreateInfo.imageExtent.width;
			img->height = swapchainCreateInfo.imageExtent.height;
			img->internalFormat = swapchainCreateInfo.imageFormat;
			img->allAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

			// assign a debug name for the image
			sprintf( img->imgName, "<swapchainImage[%d]>", i );

			// create an image view
			imageViewCreateInfo.image = images[i];

			res = vkCreateImageView( vkState.device, &imageViewCreateInfo, NULL, &img->texview );
			if( res != VK_SUCCESS ) {
				Com_Error( ERR_FATAL, "VK_InitSwapchain: failed to create swapchain image view for image %d (%d)\n", i, res );
			}
		}

		// create additional resources if number of images is greater than previously
		if( imageCount > vkState.imgcount ) {

			// semaphores
			VkSemaphoreCreateInfo semaphoreCreateInfo = {};
			semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			for( int i = vkState.imgcount; i < imageCount; ++i ) {
				if( !vkState.semaphores[i] ) {
					res = vkCreateSemaphore( vkState.device, &semaphoreCreateInfo, NULL, &vkState.semaphores[i] );
					if( res != VK_SUCCESS ) {
						Com_Error( ERR_FATAL, "VK_InitSwapchain: failed to create semaphore for image %d (%d)\n", i, res );
					}
				}
			}

			// fences
			VkFenceCreateInfo fenceCreateInfo = {};
			fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			for( int i = vkState.imgcount; i < imageCount; ++i ) {
				if( !vkState.fences[i] ) {
					res = vkCreateFence( vkState.device, &fenceCreateInfo, NULL, &vkState.fences[i] );
					if( res != VK_SUCCESS ) {
						Com_Error( ERR_FATAL, "VK_InitSwapchain: failed to create fence for image %d (%d)\n", i, res );
					}
				}
			}

			// command buffers
			VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
			commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commandBufferAllocateInfo.commandPool = vkState.cmdpool;
			commandBufferAllocateInfo.commandBufferCount = (imageCount - vkState.imgcount) * 3;

			res = vkAllocateCommandBuffers( vkState.device, &commandBufferAllocateInfo, vkState.cmdbuffers + (vkState.imgcount * 3) );
			if( res != VK_SUCCESS ) {
				Com_Error( ERR_FATAL, "VK_InitSwapchain: failed to allocate %d command buffers for new swapchain images (%d)\n",
						commandBufferAllocateInfo.commandBufferCount, res );
			}
		}

		// update the image count
		vkState.imgcount = imageCount;
		vkState.imagenum = UINT32_MAX;
	}
}

/*
** VK_Init
**
** This function is responsible for initializing a valid Vulkan subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void VK_Init( void ) {
	VkResult res;
	//
	// initialize OS specific portions of the renderer
	//
	// GLimp_Init directly or indirectly references the following cvars:
	//		- r_fullscreen
	//		- r_mode
	//		- r_(color|depth|stencil)bits
	//		- r_ignorehwgamma
	//		- r_gamma
	//

	if( glConfig.vidWidth == 0 ) {
		// initialize the window
		windowDesc_t windowDesc = { GRAPHICS_API_GENERIC };
		memset( &glConfig, 0, sizeof( glConfig ) );
		window = ri.WIN_Init( &windowDesc, &glConfig );

		// initialize the vulkan instance and window surface
		InitVulkanInstance();
		InitVulkanSurface();

		// get the physical device
		uint32_t physicalDeviceCount = 1;
		res = vkEnumeratePhysicalDevices( vkState.instance, &physicalDeviceCount, &vkState.physicalDevice );
		if( res != VK_SUCCESS && res != VK_INCOMPLETE ) {
			Com_Error( ERR_FATAL, "InitVulkan: failed to get the primary physical device (%d)\n", res );
		}

		// initialize the device on the selected display adapter
		InitVulkanDevice();
		InitVulkanObjects();
		InitVulkanDescriptorSetLayouts();

		VK_InitSwapchain();

		// get our config strings
		vkGetPhysicalDeviceProperties( vkState.physicalDevice, &vkState.physicalDeviceProperties );

		sprintf( vkState.physicalDeviceDriverVersion, "%u.%u.%u",
			 VK_API_VERSION_MAJOR( vkState.physicalDeviceProperties.driverVersion ),
			 VK_API_VERSION_MINOR( vkState.physicalDeviceProperties.driverVersion ),
			 VK_API_VERSION_PATCH( vkState.physicalDeviceProperties.driverVersion ) );

		glConfig.vendor_string = VK_GetVendorString( vkState.physicalDeviceProperties.vendorID );
		glConfig.renderer_string = vkState.physicalDeviceProperties.deviceName;
		glConfig.version_string = vkState.physicalDeviceDriverVersion;
		glConfig.extensions_string = NULL;

		// Vulkan driver constants
		glConfig.maxTextureSize = vkState.physicalDeviceProperties.limits.maxImageDimension2D;

		VK_BeginFrame();
		R_Splash(); // get something on screen asap
	}
	else {
		VK_BeginFrame();
	}
}

/*
==============================================================================

						SCREEN SHOTS

==============================================================================
*/

/*
==================

RB_ReadPixels

Reads an image but takes care of alignment issues for reading RGB images.

Reads a minimum offset for where the RGB data starts in the image from
integer stored at pointer offset. When the function has returned the actual
offset was written back to address offset. This address will always have an
alignment of packAlign to ensure efficient copying.

Stores the length of padding after a line of pixels to address padlen

Return value must be freed with Hunk_FreeTempMemory()
==================
*/

byte *RB_ReadPixels( int x, int y, int width, int height, size_t *offset, int *padlen ) {
	VkImageSubresource subresource;
	VkSubresourceLayout subresourceLayout;
	VmaAllocationInfo allocationInfo;
	byte *buffer;
	int linelen, padlinelen, srcOffset, dstOffset;

	vmaGetAllocationInfo( vkState.allocator, tr.screenshotImage->allocation, &allocationInfo );
	byte *mappedScreenshotImage = (byte *)allocationInfo.pMappedData;

	// get layout of the first color subresource
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.arrayLayer = 0;
	subresource.mipLevel = 0;

	// get the layout of the copied image
	vkGetImageSubresourceLayout( vkState.device, tr.screenshotImage->tex, &subresource, &subresourceLayout );

	linelen = width * 3;
	padlinelen = PAD( linelen, 4 );

	// copy the image data 
	buffer = (byte *)R_Malloc( padlinelen * height, TAG_TEMP_WORKSPACE, qfalse );

	for( int yy = 0; yy < height; ++yy ) {
		srcOffset = (y + yy) * subresourceLayout.rowPitch + (x * 4);
		dstOffset = yy * padlinelen + (x * 3);

		for( int xx = 0; xx < width; ++xx ) {
			buffer[dstOffset] = mappedScreenshotImage[srcOffset];
			buffer[dstOffset + 1] = mappedScreenshotImage[srcOffset + 1];
			buffer[dstOffset + 2] = mappedScreenshotImage[srcOffset + 2];
			dstOffset += 3;
			srcOffset += 4;
		}
	}

	*offset = 0;
	*padlen = ( padlinelen - linelen );

	return buffer;
}

/*
==================
R_TakeScreenshot
==================
*/
void R_TakeScreenshot( int x, int y, int width, int height, char *fileName ) {
	byte *allbuf, *buffer;
	byte *srcptr, *destptr;
	byte *endline, *endmem;
	byte temp;

	int linelen, padlen;
	size_t offset = 18, memcount;

	allbuf = RB_ReadPixels( x, y, width, height, &offset, &padlen );
	buffer = allbuf + offset - 18;

	Com_Memset( buffer, 0, 18 );
	buffer[2] = 2; // uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 24; // pixel size

	// swap rgb to bgr and remove padding from line endings
	linelen = width * 3;

	srcptr = destptr = allbuf + offset;
	endmem = srcptr + ( linelen + padlen ) * height;

	while( srcptr < endmem ) {
		endline = srcptr + linelen;

		while( srcptr < endline ) {
			temp = srcptr[0];
			*destptr++ = srcptr[2];
			*destptr++ = srcptr[1];
			*destptr++ = temp;

			srcptr += 3;
		}

		// Skip the pad
		srcptr += padlen;
	}

	memcount = linelen * height;

	// gamma correct
	if( glConfig.deviceSupportsGamma )
		R_GammaCorrect( allbuf + offset, (int)memcount );

	ri.FS_WriteFile( fileName, buffer, (int)memcount + 18 );

	R_Free( allbuf );
}

/*
==================
R_TakeScreenshotPNG
==================
*/
void R_TakeScreenshotPNG( int x, int y, int width, int height, char *fileName ) {
	byte *buffer = NULL;
	size_t offset = 0;
	int padlen = 0;

	buffer = RB_ReadPixels( x, y, width, height, &offset, &padlen );
	RE_SavePNG( fileName, buffer, width, height, 3 );
	R_Free( buffer );
}

/*
==================
R_TakeScreenshotJPEG
==================
*/
void R_TakeScreenshotJPEG( int x, int y, int width, int height, char *fileName ) {
	byte *buffer;
	size_t offset = 0, memcount;
	int padlen;

	buffer = RB_ReadPixels( x, y, width, height, &offset, &padlen );
	memcount = ( width * 3 + padlen ) * height;

	// gamma correct
	if( glConfig.deviceSupportsGamma )
		R_GammaCorrect( buffer + offset, (int)memcount );

	RE_SaveJPG( fileName, r_screenshotJpegQuality->integer, width, height, buffer + offset, padlen );
	R_Free( buffer );
}

/*
==================
R_ScreenshotFilename
==================
*/
void R_ScreenshotFilename( char *buf, int bufSize, const char *ext ) {
	time_t rawtime;
	char timeStr[32] = { 0 }; // should really only reach ~19 chars

	time( &rawtime );
	strftime( timeStr, sizeof( timeStr ), "%Y-%m-%d_%H-%M-%S", localtime( &rawtime ) ); // or gmtime

	Com_sprintf( buf, bufSize, "screenshots/shot%s%s", timeStr, ext );
}

/*
====================
R_LevelShot

levelshots are specialized 256*256 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
#define LEVELSHOTSIZE 256
static void R_LevelShot( void ) {
	char checkname[MAX_OSPATH];
	byte *buffer;
	byte *source, *allsource;
	byte *src, *dst;
	size_t offset = 0;
	int padlen;
	int x, y;
	int r, g, b;
	float xScale, yScale;
	int xx, yy;

	Com_sprintf( checkname, sizeof( checkname ), "levelshots/%s.tga", tr.world->baseName );

	allsource = RB_ReadPixels( 0, 0, glConfig.vidWidth, glConfig.vidHeight, &offset, &padlen );
	source = allsource + offset;

	buffer = ( byte * )R_Malloc( LEVELSHOTSIZE * LEVELSHOTSIZE * 3 + 18, TAG_TEMP_WORKSPACE, qfalse );
	Com_Memset( buffer, 0, 18 );
	buffer[2] = 2; // uncompressed type
	buffer[12] = LEVELSHOTSIZE & 255;
	buffer[13] = LEVELSHOTSIZE >> 8;
	buffer[14] = LEVELSHOTSIZE & 255;
	buffer[15] = LEVELSHOTSIZE >> 8;
	buffer[16] = 24; // pixel size

	// resample from source
	xScale = glConfig.vidWidth / ( 4.0 * LEVELSHOTSIZE );
	yScale = glConfig.vidHeight / ( 3.0 * LEVELSHOTSIZE );
	for( y = 0; y < LEVELSHOTSIZE; y++ ) {
		for( x = 0; x < LEVELSHOTSIZE; x++ ) {
			r = g = b = 0;
			for( yy = 0; yy < 3; yy++ ) {
				for( xx = 0; xx < 4; xx++ ) {
					src = source + 3 * ( glConfig.vidWidth * ( int )( ( y * 3 + yy ) * yScale ) + ( int )( ( x * 4 + xx ) * xScale ) );
					r += src[0];
					g += src[1];
					b += src[2];
				}
			}
			dst = buffer + 18 + 3 * ( y * LEVELSHOTSIZE + x );
			dst[0] = b / 12;
			dst[1] = g / 12;
			dst[2] = r / 12;
		}
	}

	// gamma correct
	if( ( tr.overbrightBits > 0 ) && glConfig.deviceSupportsGamma ) {
		R_GammaCorrect( buffer + 18, LEVELSHOTSIZE * LEVELSHOTSIZE * 3 );
	}

	ri.FS_WriteFile( checkname, buffer, LEVELSHOTSIZE * LEVELSHOTSIZE * 3 + 18 );

	R_Free( buffer );
	R_Free( allsource );

	Com_Printf( "Wrote %s\n", checkname );
}

/*
==================
R_ScreenShotTGA_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
==================
*/
void R_ScreenShotTGA_f( void ) {
	char checkname[MAX_OSPATH] = { 0 };
	qboolean silent = qfalse;

	if( !strcmp( ri.Cmd_Argv( 1 ), "levelshot" ) ) {
		R_LevelShot();
		return;
	}

	if( !strcmp( ri.Cmd_Argv( 1 ), "silent" ) )
		silent = qtrue;

	if( ri.Cmd_Argc() == 2 && !silent ) {
		// explicit filename
		Com_sprintf( checkname, sizeof( checkname ), "screenshots/%s.tga", ri.Cmd_Argv( 1 ) );
	}
	else {
		// timestamp the file
		R_ScreenshotFilename( checkname, sizeof( checkname ), ".tga" );

		if( ri.FS_FileExists( checkname ) ) {
			Com_Printf( "ScreenShot: Couldn't create a file\n" );
			return;
		}
	}

	R_TakeScreenshot( 0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname );

	if( !silent )
		Com_Printf( "Wrote %s\n", checkname );
}

/*
==================
R_ScreenShotPNG_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
==================
*/
void R_ScreenShotPNG_f( void ) {
	char checkname[MAX_OSPATH] = { 0 };
	qboolean silent = qfalse;

	if( !strcmp( ri.Cmd_Argv( 1 ), "levelshot" ) ) {
		R_LevelShot();
		return;
	}

	if( !strcmp( ri.Cmd_Argv( 1 ), "silent" ) )
		silent = qtrue;

	if( ri.Cmd_Argc() == 2 && !silent ) {
		// explicit filename
		Com_sprintf( checkname, sizeof( checkname ), "screenshots/%s.png", ri.Cmd_Argv( 1 ) );
	}
	else {
		// timestamp the file
		R_ScreenshotFilename( checkname, sizeof( checkname ), ".png" );

		if( ri.FS_FileExists( checkname ) ) {
			Com_Printf( "ScreenShot: Couldn't create a file\n" );
			return;
		}
	}

	R_TakeScreenshotPNG( 0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname );

	if( !silent )
		Com_Printf( "Wrote %s\n", checkname );
}

void R_ScreenShot_f( void ) {
	char checkname[MAX_OSPATH] = { 0 };
	qboolean silent = qfalse;

	if( !strcmp( ri.Cmd_Argv( 1 ), "levelshot" ) ) {
		R_LevelShot();
		return;
	}
	if( !strcmp( ri.Cmd_Argv( 1 ), "silent" ) )
		silent = qtrue;

	if( ri.Cmd_Argc() == 2 && !silent ) {
		// explicit filename
		Com_sprintf( checkname, sizeof( checkname ), "screenshots/%s.jpg", ri.Cmd_Argv( 1 ) );
	}
	else {
		// timestamp the file
		R_ScreenshotFilename( checkname, sizeof( checkname ), ".jpg" );

		if( ri.FS_FileExists( checkname ) ) {
			Com_Printf( "ScreenShot: Couldn't create a file\n" );
			return;
		}
	}

	R_TakeScreenshotJPEG( 0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname );

	if( !silent )
		Com_Printf( "Wrote %s\n", checkname );
}

//============================================================================

/*
================
R_PrintLongString

Workaround for Com_Printf's 1024 characters buffer limit.
================
*/
void R_PrintLongString( const char *string ) {
	char buffer[1024];
	const char *p = string;
	int remainingLength = (int)strlen( string );

	while( remainingLength > 0 ) {
		// Take as much characters as possible from the string without splitting words between buffers
		// This avoids the client console splitting a word up when one half fits on the current line,
		// but the second half would have to be written on a new line
		int charsToTake = sizeof( buffer ) - 1;
		if( remainingLength > charsToTake ) {
			while( p[charsToTake - 1] > ' ' && p[charsToTake] > ' ' ) {
				charsToTake--;
				if( charsToTake == 0 ) {
					charsToTake = sizeof( buffer ) - 1;
					break;
				}
			}
		}
		else if( remainingLength < charsToTake ) {
			charsToTake = remainingLength;
		}

		Q_strncpyz( buffer, p, charsToTake + 1 );
		Com_Printf( "%s", buffer );
		remainingLength -= charsToTake;
		p += charsToTake;
	}
}

/*
================
GfxInfo_f
================
*/
void GfxInfo_f( void ) {
	const char *enablestrings[] = {
		"disabled",
		"enabled"
	};
	const char *fsstrings[] = {
		"windowed",
		"fullscreen"
	};
	const char *noborderstrings[] = {
		"",
		"noborder "
	};

	const char *tc_table[] = {
		"None",
		"GL_S3_s3tc",
		"GL_EXT_texture_compression_s3tc",
	};

	int fullscreen = ri.Cvar_VariableIntegerValue( "r_fullscreen" );
	int noborder = ri.Cvar_VariableIntegerValue( "r_noborder" );

	ri.Printf( PRINT_ALL, "\nVendor: %s\n", glConfig.vendor_string );
	ri.Printf( PRINT_ALL, "Device: %s\n", glConfig.renderer_string );
	ri.Printf( PRINT_ALL, "Driver: %s\n", glConfig.version_string );
	Com_Printf( "\n" );
	ri.Printf( PRINT_ALL, "GL_MAX_TEXTURE_SIZE: %d\n", glConfig.maxTextureSize );
	ri.Printf( PRINT_ALL, "GL_MAX_ACTIVE_TEXTURES_ARB: %d\n", glConfig.maxActiveTextures );
	ri.Printf( PRINT_ALL, "\nPIXELFORMAT: color(%d-bits) Z(%d-bit) stencil(%d-bits)\n", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits );
	ri.Printf( PRINT_ALL, "MODE: %d, %d x %d %s%s hz:",
		   ri.Cvar_VariableIntegerValue( "r_mode" ),
		   glConfig.vidWidth, glConfig.vidHeight,
		   fullscreen == 0 ? noborderstrings[noborder == 1] : noborderstrings[0],
		   fsstrings[fullscreen == 1] );
	if( glConfig.displayFrequency ) {
		ri.Printf( PRINT_ALL, "%d\n", glConfig.displayFrequency );
	}
	else {
		ri.Printf( PRINT_ALL, "N/A\n" );
	}
	if( glConfig.deviceSupportsGamma ) {
		ri.Printf( PRINT_ALL, "GAMMA: hardware w/ %d overbright bits\n", tr.overbrightBits );
	}
	else {
		ri.Printf( PRINT_ALL, "GAMMA: software w/ %d overbright bits\n", tr.overbrightBits );
	}

	// rendering primitives
	{
		int primitives;

		// default is to use triangles if compiled vertex arrays are present
		ri.Printf( PRINT_ALL, "rendering primitives: " );
		primitives = r_primitives->integer;
		if( primitives == 0 ) {
#if 0
			if( qglLockArraysEXT ) {
				primitives = 2;
			}
			else {
				primitives = 1;
			}
#endif
		}
		if( primitives == -1 ) {
			ri.Printf( PRINT_ALL, "none\n" );
		}
		else if( primitives == 2 ) {
			ri.Printf( PRINT_ALL, "single glDrawElements\n" );
		}
		else if( primitives == 1 ) {
			ri.Printf( PRINT_ALL, "multiple glArrayElement\n" );
		}
		else if( primitives == 3 ) {
			ri.Printf( PRINT_ALL, "multiple glColor4ubv + glTexCoord2fv + glVertex3fv\n" );
		}
	}

	ri.Printf( PRINT_ALL, "texturemode: %s\n", r_textureMode->string );
	ri.Printf( PRINT_ALL, "picmip: %d\n", r_picmip->integer );
	ri.Printf( PRINT_ALL, "texture bits: %d\n", r_texturebits->integer );
	if( r_texturebitslm->integer > 0 )
		ri.Printf( PRINT_ALL, "lightmap texture bits: %d\n", r_texturebitslm->integer );
	ri.Printf( PRINT_ALL, "texenv add: %s\n", enablestrings[glConfig.textureEnvAddAvailable != 0] );
	ri.Printf( PRINT_ALL, "compressed textures: %s\n", enablestrings[glConfig.textureCompression != TC_NONE] );
	ri.Printf( PRINT_ALL, "compressed lightmaps: %s\n", enablestrings[( r_ext_compressed_lightmaps->integer != 0 && glConfig.textureCompression != TC_NONE )] );
	ri.Printf( PRINT_ALL, "texture compression method: %s\n", tc_table[glConfig.textureCompression] );
	ri.Printf( PRINT_ALL, "anisotropic filtering: %s  ", enablestrings[( r_ext_texture_filter_anisotropic->integer != 0 ) && glConfig.maxTextureFilterAnisotropy] );
	if( r_ext_texture_filter_anisotropic->integer != 0 && glConfig.maxTextureFilterAnisotropy ) {
		if( Q_isintegral( r_ext_texture_filter_anisotropic->value ) )
			ri.Printf( PRINT_ALL, "(%i of ", ( int )r_ext_texture_filter_anisotropic->value );
		else
			ri.Printf( PRINT_ALL, "(%f of ", r_ext_texture_filter_anisotropic->value );

		if( Q_isintegral( glConfig.maxTextureFilterAnisotropy ) )
			ri.Printf( PRINT_ALL, "%i)\n", ( int )glConfig.maxTextureFilterAnisotropy );
		else
			ri.Printf( PRINT_ALL, "%f)\n", glConfig.maxTextureFilterAnisotropy );
	}
	ri.Printf( PRINT_ALL, "Dynamic Glow: %s\n", enablestrings[r_DynamicGlow->integer ? 1 : 0] );

	if( r_finish->integer ) {
		ri.Printf( PRINT_ALL, "Forcing glFinish\n" );
	}

	int displayRefresh = ri.Cvar_VariableIntegerValue( "r_displayRefresh" );
	if( displayRefresh ) {
		ri.Printf( PRINT_ALL, "Display refresh set to %d\n", displayRefresh );
	}
	if( tr.world ) {
		ri.Printf( PRINT_ALL, "Light Grid size set to (%.2f %.2f %.2f)\n", tr.world->lightGridSize[0], tr.world->lightGridSize[1], tr.world->lightGridSize[2] );
	}
}

/************************************************************************************************
 * R_FogDistance_f                                                                              *
 *    Console command to change the global fog opacity distance.  If you specify nothing on the *
 *    command line, it will display the current fog opacity distance.  Specifying a float       *
 *    representing the world units away the fog should be completely opaque will change the     *
 *    value.                                                                                    *
 *                                                                                              *
 * Input                                                                                        *
 *    none                                                                                      *
 *                                                                                              *
 * Output / Return                                                                              *
 *    none                                                                                      *
 *                                                                                              *
 ************************************************************************************************/
void R_FogDistance_f( void ) {
	float distance;

	if( !tr.world ) {
		ri.Printf( PRINT_ALL, "R_FogDistance_f: World is not initialized\n" );
		return;
	}

	if( tr.world->globalFog == -1 ) {
		ri.Printf( PRINT_ALL, "R_FogDistance_f: World does not have a global fog\n" );
		return;
	}

	if( ri.Cmd_Argc() <= 1 ) {
		//		should not ever be 0.0
		//		if (tr.world->fogs[tr.world->globalFog].tcScale == 0.0)
		//		{
		//			distance = 0.0;
		//		}
		//		else
		{
			distance = 1.0 / ( 8.0 * tr.world->fogs[tr.world->globalFog].tcScale );
		}

		ri.Printf( PRINT_ALL, "R_FogDistance_f: Current Distance: %.0f\n", distance );
		return;
	}

	if( ri.Cmd_Argc() != 2 ) {
		ri.Printf( PRINT_ALL, "R_FogDistance_f: Invalid number of arguments to set distance\n" );
		return;
	}

	distance = atof( ri.Cmd_Argv( 1 ) );
	if( distance < 1.0 ) {
		distance = 1.0;
	}
	tr.world->fogs[tr.world->globalFog].parms.depthForOpaque = distance;
	tr.world->fogs[tr.world->globalFog].tcScale = 1.0 / ( distance * 8 );
}

/************************************************************************************************
 * R_FogColor_f                                                                                 *
 *    Console command to change the global fog color.  Specifying nothing on the command will   *
 *    display the current global fog color.  Specifying a float R G B values between 0.0 and    *
 *    1.0 will change the fog color.                                                            *
 *                                                                                              *
 * Input                                                                                        *
 *    none                                                                                      *
 *                                                                                              *
 * Output / Return                                                                              *
 *    none                                                                                      *
 *                                                                                              *
 ************************************************************************************************/
void R_FogColor_f( void ) {
	if( !tr.world ) {
		ri.Printf( PRINT_ALL, "R_FogColor_f: World is not initialized\n" );
		return;
	}

	if( tr.world->globalFog == -1 ) {
		ri.Printf( PRINT_ALL, "R_FogColor_f: World does not have a global fog\n" );
		return;
	}

	if( ri.Cmd_Argc() <= 1 ) {
		unsigned i = tr.world->fogs[tr.world->globalFog].colorInt;

		ri.Printf( PRINT_ALL, "R_FogColor_f: Current Color: %0f %0f %0f\n",
			   ( ( byte * )&i )[0] / 255.0,
			   ( ( byte * )&i )[1] / 255.0,
			   ( ( byte * )&i )[2] / 255.0 );
		return;
	}

	if( ri.Cmd_Argc() != 4 ) {
		ri.Printf( PRINT_ALL, "R_FogColor_f: Invalid number of arguments to set color\n" );
		return;
	}

	tr.world->fogs[tr.world->globalFog].parms.color[0] = atof( ri.Cmd_Argv( 1 ) );
	tr.world->fogs[tr.world->globalFog].parms.color[1] = atof( ri.Cmd_Argv( 2 ) );
	tr.world->fogs[tr.world->globalFog].parms.color[2] = atof( ri.Cmd_Argv( 3 ) );
	tr.world->fogs[tr.world->globalFog].colorInt = ColorBytes4( atof( ri.Cmd_Argv( 1 ) ) * tr.identityLight,
								    atof( ri.Cmd_Argv( 2 ) ) * tr.identityLight,
								    atof( ri.Cmd_Argv( 3 ) ) * tr.identityLight, 1.0 );
}

typedef struct consoleCommand_s {
	const char *cmd;
	xcommand_t func;
} consoleCommand_t;

void R_ReloadFonts_f( void );

static consoleCommand_t commands[] = {
	{ "imagelist", R_ImageList_f },
	{ "bufferlist", R_BufferList_f },
	{ "shaderlist", R_ShaderList_f },
	{ "skinlist", R_SkinList_f },
	{ "fontlist", R_FontList_f },
	{ "screenshot", R_ScreenShot_f },
	{ "screenshot_png", R_ScreenShotPNG_f },
	{ "screenshot_tga", R_ScreenShotTGA_f },
	{ "gfxinfo", GfxInfo_f },
	{ "r_we", R_WorldEffect_f },
	{ "imagecacheinfo", RE_RegisterImages_Info_f },
	{ "modellist", R_Modellist_f },
	{ "modelcacheinfo", RE_RegisterModels_Info_f },
	{ "r_fogDistance", R_FogDistance_f },
	{ "r_fogColor", R_FogColor_f },
	{ "r_reloadfonts", R_ReloadFonts_f },
};

static const size_t numCommands = ARRAY_LEN( commands );

#ifdef _DEBUG
#	define MIN_PRIMITIVES -1
#else
#	define MIN_PRIMITIVES 0
#endif
#define MAX_PRIMITIVES 3

/*
===============
R_Register
===============
*/
void R_Register( void ) {
	//
	// latched and archived variables
	//

	r_allowExtensions = ri.Cvar_Get( "r_allowExtensions", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ext_compressed_textures = ri.Cvar_Get( "r_ext_compress_textures", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ext_compressed_lightmaps = ri.Cvar_Get( "r_ext_compress_lightmaps", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ext_preferred_tc_method = ri.Cvar_Get( "r_ext_preferred_tc_method", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ext_gamma_control = ri.Cvar_Get( "r_ext_gamma_control", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ext_multitexture = ri.Cvar_Get( "r_ext_multitexture", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ext_compiled_vertex_array = ri.Cvar_Get( "r_ext_compiled_vertex_array", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ext_texture_env_add = ri.Cvar_Get( "r_ext_texture_env_add", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_ext_texture_filter_anisotropic = ri.Cvar_Get( "r_ext_texture_filter_anisotropic", "16", CVAR_ARCHIVE_ND );

	r_DynamicGlow = ri.Cvar_Get( "r_DynamicGlow", "0", CVAR_ARCHIVE_ND );
	r_DynamicGlowPasses = ri.Cvar_Get( "r_DynamicGlowPasses", "5", CVAR_ARCHIVE_ND );
	r_DynamicGlowDelta = ri.Cvar_Get( "r_DynamicGlowDelta", "0.8f", CVAR_ARCHIVE_ND );
	r_DynamicGlowIntensity = ri.Cvar_Get( "r_DynamicGlowIntensity", "1.13f", CVAR_ARCHIVE_ND );
	r_DynamicGlowSoft = ri.Cvar_Get( "r_DynamicGlowSoft", "1", CVAR_ARCHIVE_ND );
	r_DynamicGlowWidth = ri.Cvar_Get( "r_DynamicGlowWidth", "320", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_DynamicGlowHeight = ri.Cvar_Get( "r_DynamicGlowHeight", "240", CVAR_ARCHIVE_ND | CVAR_LATCH );

	r_picmip = ri.Cvar_Get( "r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH );
	ri.Cvar_CheckRange( r_picmip, 0, 16, qtrue );
	r_colorMipLevels = ri.Cvar_Get( "r_colorMipLevels", "0", CVAR_LATCH );
	r_detailTextures = ri.Cvar_Get( "r_detailtextures", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_texturebits = ri.Cvar_Get( "r_texturebits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_texturebitslm = ri.Cvar_Get( "r_texturebitslm", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_overBrightBits = ri.Cvar_Get( "r_overBrightBits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_mapOverBrightBits = ri.Cvar_Get( "r_mapOverBrightBits", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_simpleMipMaps = ri.Cvar_Get( "r_simpleMipMaps", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	r_vertexLight = ri.Cvar_Get( "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH );
	r_subdivisions = ri.Cvar_Get( "r_subdivisions", "4", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_subdivisions, 0, 80, qfalse );
	r_intensity = ri.Cvar_Get( "r_intensity", "1", CVAR_LATCH | CVAR_ARCHIVE_ND );

	r_api = ri.Cvar_Get( "r_api", "0", CVAR_ARCHIVE_ND | CVAR_INIT );

	//
	// temporary latched variables that can only change over a restart
	//
	r_fullbright = ri.Cvar_Get( "r_fullbright", "0", CVAR_LATCH );
	r_singleShader = ri.Cvar_Get( "r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH );

	//
	// archived variables that can change at any time
	//
	r_lodCurveError = ri.Cvar_Get( "r_lodCurveError", "250", CVAR_ARCHIVE_ND );
	r_lodbias = ri.Cvar_Get( "r_lodbias", "0", CVAR_ARCHIVE_ND );
	r_flares = ri.Cvar_Get( "r_flares", "1", CVAR_ARCHIVE_ND );
	r_lodscale = ri.Cvar_Get( "r_lodscale", "10", CVAR_ARCHIVE_ND );

	r_znear = ri.Cvar_Get( "r_znear", "4", CVAR_ARCHIVE_ND ); // if set any lower, you lose a lot of precision in the distance
	ri.Cvar_CheckRange( r_znear, 0.001f, 10, qfalse );		  // was qtrue in JA, is qfalse properly in ioq3
	r_ignoreGLErrors = ri.Cvar_Get( "r_ignoreGLErrors", "1", CVAR_ARCHIVE_ND );
	r_fastsky = ri.Cvar_Get( "r_fastsky", "0", CVAR_ARCHIVE_ND );
	r_drawSun = ri.Cvar_Get( "r_drawSun", "0", CVAR_ARCHIVE_ND );
	r_dynamiclight = ri.Cvar_Get( "r_dynamiclight", "1", CVAR_ARCHIVE );
	// rjr - removed for hacking
	//	r_dlightBacks = ri.Cvar_Get( "r_dlightBacks", "0", CVAR_ARCHIVE );
	r_finish = ri.Cvar_Get( "r_finish", "0", CVAR_ARCHIVE_ND );
	r_textureMode = ri.Cvar_Get( "r_textureMode", "GL_LINEAR_MIPMAP_LINEAR", CVAR_ARCHIVE );
	r_gamma = ri.Cvar_Get( "r_gamma", "1", CVAR_ARCHIVE_ND );
	r_facePlaneCull = ri.Cvar_Get( "r_facePlaneCull", "1", CVAR_ARCHIVE_ND );

	r_dlightStyle = ri.Cvar_Get( "r_dlightStyle", "1", CVAR_ARCHIVE_ND );
	r_surfaceSprites = ri.Cvar_Get( "r_surfaceSprites", "1", CVAR_ARCHIVE_ND );
	r_surfaceWeather = ri.Cvar_Get( "r_surfaceWeather", "0", CVAR_TEMP );

	r_windSpeed = ri.Cvar_Get( "r_windSpeed", "0", 0 );
	r_windAngle = ri.Cvar_Get( "r_windAngle", "0", 0 );
	r_windGust = ri.Cvar_Get( "r_windGust", "0", 0 );
	r_windDampFactor = ri.Cvar_Get( "r_windDampFactor", "0.1", 0 );
	r_windPointForce = ri.Cvar_Get( "r_windPointForce", "0", 0 );
	r_windPointX = ri.Cvar_Get( "r_windPointX", "0", 0 );
	r_windPointY = ri.Cvar_Get( "r_windPointY", "0", 0 );

	r_primitives = ri.Cvar_Get( "r_primitives", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_primitives, MIN_PRIMITIVES, MAX_PRIMITIVES, qtrue );

	r_ambientScale = ri.Cvar_Get( "r_ambientScale", "0.5", CVAR_CHEAT );
	r_directedScale = ri.Cvar_Get( "r_directedScale", "1", CVAR_CHEAT );

	//
	// temporary variables that can change at any time
	//
	r_showImages = ri.Cvar_Get( "r_showImages", "0", CVAR_CHEAT );

	r_debugLight = ri.Cvar_Get( "r_debuglight", "0", CVAR_TEMP );
	r_debugStyle = ri.Cvar_Get( "r_debugStyle", "-1", CVAR_CHEAT );
	r_debugSort = ri.Cvar_Get( "r_debugSort", "0", CVAR_CHEAT );

	r_nocurves = ri.Cvar_Get( "r_nocurves", "0", CVAR_CHEAT );
	r_drawworld = ri.Cvar_Get( "r_drawworld", "1", CVAR_CHEAT );
#ifdef JK2_MODE
	r_drawfog = ri.Cvar_Get( "r_drawfog", "1", CVAR_CHEAT );
#else
	r_drawfog = ri.Cvar_Get( "r_drawfog", "2", CVAR_CHEAT );
#endif
	r_lightmap = ri.Cvar_Get( "r_lightmap", "0", CVAR_CHEAT );
	r_portalOnly = ri.Cvar_Get( "r_portalOnly", "0", CVAR_CHEAT );

	r_skipBackEnd = ri.Cvar_Get( "r_skipBackEnd", "0", CVAR_CHEAT );

	r_measureOverdraw = ri.Cvar_Get( "r_measureOverdraw", "0", CVAR_CHEAT );
	r_norefresh = ri.Cvar_Get( "r_norefresh", "0", CVAR_CHEAT );
	r_drawentities = ri.Cvar_Get( "r_drawentities", "1", CVAR_CHEAT );
	r_ignore = ri.Cvar_Get( "r_ignore", "1", CVAR_TEMP );
	r_nocull = ri.Cvar_Get( "r_nocull", "0", CVAR_CHEAT );
	r_novis = ri.Cvar_Get( "r_novis", "0", CVAR_CHEAT );
	r_showcluster = ri.Cvar_Get( "r_showcluster", "0", CVAR_CHEAT );
	r_speeds = ri.Cvar_Get( "r_speeds", "0", CVAR_CHEAT );
	r_verbose = ri.Cvar_Get( "r_verbose", "0", CVAR_CHEAT );
	r_logFile = ri.Cvar_Get( "r_logFile", "0", CVAR_CHEAT );
	r_debugSurface = ri.Cvar_Get( "r_debugSurface", "0", CVAR_CHEAT );
	r_nobind = ri.Cvar_Get( "r_nobind", "0", CVAR_CHEAT );
	r_showtris = ri.Cvar_Get( "r_showtris", "0", CVAR_CHEAT );
	r_showtriscolor = ri.Cvar_Get( "r_showtriscolor", "0", CVAR_ARCHIVE_ND );
	r_showsky = ri.Cvar_Get( "r_showsky", "0", CVAR_CHEAT );
	r_shownormals = ri.Cvar_Get( "r_shownormals", "0", CVAR_CHEAT );
	r_clear = ri.Cvar_Get( "r_clear", "0", CVAR_CHEAT );
	r_offsetFactor = ri.Cvar_Get( "r_offsetfactor", "-1", CVAR_CHEAT );
	r_offsetUnits = ri.Cvar_Get( "r_offsetunits", "-2", CVAR_CHEAT );
	r_lockpvs = ri.Cvar_Get( "r_lockpvs", "0", CVAR_CHEAT );
	r_noportals = ri.Cvar_Get( "r_noportals", "0", CVAR_CHEAT );
	r_shadows = ri.Cvar_Get( "cg_shadows", "1", 0 );
	r_shadowRange = ri.Cvar_Get( "r_shadowRange", "1000", CVAR_ARCHIVE_ND );
	r_atmosphere = ri.Cvar_Get( "r_atmosphere", "1", CVAR_CHEAT );

	/*
	Ghoul2 Insert Start
	*/
	r_noGhoul2 = ri.Cvar_Get( "r_noghoul2", "0", CVAR_CHEAT );
	r_Ghoul2AnimSmooth = ri.Cvar_Get( "r_ghoul2animsmooth", "0.25", 0 );
	r_Ghoul2UnSqash = ri.Cvar_Get( "r_ghoul2unsquash", "1", 0 );
	r_Ghoul2TimeBase = ri.Cvar_Get( "r_ghoul2timebase", "2", 0 );
	r_Ghoul2NoLerp = ri.Cvar_Get( "r_ghoul2nolerp", "0", 0 );
	r_Ghoul2NoBlend = ri.Cvar_Get( "r_ghoul2noblend", "0", 0 );
	r_Ghoul2BlendMultiplier = ri.Cvar_Get( "r_ghoul2blendmultiplier", "1", 0 );
	r_Ghoul2UnSqashAfterSmooth = ri.Cvar_Get( "r_ghoul2unsquashaftersmooth", "1", 0 );

	broadsword = ri.Cvar_Get( "broadsword", "1", 0 );
	broadsword_kickbones = ri.Cvar_Get( "broadsword_kickbones", "1", 0 );
	broadsword_kickorigin = ri.Cvar_Get( "broadsword_kickorigin", "1", 0 );
	broadsword_dontstopanim = ri.Cvar_Get( "broadsword_dontstopanim", "0", 0 );
	broadsword_waitforshot = ri.Cvar_Get( "broadsword_waitforshot", "0", 0 );
	broadsword_playflop = ri.Cvar_Get( "broadsword_playflop", "1", 0 );
	broadsword_smallbbox = ri.Cvar_Get( "broadsword_smallbbox", "0", 0 );
	broadsword_extra1 = ri.Cvar_Get( "broadsword_extra1", "0", 0 );
	broadsword_extra2 = ri.Cvar_Get( "broadsword_extra2", "0", 0 );
	broadsword_effcorr = ri.Cvar_Get( "broadsword_effcorr", "1", 0 );
	broadsword_ragtobase = ri.Cvar_Get( "broadsword_ragtobase", "2", 0 );
	broadsword_dircap = ri.Cvar_Get( "broadsword_dircap", "64", 0 );

	/*
	Ghoul2 Insert End
	*/

	sv_mapname = ri.Cvar_Get( "mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM );
	sv_mapChecksum = ri.Cvar_Get( "sv_mapChecksum", "", CVAR_ROM );
	se_language = ri.Cvar_Get( "se_language", "english", CVAR_ARCHIVE | CVAR_NORESTART );
#ifdef JK2_MODE
	sp_language = ri.Cvar_Get( "sp_language", va( "%d", SP_LANGUAGE_ENGLISH ), CVAR_ARCHIVE | CVAR_NORESTART );
#endif
	com_buildScript = ri.Cvar_Get( "com_buildScript", "0", 0 );

	r_modelpoolmegs = ri.Cvar_Get( "r_modelpoolmegs", "20", CVAR_ARCHIVE );
	if( ri.LowPhysicalMemory() ) {
		ri.Cvar_Set( "r_modelpoolmegs", "0" );
	}

	r_environmentMapping = ri.Cvar_Get( "r_environmentMapping", "1", CVAR_ARCHIVE_ND );

	r_screenshotJpegQuality = ri.Cvar_Get( "r_screenshotJpegQuality", "95", CVAR_ARCHIVE_ND );

	ri.Cvar_CheckRange( r_screenshotJpegQuality, 10, 100, qtrue );

	for( size_t i = 0; i < numCommands; i++ )
		ri.Cmd_AddCommand( commands[i].cmd, commands[i].func );
}

// need to do this hackery so ghoul2 doesn't crash the game because of ITS hackery...
//
void R_ClearStuffToStopGhoul2CrashingThings( void ) {
	backEndData = NULL;
	memset( &tr, 0, sizeof( tr ) );
}

/*
===============
R_InitFuncTables
===============
*/
static void R_InitFuncTables() {
	int i, funcTablesSize;
	tr_shader::trFuncTables_t *funcTables;

	funcTablesSize = sizeof( tr_shader::trFuncTables_t );

	// create a buffer for the function tables
	tr.funcTablesBuffer = R_CreateBuffer( funcTablesSize,
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	funcTables = (tr_shader::trFuncTables_t *)VK_BeginUploadBuffer( tr.funcTablesBuffer, funcTablesSize, 0 );

	//
	// init function tables
	//
	for( i = 0; i < TR_FUNCTABLE_SIZE; i++ ) {
		funcTables->sinTable[i] = sin( DEG2RAD( i * 360.0f / ( (float)( TR_FUNCTABLE_SIZE - 1 ) ) ) );
		funcTables->squareTable[i] = ( i < TR_FUNCTABLE_SIZE / 2 ) ? 1.0f : -1.0f;
		funcTables->sawToothTable[i] = (float)i / TR_FUNCTABLE_SIZE;
		funcTables->inverseSawToothTable[i] = 1.0f - funcTables->sawToothTable[i];

		if( i < TR_FUNCTABLE_SIZE / 2 ) {
			if( i < TR_FUNCTABLE_SIZE / 4 ) {
				funcTables->triangleTable[i] = (float)i / ( TR_FUNCTABLE_SIZE / 4 );
			}
			else {
				funcTables->triangleTable[i] = 1.0f - funcTables->triangleTable[i - TR_FUNCTABLE_SIZE / 4];
			}
		}
		else {
			funcTables->triangleTable[i] = -funcTables->triangleTable[i - TR_FUNCTABLE_SIZE / 2];
		}
	}

	// upload the data to the GPU
	VK_EndUploadBuffer();
}

/*
===============
R_Init
===============
*/
extern void R_InitWorldEffects();
void R_Init( void ) {
	int i;

	// ri.Printf( PRINT_ALL, "----- R_Init -----\n" );

	ShaderEntryPtrs_Clear();

	// clear all our internal state
	memset( &tr, 0, sizeof( tr ) );
	memset( &backEnd, 0, sizeof( backEnd ) );
	memset( &tess, 0, sizeof( tess ) );

	// register cvars before initializing the vulkan context
	R_Register();
	R_ImageLoader_Init();

	backEndData = (backEndData_t *)R_Hunk_Alloc( sizeof( backEndData_t ), qtrue );

	VK_Init();

	R_InitFuncTables();
	R_InitFogTable();

	R_NoiseInit();

	const color4ub_t color = { 0xff, 0xff, 0xff, 0xff };
	for( i = 0; i < MAX_LIGHT_STYLES; i++ ) {
		byteAlias_t *ba = ( byteAlias_t * )&color;
		RE_SetLightStyle( i, ba->i );
	}

	R_InitImages();
	R_InitBuffers();
	R_InitShaders();
	R_InitSkins();
	R_ModelInit();
	R_InitWorldEffects();
	R_InitFonts();

	InitVulkanDescriptorSets();

	RestoreGhoul2InfoArray();
	// print info
	GfxInfo_f();

	// begin the first frame
	VK_EndFrame();
	VK_BeginFrame();

	// ri.Printf( PRINT_ALL, "----- finished R_Init -----\n" );
}

/*
===============
RE_Shutdown
===============
*/
extern void R_ShutdownWorldEffects( void );
void RE_Shutdown( qboolean destroyWindow, qboolean restarting ) {
	for( size_t i = 0; i < numCommands; i++ )
		ri.Cmd_RemoveCommand( commands[i].cmd );

	R_ShutdownWorldEffects();
	R_ShutdownFonts();

	if( tr.registered ) {
		R_IssuePendingRenderCommands();
		vkDeviceWaitIdle( vkState.device );

		VK_Free( vkFreeDescriptorSets, vkState.descriptorPool, tr.commonDescriptorSet );
		VK_Free( vkFreeDescriptorSets, vkState.descriptorPool, tr.samplerDescriptorSet );

		R_DeleteBuffers( TAG_HUNKALLOC );
		R_DeleteTransientTextures();

		if( destroyWindow ) {
			int i;

			R_DeleteTextures(); // only do this for vid_restart now, not during things like map load
			R_DeleteBuffers( TAG_ALL );

			if( restarting ) {
				SaveGhoul2InfoArray();
			}

			VK_Delete( vkDestroyPipelineLayout, vkState.antialiasingPipelineLayout.handle );
			VK_Delete( vkDestroyPipeline, vkState.antialiasingPipeline.handle );

			VK_Delete( vkDestroyPipelineLayout, vkState.shadePipelineLayout.handle );
			VK_Delete( vkDestroyPipelineLayout, vkState.ghoul2ShadePipelineLayout.handle );

			VK_Delete( vkDestroyPipelineLayout, vkState.skyboxPipelineLayout.handle );
			VK_Delete( vkDestroyPipeline, vkState.skyboxPipeline.handle );

			VK_Delete( vkDestroyPipelineLayout, vkState.wireframePipelineLayout.handle );
			VK_Delete( vkDestroyPipeline, vkState.wireframePipeline.handle );
			VK_Delete( vkDestroyPipeline, vkState.wireframeXRayPipeline.handle );

			VK_Delete( vkDestroyPipeline, vkState.glowBlurPipeline.handle );
			VK_Delete( vkDestroyPipelineLayout, vkState.glowBlurPipelineLayout.handle );
			VK_Delete( vkDestroyPipeline, vkState.glowCombinePipeline.handle );
			VK_Delete( vkDestroyPipelineLayout, vkState.glowCombinePipelineLayout.handle );

			VK_Delete( vkDestroyDescriptorSetLayout, vkState.commonDescriptorSetLayout );
			VK_Delete( vkDestroyDescriptorSetLayout, vkState.samplerDescriptorSetLayout );
			VK_Delete( vkDestroyDescriptorSetLayout, vkState.shaderDescriptorSetLayout );
			VK_Delete( vkDestroyDescriptorSetLayout, vkState.modelDescriptorSetLayout );
			VK_Delete( vkDestroyDescriptorSetLayout, vkState.textureDescriptorSetLayout );
			VK_Delete( vkDestroyDescriptorSetLayout, vkState.viewDescriptorSetLayout );

			VK_Delete( vkDestroySampler, vkState.wrapModeSampler );
			VK_Delete( vkDestroySampler, vkState.clampModeSampler );
			VK_Delete( vkDestroySampler, vkState.pointClampSampler );
			VK_Delete( vkDestroySampler, vkState.pointWrapSampler );
			VK_Delete( vkDestroySampler, vkState.linearClampSampler );
			VK_Delete( vkDestroySampler, vkState.linearWrapSampler );
			VK_Delete( vkDestroySampler, vkState.skyFogColorSampler );

			for( i = 0; i < vkState.imgcount; ++i ) {
				VK_Delete( vkDestroyFence, vkState.fences[i] );
				VK_Delete( vkDestroySemaphore, vkState.semaphores[i] );
			}

			VK_Delete( vkDestroyCommandPool, vkState.cmdpool );
			VK_Delete( vkDestroyDescriptorPool, vkState.descriptorPool );
			VK_Delete( vkDestroySwapchainKHR, vkState.swapchain );
			vmaDestroyAllocator( vkState.allocator );
			vkDestroyDevice( vkState.device, NULL );

			VK_IDelete( vkDestroySurfaceKHR, vkState.surface );
			VK_IDelete( vkState.pfnDestroyDebugMessenger, vkState.debugMessenger );
			vkDestroyInstance( vkState.instance, NULL );

			memset( &vkState, 0, sizeof( vkState ) );
		}
	}

	// shut down platform specific OpenGL stuff
	if( destroyWindow ) {
		ri.WIN_Shutdown();
	}
	tr.registered = qfalse;
}

/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
void RE_EndRegistration( void ) {
	R_IssuePendingRenderCommands();
}


void RE_GetLightStyle( int style, color4ub_t color ) {
	if( style >= MAX_LIGHT_STYLES ) {
		Com_Error( ERR_FATAL, "RE_GetLightStyle: %d is out of range", ( int )style );
		return;
	}

	byteAlias_t *baDest = ( byteAlias_t * )&color, *baSource = ( byteAlias_t * )&styleColors[style];
	baDest->i = baSource->i;
}

void RE_SetLightStyle( int style, int color ) {
	if( style >= MAX_LIGHT_STYLES ) {
		Com_Error( ERR_FATAL, "RE_SetLightStyle: %d is out of range", ( int )style );
		return;
	}

	byteAlias_t *ba = ( byteAlias_t * )&styleColors[style];
	if( ba->i != color ) {
		ba->i = color;
		styleUpdated[style] = true;
	}
}

/*
=====================
tr_distortionX

DLL glue
=====================
*/

extern float tr_distortionAlpha;
extern float tr_distortionStretch;
extern qboolean tr_distortionPrePost;
extern qboolean tr_distortionNegate;

float *get_tr_distortionAlpha( void ) {
	return &tr_distortionAlpha;
}

float *get_tr_distortionStretch( void ) {
	return &tr_distortionStretch;
}

qboolean *get_tr_distortionPrePost( void ) {
	return &tr_distortionPrePost;
}

qboolean *get_tr_distortionNegate( void ) {
	return &tr_distortionNegate;
}

float g_oldRangedFog = 0.0f;
void RE_SetRangedFog( float dist ) {
	if( tr.rangedFog <= 0.0f ) {
		g_oldRangedFog = tr.rangedFog;
	}
	tr.rangedFog = dist;
	if( tr.rangedFog == 0.0f && g_oldRangedFog ) { // restore to previous state if applicable
		tr.rangedFog = g_oldRangedFog;
	}
}

// bool inServer = false;
void RE_SVModelInit( void ) {
	tr.numModels = 0;
	tr.numShaders = 0;
	tr.numSkins = 0;

	InitVulkanDescriptorSetLayouts();

	R_InitImages();
	// inServer = true;
	R_InitShaders();
	// inServer = false;
	R_ModelInit();
}

/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
extern void R_LoadImage( const char *shortname, byte **pic, int *width, int *height );
extern void R_WorldEffectCommand( const char *command );
extern qboolean R_inPVS( vec3_t p1, vec3_t p2 );
extern void RE_GetModelBounds( refEntity_t *refEnt, vec3_t bounds1, vec3_t bounds2 );
extern void G2API_AnimateG2Models( CGhoul2Info_v &ghoul2, int AcurrentTime, CRagDollUpdateParams *params );
extern qboolean G2API_GetRagBonePos( CGhoul2Info_v &ghoul2, const char *boneName, vec3_t pos, vec3_t entAngles, vec3_t entPos, vec3_t entScale );
extern qboolean G2API_RagEffectorKick( CGhoul2Info_v &ghoul2, const char *boneName, vec3_t velocity );
extern qboolean G2API_RagForceSolve( CGhoul2Info_v &ghoul2, qboolean force );
extern qboolean G2API_SetBoneIKState( CGhoul2Info_v &ghoul2, int time, const char *boneName, int ikState, sharedSetBoneIKStateParams_t *params );
extern qboolean G2API_IKMove( CGhoul2Info_v &ghoul2, int time, sharedIKMoveParams_t *params );
extern qboolean G2API_RagEffectorGoal( CGhoul2Info_v &ghoul2, const char *boneName, vec3_t pos );
extern qboolean G2API_RagPCJGradientSpeed( CGhoul2Info_v &ghoul2, const char *boneName, const float speed );
extern qboolean G2API_RagPCJConstraint( CGhoul2Info_v &ghoul2, const char *boneName, vec3_t min, vec3_t max );
extern void G2API_SetRagDoll( CGhoul2Info_v &ghoul2, CRagDollParams *parms );
#ifdef G2_PERFORMANCE_ANALYSIS
extern void G2Time_ResetTimers( void );
extern void G2Time_ReportTimers( void );
#endif
extern IGhoul2InfoArray &TheGhoul2InfoArray();

#ifdef JK2_MODE
unsigned int AnyLanguage_ReadCharFromString_JK2( char **text, qboolean *pbIsTrailingPunctuation ) {
	return AnyLanguage_ReadCharFromString( text, pbIsTrailingPunctuation );
}
#endif

extern "C" Q_EXPORT refexport_t *QDECL GetRefAPI( int apiVersion, refimport_t *refimp ) {
	static refexport_t re;

	ri = *refimp;

	memset( &re, 0, sizeof( re ) );

	if( apiVersion != REF_API_VERSION ) {
		ri.Printf( PRINT_ALL, "Mismatched REF_API_VERSION: expected %i, got %i\n",
			   REF_API_VERSION, apiVersion );
		return NULL;
	}

	// the RE_ functions are Renderer Entry points

#define REX( x ) re.x = RE_##x

	REX( Shutdown );

	REX( BeginRegistration );
	REX( RegisterModel );
	REX( RegisterSkin );
	REX( GetAnimationCFG );
	REX( RegisterShader );
	REX( RegisterShaderNoMip );
	re.LoadWorld = RE_LoadWorldMap;
	re.R_LoadImage = R_LoadImage;

	REX( RegisterMedia_LevelLoadBegin );
	REX( RegisterMedia_LevelLoadEnd );
	REX( RegisterMedia_GetLevel );
	REX( RegisterImages_LevelLoadEnd );
	REX( RegisterModels_LevelLoadEnd );

	REX( SetWorldVisData );

	REX( EndRegistration );

	REX( ClearScene );
	REX( AddRefEntityToScene );
	REX( GetLighting );
	REX( AddPolyToScene );
	REX( AddLightToScene );
	REX( RenderScene );
	REX( GetLighting );

	REX( SetColor );
	re.DrawStretchPic = RE_StretchPic;
	re.DrawRotatePic = RE_RotatePic;
	re.DrawRotatePic2 = RE_RotatePic2;
	REX( LAGoggles );
	REX( Scissor );

	re.DrawStretchRaw = RE_StretchRaw;
	REX( UploadCinematic );

	REX( BeginFrame );
	REX( EndFrame );

	REX( ProcessDissolve );
	REX( InitDissolve );

	REX( GetScreenShot );
#ifdef JK2_MODE
	REX( SaveJPGToBuffer );
	re.LoadJPGFromBuffer = LoadJPGFromBuffer;
#endif
	REX( TempRawImage_ReadFromFile );
	REX( TempRawImage_CleanUp );

	re.MarkFragments = R_MarkFragments;
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;
	REX( GetLightStyle );
	REX( SetLightStyle );
	REX( GetBModelVerts );
	re.WorldEffectCommand = R_WorldEffectCommand;
	REX( GetModelBounds );

	REX( SVModelInit );

	REX( RegisterFont );
	REX( Font_HeightPixels );
	REX( Font_StrLenPixels );
	REX( Font_DrawString );
	REX( Font_StrLenChars );
	re.Language_IsAsian = Language_IsAsian;
	re.Language_UsesSpaces = Language_UsesSpaces;
	re.AnyLanguage_ReadCharFromString = AnyLanguage_ReadCharFromString;
#ifdef JK2_MODE
	re.AnyLanguage_ReadCharFromString2 = AnyLanguage_ReadCharFromString_JK2;
#endif

	re.R_InitWorldEffects = R_InitWorldEffects;
	re.R_ClearStuffToStopGhoul2CrashingThings = R_ClearStuffToStopGhoul2CrashingThings;
	re.R_inPVS = R_inPVS;

	re.tr_distortionAlpha = get_tr_distortionAlpha;
	re.tr_distortionStretch = get_tr_distortionStretch;
	re.tr_distortionPrePost = get_tr_distortionPrePost;
	re.tr_distortionNegate = get_tr_distortionNegate;

	re.GetWindVector = R_GetWindVector;
	re.GetWindGusting = R_GetWindGusting;
	re.IsOutside = R_IsOutside;
	re.IsOutsideCausingPain = R_IsOutsideCausingPain;
	re.GetChanceOfSaberFizz = R_GetChanceOfSaberFizz;
	re.IsShaking = R_IsShaking;
	re.AddWeatherZone = R_AddWeatherZone;
	re.SetTempGlobalFogColor = R_SetTempGlobalFogColor;

	REX( SetRangedFog );

	re.TheGhoul2InfoArray = TheGhoul2InfoArray;

#define G2EX( x ) re.G2API_##x = G2API_##x

	G2EX( AddBolt );
	G2EX( AddBoltSurfNum );
	G2EX( AddSurface );
	G2EX( AnimateG2Models );
	G2EX( AttachEnt );
	G2EX( AttachG2Model );
	G2EX( CollisionDetect );
	G2EX( CleanGhoul2Models );
	G2EX( CopyGhoul2Instance );
	G2EX( DetachEnt );
	G2EX( DetachG2Model );
	G2EX( GetAnimFileName );
	G2EX( GetAnimFileNameIndex );
	G2EX( GetAnimFileInternalNameIndex );
	G2EX( GetAnimIndex );
	G2EX( GetAnimRange );
	G2EX( GetAnimRangeIndex );
	G2EX( GetBoneAnim );
	G2EX( GetBoneAnimIndex );
	G2EX( GetBoneIndex );
	G2EX( GetBoltMatrix );
	G2EX( GetGhoul2ModelFlags );
	G2EX( GetGLAName );
	G2EX( GetParentSurface );
	G2EX( GetRagBonePos );
	G2EX( GetSurfaceIndex );
	G2EX( GetSurfaceName );
	G2EX( GetSurfaceRenderStatus );
	G2EX( GetTime );
	G2EX( GiveMeVectorFromMatrix );
	G2EX( HaveWeGhoul2Models );
	G2EX( IKMove );
	G2EX( InitGhoul2Model );
	G2EX( IsPaused );
	G2EX( ListBones );
	G2EX( ListSurfaces );
	G2EX( LoadGhoul2Models );
	G2EX( LoadSaveCodeDestructGhoul2Info );
	G2EX( PauseBoneAnim );
	G2EX( PauseBoneAnimIndex );
	G2EX( PrecacheGhoul2Model );
	G2EX( RagEffectorGoal );
	G2EX( RagEffectorKick );
	G2EX( RagForceSolve );
	G2EX( RagPCJConstraint );
	G2EX( RagPCJGradientSpeed );
	G2EX( RemoveBolt );
	G2EX( RemoveBone );
	G2EX( RemoveGhoul2Model );
	G2EX( RemoveSurface );
	G2EX( SaveGhoul2Models );
	G2EX( SetAnimIndex );
	G2EX( SetBoneAnim );
	G2EX( SetBoneAnimIndex );
	G2EX( SetBoneAngles );
	G2EX( SetBoneAnglesIndex );
	G2EX( SetBoneAnglesMatrix );
	G2EX( SetBoneIKState );
	G2EX( SetGhoul2ModelFlags );
	G2EX( SetGhoul2ModelIndexes );
	G2EX( SetLodBias );
	// G2EX(SetModelIndexes);
	G2EX( SetNewOrigin );
	G2EX( SetRagDoll );
	G2EX( SetRootSurface );
	G2EX( SetShader );
	G2EX( SetSkin );
	G2EX( SetSurfaceOnOff );
	G2EX( SetTime );
	G2EX( StopBoneAnim );
	G2EX( StopBoneAnimIndex );
	G2EX( StopBoneAngles );
	G2EX( StopBoneAnglesIndex );
#ifdef _G2_GORE
	G2EX( AddSkinGore );
	G2EX( ClearSkinGore );
#endif

#ifdef G2_PERFORMANCE_ANALYSIS
	re.G2Time_ReportTimers = G2Time_ReportTimers;
	re.G2Time_ResetTimers = G2Time_ResetTimers;
#endif

	// Swap_Init();

	return &re;
}
