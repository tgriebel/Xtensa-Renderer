#include "renderResource.h"
#include "../render_state/cmdContext.h"
#include "../render_resources/gpuImage.h"
#include <algorithm>
#include "../app/window.h"
#include "log.h"

extern Window g_window;

static std::vector<RenderResource*> m_taskDependentResources;
static std::vector<RenderResource*> m_frameDependentResources;
static std::vector<RenderResource*> m_viewDependentResources;
static std::vector<RenderResource*> m_appDependentResources;
static std::vector<RenderResource*> m_unmanagedResources;

static std::vector<RenderResource*> m_newImages;

static void InsertSorted( std::vector<RenderResource*>& list, RenderResource* resource )
{
	auto it = std::lower_bound( list.begin(), list.end(), resource,
		[]( const RenderResource* a, const RenderResource* b )
		{
			return a->GetPriority() < b->GetPriority();
		} );
	list.insert( it, resource );
}

std::vector<RenderResource*>& RenderResource::GetResourceList( const resourceLifeTime_t lifetime )
{
	switch( lifetime )
	{
	case resourceLifeTime_t::FRAME:		return m_frameDependentResources;
	case resourceLifeTime_t::RESIZE:	return m_viewDependentResources;
	case resourceLifeTime_t::REBOOT:	return m_appDependentResources;
	case resourceLifeTime_t::TASK:		return m_taskDependentResources;
	case resourceLifeTime_t::UNMANAGED:	return m_unmanagedResources;
	}
	assert( 0 );
	return m_unmanagedResources;
}


void RenderResource::ResizeResources( const uint32_t displayWidth, const uint32_t displayHeight )
{
	// This is tricky b/c recreating new resources adds to this exact list! Uh-oh! No worries though!
	// The solution is to use a move which clears the global list while providing a local copy
	std::vector<RenderResource*> resizeList = std::move( m_viewDependentResources );

	const uint32_t resourceCount = static_cast<uint32_t>( resizeList.size() );
	for( uint32_t i = 0; i < resourceCount; ++i )
	{
		const bool recreated = resizeList[ i ]->OnResize( displayWidth, displayHeight );

		// Add back into the list if no new resource was created
		if( recreated == false ) {
			InsertSorted( m_viewDependentResources, resizeList[ i ] );
		}
	}
}


void RenderResource::Cleanup( const resourceLifeTime_t lifetime )
{
	std::vector<RenderResource*> resourceList = std::move( GetResourceList( lifetime ) );

	// List goes in reverse priority
	const int32_t resourceCount = static_cast<int32_t>( resourceList.size() );

	if( resourceCount == 0 ) {
		return;
	}

	for( int32_t i = ( resourceCount - 1 ); i >= 0; --i )
	{
		if( resourceList[ i ]->m_type == resourceType_t::GPU_IMAGE )
		{
			GpuImage* gpuImage = reinterpret_cast<GpuImage*>( resourceList[ i ] );
			assert( gpuImage != nullptr );
			LogMsg( "Vulkan", logSeverity_t::Info, "Destroying GPU Image: %s", gpuImage->GetDebugName() );
		}
		resourceList[ i ]->Destroy();
	}
}


void RenderResource::TransitionNewImages( CommandList* cmdList )
{
#ifdef USE_VULKAN
	std::vector<RenderResource*> resourceList = std::move( m_newImages );

	const uint32_t resourceCount = static_cast<uint32_t>( resourceList.size() );
	for ( uint32_t i = 0; i < resourceCount; ++i )
	{
		if ( resourceList[ i ]->m_type == resourceType_t::GPU_IMAGE )
		{
			GpuImage* gpuImage = reinterpret_cast<GpuImage*>( resourceList[ i ] );
			if( HasFlags( gpuImage->GetFlags(), gpuImageStateFlags_t::GPU_IMAGE_READ ) )
			{
				Transition( cmdList, gpuImage, swapBuffering_t::MULTI_FRAME, GPU_IMAGE_NONE, GPU_IMAGE_READ );
			}
			else if( HasFlags( gpuImage->GetFlags(), gpuImageStateFlags_t::GPU_IMAGE_STORAGE ) )
			{
				Transition( cmdList, gpuImage, swapBuffering_t::MULTI_FRAME, GPU_IMAGE_NONE, GPU_IMAGE_STORAGE );
			}
		}
	}
#else
	(void)cmdList;
#endif
}


static resourcePriority_t GetPriorityForType( const resourceType_t type )
{
	// Defines a dependency hierachy from most fundamental to least
	// Destructures should reverse the priority
	// Resize bascially needs this most since GpuImage, ImageView, and FrameBuffer build off Image class
	switch( type )
	{
	case resourceType_t::MEMORY:		return resourcePriority_t::HIGHEST; break;
	case resourceType_t::BUFFER:		return resourcePriority_t::HIGHEST; break;
	case resourceType_t::IMAGE:			return resourcePriority_t::HIGHEST; break;	// Needs to resize first, deleted last
	case resourceType_t::GPU_IMAGE:		return resourcePriority_t::MEDIUM; break;
	case resourceType_t::SWAPCHAIN:		return resourcePriority_t::MEDIUM; break;
	case resourceType_t::IMAGE_VIEW:	return resourcePriority_t::MEDIUM; break;
	case resourceType_t::FRAMEBUFFER:	return resourcePriority_t::LOWEST; break;
	case resourceType_t::BINDSET:		return resourcePriority_t::LOWEST; break;
	case resourceType_t::IMAGE_SAMPLER:	return resourcePriority_t::LOWEST; break;
	default:							return resourcePriority_t::LOWEST; break;
	}
}


void RenderResource::Create( const resourceType_t type, const resourceLifeTime_t lifetime )
{
	m_type = type;
	m_lifetime = lifetime;
	m_priority = GetPriorityForType( type );

	m_resourceMemoryRegion = memoryRegion_t::UNKNOWN;
	m_resourceByteCount = 0;

	switch ( m_lifetime )
	{
	case resourceLifeTime_t::TASK:		InsertSorted( m_taskDependentResources,  this ); break;
	case resourceLifeTime_t::FRAME:		InsertSorted( m_frameDependentResources, this ); break;
	case resourceLifeTime_t::RESIZE:	InsertSorted( m_viewDependentResources,  this ); break;
	case resourceLifeTime_t::REBOOT:	InsertSorted( m_appDependentResources,   this ); break;
	case resourceLifeTime_t::UNMANAGED:	InsertSorted( m_unmanagedResources,   this ); break;
	}

	if( type == resourceType_t::GPU_IMAGE )
	{
		InsertSorted( m_newImages, this );
	}
}