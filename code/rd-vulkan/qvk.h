
#pragma once

#if defined( __LINT__ )
#	error
#elif defined( _WIN32 )
#	define VK_USE_PLATFORM_WIN32_KHR
#	define SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined( MACOS_X )
#	error
#elif defined( __linux__ ) || defined( __FreeBSD__ ) || defined( __OpenBSD__ )
#	define VK_USE_PLATFORM_XLIB_KHR
#	define SURFACE_EXTENSION_NAME VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#endif

#include <vulkan/vulkan.h>

#define VMA_VULKAN_VERSION 1001000 // Vulkan 1.1
#include "VulkanMemoryAllocator/vk_mem_alloc.h"
