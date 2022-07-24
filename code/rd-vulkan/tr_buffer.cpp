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

#include "../rd-common/tr_common.h"
#include "tr_local.h"
#include <set>

/*
===============
R_BufferList_f
===============
*/
void R_BufferList_f( void ) {
	int i = 0;
	buffer_t *buffer;
	int bytes = 0;

	ri.Printf( PRINT_ALL, "\n      --bytes----- -ty- --memory-------- -offset- --name------\n" );

	int iNumBuffers = R_Buffers_StartIteration();
	while( ( buffer = R_Buffers_GetNextIteration() ) != NULL ) {
		bytes += buffer->size;
		ri.Printf( PRINT_ALL, "%4i: %12i %4u %p %8i",
			   i, buffer->size,
			   buffer->allocationInfo.memoryType,
			   buffer->allocationInfo.deviceMemory,
			   ( int )buffer->allocationInfo.offset );
		if( buffer->allocationInfo.pName ) {
			ri.Printf( PRINT_ALL, "%s", buffer->allocationInfo.pName );
		}
		ri.Printf( PRINT_ALL, "\n" );
		i++;
	}
	ri.Printf( PRINT_ALL, " ---------\n" );
	ri.Printf( PRINT_ALL, "\n      --bytes----- -ty- --memory-------- -offset- --name------\n" );
	ri.Printf( PRINT_ALL, " %.2fMB total buffer mem (not including mipmaps)\n", bytes / 1048576.0f );
	ri.Printf( PRINT_ALL, " %i total buffers\n\n", iNumBuffers );
}

//=======================================================================

/*
===============
VK_GetUploadBuffer

===============
*/
uploadBuffer_t *VK_GetUploadBuffer( int size ) {
	// check if there are any upload buffers associated with the current frame
	auto &frameUploadBuffers = vkState.frameUploadBuffers[vkState.resnum];
	for( int index: frameUploadBuffers ) {
		uploadBuffer_t *uploadBuffer = &vkState.uploadBuffers[index];
		if( ( uploadBuffer->buffer->size - uploadBuffer->offset ) >= size ) {
			return uploadBuffer;
		}
	}

	if( !vkState.uploadBuffers.full() ) {
		// allocate a new buffer from the free pool
		int index = vkState.uploadBuffers.alloc();
		uploadBuffer_t *uploadBuffer = &vkState.uploadBuffers[index];

		if( !uploadBuffer->buffer ) {
			uploadBuffer->buffer = R_CreateBuffer(
				Q_max( MIN_UPLOADBUFFER_SIZE, size ),
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT );
		}

		uploadBuffer->offset = 0;
		uploadBuffer->age = 0;

		// assign the buffer to the current frame
		frameUploadBuffers.push_back( index );

		return uploadBuffer;
	}
	else {
		Com_Error( ERR_FATAL, "VK_GetUploadBuffer: out of buffers\n" );
	}

	return NULL;
}

/*
===============
VK_PrepareUploadBuffers

===============
*/
void VK_PrepareUploadBuffers( void ) {
	// increase the age of all unallocated buffers
	for( int i = 0; i < vkState.uploadBuffers.CAPACITY; ++i ) {
		if( vkState.uploadBuffers.is_used( i ) ) {
			uploadBuffer_t *uploadBuffer = &vkState.uploadBuffers[i];
			if( uploadBuffer->buffer ) {
				uploadBuffer->age++;

				if( uploadBuffer->age > 60 ) {
					// it hasn't been used for some time
					R_Buffers_DeleteBuffer( uploadBuffer->buffer );
					uploadBuffer->buffer = NULL;
				}
			}
		}
	}

	// return the upload buffers from the current frame back to the free pool
	auto &frameUploadBuffers = vkState.frameUploadBuffers[vkState.resnum];
	if( !frameUploadBuffers.empty() ) {
		for( int i: frameUploadBuffers ) {
			vkState.uploadBuffers.free( i );
		}
		vkState.frameUploadBuffers[vkState.resnum].clear();
	}
}

/*
===============
VK_UploadBuffer

===============
*/
void VK_UploadBuffer( buffer_t *buffer, const byte *data, int size, int offset ) {
	VkResult res;

	if( !buffer->allocationInfo.pMappedData ) {
		uploadBuffer_t *uploadBuffer = VK_GetUploadBuffer( size );

		assert( uploadBuffer );
		assert( ( uploadBuffer->buffer->size - uploadBuffer->offset ) >= size );

		// update the upload buffer
		VK_UploadBuffer( uploadBuffer->buffer, data, size, uploadBuffer->offset );

		// copy the data from the upload buffer to the final resource
		VkBufferCopy uploadRegion = {};
		uploadRegion.srcOffset = uploadBuffer->offset;
		uploadRegion.dstOffset = offset;
		uploadRegion.size = size;

		vkCmdCopyBuffer( backEndData->uploadCmdbuf,
				 uploadBuffer->buffer->buf,
				 buffer->buf,
				 1, &uploadRegion );

		uploadBuffer->offset += size;
	}

	else {
		// buffer is host-visible and can be updated directly
		assert( buffer->allocationInfo.pMappedData );
		memcpy( ( ( byte * )buffer->allocationInfo.pMappedData ) + offset, data, size );

		if( ( buffer->memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT ) == 0 ) {
			// if memory is not coherent, it has to be manually flushed
			VkMappedMemoryRange memoryRange = {};
			memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
			memoryRange.memory = buffer->allocationInfo.deviceMemory;
			memoryRange.offset = buffer->allocationInfo.offset + offset;
			memoryRange.size = size;

			res = vkFlushMappedMemoryRanges( vkState.device, 1, &memoryRange );
			if( res != VK_SUCCESS ) {
				Com_Error( ERR_FATAL, "VK_UploadBuffer: failed to flush mapped memory range [%llu:%llu] (%d)\n",
					   memoryRange.offset, memoryRange.offset + size, res );
			}
		}
	}
}

void *VK_UploadBuffer( buffer_t *buffer, int size, int offset ) {
	VkResult res;

	if( !buffer->allocationInfo.pMappedData ) {
		uploadBuffer_t *uploadBuffer = VK_GetUploadBuffer( size );

		assert( uploadBuffer );
		assert( ( uploadBuffer->buffer->size - uploadBuffer->offset ) >= size );

		// update the upload buffer
		void* data = VK_UploadBuffer( uploadBuffer->buffer, size, offset );

		// copy the data from the upload buffer to the final resource
		VkBufferCopy uploadRegion = {};
		uploadRegion.srcOffset = offset;
		uploadRegion.dstOffset = offset;
		uploadRegion.size = size;

		vkCmdCopyBuffer( backEndData->uploadCmdbuf,
			uploadBuffer->buffer->buf,
			buffer->buf,
			1, &uploadRegion );

		return data;
	}

	else {
		// buffer is host-visible and can be updated directly
		assert( buffer->allocationInfo.pMappedData );
		assert( buffer->memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

		return buffer->allocationInfo.pMappedData;
	}
}

typedef std::set<buffer_t *> AllocatedBuffers_t;
AllocatedBuffers_t AllocatedBuffers;
AllocatedBuffers_t::iterator itAllocatedBuffers;

int R_Buffers_StartIteration( void ) {
	itAllocatedBuffers = AllocatedBuffers.begin();
	return AllocatedBuffers.size();
}

buffer_t *R_Buffers_GetNextIteration( void ) {
	if( itAllocatedBuffers == AllocatedBuffers.end() )
		return NULL;

	buffer_t *pBuffer = ( *itAllocatedBuffers );
	++itAllocatedBuffers;
	return pBuffer;
}

static void R_Buffers_DeleteBufferContents( buffer_t *buffer ) {
	assert( buffer ); // should never be called with NULL
	if( buffer ) {
		vmaDestroyBuffer( vkState.allocator, buffer->buf, buffer->allocation );
		R_Free( buffer );
	}
}

// special function currently only called by Dissolve code...
//
void R_Buffers_DeleteBuffer( buffer_t *buffer ) {
	// Even though we supply the buffer handle, we need to get the corresponding iterator entry...
	//
	AllocatedBuffers_t::iterator itBuffer = AllocatedBuffers.find( buffer );
	if( itBuffer != AllocatedBuffers.end() ) {
		R_Buffers_DeleteBufferContents( *itBuffer );
		AllocatedBuffers.erase( itBuffer );
	}
	else {
		assert( 0 );
	}
}

// called only at app startup, vid_restart, app-exit
//
void R_Buffers_Clear( void ) {
	buffer_t *buffer;
	//	int iNumImages =
	R_Buffers_StartIteration();
	while( ( buffer = R_Buffers_GetNextIteration() ) != NULL ) {
		R_Buffers_DeleteBufferContents( buffer );
	}

	AllocatedBuffers.clear();
}

void RE_RegisterBuffers_Info_f( void ) {
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
	ri.Printf( PRINT_ALL, "%d Images. %d (%.2fMB) texels total, (not including mipmaps)\n", iNumImages, iTexels, ( float )iTexels / 1024.0f / 1024.0f );
	ri.Printf( PRINT_DEVELOPER, "RE_RegisterMedia_GetLevel(): %d", RE_RegisterMedia_GetLevel() );
}

/*
================
R_CreateBuffer

This is the only way any buffer_t are created
================
*/
buffer_t *R_CreateBuffer( int size, VkBufferUsageFlags usage, VkMemoryPropertyFlags requiredFlags ) {
	VkResult res;
	buffer_t *buffer;

	buffer = ( buffer_t * )R_Malloc( sizeof( buffer_t ), TAG_ALL, qtrue );
	buffer->size = size;

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.usage = usage;
	bufferCreateInfo.size = ( VkDeviceSize )size;

	VmaAllocationCreateInfo allocationCreateInfo = {};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocationCreateInfo.requiredFlags = requiredFlags;

	if( requiredFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) {
		allocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	}

	res = vmaCreateBuffer(
		vkState.allocator,
		&bufferCreateInfo,
		&allocationCreateInfo,
		&buffer->buf,
		&buffer->allocation,
		&buffer->allocationInfo );

	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "R_CreateBuffer: failed to allocate buffer with size = %llu (%d)\n",
			   bufferCreateInfo.size, res );
	}

	// get memory property flags of the allocation
	vmaGetMemoryTypeProperties( vkState.allocator, buffer->allocationInfo.memoryType, &buffer->memoryPropertyFlags );

	return buffer;
}

/*
================
R_CreateBuffer

This is the only way any buffer_t are created
================
*/
vertexBuffer_t *R_CreateVertexBuffer( int numVertexes, int numIndexes, int indexOffset ) {
	VkResult res;
	vertexBuffer_t *buffer;

	buffer = (vertexBuffer_t *)R_Malloc( sizeof( vertexBuffer_t ), TAG_ALL, qtrue );
	buffer->b.size = indexOffset +
		sizeof( tr_shader::vertex_t ) * numVertexes +
		sizeof( trIndex_t ) * numIndexes;

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferCreateInfo.size = (VkDeviceSize)buffer->b.size;

	VmaAllocationCreateInfo allocationCreateInfo = {};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

	res = vmaCreateBuffer(
		vkState.allocator,
		&bufferCreateInfo,
		&allocationCreateInfo,
		&buffer->b.buf,
		&buffer->b.allocation,
		&buffer->b.allocationInfo );

	if( res != VK_SUCCESS ) {
		Com_Error( ERR_FATAL, "R_CreateBuffer: failed to allocate buffer with size = %llu (%d)\n",
			bufferCreateInfo.size, res );
	}

	// get memory property flags of the allocation
	vmaGetMemoryTypeProperties( vkState.allocator, buffer->b.allocationInfo.memoryType, &buffer->b.memoryPropertyFlags );

	// fill vertex info
	buffer->numVertexes = numVertexes;
	buffer->numIndexes = numIndexes;
	buffer->indexOffset = indexOffset;
	buffer->vertexOffset = indexOffset + sizeof( trIndex_t ) * numIndexes;

	return buffer;
}

/*
==================
R_CreateBuiltinBuffers
==================
*/
void R_CreateBuiltinBuffers( void ) {
	tr_shader::fog_t fog;

	// allocate a buffer for global parameters
	tr.globalsBuffer = R_CreateBuffer( sizeof( tr.globals ), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0 );

	// allocate a buffer for identity model constants
	tr.identityModelBuffer = R_CreateBuffer( sizeof( tr_shader::model_t ),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0 );

	// allocate a buffer with an identity fog
	tr.fogsBuffer = R_CreateBuffer( sizeof( fog ),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, 0 );

	memset( &fog, 0, sizeof( fog ) );
	VK_UploadBuffer( tr.fogsBuffer, (byte *)&fog, sizeof( fog ), 0 );

	// initialize a dynamic geometry buffer
	backEndData->dynamicGeometryBuilder.init();

	// allocate a 2D entity model buffer
	backEnd.entity2D.modelBuffer = R_CreateBuffer( sizeof( backEnd.entity2D.model ), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0 );
	memset( &backEnd.entity2D.model, 0, sizeof( backEnd.entity2D.model ) );

	backEnd.entity2D.model.entity2D = qtrue;

	backEnd.entity2D.model.ori.modelMatrix[0][0] = 1.f / 320;
	backEnd.entity2D.model.ori.modelMatrix[1][1] = 1.f / 240;
	backEnd.entity2D.model.ori.modelMatrix[2][2] = 1;
	backEnd.entity2D.model.ori.modelMatrix[3][3] = 1;
	backEnd.entity2D.model.ori.modelMatrix[3][0] = -1;
	backEnd.entity2D.model.ori.modelMatrix[3][1] = -1;

	VK_UploadBuffer( backEnd.entity2D.modelBuffer, (byte *)&backEnd.entity2D.model, sizeof( backEnd.entity2D.model ), 0 );

	// allocate a viewParms buffer
	tr.viewParms.buffer = R_CreateBuffer( sizeof( tr.viewParms.shaderData ), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 0 );
	memset( &tr.viewParms.shaderData, 0, sizeof( tr.viewParms.shaderData ) );
}

/*
===============
R_InitBuffers
===============
*/
void R_InitBuffers( void ) {
	// create globally-available buffers
	R_CreateBuiltinBuffers();
}

/*
===============
R_DeleteBuffers
===============
*/
// (only gets called during vid_restart now (and app exit), not during map load)
//
void R_DeleteBuffers( void ) {
	R_Buffers_Clear();
}
