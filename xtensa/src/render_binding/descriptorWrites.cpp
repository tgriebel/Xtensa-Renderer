#include <algorithm>
#include <iterator>
#include <numeric>
#include <map>
#include <sstream>
//#include "../scene/scene.h"
//#include "../scene/entity.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"
#include "../render_resources/gpuAccelerationStructure.h"
#include "../render_core/debugMenu.h"
#include "../render_resources/gpuImage.h"
#include "../render_resources/imageSampler.h"
#include "../render_state/rhi.h"
#include "../render_state/deviceContext.h"
#include "../render_core/log.h"
#include "bindings.h"

union descriptorInfo_t
{
	VkDescriptorBufferInfo	bufferInfo;
	VkDescriptorImageInfo	imageInfo;
};

class DescriptorWritesBuilder
{
private:
	static const uint32_t MaxPoolSize = 512;
	uint32_t							bufferInfoPoolFreeList = 0;
	uint32_t							imageInfoPoolFreeList = 0;
	uint32_t							bufferInfoArrayPoolFreeList = 0;
	uint32_t							imageInfoArrayPoolFreeList = 0;
#ifdef USE_VULKAN_RTX
	uint32_t							tlasInfoPoolFreeList = 0;
	VkWriteDescriptorSetAccelerationStructureKHR tlasInfoPool[ MaxPoolSize ];
	VkAccelerationStructureKHR			tlasHandlePool[ MaxPoolSize ];
#endif
	VkDescriptorBufferInfo				bufferInfoPool[ MaxPoolSize ];
	VkDescriptorImageInfo				imageInfoPool[ MaxPoolSize ];
	std::vector<VkDescriptorBufferInfo>	bufferInfoArrayPool[ MaxPoolSize ];
	std::vector<VkDescriptorImageInfo>	imageInfoArrayPool[ MaxPoolSize ];
public:
	DescriptorWritesBuilder()
	{
		const uint32_t reservationSize = 128;
		for ( uint32_t i = 0; i < MaxPoolSize; ++i )
		{
			bufferInfoArrayPool[ i ].reserve( reservationSize );
			imageInfoArrayPool[ i ].reserve( reservationSize );
		}
		bufferInfoPoolFreeList = 0;
		imageInfoPoolFreeList = 0;
		bufferInfoArrayPoolFreeList = 0;
		imageInfoArrayPoolFreeList = 0;
#ifdef USE_VULKAN_RTX
		tlasInfoPoolFreeList = 0;
#endif
	}

	void Reset()
	{
		for ( uint32_t i = 0; i < MaxPoolSize; ++i )
		{
			bufferInfoArrayPool[ i ].resize( 0 );
			imageInfoArrayPool[ i ].resize( 0 );
		}
		bufferInfoPoolFreeList = 0;
		imageInfoPoolFreeList = 0;
		bufferInfoArrayPoolFreeList = 0;
		imageInfoArrayPoolFreeList = 0;
#ifdef USE_VULKAN_RTX
		tlasInfoPoolFreeList = 0;
#endif
	}

	[[nodiscard]]
	std::vector<VkDescriptorBufferInfo>& NextBufferInfoArray()
	{
		assert( bufferInfoArrayPoolFreeList < MaxPoolSize );
		return bufferInfoArrayPool[ bufferInfoArrayPoolFreeList++ ];
	}

	[[nodiscard]]
	std::vector<VkDescriptorImageInfo>& NextImageInfoArray()
	{
		assert( imageInfoArrayPoolFreeList < MaxPoolSize );
		return imageInfoArrayPool[ imageInfoArrayPoolFreeList++ ];
	}

	[[nodiscard]]
	VkDescriptorBufferInfo& NextBufferInfo()
	{
		assert( bufferInfoPoolFreeList < MaxPoolSize );
		bufferInfoPool[ bufferInfoPoolFreeList ] = {};
		return bufferInfoPool[ bufferInfoPoolFreeList++ ];
	}

	[[nodiscard]]
	VkDescriptorImageInfo& NextImageInfo()
	{
		assert( imageInfoPoolFreeList < MaxPoolSize );
		imageInfoPool[ imageInfoPoolFreeList ] = {};
		return imageInfoPool[ imageInfoPoolFreeList++ ];
	}

#ifdef USE_VULKAN_RTX
	[[nodiscard]]
	uint32_t NextTlasInfoIndex()
	{
		assert( tlasInfoPoolFreeList < MaxPoolSize );
		const uint32_t idx = tlasInfoPoolFreeList++;
		tlasInfoPool[ idx ] = {};
		tlasHandlePool[ idx ] = VK_NULL_HANDLE;
		return idx;
	}

	VkWriteDescriptorSetAccelerationStructureKHR& GetTlasInfo( uint32_t idx ) { return tlasInfoPool[ idx ]; }
	VkAccelerationStructureKHR& GetTlasHandle( uint32_t idx ) { return tlasHandlePool[ idx ]; }
#endif
};

static DescriptorWritesBuilder writeBuilder;

static void AppendDescriptorWrites( const ShaderBindParms& parms, const uint32_t currentBuffer, std::vector<VkWriteDescriptorSet>& descSetWrites )
{
	LOG_SCOPE_SYSTEM( Vulkan );

	const ShaderBindSet* set = parms.GetSet();

	const uint32_t count = set->Count();
	descSetWrites.reserve( descSetWrites.size() + count );

	for ( uint32_t i = 0; i < count; ++i )
	{
		const ShaderBinding* binding = set->GetBinding( i );

		const ShaderAttachment* attachment = parms.GetAttachment( *binding );
		assert( attachment != nullptr );

		// FIXME: Image array needs some check for every element
		// Might be good to have a special image array class that has a dirty bitfield and semantics (e.g cubemaps)
		if( parms.AttachmentChanged( *binding ) == false )
		{
			if ( attachment->GetSemantic() != bindSemantic_t::IMAGE_ARRAY ) {
				continue;
			}
			const ImageArray& images = *attachment->GetImageArray();
			if ( images.HasPossibleUpdates() == false ) {
				continue;
			}
		}

		static bool print = false;
		if( print ) {
			LogMsg( logSeverity_t::Verbose, "%s", parms.AsString().c_str() );
		}

		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.descriptorCount = binding->GetMaxDescriptorCount();
		writeInfo.dstSet = parms.GetVkObject();
		writeInfo.descriptorType = vk_GetDescriptorType( binding->GetType() );
		writeInfo.dstArrayElement = 0;
		writeInfo.dstBinding = binding->GetSlot();

		if ( attachment->GetSemantic() == bindSemantic_t::BUFFER )
		{
			const GpuBuffer* buffer = attachment->GetBuffer();

			VkDescriptorBufferInfo& info = writeBuilder.NextBufferInfo();
			info.buffer = buffer->GetVkObject();
			info.offset = buffer->GetBaseOffset();
			info.range = buffer->GetSize();

			info.range = ( info.range == 0 ) ? VK_WHOLE_SIZE : info.range;

			assert( info.buffer != nullptr );

			if ( writeInfo.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER )
			{
				const uint64_t storageAlign = context.deviceProperties.limits.minStorageBufferOffsetAlignment;
				assert( storageAlign == 0 || ( info.offset % storageAlign ) == 0 );
			}
			else if ( writeInfo.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER )
			{
				const uint64_t uniformAlign = context.deviceProperties.limits.minUniformBufferOffsetAlignment;
				assert( uniformAlign == 0 || ( info.offset % uniformAlign ) == 0 );
			}

			writeInfo.pBufferInfo = &info;
		}
		else if ( attachment->GetSemantic() == bindSemantic_t::IMAGE )
		{
			const Image* image = attachment->GetImage();
			if ( image == nullptr ) {
				image = rc.whiteImage;
			}

			const gpuImageStateFlags_t imageFlags = attachment->GetImage()->gpuImage->GetFlags();

			VkDescriptorImageInfo& info = writeBuilder.NextImageInfo();
			info.sampler = nullptr;
			info.imageView = attachment->GetImage()->gpuImage->GetVkImageView( currentBuffer );
			assert( info.imageView != nullptr );

			if( HasFlags( imageFlags, gpuImageStateFlags_t::GPU_IMAGE_STORAGE ) ) {
				info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			} else if( IsDepthStencilCompatible( image->info.fmt ) ) {
				info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			} else {
				info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
			writeInfo.pImageInfo = &info;
		}
		else if ( attachment->GetSemantic() == bindSemantic_t::IMAGE_ARRAY )
		{
			std::vector<VkDescriptorImageInfo>& infos = writeBuilder.NextImageInfoArray();

			const ImageArray& images = *attachment->GetImageArray();

			const uint32_t descCount = binding->GetMaxDescriptorCount();
			const uint32_t imageCount = images.Count();
			assert( imageCount <= descCount );

			infos.resize( descCount );
			writeInfo.descriptorCount = descCount;

			for ( uint32_t descIx = 0; descIx < descCount; ++descIx )
			{
				const Image* image = rc.whiteImage;
				if ( ( descIx < imageCount ) && ( images[ descIx ] != nullptr ) ) {
					image = images[ descIx ];
				}

				if( image->gpuImage == nullptr )
				{
					LogMsg( logSeverity_t::Error, "%s", parms.AsString().c_str() );
					FATAL_ERROR( Shader Binding );
				}

				VkDescriptorImageInfo& info = infos[ descIx ];

				info = {};
				info.imageView = image->gpuImage->GetVkImageView( currentBuffer );
				assert( info.imageView != nullptr );

				const gpuImageStateFlags_t imageFlags = image->gpuImage->GetFlags();

				if( HasFlags( imageFlags, gpuImageStateFlags_t::GPU_IMAGE_STORAGE ) ) {
					info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
				} else if ( IsDepthStencilCompatible( image->info.fmt ) ) {
					info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				} else {
					info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}
			}
			writeInfo.pImageInfo = infos.data();
		}
		else if ( attachment->GetSemantic() == bindSemantic_t::IMAGE_SAMPLER )
		{
			const ImageSampler* sampler = attachment->GetImageSampler();
			if (sampler == nullptr) {
				assert( 0 ); // FIXME: Need default
			}

			VkDescriptorImageInfo& info = writeBuilder.NextImageInfo();
			info.sampler = sampler->GetVkObject();
			info.imageView = nullptr;

			assert(info.sampler != nullptr);

			writeInfo.pImageInfo = &info;
		}
#ifdef USE_VULKAN_RTX
		else if ( attachment->GetSemantic() == bindSemantic_t::ACCELERATION_STRUCTURE )
		{
			const GpuAccelerationStructure* accelStruct = attachment->GetAccelerationStructure();
			assert( accelStruct != nullptr );

			const uint32_t idx = writeBuilder.NextTlasInfoIndex();
			VkWriteDescriptorSetAccelerationStructureKHR& asInfo = writeBuilder.GetTlasInfo( idx );
			VkAccelerationStructureKHR& asHandle = writeBuilder.GetTlasHandle( idx );

			asHandle = accelStruct->GetVkObject();
			asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
			asInfo.accelerationStructureCount = 1;
			asInfo.pAccelerationStructures = &asHandle;

			writeInfo.descriptorCount = 1;
			writeInfo.pNext = &asInfo;
		}
#endif

		descSetWrites.push_back( writeInfo );
	}
}


void RenderContext::UpdateBindParms()
{
	AllocRegisteredBindParms();

	writeBuilder.Reset();
	std::vector<VkWriteDescriptorSet> descriptorWrites;

	const uint32_t bindParmCount = bindParmsList.Count();
	for ( uint32_t i = 0; i < bindParmCount; ++i )
	{
		if( bindParmsList[ i ].IsValid() == false ) {
			continue;
		}
		AppendDescriptorWrites( bindParmsList[ i ], context.bufferId, descriptorWrites );
	}

	if( descriptorWrites.size() > 0 ) {
		vkUpdateDescriptorSets( context.device, static_cast<uint32_t>( descriptorWrites.size() ), descriptorWrites.data(), 0, nullptr );
	}
}
