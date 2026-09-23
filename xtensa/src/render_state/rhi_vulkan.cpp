#include "rhi.h"

struct vk_formatTableEntry_t
{
	imageFmt_t	imgFmt;
	VkFormat	vk_imgFmt;
};


static const vk_formatTableEntry_t vk_formatTable[] =
{
	{ IMAGE_FMT_UNKNOWN,			VK_FORMAT_UNDEFINED					},

	// Depth / stencil
	{ IMAGE_FMT_D_16,				VK_FORMAT_D16_UNORM					},
	{ IMAGE_FMT_D24S8,				VK_FORMAT_D24_UNORM_S8_UINT			},
	{ IMAGE_FMT_D_32,				VK_FORMAT_D32_SFLOAT				},
	{ IMAGE_FMT_D_32_S8,			VK_FORMAT_D32_SFLOAT_S8_UINT		},

	// Single channel (R)
	{ IMAGE_FMT_R_8,				VK_FORMAT_R8_SRGB					},
	{ IMAGE_FMT_R_8_UNORM,			VK_FORMAT_R8_UNORM					},
	{ IMAGE_FMT_R_16,				VK_FORMAT_R16_SFLOAT				},
	{ IMAGE_FMT_R_16_UNORM,			VK_FORMAT_R16_UNORM					},
	{ IMAGE_FMT_R_32,				VK_FORMAT_R32_SFLOAT				},
	{ IMAGE_FMT_R_32_UINT,			VK_FORMAT_R32_UINT					},

	// Two channel (RG)
	{ IMAGE_FMT_RG_16,				VK_FORMAT_R16G16_SFLOAT				},
	{ IMAGE_FMT_RG_16_UINT,			VK_FORMAT_R16G16_UINT				},
	{ IMAGE_FMT_RG_16_SNORM,		VK_FORMAT_R16G16_SNORM				},
	{ IMAGE_FMT_RG_8_UNORM,			VK_FORMAT_R8G8_UNORM				},
	{ IMAGE_FMT_RG_32,				VK_FORMAT_R32G32_SFLOAT				},
	{ IMAGE_FMT_RG_32_UINT,			VK_FORMAT_R32G32_UINT				},

	// Three channel (RGB / BGR)
	{ IMAGE_FMT_RGB_8,				VK_FORMAT_R8G8B8_SRGB				},
	{ IMAGE_FMT_BGR_8,				VK_FORMAT_B8G8R8_SRGB				},
	{ IMAGE_FMT_RGB_16,				VK_FORMAT_R16G16B16_SFLOAT			},
	{ IMAGE_FMT_RGB_32,				VK_FORMAT_R32G32B32_SFLOAT			},

	// Four channel (RGBA / BGRA / ABGR)
	{ IMAGE_FMT_RGBA_8,				VK_FORMAT_R8G8B8A8_SRGB				},
	{ IMAGE_FMT_RGBA_8_UNORM,		VK_FORMAT_R8G8B8A8_UNORM			},
	{ IMAGE_FMT_ABGR_8,				VK_FORMAT_A8B8G8R8_SRGB_PACK32		},
	{ IMAGE_FMT_BGRA_8,				VK_FORMAT_B8G8R8A8_SRGB				},
	{ IMAGE_FMT_RGBA_16,			VK_FORMAT_R16G16B16A16_SFLOAT		},
	{ IMAGE_FMT_RGBA_32,			VK_FORMAT_R32G32B32A32_SFLOAT		},
	{ IMAGE_FMT_RGBA_32_UINT,		VK_FORMAT_R32G32B32A32_UINT			},

	// Packed
	{ IMAGE_FMT_R11G11B10_US,		VK_FORMAT_B10G11R11_UFLOAT_PACK32	},
	{ IMAGE_FMT_A2B10G10R10_UNORM,	VK_FORMAT_A2B10G10R10_UNORM_PACK32	},
};
static_assert( COUNTARRAY( vk_formatTable ) == IMAGE_FMT_COUNT );


struct vk_samplerAddressTableEntry_t
{
	samplerAddress_t		samplerAddr;
	VkSamplerAddressMode	vk_samplerAddr;
};


static const vk_samplerAddressTableEntry_t vk_samplerAddressTable[] =
{
	{ SAMPLER_ADDRESS_WRAP,			VK_SAMPLER_ADDRESS_MODE_REPEAT },
	{ SAMPLER_ADDRESS_CLAMP_EDGE,	VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
	{ SAMPLER_ADDRESS_CLAMP_BORDER,	VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER },
};


VkFormat vk_GetTextureFormat( const imageFmt_t fmt )
{
	for( uint32_t i = 0; i < COUNTARRAY( vk_formatTable ); ++i )
	{
		if( vk_formatTable[i].imgFmt == fmt ) {
			return vk_formatTable[ i ].vk_imgFmt;
		}
	}
	return VK_FORMAT_UNDEFINED;
}


imageFmt_t vk_GetTextureFormat( const VkFormat fmt )
{
	for ( uint32_t i = 0; i < COUNTARRAY( vk_formatTable ); ++i )
	{
		if ( vk_formatTable[ i ].vk_imgFmt == fmt ) {
			return vk_formatTable[ i ].imgFmt;
		}
	}
	return IMAGE_FMT_UNKNOWN;
}


VkSamplerAddressMode vk_GetSamplerAddress( const samplerAddress_t addr )
{
	for ( uint32_t i = 0; i < COUNTARRAY( vk_samplerAddressTable ); ++i ) {
		if ( vk_samplerAddressTable[ i ].samplerAddr == addr ) {
			return vk_samplerAddressTable[ i ].vk_samplerAddr;
		}
	}
	return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}


samplerAddress_t vk_GetSamplerAddress( const VkSamplerAddressMode addr )
{
	for ( uint32_t i = 0; i < COUNTARRAY( vk_samplerAddressTable ); ++i ) {
		if ( vk_samplerAddressTable[ i ].vk_samplerAddr == addr ) {
			return vk_samplerAddressTable[ i ].samplerAddr;
		}
	}
	return SAMPLER_ADDRESS_WRAP;
}


VkBorderColor vk_GetBorderColor( const samplerBorderColor_t borderColor, const bool isTransparent, const bool isFloat )
{
	if( borderColor == SAMPLER_BORDER_BLACK )
	{
		if( isTransparent )
		{
			if( isFloat ) {
				return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			} else {
				return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
			}
		}
		else
		{
			if( isFloat ) {
				return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			} else {
				return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			}
		}
	}
	else if( borderColor == SAMPLER_BORDER_WHITE )
	{
		if( isFloat ) {
			return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		}
		else {
			return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		}
	}
	assert( 0 );
	return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
}


void vk_GetBorderColor( const VkBorderColor vk_borderColor, samplerBorderColor_t& borderColor, bool& isTransparent, bool& isFloat )
{
	switch( vk_borderColor )
	{
		case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
		{
			borderColor = SAMPLER_BORDER_BLACK;
			isTransparent = true;
			isFloat = true;
		}
		break;

		case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
		{
			borderColor = SAMPLER_BORDER_BLACK;
			isTransparent = true;
			isFloat = false;
		}
		break;

		case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
		{
			borderColor = SAMPLER_BORDER_BLACK;
			isTransparent = false;
			isFloat = true;
		}
		break;

		case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
		{
			borderColor = SAMPLER_BORDER_BLACK;
			isTransparent = false;
			isFloat = false;
		}
		break;

		case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
		{
			borderColor = SAMPLER_BORDER_WHITE;
			isTransparent = false;
			isFloat = true;
		}
		break;

		case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
		{
			borderColor = SAMPLER_BORDER_WHITE;
			isTransparent = false;
			isFloat = false;
		}
		break;
	}
	assert( 0 );
	return;
}


VkImageAspectFlagBits vk_GetAspectFlags( const imageAspectFlags_t flags )
{
	uint32_t bitMask = 0x01;

	uint32_t vkFlags = 0;

	while ( bitMask < IMAGE_ASPECT_ALL )
	{
		uint32_t bitFlag = flags & bitMask;
		switch ( bitFlag )
		{
			case IMAGE_ASPECT_COLOR_FLAG:		vkFlags |= VK_IMAGE_ASPECT_COLOR_BIT;	break;
			case IMAGE_ASPECT_DEPTH_FLAG:		vkFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;	break;
			case IMAGE_ASPECT_STENCIL_FLAG:		vkFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;	break;
			case IMAGE_ASPECT_PLANE0:			vkFlags |= VK_IMAGE_ASPECT_PLANE_0_BIT;	break;
			case IMAGE_ASPECT_PLANE1:			vkFlags |= VK_IMAGE_ASPECT_PLANE_1_BIT;	break;
			case IMAGE_ASPECT_PLANE2:			vkFlags |= VK_IMAGE_ASPECT_PLANE_2_BIT;	break;
		}
		bitMask <<= 1;
	}
	return VkImageAspectFlagBits( vkFlags );
}


VkImageAspectFlagBits vk_GetColorAspectFlags( const imageFmt_t fmt )
{
	return vk_GetAspectFlags( GetColorAspectFlags( fmt ) );
}


VkImageViewType vk_GetImageViewType( const imageType_t type )
{
	switch ( type ) {
	default:
		case IMAGE_TYPE_2D:				return VK_IMAGE_VIEW_TYPE_2D;
		case IMAGE_TYPE_2D_ARRAY:		return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		case IMAGE_TYPE_3D:				return VK_IMAGE_VIEW_TYPE_3D;
		case IMAGE_TYPE_CUBE:			return VK_IMAGE_VIEW_TYPE_CUBE;
		case IMAGE_TYPE_CUBE_ARRAY:		return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
	}
	assert( 0 );
	return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
}


VkSampleCountFlagBits vk_GetSampleCount( const imageSamples_t sampleCount )
{
	switch ( sampleCount )
	{
		case IMAGE_SMP_1:			return VK_SAMPLE_COUNT_1_BIT;
		case IMAGE_SMP_2:			return VK_SAMPLE_COUNT_2_BIT;
		case IMAGE_SMP_4:			return VK_SAMPLE_COUNT_4_BIT;
		case IMAGE_SMP_8:			return VK_SAMPLE_COUNT_8_BIT;
		case IMAGE_SMP_16:			return VK_SAMPLE_COUNT_16_BIT;
		case IMAGE_SMP_32:			return VK_SAMPLE_COUNT_32_BIT;
		case IMAGE_SMP_64:			return VK_SAMPLE_COUNT_64_BIT;
		default: assert( false );	break;
	}
	return VK_SAMPLE_COUNT_1_BIT;
}


VkDescriptorType vk_GetDescriptorType( const bindType_t type )
{
	switch ( type )
	{
		case bindType_t::CONSTANT_BUFFER:			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case bindType_t::IMAGE_2D:					return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case bindType_t::IMAGE_2D_ARRAY:			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case bindType_t::IMAGE_3D:					return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case bindType_t::IMAGE_CUBE:				return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case bindType_t::IMAGE_CUBE_ARRAY:			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case bindType_t::READ_BUFFER:				return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case bindType_t::WRITE_BUFFER:				return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case bindType_t::READ_IMAGE_BUFFER:			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case bindType_t::WRITE_IMAGE_BUFFER:		return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case bindType_t::IMAGE_SAMPLER:				return VK_DESCRIPTOR_TYPE_SAMPLER;
#ifdef USE_VULKAN_RTX
		case bindType_t::ACCELERATION_STRUCTURE:	return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
#endif
		default: break;
	}
	assert( 0 );
	return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}


VkShaderStageFlagBits vk_GetStageFlags( const bindStateFlag_t flags )
{
	uint32_t bitMask = 0x01;

	uint32_t vkFlags = 0;

	while ( bitMask <= BIND_STATE_INTERSECTION )
	{
		uint32_t bitFlag = flags & bitMask;
		switch ( bitFlag )
		{
			case BIND_STATE_VS:				vkFlags |= VK_SHADER_STAGE_VERTEX_BIT;				break;
			case BIND_STATE_PS:				vkFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;			break;
			case BIND_STATE_ALL_GFX:		vkFlags |= VK_SHADER_STAGE_ALL_GRAPHICS;			break;
			case BIND_STATE_CS:				vkFlags |= VK_SHADER_STAGE_COMPUTE_BIT;				break;
			case BIND_STATE_ANYHIT:			vkFlags |= VK_SHADER_STAGE_ANY_HIT_BIT_KHR;			break;
			case BIND_STATE_CLOSEST_HIT:	vkFlags |= VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;		break;
			case BIND_STATE_RAYGEN:			vkFlags |= VK_SHADER_STAGE_RAYGEN_BIT_KHR;			break;
			case BIND_STATE_MISS:			vkFlags |= VK_SHADER_STAGE_MISS_BIT_KHR;			break;
			case BIND_STATE_INTERSECTION:	vkFlags |= VK_SHADER_STAGE_INTERSECTION_BIT_KHR;	break;

			default:
			case BIND_STATE_ALL:		vkFlags |= VK_SHADER_STAGE_ALL;				break;
		}
		bitMask <<= 1;
	}
	return VkShaderStageFlagBits( vkFlags );
}
