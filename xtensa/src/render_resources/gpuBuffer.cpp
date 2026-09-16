#include "gpuBuffer.h"
#include "../render_state/deviceContext.h"

extern DeviceContext context;


uint32_t GpuBuffer::ClampId( const uint32_t bufferId ) const
{
	if( m_swapBuffering == swapBuffering_t::SINGLE_FRAME ) {
		return 0;
	}
	return ( bufferId % m_bufferCount );
}


void GpuBuffer::SetPos( const uint64_t pos )
{
	const uint32_t id = ClampId( context.bufferId );
	m_buffer[ id ].offset = Clamp( m_buffer[ id ].baseOffset + pos, m_buffer[ id ].baseOffset, GetMaxSize() );
}


uint64_t GpuBuffer::GetSize() const
{
	const uint32_t id = ClampId( context.bufferId );
	return ( m_buffer[ id ].offset - m_buffer[ id ].baseOffset );
}


uint64_t GpuBuffer::GetBaseOffset() const
{
	const uint32_t id = ClampId( context.bufferId );
	return m_buffer[ id ].baseOffset;
}


uint64_t GpuBuffer::GetElementSize() const
{
	return m_elementSize;
}


uint64_t GpuBuffer::GetElementSizeAligned() const
{
	return m_elementPadding;
}


uint64_t GpuBuffer::GetMaxSize() const
{
	return m_end;
}


void GpuBuffer::Allocate( const uint64_t size )
{
	const uint32_t id = ClampId( context.bufferId );
	SetPos( m_buffer[ id ].offset + size );
}


VkBuffer GpuBuffer::GetVkObject() const
{
	const uint32_t id = ClampId( context.bufferId );
	return m_buffer[ id ].buffer;
}


VkBuffer& GpuBuffer::VkObject()
{
	const uint32_t id = ClampId( context.bufferId );
	return m_buffer[ id ].buffer;
}


void GpuBuffer::Create( const bufferCreateInfo_t info )
{
	Create( info.name, info.swapBuffering, info.lifetime, info.elements, info.elementSizeBytes, info.type, info.flags );
}


void GpuBuffer::Create( const char* name, const swapBuffering_t swapBuffering, const resourceLifeTime_t lifetime, const uint32_t elements, const uint32_t elementSizeBytes, const bufferType_t type, const bufferFlags_t flags )
{
	// Resource Management
	{
		RenderResource::Create( resourceType_t::BUFFER, lifetime );
	}

#ifdef USE_VULKAN
	{
		VkBufferUsageFlags usage = 0;
		VkDeviceSize alignment = elementSizeBytes;

		m_swapBuffering = swapBuffering;
		if( m_swapBuffering == swapBuffering_t::MULTI_FRAME ) {
			m_bufferCount = MaxFrameStates;
		} else {
			m_bufferCount = 1;
		}

		if( type == bufferType_t::STORAGE )
		{
			alignment = context.deviceProperties.limits.minStorageBufferOffsetAlignment;
			usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

			if ( HasFlags( flags, bufferFlags_t::RT_VISIBLE ) )
			{
				usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			}
		}
		else if ( type == bufferType_t::UNIFORM )
		{
			alignment = context.deviceProperties.limits.minUniformBufferOffsetAlignment;
			usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		}
		else if ( type == bufferType_t::VERTEX )
		{
			usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

			if( HasFlags( flags, bufferFlags_t::RT_VISIBLE ) )
			{
				usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
				usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			}
		}
		else if ( type == bufferType_t::INDEX )
		{
			usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

			if( HasFlags( flags, bufferFlags_t::RT_VISIBLE ) )
			{
				usage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
				usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
			}
		}
		else if ( type == bufferType_t::STAGING )
		{
			alignment = 1;
			usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		}
		else if ( type == bufferType_t::READBACK )
		{
			alignment = context.deviceProperties.limits.minStorageBufferOffsetAlignment;
			usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		}
		else if ( type == bufferType_t::ACCELERATION_STRUCTURE )
		{
			alignment = 256;
			usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
				  | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		}
		else if ( type == bufferType_t::TLAS_INSTANCE_DATA )
		{
			// VkAccelerationStructureInstanceKHR requires 16-byte alignment (Vulkan spec)
			alignment = 16;
			usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
				  | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		}
		else if ( type == bufferType_t::SHADER_BINDING_TABLE )
		{
			alignment = 1;
			usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
				  | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
		}
		else
		{
			assert(0);
		}

		m_type = type;
		m_elementSize = elementSizeBytes;
		m_elementPadding = GpuBuffer::GetAlignedSize( elementSizeBytes, alignment );

		VkDeviceSize stride = m_elementPadding;
		VkDeviceSize bufferSize = stride * elements;

		VkBufferCreateInfo bufferInfo{ };
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = bufferSize;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocCreateInfo = {};
		allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
		if ( type == bufferType_t::READBACK )
		{
			allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT
								  | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
		else if ( ( type == bufferType_t::UNIFORM ) || ( type == bufferType_t::STAGING ) ||
				  ( type == bufferType_t::TLAS_INSTANCE_DATA ) || ( type == bufferType_t::SHADER_BINDING_TABLE ) ||
				  ( type == bufferType_t::STORAGE && HasFlags( flags, bufferFlags_t::RT_VISIBLE ) == false ) )
		{
			allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
								  | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}

		for( uint32_t bufferId = 0; bufferId < m_bufferCount; ++bufferId )
		{
			VmaAllocation allocation;
			VmaAllocationInfo allocInfo;

			VkResult result = vmaCreateBuffer(
				AllocatorMemory::GetVmaAllocator(),
				&bufferInfo, &allocCreateInfo,
				&m_buffer[ bufferId ].buffer,
				&allocation, &allocInfo );

			if ( result != VK_SUCCESS ) {
				THROW_ERROR( "Buffer '" + std::string( name ) + "' [" + std::to_string( bufferId ) + "] could not allocate "
					+ std::to_string( bufferSize ) + " bytes (alignment: " + std::to_string( alignment ) + ")" );
			}

			vmaSetAllocationName( AllocatorMemory::GetVmaAllocator(), allocation, name );

			m_buffer[ bufferId ].alloc.m_allocation = allocation;
			m_buffer[ bufferId ].alloc.m_info = allocInfo;
			m_buffer[ bufferId ].alloc.m_alignment = alignment;

			vk_SetObjectName( (uint64_t)m_buffer[ bufferId ].buffer, VK_OBJECT_TYPE_BUFFER, vk_BuildObjectName( "Buffer", name, bufferId ).c_str() );

			m_name = name;

			m_end = m_buffer[ bufferId ].alloc.GetSize();
			m_buffer[ bufferId ].baseOffset = 0;
			m_buffer[ bufferId ].offset = 0;
		}
	}
#endif
}


#ifdef USE_VULKAN
VkDeviceAddress GpuBuffer::GetDeviceAddress() const
{
	const uint32_t id = ClampId( context.bufferId );
	VkBufferDeviceAddressInfo info{};
	info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
	info.buffer = m_buffer[ id ].buffer;
	return vkGetBufferDeviceAddress( context.device, &info ) + m_buffer[ id ].baseOffset;
}
#endif


void GpuBuffer::Invalidate()
{
#ifdef USE_VULKAN
	const uint32_t id = ClampId( context.bufferId );
	VmaAllocation alloc = m_buffer[ id ].alloc.m_allocation;
	if ( alloc == VK_NULL_HANDLE ) {
		return;
	}
	vmaInvalidateAllocation( AllocatorMemory::GetVmaAllocator(), alloc, 0, VK_WHOLE_SIZE );
#endif
}


void GpuBuffer::Flush()
{
#ifdef USE_VULKAN
	const uint32_t id = ClampId( context.bufferId );
	VmaAllocation alloc = m_buffer[ id ].alloc.m_allocation;
	if ( alloc == VK_NULL_HANDLE ) {
		return;
	}
	vmaFlushAllocation( AllocatorMemory::GetVmaAllocator(), alloc, 0, VK_WHOLE_SIZE );
#endif
}


void GpuBuffer::Destroy()
{
	if ( context.device == VK_NULL_HANDLE ) {
		THROW_ERROR( "GPU Buffer: Destroy: No device context!" );
	}
	for ( uint32_t bufferId = 0; bufferId < m_bufferCount; ++bufferId )
	{
		if ( m_buffer[ bufferId ].buffer != VK_NULL_HANDLE )
		{
			vmaDestroyBuffer( AllocatorMemory::GetVmaAllocator(), m_buffer[ bufferId ].buffer, m_buffer[ bufferId ].alloc.m_allocation );
			m_buffer[ bufferId ].buffer = VK_NULL_HANDLE;
			m_buffer[ bufferId ].alloc.m_allocation = VK_NULL_HANDLE;
			m_buffer[ bufferId ].alloc.m_info = {};
		}
		m_buffer[ bufferId ].offset = 0;
	}
}


uint64_t GpuBuffer::GetAlignedSize( const uint64_t size, const uint64_t alignment )
{
	return static_cast<uint32_t>( ( size + ( alignment - 1 ) ) & ~( alignment - 1 ) );
}


bool GpuBuffer::VisibleToCpu() const
{
	const void* mappedData = m_buffer[ 0 ].alloc.GetPtr();
	return ( mappedData != nullptr );
}


void GpuBuffer::CopyData( const void* data, const size_t sizeInBytes )
{
	const uint32_t id = ClampId( context.bufferId );

	const uint64_t remaining = GetMaxSize() - GetSize();
	assert( sizeInBytes <= remaining );
	if ( sizeInBytes > remaining ) {
		return;
	}

	void* mappedData = m_buffer[ id ].alloc.GetPtr();
	assert( mappedData != nullptr );
	if ( mappedData == nullptr ) {
		return;
	}

	memcpy( (uint8_t*)mappedData + m_buffer[ id ].offset, data, sizeInBytes );
	m_buffer[ id ].offset += GetAlignedSize( sizeInBytes, m_buffer[ id ].alloc.GetAlignment() );
	m_buffer[ id ].offset = Min( m_buffer[ id ].offset, m_end );
}


void GpuBuffer::CopyFrom( void* data, const size_t sizeInBytes ) const
{
	const uint32_t id = ClampId( context.bufferId );

	assert( ( GetSize() + sizeInBytes ) <= GetMaxSize() );
	void* mappedData = m_buffer[ id ].alloc.GetPtr();

	if ( mappedData != nullptr )
	{
		memcpy( data, reinterpret_cast<uint8_t*>( mappedData ), sizeInBytes );
	}
	else {
		assert( 0 );
	}
}


void* GpuBuffer::Get() const
{
	const uint32_t id = ClampId( context.bufferId );

	void* mappedData = m_buffer[ id ].alloc.GetPtr();
	assert( mappedData != nullptr );
	return mappedData;
}


void* GpuBuffer::GetPrevious() const
{
	const uint32_t id = ClampId( context.bufferId + m_bufferCount - 1 );

	void* mappedData = m_buffer[ id ].alloc.GetPtr();
	assert( mappedData != nullptr );
	return mappedData;
}


const char* GpuBuffer::GetName() const
{
	return m_name;
}


GpuBufferView GpuBuffer::GetBufferView( const uint64_t baseElementIx, const uint64_t elementCount ) const
{
	GpuBufferView view;

	const uint64_t maxSize = GetMaxSize();

	for ( uint32_t bufferId = 0; bufferId < m_bufferCount; ++bufferId )
	{
		view.m_buffer[ bufferId ] = m_buffer[ bufferId ];
		view.m_buffer[ bufferId ].baseOffset = baseElementIx * m_elementPadding;
		assert( view.m_buffer[ bufferId ].baseOffset <= maxSize );
		view.m_buffer[ bufferId ].baseOffset = Clamp( view.m_buffer[ bufferId ].baseOffset, m_buffer[ bufferId ].baseOffset, maxSize );
	}
	view.m_end = Min( view.m_buffer[ 0 ].baseOffset + elementCount * m_elementPadding, maxSize );
	view.m_elementSize = m_elementSize;
	view.m_elementPadding = m_elementPadding;
	view.m_bufferCount = m_bufferCount;
	view.m_type = m_type;
	view.m_swapBuffering = m_swapBuffering;

	return view;
}
