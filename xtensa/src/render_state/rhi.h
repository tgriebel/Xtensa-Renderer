#pragma once

#include "../asset_types/image.h"
#include "../render_state/deviceContext.h"
#include "../render_binding/shaderBinding.h"
#include "../render_resources/imageSampler.h"

#ifdef USE_VULKAN
static const uint32_t VkPassBitsSize = 16;
#endif


#ifdef USE_VULKAN
struct vk_RenderPassBits_t;
VkRenderPass vk_CreateRenderPass( const vk_RenderPassBits_t& passState );
#endif

#ifdef USE_VULKAN

VkFormat				vk_GetTextureFormat( const imageFmt_t fmt );
imageFmt_t				vk_GetTextureFormat( const VkFormat fmt );

VkSamplerAddressMode	vk_GetSamplerAddress( const samplerAddress_t addr );
samplerAddress_t		vk_GetSamplerAddress( const VkSamplerAddressMode addr );

VkBorderColor			vk_GetBorderColor( const samplerBorderColor_t borderColor, const bool isTransparent, const bool isFloat );
void					vk_GetBorderColor( const VkBorderColor vk_borderColor, samplerBorderColor_t& borderColor, bool& isTransparent, bool& isFloat );

VkImageAspectFlagBits	vk_GetAspectFlags( const imageAspectFlags_t flags );
VkImageAspectFlagBits	vk_GetColorAspectFlags( const imageFmt_t fmt );
VkImageViewType			vk_GetImageViewType( const imageType_t type );
VkSampleCountFlagBits	vk_GetSampleCount( const imageSamples_t sampleCount );
VkDescriptorType		vk_GetDescriptorType( const bindType_t type );
VkShaderStageFlagBits	vk_GetStageFlags( const bindStateFlag_t flags );

struct pipelineState_t;
struct pipelineObject_t;

bool	vk_CreateGraphicsPipeline( const pipelineState_t& state, pipelineObject_t& pipelineObject );
void	vk_CreateComputePipeline( const pipelineState_t& state, pipelineObject_t& pipelineObject );

#ifdef USE_VULKAN_RTX
void	vk_CreateRtPipeline( const pipelineState_t& state, pipelineObject_t& pipelineObject );
#endif

#endif // USE_VULKAN
