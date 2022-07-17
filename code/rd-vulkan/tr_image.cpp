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

// tr_image.c

#include "../server/exe_headers.h"

#include "tr_local.h"
#include "../rd-common/tr_common.h"
#include <png.h>
#include <map>
#include <cmp_core.h>

static byte s_intensitytable[256];
static unsigned char s_gammatable[256];

#define FILE_HASH_SIZE 1024 // actually, the shader code needs this (from another module, great).
// static	image_t*		hashTable[FILE_HASH_SIZE];

/*
** R_GammaCorrect
*/
void R_GammaCorrect( byte *buffer, int bufSize ) {
	int i;

	for( i = 0; i < bufSize; i++ ) {
		buffer[i] = s_gammatable[buffer[i]];
	}
}

typedef struct {
	const char *name;
	VkFilter minimize, maximize;
	VkSamplerMipmapMode mipmap;
} textureMode_t;

textureMode_t modes[] = {
	{ "GL_NEAREST", VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST },
	{ "GL_LINEAR", VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR },
	{ "GL_NEAREST_MIPMAP_NEAREST", VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_MIPMAP_MODE_NEAREST },
	{ "GL_LINEAR_MIPMAP_NEAREST", VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_NEAREST },
	{ "GL_NEAREST_MIPMAP_LINEAR", VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR },
	{ "GL_LINEAR_MIPMAP_LINEAR", VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR }
};

static const size_t numTextureModes = ARRAY_LEN( modes );

/*
================
return a hash value for the filename
================
*/
long generateHashValue( const char *fname ) {
	int i;
	long hash;
	char letter;

	hash = 0;
	i = 0;
	while( fname[i] != '\0' ) {
		letter = tolower( fname[i] );
		if( letter == '.' )
			break; // don't include extension
		if( letter == '\\' )
			letter = '/'; // damn path names
		hash += (long)( letter ) * ( i + 119 );
		i++;
	}
	hash &= ( FILE_HASH_SIZE - 1 );
	return hash;
}

// makeup a nice clean, consistant name to query for and file under, for map<> usage...
//
char *GenerateImageMappingName( const char *name ) {
	static char sName[MAX_QPATH];
	int i = 0;
	char letter;

	while( name[i] != '\0' && i < MAX_QPATH - 1 ) {
		letter = tolower( name[i] );
		if( letter == '.' )
			break; // don't include extension
		if( letter == '\\' )
			letter = '/'; // damn path names
		sName[i++] = letter;
	}
	sName[i] = 0;

	return &sName[0];
}

/*
===============
VK_TextureMode
===============
*/
void VK_TextureMode( const char *string ) {
	size_t i;
	image_t *glt;
	VkResult res;

	for( i = 0; i < numTextureModes; i++ ) {
		if( !Q_stricmp( modes[i].name, string ) ) {
			break;
		}
	}

	if( i == numTextureModes ) {
		ri.Printf( PRINT_ALL, "bad filter name\n" );
		for( i = 0; i < numTextureModes; i++ ) {
			ri.Printf( PRINT_ALL, "%s\n", modes[i].name );
		}
		return;
	}

	textureMode_t *mode = &modes[i];

	// If the level they requested is less than possible, set the max possible...
	if( r_ext_texture_filter_anisotropic->value > glConfig.maxTextureFilterAnisotropy ) {
		ri.Cvar_SetValue( "r_ext_texture_filter_anisotropic", glConfig.maxTextureFilterAnisotropy );
	}

	float anisotropy = r_ext_texture_filter_anisotropic->value;

	// destroy the old samplers
	if( tr.wrapModeSampler )
		vkDestroySampler( vkState.device, tr.wrapModeSampler, NULL );
	if( tr.clampModeSampler )
		vkDestroySampler( vkState.device, tr.clampModeSampler, NULL );

	// create the new sampler objects
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.minFilter = mode->minimize;
	samplerCreateInfo.magFilter = mode->maximize;
	samplerCreateInfo.mipmapMode = mode->mipmap;
	samplerCreateInfo.anisotropyEnable = ( anisotropy > 0.f );
	samplerCreateInfo.maxAnisotropy = anisotropy;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	samplerCreateInfo.maxLod = FLT_MAX;

	res = vkCreateSampler( vkState.device, &samplerCreateInfo, NULL, &tr.wrapModeSampler );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "VK_TextureMode: failed to create a wrap mode sampler (%d)\n", res );
	}

	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	res = vkCreateSampler( vkState.device, &samplerCreateInfo, NULL, &tr.clampModeSampler );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "VK_TextureMode: failed to create a clamp mode sampler (%d)\n", res );
	}
}


/*
===============
VK_GetInternalFormat

===============
*/
static VkFormat VK_GetInternalFormat( const byte *data, int width, int height, qboolean isLightmap, qboolean allowTC ) {
	int samples;
	int i, c;
	float rMax = 0, gMax = 0, bMax = 0;
	byte *scan;

	//
	// scan the texture for each channel's max values
	// and verify if the alpha channel is being used or not
	//
	c = width * height;
	scan = ( (byte *)data );
	samples = 3;
	for( i = 0; i < c; i++ ) {
		if( scan[i * 4 + 0] > rMax ) {
			rMax = scan[i * 4 + 0];
		}
		if( scan[i * 4 + 1] > gMax ) {
			gMax = scan[i * 4 + 1];
		}
		if( scan[i * 4 + 2] > bMax ) {
			bMax = scan[i * 4 + 2];
		}
		if( scan[i * 4 + 3] != 255 ) {
			samples = 4;
			break;
		}
	}

	// select proper internal format
	if( samples == 3 ) {
		if( glConfig.textureCompression == TC_S3TC_DXT && allowTC ) { // Compress purely color - no alpha
			if( r_texturebits->integer == 16 ) {
				return VK_FORMAT_BC1_RGB_UNORM_BLOCK; // this format cuts to 16 bit
			}
			else { // if we aren't using 16 bit then, use 32 bit compression
				return VK_FORMAT_BC3_UNORM_BLOCK;
			}
		}
		else if( isLightmap && r_texturebitslm->integer > 0 ) {
			// Allow different bit depth when we are a lightmap
			if( r_texturebitslm->integer == 16 ) {
				return VK_FORMAT_R5G6B5_UNORM_PACK16;
			}
			else if( r_texturebitslm->integer == 32 ) {
				return VK_FORMAT_R8G8B8A8_UNORM;
			}
		}
		else if( r_texturebits->integer == 16 ) {
			return VK_FORMAT_R5G6B5_UNORM_PACK16;
		}
		return VK_FORMAT_R8G8B8A8_UNORM;
	}
	else if( samples == 4 ) {
		if( glConfig.textureCompression == TC_S3TC_DXT && allowTC ) { // Compress both alpha and color
			if( r_texturebits->integer == 16 ) {
				return VK_FORMAT_BC1_RGBA_UNORM_BLOCK; // this format cuts to 16 bit
			}
			else { // if we aren't using 16 bit then, use 32 bit compression
				return VK_FORMAT_BC3_UNORM_BLOCK;
			}
		}
		else if( r_texturebits->integer == 16 ) {
			return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
		}
		return VK_FORMAT_R8G8B8A8_UNORM;
	}

	return VK_FORMAT_UNDEFINED;
}

static int VK_GetInternalFormatTexelSize( VkFormat format ) {
	switch( format ) {
	case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
		return 8; // per block
	case VK_FORMAT_BC3_UNORM_BLOCK:
		return 16; // per block
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_UNORM:
		return 4; // per pixel
	case VK_FORMAT_R8G8B8_UNORM:
		return 3; // per pixel
	case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
	case VK_FORMAT_R5G6B5_UNORM_PACK16:
		return 2; // per pixel
	default:
		return 0;
	}
}

static bool VK_IsCompressed( VkFormat format ) {
	switch( format ) {
	case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
	case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
	case VK_FORMAT_BC3_UNORM_BLOCK:
		return true;
	default:
		return false;
	}
}

static bool VK_IsDepthStencil( VkFormat format ) {
	switch( format ) {
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D16_UNORM_S8_UINT:
	case VK_FORMAT_D24_UNORM_S8_UINT:
	case VK_FORMAT_D32_SFLOAT:
	case VK_FORMAT_D32_SFLOAT_S8_UINT:
	case VK_FORMAT_S8_UINT:
		return true;
	default:
		return false;
	}
}

static bool VK_IsDepthOnly( VkFormat format ) {
	switch( format ) {
	case VK_FORMAT_D16_UNORM:
	case VK_FORMAT_D32_SFLOAT:
		return true;
	default:
		return false;
	}
}

static bool VK_IsStencilOnly( VkFormat format ) {
	return format == VK_FORMAT_S8_UINT;
}

int VK_GetRequiredImageUploadBufferSize( VkFormat format, int width, int height );

/*
===============
R_SumOfUsedImages
===============
*/
float R_SumOfUsedImages( qboolean bUseFormat ) {
	int total = 0;
	image_t *pImage;

	R_Images_StartIteration();
	while( ( pImage = R_Images_GetNextIteration() ) != NULL ) {
		if( pImage->frameUsed == tr.frameCount - 1 ) { // it has already been advanced for the next frame, so...
			if( bUseFormat ) {
				total += VK_GetRequiredImageUploadBufferSize( pImage->internalFormat, pImage->width, pImage->height );
			}
			else {
				total += pImage->width * pImage->height;
			}
		}
	}

	return total;
}

/*
===============
R_ImageList_f
===============
*/
void R_ImageList_f( void ) {
	int i = 0;
	image_t *image;
	int texels = 0;
	//	int		totalFileSizeK = 0;
	float texBytes = 0.0f;
	const char *yesno[] = { "no ", "yes" };

	ri.Printf( PRINT_ALL, "\n      -w-- -h-- -fsK- -mm- -if- wrap --name-------\n" );

	int iNumImages = R_Images_StartIteration();
	while( ( image = R_Images_GetNextIteration() ) != NULL ) {
		texels += image->width * image->height;
		texBytes += VK_GetRequiredImageUploadBufferSize( image->internalFormat, image->width, image->height );
		//		totalFileSizeK += (image->imgfileSize+1023)/1024;
		// ri.Printf (PRINT_ALL,  "%4i: %4i %4i %5i  %s ",
		//	i, image->width, image->height,(image->fileSize+1023)/1024, yesno[image->mipmap] );
		ri.Printf( PRINT_ALL, "%4i: %4i %4i  %s ",
			i, image->width, image->height, yesno[image->mipmap] );

		switch( image->internalFormat ) {
		case VK_FORMAT_B8G8R8A8_UNORM:
			ri.Printf( PRINT_ALL, "RGBA8" );
			break;
		case VK_FORMAT_R8G8B8_UNORM:
			ri.Printf( PRINT_ALL, "RGB8 " );
			break;
		case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
			ri.Printf( PRINT_ALL, "DXT1 " );
			break;
		case VK_FORMAT_BC3_UNORM_BLOCK:
			ri.Printf( PRINT_ALL, "DXT5 " );
			break;
		case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
			ri.Printf( PRINT_ALL, "RGBA4" );
			break;
		case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
			ri.Printf( PRINT_ALL, "RGB5 " );
			break;
		default:
			ri.Printf( PRINT_ALL, "???? " );
		}

		switch( image->wrapClampMode ) {
		case VK_SAMPLER_ADDRESS_MODE_REPEAT:
			ri.Printf( PRINT_ALL, "rept " );
			break;
		case VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE:
			ri.Printf( PRINT_ALL, "clmp " );
			break;
		default:
			ri.Printf( PRINT_ALL, "%4i ", image->wrapClampMode );
			break;
		}

		ri.Printf( PRINT_ALL, "%s\n", image->imgName );
		i++;
	}
	ri.Printf( PRINT_ALL, " ---------\n" );
	ri.Printf( PRINT_ALL, "      -w-- -h-- -mm- -if- wrap --name-------\n" );
	ri.Printf( PRINT_ALL, " %i total texels (not including mipmaps)\n", texels );
	//	ri.Printf (PRINT_ALL, " %iMB total filesize\n", (totalFileSizeK+1023)/1024 );
	ri.Printf( PRINT_ALL, " %.2fMB total texture mem (not including mipmaps)\n", texBytes / 1048576.0f );
	ri.Printf( PRINT_ALL, " %i total images\n\n", iNumImages );
}

//=======================================================================


/*
================
R_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
static void R_LightScaleTexture( unsigned *in, int inwidth, int inheight, qboolean only_gamma ) {
	if( only_gamma ) {
		if( !glConfig.deviceSupportsGamma ) {
			int i, c;
			byte *p;

			p = (byte *)in;

			c = inwidth * inheight;
			for( i = 0; i < c; i++, p += 4 ) {
				p[0] = s_gammatable[p[0]];
				p[1] = s_gammatable[p[1]];
				p[2] = s_gammatable[p[2]];
			}
		}
	}
	else {
		int i, c;
		byte *p;

		p = (byte *)in;

		c = inwidth * inheight;

		if( glConfig.deviceSupportsGamma ) {
			for( i = 0; i < c; i++, p += 4 ) {
				p[0] = s_intensitytable[p[0]];
				p[1] = s_intensitytable[p[1]];
				p[2] = s_intensitytable[p[2]];
			}
		}
		else {
			for( i = 0; i < c; i++, p += 4 ) {
				p[0] = s_gammatable[s_intensitytable[p[0]]];
				p[1] = s_gammatable[s_intensitytable[p[1]]];
				p[2] = s_gammatable[s_intensitytable[p[2]]];
			}
		}
	}
}


/*
================
R_MipMap2

Uses temp mem, but then copies back to input, quartering the size of the texture
Proper linear filter
================
*/
static void R_MipMap2( unsigned *in, int inWidth, int inHeight ) {
	int i, j, k;
	byte *outpix;
	int inWidthMask, inHeightMask;
	int total;
	int outWidth, outHeight;
	unsigned *temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;
	temp = (unsigned int *)R_Malloc( outWidth * outHeight * 4, TAG_TEMP_WORKSPACE, qfalse );

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for( i = 0; i < outHeight; i++ ) {
		for( j = 0; j < outWidth; j++ ) {
			outpix = (byte *)( temp + i * outWidth + j );
			for( k = 0; k < 4; k++ ) {
				total =
					1 * ( (byte *)&in[( ( i * 2 - 1 ) & inHeightMask ) * inWidth + ( ( j * 2 - 1 ) & inWidthMask )] )[k] +
					2 * ( (byte *)&in[( ( i * 2 - 1 ) & inHeightMask ) * inWidth + ( ( j * 2 ) & inWidthMask )] )[k] +
					2 * ( (byte *)&in[( ( i * 2 - 1 ) & inHeightMask ) * inWidth + ( ( j * 2 + 1 ) & inWidthMask )] )[k] +
					1 * ( (byte *)&in[( ( i * 2 - 1 ) & inHeightMask ) * inWidth + ( ( j * 2 + 2 ) & inWidthMask )] )[k] +

					2 * ( (byte *)&in[( ( i * 2 ) & inHeightMask ) * inWidth + ( ( j * 2 - 1 ) & inWidthMask )] )[k] +
					4 * ( (byte *)&in[( ( i * 2 ) & inHeightMask ) * inWidth + ( ( j * 2 ) & inWidthMask )] )[k] +
					4 * ( (byte *)&in[( ( i * 2 ) & inHeightMask ) * inWidth + ( ( j * 2 + 1 ) & inWidthMask )] )[k] +
					2 * ( (byte *)&in[( ( i * 2 ) & inHeightMask ) * inWidth + ( ( j * 2 + 2 ) & inWidthMask )] )[k] +

					2 * ( (byte *)&in[( ( i * 2 + 1 ) & inHeightMask ) * inWidth + ( ( j * 2 - 1 ) & inWidthMask )] )[k] +
					4 * ( (byte *)&in[( ( i * 2 + 1 ) & inHeightMask ) * inWidth + ( ( j * 2 ) & inWidthMask )] )[k] +
					4 * ( (byte *)&in[( ( i * 2 + 1 ) & inHeightMask ) * inWidth + ( ( j * 2 + 1 ) & inWidthMask )] )[k] +
					2 * ( (byte *)&in[( ( i * 2 + 1 ) & inHeightMask ) * inWidth + ( ( j * 2 + 2 ) & inWidthMask )] )[k] +

					1 * ( (byte *)&in[( ( i * 2 + 2 ) & inHeightMask ) * inWidth + ( ( j * 2 - 1 ) & inWidthMask )] )[k] +
					2 * ( (byte *)&in[( ( i * 2 + 2 ) & inHeightMask ) * inWidth + ( ( j * 2 ) & inWidthMask )] )[k] +
					2 * ( (byte *)&in[( ( i * 2 + 2 ) & inHeightMask ) * inWidth + ( ( j * 2 + 1 ) & inWidthMask )] )[k] +
					1 * ( (byte *)&in[( ( i * 2 + 2 ) & inHeightMask ) * inWidth + ( ( j * 2 + 2 ) & inWidthMask )] )[k];
				outpix[k] = total / 36;
			}
		}
	}

	memcpy( in, temp, outWidth * outHeight * 4 );
	R_Free( temp );
}

/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap( byte *in, int width, int height ) {
	int i, j;
	byte *out;
	int row;

	if( width == 1 && height == 1 ) {
		return;
	}

	if( !r_simpleMipMaps->integer ) {
		R_MipMap2( (unsigned *)in, width, height );
		return;
	}

	row = width * 4;
	out = in;
	width >>= 1;
	height >>= 1;

	if( width == 0 || height == 0 ) {
		width += height; // get largest
		for( i = 0; i < width; i++, out += 4, in += 8 ) {
			out[0] = ( in[0] + in[4] ) >> 1;
			out[1] = ( in[1] + in[5] ) >> 1;
			out[2] = ( in[2] + in[6] ) >> 1;
			out[3] = ( in[3] + in[7] ) >> 1;
		}
		return;
	}

	for( i = 0; i < height; i++, in += row ) {
		for( j = 0; j < width; j++, out += 4, in += 8 ) {
			out[0] = ( in[0] + in[4] + in[row + 0] + in[row + 4] ) >> 2;
			out[1] = ( in[1] + in[5] + in[row + 1] + in[row + 5] ) >> 2;
			out[2] = ( in[2] + in[6] + in[row + 2] + in[row + 6] ) >> 2;
			out[3] = ( in[3] + in[7] + in[row + 3] + in[row + 7] ) >> 2;
		}
	}
}


/*
==================
R_BlendOverTexture

Apply a color blend over a set of pixels
==================
*/
static void R_BlendOverTexture( byte *data, int pixelCount, byte blend[4] ) {
	int i;
	int inverseAlpha;
	int premult[3];

	inverseAlpha = 255 - blend[3];
	premult[0] = blend[0] * blend[3];
	premult[1] = blend[1] * blend[3];
	premult[2] = blend[2] * blend[3];

	for( i = 0; i < pixelCount; i++, data += 4 ) {
		data[0] = ( data[0] * inverseAlpha + premult[0] ) >> 9;
		data[1] = ( data[1] * inverseAlpha + premult[1] ) >> 9;
		data[2] = ( data[2] * inverseAlpha + premult[2] ) >> 9;
	}
}

byte mipBlendColors[16][4] = {
	{ 0, 0, 0, 0 },
	{ 255, 0, 0, 128 },
	{ 0, 255, 0, 128 },
	{ 0, 0, 255, 128 },
	{ 255, 0, 0, 128 },
	{ 0, 255, 0, 128 },
	{ 0, 0, 255, 128 },
	{ 255, 0, 0, 128 },
	{ 0, 255, 0, 128 },
	{ 0, 0, 255, 128 },
	{ 255, 0, 0, 128 },
	{ 0, 255, 0, 128 },
	{ 0, 0, 255, 128 },
	{ 255, 0, 0, 128 },
	{ 0, 255, 0, 128 },
	{ 0, 0, 255, 128 },
};


/*
===============
VK_AdjustTextureSize

===============
*/
static void VK_AdjustTextureSize( byte *data, int *pWidth, int *pHeight, qboolean picmip ) {
	int i;
	int width = *pWidth;
	int height = *pHeight;

	//
	// perform optional picmip operation
	//
	if( picmip ) {
		for( i = 0; i < r_picmip->integer; i++ ) {
			R_MipMap( data, width, height );
			width >>= 1;
			height >>= 1;
			if( width < 1 ) {
				width = 1;
			}
			if( height < 1 ) {
				height = 1;
			}
		}
	}

	//
	// clamp to the current upper OpenGL limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	while( width > glConfig.maxTextureSize || height > glConfig.maxTextureSize ) {
		R_MipMap( data, width, height );
		width >>= 1;
		height >>= 1;
	}

	*pWidth = width;
	*pHeight = height;
}


/*
===============
VKimp_Convert

Converts the picture from RGBA8 format to the internal format, performing any texture compression or
mipmap generation if required.

dst:			buffer for the output data
pic:			source image data
width:			width of the image, in pixels
height:			height of the image, in pixels
internalFormat:	format of the image after conversion
mipmap:			whether to generate mipmaps

===============
*/
static bool VK_ConvertImage( byte *dst, const byte *pic, int width, int height, VkFormat internalFormat ) {
	if( internalFormat == VK_FORMAT_BC1_RGB_UNORM_BLOCK ) {
		// block conversion without alpha
		const int wblocks = maximum( 1, ( width >> 2 ) );
		const int hblocks = maximum( 1, ( height >> 2 ) );
		// compress the first mip
		int stride = width << 2;
		for( int yb = 0; yb < hblocks; ++yb ) {
			for( int xb = 0; xb < wblocks; ++xb ) {
				byte *dstBlock = dst + ( ( ( yb * wblocks ) + xb ) << 3 );	// x 8 bytes/block
				const byte *src = pic + ( ( ( yb * wblocks ) + xb ) << 6 ); // x 64 bytes/block
				CompressBlockBC1( src, stride, dstBlock, NULL );
			}
		}
		return true;
	}
	else if( internalFormat == VK_FORMAT_BC3_UNORM_BLOCK ) {
		// block conversion without alpha
		const int wblocks = maximum( 1, ( width >> 2 ) );
		const int hblocks = maximum( 1, ( height >> 2 ) );
		// compress the first mip
		int stride = width << 2;
		for( int yb = 0; yb < hblocks; ++yb ) {
			for( int xb = 0; xb < wblocks; ++xb ) {
				byte *dstBlock = dst + ( ( ( yb * wblocks ) + xb ) << 4 );	// x 16 bytes/block
				const byte *src = pic + ( ( ( yb * wblocks ) + xb ) << 6 ); // x 64 bytes/block
				CompressBlockBC3( src, stride, dstBlock, NULL );
			}
		}
		return true;
	}
	else {
		assert( !VK_IsCompressed( internalFormat ) );
		const int texelSize = VK_GetInternalFormatTexelSize( internalFormat );
		// copy the data
		memcpy( dst, pic, width * height * texelSize );
	}
	return true;
}

static int VK_GetRequiredImageUploadBufferSize( VkFormat format, int width, int height ) {
	int texelSize = VK_GetInternalFormatTexelSize( format );

	if( VK_IsCompressed( format ) ) {
		return ( ( width + 3 ) >> 2 ) * ( ( height + 3 ) >> 2 ) * texelSize;
	}

	else {
		return width * height * texelSize;
	}
}


/*
===============
VK_UploadImage

===============
*/
void VK_UploadImage( image_t *image, const byte *pic, int width, int height, int mip ) {
	int i, c;
	int requiredBufferSize = VK_GetRequiredImageUploadBufferSize( image->internalFormat, width, height );
	uploadBuffer_t *uploadBuffer = VK_GetUploadBuffer( requiredBufferSize );

	assert( uploadBuffer );
	assert( ( uploadBuffer->buffer->size - uploadBuffer->offset ) >= requiredBufferSize );

	// copy or resample data as appropriate for first MIP level
	VK_ConvertImage( ( (byte *)uploadBuffer->buffer->allocationInfo.pMappedData ) + uploadBuffer->offset,
		pic, width, height, image->internalFormat );

	// R_LightScaleTexture( (unsigned int *)pic, width, height, (qboolean)!mipmap );

	// copy the data from the upload buffer to the image resource
	VkBufferImageCopy uploadRegion = {};
	uploadRegion.imageExtent.width = width;
	uploadRegion.imageExtent.height = height;
	uploadRegion.imageExtent.depth = 1;
	uploadRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	uploadRegion.imageSubresource.mipLevel = mip;
	uploadRegion.imageSubresource.layerCount = 1;
	uploadRegion.bufferOffset = uploadBuffer->offset;

	VK_SetImageLayout( image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT );
	vkCmdCopyBufferToImage( backEndData->cmdbuf,
		uploadBuffer->buffer->buf,
		image->tex,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &uploadRegion );

	uploadBuffer->offset += requiredBufferSize;
}

class CStringComparator {
public:
	bool operator()( const char *s1, const char *s2 ) const { return ( Q_stricmp( s1, s2 ) < 0 ); }
};

typedef std::map<const char *, image_t *, CStringComparator> AllocatedImages_t;
AllocatedImages_t AllocatedImages;
AllocatedImages_t::iterator itAllocatedImages;

int giTextureBindNum = 1024; // will be set to this anyway at runtime, but wtf?

int R_Images_StartIteration( void ) {
	itAllocatedImages = AllocatedImages.begin();
	return AllocatedImages.size();
}

image_t *R_Images_GetNextIteration( void ) {
	if( itAllocatedImages == AllocatedImages.end() )
		return NULL;

	image_t *pImage = ( *itAllocatedImages ).second;
	++itAllocatedImages;
	return pImage;
}

// since the ownership of image_t structures have been moved to vkresmgr_t, free only the allocation and Vulkan resource,
// but keep the image_t object, so that it can be reused.
static void R_Images_DeleteImageContents( image_t *pImage ) {
	assert( pImage ); // should never be called with NULL
	if( pImage ) {
		if( pImage->texview ) {
			vkFreeDescriptorSets( vkState.device, vkState.descriptorPool, 1, &pImage->descriptorSet );
			vkDestroyImageView( vkState.device, pImage->texview, NULL );
		}
		vmaDestroyImage( vkState.allocator, pImage->tex, pImage->allocation );
		R_Free( pImage );
	}
}

// special function used in conjunction with "devmapbsp"...
//
void R_Images_DeleteLightMaps( void ) {
	for( AllocatedImages_t::iterator itImage = AllocatedImages.begin(); itImage != AllocatedImages.end(); /* empty */ ) {
		image_t *pImage = ( *itImage ).second;

		if( pImage->imgName[0] == '$' /*&& strstr(pImage->imgName,"lightmap")*/ ) // loose check, but should be ok
		{
			R_Images_DeleteImageContents( pImage );

			AllocatedImages.erase( itImage++ );
		}
		else {
			++itImage;
		}
	}
}

// special function currently only called by Dissolve code...
//
void R_Images_DeleteImage( image_t *pImage ) {
	// Even though we supply the image handle, we need to get the corresponding iterator entry...
	//
	AllocatedImages_t::iterator itImage = AllocatedImages.find( pImage->imgName );
	if( itImage != AllocatedImages.end() ) {
		R_Images_DeleteImageContents( pImage );
		AllocatedImages.erase( itImage );
	}
	else {
		assert( 0 );
	}
}

// called only at app startup, vid_restart, app-exit
//
void R_Images_Clear( void ) {
	image_t *pImage;
	//	int iNumImages =
	R_Images_StartIteration();
	while( ( pImage = R_Images_GetNextIteration() ) != NULL ) {
		R_Images_DeleteImageContents( pImage );
	}

	AllocatedImages.clear();
}


void RE_RegisterImages_Info_f( void ) {
	image_t *pImage = NULL;
	int iImage = 0;
	int iTexels = 0;

	int iNumImages = R_Images_StartIteration();
	while( ( pImage = R_Images_GetNextIteration() ) != NULL ) {
		ri.Printf( PRINT_ALL, "%d: (%4dx%4dy) \"%s\"", iImage, pImage->width, pImage->height, pImage->imgName );
		ri.Printf( PRINT_ALL, ", levused %d", pImage->iLastLevelUsedOn );
		ri.Printf( PRINT_ALL, "\n" );

		iTexels += pImage->width * pImage->height;
		iImage++;
	}
	ri.Printf( PRINT_ALL, "%d Images. %d (%.2fMB) texels total, (not including mipmaps)\n", iNumImages, iTexels, (float)iTexels / 1024.0f / 1024.0f );
	ri.Printf( PRINT_DEVELOPER, "RE_RegisterMedia_GetLevel(): %d", RE_RegisterMedia_GetLevel() );
}

// currently, this just goes through all the images and dumps any not referenced on this level...
//
qboolean RE_RegisterImages_LevelLoadEnd( void ) {
	// ri.Printf( PRINT_DEVELOPER, "RE_RegisterImages_LevelLoadEnd():\n");

	qboolean imageDeleted = qtrue;
	for( AllocatedImages_t::iterator itImage = AllocatedImages.begin(); itImage != AllocatedImages.end(); /* blank */ ) {
		qboolean bEraseOccured = qfalse;

		image_t *pImage = ( *itImage ).second;

		// don't un-register system shaders (*fog, *dlight, *white, *default), but DO de-register lightmaps ("$<mapname>/lightmap%d")
		if( pImage->imgName[0] != '*' ) {
			// image used on this level?
			//
			if( pImage->iLastLevelUsedOn != RE_RegisterMedia_GetLevel() ) { // nope, so dump it...
				// ri.Printf( PRINT_DEVELOPER, "Dumping image \"%s\"\n",pImage->imgName);
				R_Images_DeleteImageContents( pImage );

				AllocatedImages.erase( itImage++ );
				bEraseOccured = qtrue;
				imageDeleted = qtrue;
			}
		}

		if( !bEraseOccured ) {
			++itImage;
		}
	}

	// ri.Printf( PRINT_DEVELOPER, "RE_RegisterImages_LevelLoadEnd(): Ok\n");

	return imageDeleted;
}



// returns image_t struct if we already have this, else NULL. No disk-open performed
//	(important for creating default images).
//
// This is called by both R_FindImageFile and anything that creates default images...
//
static image_t *R_FindImageFile_NoLoad( const char *name, qboolean mipmap, qboolean allowPicmip, qboolean allowTC, VkSamplerAddressMode wrapClampMode ) {
	if( !name ) {
		return NULL;
	}

	char *pName = GenerateImageMappingName( name );

	//
	// see if the image is already loaded
	//
	AllocatedImages_t::iterator itAllocatedImage = AllocatedImages.find( pName );
	if( itAllocatedImage != AllocatedImages.end() ) {
		image_t *pImage = ( *itAllocatedImage ).second;

		// the white image can be used with any set of parms, but other mismatches are errors...
		//
		if( strcmp( pName, "*white" ) ) {
			if( pImage->mipmap != !!mipmap ) {
				ri.Printf( PRINT_WARNING, "WARNING: reused image %s with mixed mipmap parm\n", pName );
			}
			if( pImage->allowPicmip != !!allowPicmip ) {
				ri.Printf( PRINT_WARNING, "WARNING: reused image %s with mixed allowPicmip parm\n", pName );
			}
			if( pImage->wrapClampMode != wrapClampMode ) {
				ri.Printf( PRINT_WARNING, "WARNING: reused image %s with mixed wrapClampMode parm\n", pName );
			}
		}

		pImage->iLastLevelUsedOn = RE_RegisterMedia_GetLevel();

		return pImage;
	}

	return NULL;
}


// initializes the image object
static void VK_InitImage( image_t *image, const char *name, int width, int height, VkFormat internalFormat, qboolean mipmap, qboolean allowPicmip,
	VkSamplerAddressMode wrapClampMode, VkImageUsageFlags usage, VkImageTiling tiling ) {
	VkResult res;

	// record which map it was used on...
	//
	image->iLastLevelUsedOn = RE_RegisterMedia_GetLevel();

	image->mipmap = !!mipmap;
	image->allowPicmip = !!allowPicmip;

	Q_strncpyz( image->imgName, name, sizeof( image->imgName ) );

	image->width = width;
	image->height = height;
	image->internalFormat = internalFormat;
	image->wrapClampMode = wrapClampMode;

	// create the Vulkan resource
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.format = image->internalFormat;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.usage = usage;
	imageCreateInfo.tiling = tiling;

	if( mipmap ) {
		int mipwidth = width;
		int mipheight = height;

		// compute number of mipmaps to generate
		while( mipwidth > 1 || mipheight > 1 ) {
			imageCreateInfo.mipLevels++;

			mipwidth >>= 1;
			mipheight >>= 1;
			if( mipwidth < 1 )
				mipwidth = 1;
			if( mipheight < 1 )
				mipheight = 1;
		}
	}

	VmaAllocationCreateInfo allocationCreateInfo = {};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	if( tiling == VK_IMAGE_TILING_LINEAR ) {
		// readback images must be host-visible
		allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		allocationCreateInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	}

	res = vmaCreateImage( vkState.allocator, &imageCreateInfo, &allocationCreateInfo, &image->tex, &image->allocation, NULL );
	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "VK_InitImage: failed to create VkImage object (%d)\n", res );
	}

	if( usage & ( VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT ) ) {
		VkImageViewCreateInfo imageViewCreateInfo = {};

		// create an image view
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.image = image->tex;
		imageViewCreateInfo.format = image->internalFormat;
		imageViewCreateInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		imageViewCreateInfo.subresourceRange.layerCount = 1;

		// set the aspect flags based on the format of the image
		if( VK_IsDepthStencil( image->internalFormat ) ) {
			if( VK_IsStencilOnly( image->internalFormat ) )
				imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
			else if( VK_IsDepthOnly( image->internalFormat ) )
				imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			else
				imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		else {
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		res = vkCreateImageView( vkState.device, &imageViewCreateInfo, NULL, &image->texview );
		if( res != VK_SUCCESS ) {
			Com_Error( ERR_FATAL, "VK_InitImage: failed to create VkImageView object (%d)\n", res );
		}

		// allocate a descriptor set for the texture
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.descriptorPool = vkState.descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &tr.textureDescriptorSetLayout;

		res = vkAllocateDescriptorSets( vkState.device, &descriptorSetAllocateInfo, &image->descriptorSet );
		if( res != VK_SUCCESS ) {
			Com_Error( ERR_FATAL, "VK_InitImage: failed to allocate VkDescriptorSet object (%d)\n", res );
		}

		CDescriptorSetWriter descriptorSetWriter( image->descriptorSet );
		descriptorSetWriter.writeImage( 0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, image );
		descriptorSetWriter.writeSampler( 1, ( wrapClampMode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE ) ? tr.linearClampSampler : tr.linearWrapSampler );
		descriptorSetWriter.flush();
	}
}


/*
================
R_CreateImage

This is the only way any image_t are created
================
*/
image_t *R_CreateImage( const char *name, const byte *pic, int width, int height,
	VkFormat format, qboolean mipmap, qboolean allowPicmip, qboolean allowTC, VkSamplerAddressMode wrapClampMode ) {
	image_t *image;
	qboolean isLightmap = qfalse;
	VkFormat internalFormat;

	if( strlen( name ) >= MAX_QPATH ) {
		Com_Error( ERR_DROP, "R_CreateImage: \"%s\" is too long\n", name );
	}

	if( name[0] == '$' ) {
		isLightmap = qtrue;
	}

	if( ( width & ( width - 1 ) ) || ( height & ( height - 1 ) ) ) {
		Com_Error( ERR_FATAL, "R_CreateImage: %s dimensions (%i x %i) not power of 2!\n", name, width, height );
	}

	image = R_FindImageFile_NoLoad( name, mipmap, allowPicmip, allowTC, wrapClampMode );
	if( image ) {
		return image;
	}

	image = (image_t *)R_Malloc( sizeof( image_t ), TAG_IMAGE_T, qtrue );

	// Resize the image if needed
	VK_AdjustTextureSize( (byte *)pic, &width, &height, allowPicmip );

	// image->imgfileSize=fileSize;

	internalFormat = VK_GetInternalFormat( pic, width, height, isLightmap, allowTC );
	VK_InitImage( image, name, width, height, internalFormat, mipmap, allowPicmip, wrapClampMode,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_IMAGE_TILING_OPTIMAL );

	// upload the first mip
	VK_UploadImage( image, pic, width, height, 0 );

	if( mipmap ) {
		int mipwidth = width;
		int mipheight = height;
		int miplevel = 0;

		// generate and upload the remaining mips
		while( mipwidth > 1 || mipheight > 1 ) {
			miplevel++;

			R_MipMap( (byte *)pic, mipwidth, mipheight );

			if( r_colorMipLevels->integer ) {
				R_BlendOverTexture( (byte *)pic, mipwidth * mipheight, mipBlendColors[miplevel] );
			}

			mipwidth >>= 1;
			mipheight >>= 1;
			if( mipwidth < 1 )
				mipwidth = 1;
			if( mipheight < 1 )
				mipheight = 1;

			VK_UploadImage( image, pic, mipwidth, mipheight, miplevel );
		}
	}

	return image;
}

image_t *R_CreateTransientImage( const char *name, int width, int height, VkFormat format, VkSamplerAddressMode wrapClampMode ) {
	image_t *image;

	VkImageUsageFlags usage;

	if( strlen( name ) >= MAX_QPATH ) {
		Com_Error( ERR_DROP, "R_CreateTransientImage: \"%s\" is too long\n", name );
	}

	image = (image_t *)R_Malloc( sizeof( image_t ), TAG_IMAGE_T, qtrue );

	// set the usage flags based on the format of the image
	usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if( VK_IsDepthStencil( format ) )
		usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	else
		usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VK_InitImage( image, name, width, height, format, qfalse, qfalse, wrapClampMode, usage, VK_IMAGE_TILING_OPTIMAL );

	return image;
}

image_t *R_CreateReadbackImage( const char *name, int width, int height, VkFormat format ) {
	image_t *image;

	VkImageUsageFlags usage;

	if( strlen( name ) >= MAX_QPATH ) {
		Com_Error( ERR_DROP, "R_CreateReadbackImage: \"%s\" is too long\n", name );
	}

	image = (image_t *)R_Malloc( sizeof( image_t ), TAG_IMAGE_T, qtrue );

	VK_InitImage( image, name, width, height, format, qfalse, qfalse,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_IMAGE_TILING_LINEAR );

	return image;
}

/*
===============
R_FindImageFile

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
image_t *R_FindImageFile( const char *name, qboolean mipmap, qboolean allowPicmip, qboolean allowTC, VkSamplerAddressMode wrapClampMode ) {
	image_t *image;
	int width, height;
	byte *pic;

	if( !name ) {
		return NULL;
	}

	image = R_FindImageFile_NoLoad( name, mipmap, allowPicmip, allowTC, wrapClampMode );
	if( image ) {
		return image;
	}

	//
	// load the pic from disk
	//
	R_LoadImage( name, &pic, &width, &height );
	if( !pic ) {
		return NULL;
	}

	image = R_CreateImage( (char *)name, pic, width, height, VK_FORMAT_B8G8R8A8_UNORM, mipmap, allowPicmip, allowTC, wrapClampMode );
	R_Free( pic );
	return image;
}


/*
================
R_CreateDlightImage
================
*/
#define DLIGHT_SIZE 64
static void R_CreateDlightImage( void ) {
#ifdef JK2_MODE
	int x, y;
	byte data[DLIGHT_SIZE][DLIGHT_SIZE][4];
	int xs, ys;
	int b;

	// The old code claims to have made a centered inverse-square falloff blob for dynamic lighting
	//	and it looked nasty, so, just doing something simpler that seems to have a much softer result
	for( x = 0; x < DLIGHT_SIZE; x++ ) {
		for( y = 0; y < DLIGHT_SIZE; y++ ) {
			xs = ( DLIGHT_SIZE * 0.5f - x );
			ys = ( DLIGHT_SIZE * 0.5f - y );

			b = 255 - sqrt( (double)xs * xs + ys * ys ) * 9.0f; // try and generate numbers in the range of 255-0

			// should be close, but clamp anyway
			if( b > 255 ) {
				b = 255;
			}
			else if( b < 0 ) {
				b = 0;
			}
			data[y][x][0] =
				data[y][x][1] =
					data[y][x][2] = b;
			data[y][x][3] = 255;
		}
	}
	tr.dlightImage = R_CreateImage( "*dlight", (byte *)data, DLIGHT_SIZE, DLIGHT_SIZE, VK_FORMAT_B8G8R8A8_UNORM, qfalse, qfalse, qfalse, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
#else
	int width, height;
	byte *pic;

	R_LoadImage( "gfx/2d/dlight", &pic, &width, &height );
	if( pic ) {
		tr.dlightImage = R_CreateImage( "*dlight", pic, width, height, VK_FORMAT_B8G8R8A8_UNORM, qfalse, qfalse, qfalse, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
		R_Free( pic );
	}
	else { // if we dont get a successful load
		int x, y;
		byte data[DLIGHT_SIZE][DLIGHT_SIZE][4];
		int b;

		// make a centered inverse-square falloff blob for dynamic lighting
		for( x = 0; x < DLIGHT_SIZE; x++ ) {
			for( y = 0; y < DLIGHT_SIZE; y++ ) {
				float d;

				d = ( DLIGHT_SIZE / 2 - 0.5f - x ) * ( DLIGHT_SIZE / 2 - 0.5f - x ) +
					( DLIGHT_SIZE / 2 - 0.5f - y ) * ( DLIGHT_SIZE / 2 - 0.5f - y );
				b = 4000 / d;
				if( b > 255 ) {
					b = 255;
				}
				else if( b < 75 ) {
					b = 0;
				}
				data[y][x][0] =
					data[y][x][1] =
						data[y][x][2] = b;
				data[y][x][3] = 255;
			}
		}
		tr.dlightImage = R_CreateImage( "*dlight", (byte *)data, DLIGHT_SIZE, DLIGHT_SIZE, VK_FORMAT_B8G8R8A8_UNORM, qfalse, qfalse, qfalse, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
	}
#endif
}

/*
=================
R_InitFogTable
=================
*/
void R_InitFogTable( void ) {
#if 0
	int i;
	float d;
	float exp;

	exp = 0.5;

	for( i = 0; i < FOG_TABLE_SIZE; i++ ) {
		d = pow( (float)i / ( FOG_TABLE_SIZE - 1 ), exp );

		tr.fogTable[i] = d;
	}
#endif
}

/*
================
R_CreateFogImage
================
*/
#define FOG_S 256
#define FOG_T 32
static void R_CreateFogImage( void ) {
	int x, y;
	byte *data;
	float d;
	float borderColor[4];

	data = (byte *)R_Malloc( FOG_S * FOG_T * 4, TAG_TEMP_WORKSPACE, qfalse );

	// S is distance, T is depth
	for( x = 0; x < FOG_S; x++ ) {
		for( y = 0; y < FOG_T; y++ ) {
			#if 0
			d = R_FogFactor( ( x + 0.5f ) / FOG_S, ( y + 0.5f ) / FOG_T );
			#endif
			d = 0;
			data[( y * FOG_S + x ) * 4 + 0] =
				data[( y * FOG_S + x ) * 4 + 1] =
					data[( y * FOG_S + x ) * 4 + 2] = 255;
			data[( y * FOG_S + x ) * 4 + 3] = 255 * d;
		}
	}

	tr.fogImage = R_CreateImage( "*fog", (byte *)data, FOG_S, FOG_T, VK_FORMAT_B8G8R8A8_UNORM, qfalse, qfalse, qfalse, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
	R_Free( data );
}

/*
==================
R_CreateDefaultImage
==================
*/
#define DEFAULT_SIZE 16
static void R_CreateDefaultImage( void ) {
	int x;
	byte data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// the default image will be a box, to allow you to see the mapping coordinates
	memset( data, 32, sizeof( data ) );
	for( x = 0; x < DEFAULT_SIZE; x++ ) {
		data[0][x][0] =
			data[0][x][1] =
				data[0][x][2] =
					data[0][x][3] = 255;

		data[x][0][0] =
			data[x][0][1] =
				data[x][0][2] =
					data[x][0][3] = 255;

		data[DEFAULT_SIZE - 1][x][0] =
			data[DEFAULT_SIZE - 1][x][1] =
				data[DEFAULT_SIZE - 1][x][2] =
					data[DEFAULT_SIZE - 1][x][3] = 255;

		data[x][DEFAULT_SIZE - 1][0] =
			data[x][DEFAULT_SIZE - 1][1] =
				data[x][DEFAULT_SIZE - 1][2] =
					data[x][DEFAULT_SIZE - 1][3] = 255;
	}
	tr.defaultImage = R_CreateImage( "*default", (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, VK_FORMAT_B8G8R8A8_UNORM, qtrue, qfalse, qtrue, VK_SAMPLER_ADDRESS_MODE_REPEAT );
}

/*
==================
R_CreateBuiltinImages
==================
*/
void R_UpdateSaveGameImage( const char *filename );

void R_CreateBuiltinImages( void ) {
	int x, y;
	byte data[DEFAULT_SIZE][DEFAULT_SIZE][4];
	CFrameBufferBuilder frameBufferBuilder;

	R_CreateDefaultImage();

	// we use a solid white image instead of disabling texturing
	memset( data, 255, sizeof( data ) );

	tr.whiteImage = R_CreateImage( "*white", (byte *)data, 8, 8, VK_FORMAT_B8G8R8A8_UNORM, qfalse, qfalse, qtrue, VK_SAMPLER_ADDRESS_MODE_REPEAT );
	tr.screenImage = R_CreateTransientImage( "*screen", glConfig.vidWidth, glConfig.vidHeight, VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLER_ADDRESS_MODE_REPEAT );

	// Create the scene image
	frameBufferBuilder.width = glConfig.vidWidth;
	frameBufferBuilder.height = glConfig.vidHeight;
	frameBufferBuilder.addColorAttachment( VK_FORMAT_R16G16B16A16_SFLOAT, true );
	frameBufferBuilder.addDepthStencilAttachment( VK_FORMAT_D24_UNORM_S8_UINT, true );
	frameBufferBuilder.build( &tr.sceneFrameBuffer );

	// Create the scene glow image
	frameBufferBuilder.reset();
	frameBufferBuilder.width = glConfig.vidWidth;
	frameBufferBuilder.height = glConfig.vidHeight;
	frameBufferBuilder.addColorAttachment( VK_FORMAT_R16G16B16A16_SFLOAT, true );
	frameBufferBuilder.addDepthStencilAttachment( tr.sceneFrameBuffer->images[tr.sceneFrameBuffer->depthBufferIndex].i );
	frameBufferBuilder.build( &tr.glowFrameBuffer );

	// Create the minimized scene blur image
	if( r_DynamicGlowWidth->integer > glConfig.vidWidth ) {
		r_DynamicGlowWidth->integer = glConfig.vidWidth;
	}
	if( r_DynamicGlowHeight->integer > glConfig.vidHeight ) {
		r_DynamicGlowHeight->integer = glConfig.vidHeight;
	}
	frameBufferBuilder.reset();
	frameBufferBuilder.width = r_DynamicGlowWidth->integer;
	frameBufferBuilder.height = r_DynamicGlowHeight->integer;
	frameBufferBuilder.addColorAttachment( VK_FORMAT_R16G16B16A16_SFLOAT, true );
	frameBufferBuilder.build( &tr.glowBlurFrameBuffer );

	// with overbright bits active, we need an image which is some fraction of full color,
	// for default lightmaps, etc
	for( x = 0; x < DEFAULT_SIZE; x++ ) {
		for( y = 0; y < DEFAULT_SIZE; y++ ) {
			data[y][x][0] =
				data[y][x][1] =
					data[y][x][2] = tr.identityLightByte;
			data[y][x][3] = 255;
		}
	}

	tr.identityLightImage = R_CreateImage( "*identityLight", (byte *)data, 8, 8, VK_FORMAT_B8G8R8A8_UNORM, qfalse, qfalse, qtrue, VK_SAMPLER_ADDRESS_MODE_REPEAT );

	// scratchimage is usually used for cinematic drawing
	for( x = 0; x < NUM_SCRATCH_IMAGES; x++ ) {
		// scratchimage is usually used for cinematic drawing
		tr.scratchImage[x] = R_CreateImage( va( "*scratch%d", x ), (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, VK_FORMAT_B8G8R8A8_UNORM, qfalse, qfalse, qfalse, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
	}

	R_CreateDlightImage();
	R_CreateFogImage();
}


/*
===============
R_SetColorMappings
===============
*/
void R_SetColorMappings( void ) {
	int i, j;
	float g;
	int inf;
	int shift;

	// setup the overbright lighting
	tr.overbrightBits = r_overBrightBits->integer;
	if( !glConfig.deviceSupportsGamma ) {
		tr.overbrightBits = 0; // need hardware gamma for overbright
	}

	// never overbright in windowed mode
	if( !glConfig.isFullscreen ) {
		tr.overbrightBits = 0;
	}

	if( tr.overbrightBits > 1 ) {
		tr.overbrightBits = 1;
	}
	if( tr.overbrightBits < 0 ) {
		tr.overbrightBits = 0;
	}

	tr.identityLight = 1.0 / ( 1 << tr.overbrightBits );
	tr.identityLightByte = 255 * tr.identityLight;


	if( r_intensity->value < 1.0f ) {
		ri.Cvar_Set( "r_intensity", "1.0" );
	}

	if( r_gamma->value < 0.5f ) {
		ri.Cvar_Set( "r_gamma", "0.5" );
	}
	else if( r_gamma->value > 3.0f ) {
		ri.Cvar_Set( "r_gamma", "3.0" );
	}

	g = r_gamma->value;

	shift = tr.overbrightBits;

	for( i = 0; i < 256; i++ ) {
		if( g == 1 ) {
			inf = i;
		}
		else {
			inf = 255 * pow( i / 255.0f, 1.0f / g ) + 0.5f;
		}
		inf <<= shift;
		if( inf < 0 ) {
			inf = 0;
		}
		if( inf > 255 ) {
			inf = 255;
		}
		s_gammatable[i] = inf;
	}

	for( i = 0; i < 256; i++ ) {
		j = i * r_intensity->value;
		if( j > 255 ) {
			j = 255;
		}
		s_intensitytable[i] = j;
	}

	if( glConfig.deviceSupportsGamma ) {
		ri.WIN_SetGamma( &glConfig, s_gammatable, s_gammatable, s_gammatable );
	}
}

/*
===============
R_InitImages
===============
*/
void R_InitImages( void ) {
	// memset(hashTable, 0, sizeof(hashTable));	// DO NOT DO THIS NOW (because of image cacheing)	-ste.

	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();
}

/*
===============
R_DeleteTextures
===============
*/
// (only gets called during vid_restart now (and app exit), not during map load)
//
void R_DeleteTextures( void ) {

	R_Images_Clear();
}
