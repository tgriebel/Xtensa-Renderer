#pragma once

#include "../asset_types/image.h"
#include "../render_state/deviceContext.h"
#include "../render_binding/shaderBinding.h"
#include "../render_resources/imageSampler.h"

enum renderPassAttachmentMask_t : uint8_t
{
	RENDER_PASS_MASK_NONE = 0,
	RENDER_PASS_MASK_COLOR0 = ( 1 << 0 ),
	RENDER_PASS_MASK_COLOR1 = ( 1 << 1 ),
	RENDER_PASS_MASK_COLOR2 = ( 1 << 2 ),
	RENDER_PASS_MASK_DEPTH = ( 1 << 3 ),
	RENDER_PASS_MASK_STENCIL = ( 1 << 4 ),

	RENDER_PASS_MASK_COLOR = ( RENDER_PASS_MASK_COLOR0 | RENDER_PASS_MASK_COLOR1 | RENDER_PASS_MASK_COLOR2 ),
	RENDER_PASS_MASK_DS = ( RENDER_PASS_MASK_DEPTH | RENDER_PASS_MASK_STENCIL ),
};
DEFINE_ENUM_OPERATORS( renderPassAttachmentMask_t, uint8_t )

// https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#renderpass-compatibility
struct renderPassAttachmentBits_t
{
	imageSamples_t	samples : 8;
	imageFmt_t		fmt		: 8;
};
static_assert( sizeof( renderPassAttachmentBits_t ) == 2, "Bits overflowed" );


struct renderAttachmentBits_t
{
	renderPassAttachmentBits_t	color0;
	renderPassAttachmentBits_t	color1;
	renderPassAttachmentBits_t	color2;
	renderPassAttachmentBits_t	depth;
	renderPassAttachmentBits_t	stencil;
};
static_assert( sizeof( renderAttachmentBits_t ) == 10, "Bits overflowed" );

inline bool operator==( const renderPassAttachmentBits_t& a, const renderPassAttachmentBits_t& b )
{
	return a.samples == b.samples && a.fmt == b.fmt;
}
inline bool operator!=( const renderPassAttachmentBits_t& a, const renderPassAttachmentBits_t& b )
{
	return !( a == b );
}
inline bool operator<( const renderPassAttachmentBits_t& a, const renderPassAttachmentBits_t& b )
{
	if ( a.samples != b.samples ) return a.samples < b.samples;
	return a.fmt < b.fmt;
}

inline bool operator==( const renderAttachmentBits_t& a, const renderAttachmentBits_t& b )
{
	return a.color0 == b.color0 && a.color1 == b.color1 && a.color2 == b.color2
	    && a.depth  == b.depth  && a.stencil == b.stencil;
}
inline bool operator!=( const renderAttachmentBits_t& a, const renderAttachmentBits_t& b )
{
	return !( a == b );
}
inline bool operator<( const renderAttachmentBits_t& a, const renderAttachmentBits_t& b )
{
	if ( a.color0  != b.color0  ) return a.color0  < b.color0;
	if ( a.color1  != b.color1  ) return a.color1  < b.color1;
	if ( a.color2  != b.color2  ) return a.color2  < b.color2;
	if ( a.depth   != b.depth   ) return a.depth   < b.depth;
	return a.stencil < b.stencil;
}


union renderPassTransition_t
{
	struct renderPassStateBits_t
	{
		uint8_t	clear			: 1;
		uint8_t	store			: 1;
		uint8_t	readAfter		: 1;
		uint8_t	presentAfter	: 1;
		uint8_t	readBefore		: 1;
		uint8_t	presentBefore	: 1;
	} flags;
	uint8_t						bits;
};
static_assert( sizeof( imageSamples_t ) == 1, "Bits overflowed" );
static_assert( sizeof( imageFmt_t ) == 1, "Bits overflowed" );
static_assert( sizeof( renderPassAttachmentBits_t ) == 2, "Bits overflowed" );
static_assert( sizeof( renderPassTransition_t ) == 1, "Bits overflowed" );

static const uint32_t PassPermBits = 6;
static const uint32_t PassPermCount = ( 1 << PassPermBits );

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

#endif // USE_VULKAN
