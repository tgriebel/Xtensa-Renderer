#include <SysCore/log.h>

// Verbose debug prints
#if 0
#define VMA_DEBUG_LOG_LEVEL 4
#define VMA_DEBUG_LOG_FORMAT( format, ... ) \
    do { \
        char _vmaBuf[ 512 ]; \
        snprintf( _vmaBuf, sizeof( _vmaBuf ), (format), __VA_ARGS__ ); \
        LogMsg( "VMA", logSeverity_t::Verbose, "%s", _vmaBuf ); \
    } while( false )
#endif

#define VMA_IMPLEMENTATION
#include "allocator.h"
#include "../render_state/deviceContext.h"

extern DeviceContext context;


// ---- Allocation ----

uint64_t Allocation::GetOffset() const
{
#ifdef USE_VULKAN
	return m_info.offset;
#else
	return 0;
#endif
}


uint64_t Allocation::GetSize() const
{
#ifdef USE_VULKAN
	return m_info.size;
#else
	return 0;
#endif
}


uint64_t Allocation::GetAlignment() const
{
	return m_alignment;
}


void* Allocation::GetPtr() const
{
#ifdef USE_VULKAN
	return m_info.pMappedData;
#else
	return nullptr;
#endif
}


void Allocation::Free()
{
#ifdef USE_VULKAN
	if ( m_allocation != VK_NULL_HANDLE )
	{
		vmaFreeMemory( AllocatorMemory::GetVmaAllocator(), m_allocation );
		m_allocation = VK_NULL_HANDLE;
		m_info = {};
		m_alignment = 0;
	}
#endif
}


// ---- AllocatorMemory ----

#ifdef USE_VULKAN
VmaAllocator AllocatorMemory::s_allocator = VK_NULL_HANDLE;


// Workaround for profile code injection
static void SuppressDedicatedAllocation( void* pNext )
{
	VkBaseOutStructure* node = static_cast<VkBaseOutStructure*>( pNext );
	while ( node != nullptr )
	{
		if ( node->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS )
		{
			VkMemoryDedicatedRequirements* ded = reinterpret_cast<VkMemoryDedicatedRequirements*>( node );
			ded->prefersDedicatedAllocation = VK_FALSE;
			ded->requiresDedicatedAllocation = VK_FALSE;
			break;
		}
		node = node->pNext;
	}
}

static void VKAPI_CALL vma_GetBufferMemoryRequirements2(
	VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemReqs )
{
	vkGetBufferMemoryRequirements2( device, pInfo, pMemReqs );
	SuppressDedicatedAllocation( pMemReqs->pNext );
}

static void VKAPI_CALL vma_GetImageMemoryRequirements2(
	VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemReqs )
{
	vkGetImageMemoryRequirements2( device, pInfo, pMemReqs );
	SuppressDedicatedAllocation( pMemReqs->pNext );
}


void AllocatorMemory::CreateVmaAllocator()
{
	if ( s_allocator != VK_NULL_HANDLE ) {
		return;
	}

	VmaAllocatorCreateInfo createInfo = {};
	createInfo.physicalDevice = context.physicalDevice;
	createInfo.device = context.device;
	createInfo.instance = context.instance;
	createInfo.vulkanApiVersion = VK_API_VERSION_1_2;
#ifdef USE_VULKAN_RTX
	createInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
#endif

	VmaVulkanFunctions vmaFunctions{};
	if ( context.IsProfilerAttached() )
	{
		vmaFunctions.vkGetBufferMemoryRequirements2KHR = vma_GetBufferMemoryRequirements2;
		vmaFunctions.vkGetImageMemoryRequirements2KHR = vma_GetImageMemoryRequirements2;
		createInfo.pVulkanFunctions = &vmaFunctions;
	}

	VK_CHECK_RESULT( vmaCreateAllocator( &createInfo, &s_allocator ) );
}


void AllocatorMemory::DestroyVmaAllocator()
{
	if ( s_allocator != VK_NULL_HANDLE )
	{
		vmaDestroyAllocator( s_allocator );
		s_allocator = VK_NULL_HANDLE;
	}
}


VmaAllocator AllocatorMemory::GetVmaAllocator()
{
	return s_allocator;
}
#endif


void AllocatorMemory::Create( const uint32_t sizeBytes, const memoryRegion_t region, const resourceLifeTime_t lifetime )
{
	RenderResource::Create( resourceType_t::MEMORY, lifetime );
	m_memoryRegion = region;

#ifdef USE_VULKAN
	CreateVmaAllocator();
#endif
}


void AllocatorMemory::Destroy()
{
	// VMA manages memory internally — individual pools are not backed by dedicated VkDeviceMemory.
	// Call AllocatorMemory::DestroyVmaAllocator() at shutdown after all resources are freed.
}


memoryRegion_t AllocatorMemory::GetMemoryRegion() const
{
	return m_memoryRegion;
}
