
#pragma once

#if defined( __LINT__ )
#	error
#elif defined( _WIN32 )
#	define VK_USE_PLATFORM_WIN32_KHR
#	include <vulkan/vulkan.h>
#elif defined( MACOS_X )
#	error
#elif defined( __linux__ )
#	error
#elif defined( __FreeBSD__ ) || defined( __OpenBSD__ ) // rb010123
#	error
#else
#	include <gl.h>
#endif

#include "VulkanMemoryAllocator/vk_mem_alloc.h"
