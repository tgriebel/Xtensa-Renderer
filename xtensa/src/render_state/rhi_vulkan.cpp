#include "rhi.h"
#include "../render_core/renderer.h"
#include "../render_binding/pipeline.h"
#include "../render_binding/shaderBinding.h"
#include "../render_binding/vertexInput.h"
#include "../draw_passes/drawpass.h"

#include <SysCore/common.h>

#ifdef USE_VULKAN

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


bool vk_CreateGraphicsPipeline( const pipelineState_t& state, pipelineObject_t& pipelineObject )
{
	const GpuProgram& prog = *state.prog;

	const rasterPipelineState_t& rasterState = state.rasterState;

	const uint32_t permIndex = static_cast<uint32_t>( state.permSet );

	assert( prog.shaderCount == 2 );

	auto shaderMapItVs = prog.shaderBins[ 0 ].find( permIndex );
	if( shaderMapItVs == prog.shaderBins[ 0 ].end() ) {
		return false;
	}

	const ShaderBin& vsBin = shaderMapItVs->second;
	assert( vsBin.type == shaderType_t::VERTEX );

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{ };
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vsBin.vk_shader;
	vertShaderStageInfo.pName = "VSMain";

	auto shaderMapItPs = prog.shaderBins[ 1 ].find( permIndex );
	if( shaderMapItPs == prog.shaderBins[ 1 ].end() ) {
		return false;
	}

	const ShaderBin& psBin = shaderMapItPs->second;
	assert( psBin.type == shaderType_t::PIXEL );

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{ };
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = psBin.vk_shader;
	fragShaderStageInfo.pName = "PSMain";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	VkVertexInputBindingDescription bindingDescription{ };
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof( vsInput_t );
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VertexDescription attributeDescriptions = GetVertexAttributeDescriptions();

	VkVertexInputAttributeDescription vertDesc[ VertexDescription::Capacity ] = {};

	for( uint32_t i = 0; i < VertexDescription::Capacity; ++i ) {
		vertDesc[ i ] = attributeDescriptions[ i ].desc;
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{ };
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	if( HasFlags( prog.flags, shaderFlags_t::NO_VERTEX_BUFFER ) )
	{
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;
	}
	else
	{
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = VertexDescription::Capacity;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = vertDesc;
	}

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{ };
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport{ };
	viewport.x = rasterState.viewportX;
	viewport.y = rasterState.viewportY;
	viewport.width = rasterState.viewportWidth;
	viewport.height = rasterState.viewportHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor{ };
	scissor.offset = { (int32_t)rasterState.viewportX, (int32_t)rasterState.viewportY };
	scissor.extent = { (uint32_t)rasterState.viewportWidth, (uint32_t)rasterState.viewportHeight };

	VkPipelineViewportStateCreateInfo viewportState{ };
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	uint32_t cullBits = VK_CULL_MODE_NONE;
	cullBits |= ( ( rasterState.stateBits & GFX_STATE_CULL_MODE_BACK ) != 0 ) ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
	cullBits |= ( ( rasterState.stateBits & GFX_STATE_CULL_MODE_FRONT ) != 0 ) ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE;

	VkPipelineRasterizationStateCreateInfo rasterizer{ };
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = ( ( rasterState.stateBits & GFX_STATE_WIREFRAME_ENABLE ) != 0 ) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = cullBits;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	VkPipelineMultisampleStateCreateInfo multisampling{ };
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = ForceDisableMSAA ? VK_FALSE : VK_TRUE;
	multisampling.rasterizationSamples = vk_GetSampleCount( rasterState.samplingRate );
	multisampling.minSampleShading = 0.25f;
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	const VkColorComponentFlags allColorFlags = ( VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT );

	VkColorComponentFlags colorFlags[3] = {};
	if ( ( rasterState.stateBits & GFX_STATE_COLOR0_MASK ) == 0 ) {
		colorFlags[ 0 ] = allColorFlags;
	}
	if ( ( rasterState.stateBits & GFX_STATE_COLOR1_MASK ) == 0 ) {
		colorFlags[ 1 ] = allColorFlags;
	}
	if ( ( rasterState.stateBits & GFX_STATE_COLOR2_MASK ) == 0 ) {
		colorFlags[ 2 ] = allColorFlags;
	}

	const bool blendEnable = ( ( rasterState.stateBits & GFX_STATE_BLEND_ENABLE ) != 0 );

	uint32_t colorAttachmentCount = 0;
	colorAttachmentCount += HasFlags( rasterState.attachmentMask, RENDER_PASS_MASK_COLOR0 ) ? 1 : 0;
	colorAttachmentCount += HasFlags( rasterState.attachmentMask, RENDER_PASS_MASK_COLOR1 ) ? 1 : 0;
	colorAttachmentCount += HasFlags( rasterState.attachmentMask, RENDER_PASS_MASK_COLOR2 ) ? 1 : 0;

	assert( colorAttachmentCount <= 3 );

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
	colorBlendAttachments.resize( colorAttachmentCount );

	for ( uint32_t i = 0; i < colorAttachmentCount; ++i )
	{
		colorBlendAttachments[ i ].colorWriteMask = colorFlags[ i ];
		colorBlendAttachments[ i ].blendEnable = blendEnable ? VK_TRUE : VK_FALSE;
		colorBlendAttachments[ i ].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachments[ i ].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachments[ i ].colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachments[ i ].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachments[ i ].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachments[ i ].alphaBlendOp = VK_BLEND_OP_ADD;
	}

	VkPipelineColorBlendStateCreateInfo colorBlending{ };
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = static_cast<uint32_t>( colorBlendAttachments.size() );
	colorBlending.pAttachments = colorBlendAttachments.data();

	uint32_t dynamicStatesCount = 0;
	VkDynamicState dynamicStates[4];
	if( ( ( rasterState.stateBits & GFX_STATE_STENCIL_ENABLE ) != 0 ) ) {
		dynamicStates[ dynamicStatesCount++ ] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
	}
	dynamicStates[ dynamicStatesCount++ ] = VK_DYNAMIC_STATE_VIEWPORT;
	dynamicStates[ dynamicStatesCount++ ] = VK_DYNAMIC_STATE_SCISSOR;
	dynamicStates[ dynamicStatesCount++ ] = VK_DYNAMIC_STATE_LINE_WIDTH;

	VkPipelineDynamicStateCreateInfo dynamicState{ };
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = dynamicStatesCount;
	dynamicState.pDynamicStates = dynamicStates;

	VkDescriptorSetLayout layouts[GpuProgram::MaxBindSets];
	for( uint32_t i = 0; i < prog.bindsetCount; ++i ) {
		layouts[ i ] = prog.bindsets[ i ]->GetVkObject();
	}

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ };
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pSetLayouts = layouts;
	pipelineLayoutInfo.setLayoutCount = prog.bindsetCount;

	pipelineLayoutInfo.pushConstantRangeCount = 1;

	VkPushConstantRange pushRanges{};
	pushRanges.offset = 0;
	pushRanges.size = sizeof( gpuPushConstants_t );
	pushRanges.stageFlags = VK_SHADER_STAGE_ALL;
	pipelineLayoutInfo.pPushConstantRanges = &pushRanges;

	VK_CHECK_RESULT( vkCreatePipelineLayout( context.device, &pipelineLayoutInfo, nullptr, &pipelineObject.pipelineLayout ) );

	vk_SetObjectName( (uint64_t)pipelineObject.pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, ( "Pipeline Layout (GFX): < " + vsBin.binName + " | " + psBin.binName + " >" ).c_str() );

	const bool depthTestEnable = ( ( rasterState.stateBits & GFX_STATE_DEPTH_TEST ) != 0 );
	const bool depthWriteEnable = ( ( rasterState.stateBits & GFX_STATE_DEPTH_WRITE ) != 0 );

	VkCompareOp blendOp;
	if( ( rasterState.stateBits & GFX_STATE_DEPTH_OP_0 ) != 0 ) {
		blendOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	} else {
		blendOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	}

	VkPipelineDepthStencilStateCreateInfo depthStencil{ };
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = depthTestEnable ? VK_TRUE : VK_FALSE;
	depthStencil.depthWriteEnable = depthWriteEnable && !blendEnable ? VK_TRUE : VK_FALSE;
	depthStencil.depthCompareOp = blendOp;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional

	const bool stencilEnable = ( ( rasterState.stateBits & GFX_STATE_STENCIL_ENABLE ) != 0 );

	if ( stencilEnable && ( blendEnable == false ) )
	{
		depthStencil.stencilTestEnable = VK_TRUE;
		depthStencil.back.compareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
		depthStencil.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencil.back.depthFailOp = VK_STENCIL_OP_REPLACE;
		depthStencil.back.passOp = VK_STENCIL_OP_REPLACE;
		depthStencil.back.compareMask = 0xFF;
		depthStencil.back.writeMask = 0xFF;
		depthStencil.back.reference = 0;
		depthStencil.front = depthStencil.back;
	}
	else
	{
		depthStencil.back.compareOp = VK_COMPARE_OP_NEVER;
		depthStencil.back.failOp = VK_STENCIL_OP_KEEP;
		depthStencil.back.depthFailOp = VK_STENCIL_OP_KEEP;
		depthStencil.back.passOp = VK_STENCIL_OP_REPLACE;
		depthStencil.front = depthStencil.back;
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{ };
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = ( colorAttachmentCount > 0 ) ? 2 : 1;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineObject.pipelineLayout;
	const renderPassAttachmentMask_t attachMask = rasterState.attachmentMask;
	const renderPassAttachmentBits_t* colorBits[ 3 ] = { &rasterState.passBits.color0, &rasterState.passBits.color1, &rasterState.passBits.color2 };

	VkFormat colorAttachmentFormats[ 3 ] = {};
	for ( uint32_t i = 0; i < colorAttachmentCount; ++i ) {
		colorAttachmentFormats[ i ] = vk_GetTextureFormat( colorBits[ i ]->fmt );
	}

	VkPipelineRenderingCreateInfo renderingCreateInfo = {};
	renderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingCreateInfo.colorAttachmentCount = colorAttachmentCount;
	renderingCreateInfo.pColorAttachmentFormats = colorAttachmentFormats;
	renderingCreateInfo.depthAttachmentFormat = ( attachMask & RENDER_PASS_MASK_DEPTH   ) ? vk_GetTextureFormat( rasterState.passBits.depth.fmt   ) : VK_FORMAT_UNDEFINED;
	renderingCreateInfo.stencilAttachmentFormat = ( attachMask & RENDER_PASS_MASK_STENCIL ) ? vk_GetTextureFormat( rasterState.passBits.stencil.fmt ) : VK_FORMAT_UNDEFINED;

	pipelineInfo.pNext = &renderingCreateInfo;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional
	pipelineInfo.pDepthStencilState = &depthStencil;

	VK_CHECK_RESULT( vkCreateGraphicsPipelines( context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineObject.pipeline ) );

	vk_SetObjectName( (uint64_t)pipelineObject.pipeline, VK_OBJECT_TYPE_PIPELINE,  ( "Pipeline (GFX): < " + vsBin.binName + " | " + psBin.binName + " >" ).c_str() );

	return true;
}


void vk_CreateComputePipeline( const pipelineState_t& state, pipelineObject_t& pipelineObject )
{
	const GpuProgram& prog = *state.prog;

	VkDescriptorSetLayout layouts[ GpuProgram::MaxBindSets ];
	for ( uint32_t i = 0; i < prog.bindsetCount; ++i ) {
		layouts[ i ] = prog.bindsets[ i ]->GetVkObject();
	}

	const ShaderBin& csBin = prog.shaderBins[ 0 ].find( 0 )->second;
	assert( csBin.type == shaderType_t::COMPUTE );

	VkPipelineShaderStageCreateInfo computeShaderStageInfo {};
	computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageInfo.module = csBin.vk_shader;
	computeShaderStageInfo.pName = "CSMain";
	computeShaderStageInfo.pNext = nullptr;

	VkPushConstantRange pushRanges;
	pushRanges.offset = 0;
	pushRanges.size = 128;
	pushRanges.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{ };
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.pSetLayouts = layouts;
	pipelineLayoutInfo.setLayoutCount = prog.bindsetCount;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushRanges;

	VK_CHECK_RESULT( vkCreatePipelineLayout( context.device, &pipelineLayoutInfo, nullptr, &pipelineObject.pipelineLayout ) );

	vk_SetObjectName( (uint64_t)pipelineObject.pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, ( "PipelineLayout (Compute): < " + csBin.binName + " >" ).c_str() );

	VkComputePipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineInfo.flags = 0;
	pipelineInfo.layout = pipelineObject.pipelineLayout;
	pipelineInfo.stage = computeShaderStageInfo;
	pipelineInfo.pNext = nullptr;

	VK_CHECK_RESULT( vkCreateComputePipelines( context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipelineObject.pipeline ) );

	vk_SetObjectName( (uint64_t)pipelineObject.pipeline, VK_OBJECT_TYPE_PIPELINE, ( "Pipeline (Compute): < " + csBin.binName + " >" ).c_str() );
}


#ifdef USE_VULKAN_RTX

static constexpr uint32_t s_rgenGroupIndex = 0;
static constexpr uint32_t s_missGroupIndex = 1;

static VkShaderStageFlagBits vk_ShaderTypeToVkStage( const shaderType_t type )
{
	switch( type )
	{
		case VERTEX:			return VK_SHADER_STAGE_VERTEX_BIT;
		case PIXEL:				return VK_SHADER_STAGE_FRAGMENT_BIT;
		case COMPUTE:			return VK_SHADER_STAGE_COMPUTE_BIT;
		case RT_GEN:			return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
		case RT_MISS:			return VK_SHADER_STAGE_MISS_BIT_KHR;
		case RT_CLOSEST_HIT:	return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
		case RT_ANY_HIT:		return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
		case RT_INTERSECTION:	return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
		case RT_CALLABLE:		return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
		default:
			assert( 0 );
			return VK_SHADER_STAGE_ALL;
	}
}


static const char* GetShaderEntryPoint( const shaderType_t type )
{
	switch( type )
	{
	case VERTEX:			return "VSMain";
	case PIXEL:				return "PSMain";
	case COMPUTE:			return "CSMain";
	case RT_GEN:			return "RayGen";
	case RT_MISS:			return "miss_main";
	case RT_CLOSEST_HIT:	return "closesthit_main";
	case RT_ANY_HIT:		return "anyhit_main";
	case RT_INTERSECTION:	return "intersection_main";
	case RT_CALLABLE:		return "callable_main";

	default:
		assert( 0 );
		return "main";
	}
}


void vk_CreateRtPipeline( const pipelineState_t& state, pipelineObject_t& obj )
{
	const GpuProgram& rgen = *state.prog;
	const GpuProgram& miss = *state.rtState.missProg;
	const GpuProgram& hitGroup = *state.rtState.hitGroupProg;

	std::vector<VkPipelineShaderStageCreateInfo> stages;
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

	// Build pipeline
	{
		// Append the raygen and miss shaders
		const GpuProgram* generalProgs[] = { &rgen, &miss };
		for( const GpuProgram* prog : generalProgs )
		{
			assert( prog->shaderCount == 1 );
			const ShaderBin& bin = prog->shaderBins[ 0 ].at( 0 );

			VkPipelineShaderStageCreateInfo& stage = stages.emplace_back();
			stage = {};
			stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage.stage = vk_ShaderTypeToVkStage( bin.type );
			stage.module = bin.vk_shader;
			stage.pName = GetShaderEntryPoint( bin.type );

			VkRayTracingShaderGroupCreateInfoKHR& group = groups.emplace_back();
			group = {};
			group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
			group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
			group.generalShader = static_cast<uint32_t>( stages.size() - 1 );
			group.closestHitShader = VK_SHADER_UNUSED_KHR;
			group.anyHitShader = VK_SHADER_UNUSED_KHR;
			group.intersectionShader = VK_SHADER_UNUSED_KHR;
		}

		uint32_t rchitStage = VK_SHADER_UNUSED_KHR;
		uint32_t rahitStage = VK_SHADER_UNUSED_KHR;
		uint32_t rintStage = VK_SHADER_UNUSED_KHR;

		for( uint32_t i = 0; i < hitGroup.shaderCount; ++i )
		{
			const ShaderBin& bin = hitGroup.shaderBins[ i ].at( 0 );

			VkPipelineShaderStageCreateInfo& stage = stages.emplace_back();
			stage = {};
			stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stage.stage = vk_ShaderTypeToVkStage( bin.type );
			stage.module = bin.vk_shader;
			stage.pName = GetShaderEntryPoint( bin.type );

			const uint32_t stageIndex = static_cast<uint32_t>( stages.size() - 1 );
			switch( bin.type )
			{
				case RT_CLOSEST_HIT:  rchitStage = stageIndex; break;
				case RT_ANY_HIT:      rahitStage = stageIndex; break;
				case RT_INTERSECTION: rintStage = stageIndex; break;
				default: break;
			}
		}

		VkRayTracingShaderGroupCreateInfoKHR& hitGroupInfo = groups.emplace_back();
		hitGroupInfo = {};
		hitGroupInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		hitGroupInfo.type = ( rintStage != VK_SHADER_UNUSED_KHR ) ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
			: VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		hitGroupInfo.generalShader = VK_SHADER_UNUSED_KHR;
		hitGroupInfo.closestHitShader = rchitStage;
		hitGroupInfo.anyHitShader = rahitStage;
		hitGroupInfo.intersectionShader = rintStage;

		VkDescriptorSetLayout layouts[ GpuProgram::MaxBindSets ];
		for( uint32_t i = 0; i < rgen.bindsetCount; ++i )
		{
			layouts[ i ] = rgen.bindsets[ i ]->GetVkObject();
		}

		VkPushConstantRange pushRange{};
		pushRange.stageFlags = ( VK_SHADER_STAGE_RAYGEN_BIT_KHR
			| VK_SHADER_STAGE_MISS_BIT_KHR
			| VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
			| VK_SHADER_STAGE_ANY_HIT_BIT_KHR
			| VK_SHADER_STAGE_INTERSECTION_BIT_KHR );
		pushRange.offset = 0;
		pushRange.size = 128;

		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutInfo.setLayoutCount = rgen.bindsetCount;
		layoutInfo.pSetLayouts = rgen.bindsetCount > 0 ? layouts : nullptr;
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pushRange;

		VK_CHECK_RESULT( vkCreatePipelineLayout( context.device, &layoutInfo, nullptr, &obj.pipelineLayout ) );

		VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
		pipelineInfo.stageCount = static_cast<uint32_t>( stages.size() );
		pipelineInfo.pStages = stages.data();
		pipelineInfo.groupCount = static_cast<uint32_t>( groups.size() );
		pipelineInfo.pGroups = groups.data();
		pipelineInfo.maxPipelineRayRecursionDepth = 2;
		pipelineInfo.layout = obj.pipelineLayout;

		VK_CHECK_RESULT( context.vkCreateRayTracingPipelinesKHR(
			context.device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &obj.pipeline ) );

		vk_SetObjectName( (uint64_t)obj.pipeline, VK_OBJECT_TYPE_PIPELINE, "Pipeline (RT)" );
		vk_SetObjectName( (uint64_t)obj.pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "PipelineLayout (RT)" );
	}

	// Build shader binding table
	{
		const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtProps = context.rayTracingPipelineProperties;

		const uint32_t handleSize = rtProps.shaderGroupHandleSize;
		const uint32_t handleSizeAligned = SysCore::Align( handleSize, rtProps.shaderGroupHandleAlignment );
		const uint32_t baseAlignment = rtProps.shaderGroupBaseAlignment;

		const uint32_t totalGroupCount = static_cast<uint32_t>( groups.size() );
		const uint32_t hitGroupIndex = ( totalGroupCount - 1 );

		const uint32_t dataSize = totalGroupCount * handleSize;
		std::vector<uint8_t> handles( dataSize );
		VK_CHECK_RESULT( context.vkGetRayTracingShaderGroupHandlesKHR(
			context.device, obj.pipeline, 0, totalGroupCount, dataSize, handles.data() ) );

		// Layout: <|rgen|pad|miss|pad|hitGroup>
		const uint32_t rgenOffset = 0;
		const uint32_t missOffset = SysCore::Align( rgenOffset + handleSizeAligned, baseAlignment );
		const uint32_t hitGroupOffset = SysCore::Align( missOffset + handleSizeAligned, baseAlignment );
		const uint32_t totalSize = hitGroupOffset + handleSizeAligned;

		vk_shaderBindTable_t& sbt = obj.state.rtState.sbt;

		obj.state.rtState.sbtBuffer = new GpuBuffer();
		GpuBuffer& sbtBuffer = *obj.state.rtState.sbtBuffer;
		sbtBuffer.Create( "ShaderBindTable", swapBuffering_t::SINGLE_FRAME, resourceLifeTime_t::REBOOT, 1, totalSize, bufferType_t::SHADER_BINDING_TABLE );

		struct sbtEntry_t
		{
			uint32_t							groupIndex;
			uint32_t							offset;
			VkStridedDeviceAddressRegionKHR*	outRegion;
		};

		const sbtEntry_t entries[] =
		{
			{ s_rgenGroupIndex,	rgenOffset,		&sbt.rgenRegion		},
			{ s_missGroupIndex,	missOffset,		&sbt.missRegion		},
			{ hitGroupIndex,	hitGroupOffset,	&sbt.hitGroupRegion	},
		};

		for ( const sbtEntry_t& entry : entries )
		{
			sbtBuffer.SetPos( entry.offset );
			sbtBuffer.CopyData( handles.data() + entry.groupIndex * handleSize, handleSize );

			entry.outRegion->deviceAddress = 0; // Filled after Flush + GetDeviceAddress
			entry.outRegion->stride = handleSizeAligned;
			entry.outRegion->size = handleSizeAligned;
		}

		sbtBuffer.Flush();

		const VkDeviceAddress baseAddr = sbtBuffer.GetDeviceAddress();
		for ( const sbtEntry_t& entry : entries ) {
			entry.outRegion->deviceAddress = baseAddr + entry.offset;
		}

		sbt.callableRegion = {};
	}
}

#endif // USE_VULKAN_RTX


bool vk_ValidTextureFormat( const VkFormat format, VkImageTiling tiling, VkFormatFeatureFlags features )
{
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties( context.physicalDevice, format, &props );

	if ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features ) {
		return true;
	}
	else if ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features ) {
		return true;
	}
	return false;
}


uint32_t vk_FindMemoryType( uint32_t typeFilter, VkMemoryPropertyFlags properties )
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties( context.physicalDevice, &memProperties );

	for ( uint32_t i = 0; i < memProperties.memoryTypeCount; i++ )
	{
		if ( typeFilter & ( 1 << i ) && ( memProperties.memoryTypes[ i ].propertyFlags & properties ) == properties )
		{
			return i;
		}
	}

	THROW_ERROR( "Failed to find suitable memory type!" );
}


int32_t vk_MapToGlslCubemapConvention( const uint32_t index )
{
	static const int32_t glslCubeMapping[ 6 ] = { 4, 5, 1, 0, 2, 3 };
	return glslCubeMapping[ index ];
}


VkImageView vk_CreateImageView( const VkImage image, const imageInfo_t& info, const char* debugName, const uint32_t debugBufferId )
{
	imageSubResourceView_t subResourceView;
	subResourceView.baseMip = 0;
	subResourceView.mipLevels = info.mipLevels;
	subResourceView.baseArray = 0;
	subResourceView.arrayCount = info.layers;
	subResourceView.aspect = GetColorAspectFlags( info.fmt );

	return vk_CreateImageView( image, info, subResourceView, debugName, debugBufferId );
}


VkImageView vk_CreateImageView( const VkImage image, const imageInfo_t& info, const imageSubResourceView_t& subResourceView, const char* debugName, const uint32_t debugBufferId )
{
	const VkImageAspectFlags aspectFlags = vk_GetColorAspectFlags( info.fmt ) & vk_GetAspectFlags( subResourceView.aspect );

	assert( subResourceView.mipLevels >= 1 );
	assert( subResourceView.arrayCount >= 1 );

	VkImageViewCreateInfo viewInfo{ };
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = image;
	viewInfo.viewType = vk_GetImageViewType( info.type );
	viewInfo.format = vk_GetTextureFormat( info.fmt );
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = subResourceView.baseMip;
	viewInfo.subresourceRange.levelCount = subResourceView.mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = subResourceView.baseArray;
	viewInfo.subresourceRange.layerCount = ( info.type == IMAGE_TYPE_CUBE ) ? 6 : subResourceView.arrayCount;

	VkImageView imageView;
	VK_CHECK_RESULT( vkCreateImageView( context.device, &viewInfo, nullptr, &imageView ) );

	if( debugName != "" )
	{
		vk_SetObjectName( (uint64_t)imageView, VK_OBJECT_TYPE_IMAGE_VIEW, vk_BuildObjectName( "ImageView", debugName, debugBufferId ).c_str() );
	}

	return imageView;
}


void vk_TransitionImageLayout( VkCommandBuffer cmdBuffer, const Image* image, const imageSubResourceView_t& subView, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	vk_TransitionImageLayout( cmdBuffer, image->gpuImage, subView, buffering, current, next );
}


void vk_TransitionImageLayout( VkCommandBuffer cmdBuffer, const GpuImage* gpuImage, const imageSubResourceView_t& subView, swapBuffering_t buffering, gpuImageStateFlags_t current, gpuImageStateFlags_t next )
{
	VkImageMemoryBarrier barrier{ };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.baseMipLevel = subView.baseMip;
	barrier.subresourceRange.levelCount = subView.mipLevels;
	barrier.subresourceRange.baseArrayLayer = subView.baseArray;
	barrier.subresourceRange.layerCount = subView.arrayCount;

	const VkImageAspectFlagBits aspectFlags = vk_GetColorAspectFlags( gpuImage->GetInfo().fmt );

	const bool hasColorAspect = ( aspectFlags & VK_IMAGE_ASPECT_COLOR_BIT ) != 0;
	const bool hasDepthAspect = ( aspectFlags & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0;
	const bool hasStencilAspect = ( aspectFlags & VK_IMAGE_ASPECT_STENCIL_BIT ) != 0;

	VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	barrier.srcAccessMask = 0;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( ( current & GPU_IMAGE_READ ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
		barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		barrier.oldLayout = hasColorAspect ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	}
	else if ( ( current & GPU_IMAGE_PRESENT ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else if ( ( current & GPU_IMAGE_TRANSFER_SRC ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	else if ( ( current & GPU_IMAGE_TRANSFER_DST ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
	else if( ( current & GPU_IMAGE_STORAGE ) != 0 )
	{
		sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	else if ( ( current & GPU_IMAGE_WRITE ) != 0 )
	{
		if ( hasDepthAspect )
		{
			sourceStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	barrier.dstAccessMask = 0;
	barrier.newLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if ( ( next & GPU_IMAGE_READ ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		barrier.newLayout = hasColorAspect ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	}
	else if ( ( next & GPU_IMAGE_PRESENT ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	}
	else if ( ( next & GPU_IMAGE_TRANSFER_SRC ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	}
	else if ( ( next & GPU_IMAGE_TRANSFER_DST ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	}
	else if( ( next & GPU_IMAGE_STORAGE ) != 0 )
	{
		destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	}
	else if ( ( next & GPU_IMAGE_WRITE ) != 0 )
	{
		if ( hasDepthAspect )
		{
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}
		else
		{
			destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}

	barrier.subresourceRange.aspectMask = aspectFlags;

	if ( buffering == swapBuffering_t::SINGLE_FRAME )
	{
		barrier.image = gpuImage->GetVkImage( context.bufferId );
		vkCmdPipelineBarrier(
			cmdBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);
	}
	else
	{
		const uint32_t bufferCount = gpuImage->GetBufferCount();
		for ( uint32_t i = 0; i < bufferCount; ++i )
		{
			barrier.image = gpuImage->GetVkImage( i );
			vkCmdPipelineBarrier(
				cmdBuffer,
				sourceStage, destinationStage,
				0,
				0, nullptr,
				0, nullptr,
				1, &barrier
			);
		}
	}
}


void vk_GenerateMipmaps( VkCommandBuffer cmdBuffer, Image* image )
{
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties( context.physicalDevice, vk_GetTextureFormat( image->info.fmt ), &formatProperties );

	if ( !( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT ) )
	{
		THROW_ERROR( "texture outputImage format does not support linear blitting!" );
	}

	VkImageAspectFlags aspectMask = vk_GetColorAspectFlags( image->info.fmt );

	VkImageMemoryBarrier barrier{ };
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image->gpuImage->GetVkImage( context.bufferId );
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = aspectMask;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = image->subResourceView.arrayCount;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = image->info.width;
	int32_t mipHeight = image->info.height;

	for ( uint32_t i = 1; i < image->subResourceView.mipLevels; i++ )
	{
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(	cmdBuffer,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								0,
								0, nullptr,
								0, nullptr,
								1, &barrier );

		uint32_t dstMipWidth;
		uint32_t dstMipHeight;
		MipDimensions( i, image->info.width, image->info.height, &dstMipWidth, &dstMipHeight );

		VkImageBlit blit{ };
		blit.srcOffsets[ 0 ] = { 0, 0, 0 };
		blit.srcOffsets[ 1 ] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = aspectMask;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = image->subResourceView.arrayCount;
		blit.dstOffsets[ 0 ] = { 0, 0, 0 };
		blit.dstOffsets[ 1 ] = { (int32_t)dstMipWidth, (int32_t)dstMipHeight, 1 };
		blit.dstSubresource.aspectMask = aspectMask;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = image->subResourceView.arrayCount;

		vkCmdBlitImage( cmdBuffer,
						image->gpuImage->GetVkImage( context.bufferId ),
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						image->gpuImage->GetVkImage( context.bufferId ),
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&blit,
						VK_FILTER_LINEAR );

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(	cmdBuffer,
								VK_PIPELINE_STAGE_TRANSFER_BIT,
								VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
								0,
								0, nullptr,
								0, nullptr,
								1, &barrier );

		mipWidth = static_cast<int32_t>( dstMipWidth );
		mipHeight = static_cast<int32_t>( dstMipHeight );
	}

	barrier.subresourceRange.baseMipLevel = image->subResourceView.mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(	cmdBuffer,
							VK_PIPELINE_STAGE_TRANSFER_BIT,
							VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							0,
							0, nullptr,
							0, nullptr,
							1, &barrier );
}


void vk_QuadDraw( CommandList& cmdContext, const hdl_t pipeLineHandle, const vec4f& extent, const scissor_t& clipRect, const DrawPass* pass )
{
	VkCommandBuffer cmdBuffer = cmdContext.CommandBuffer();

	const viewport_t& viewport = pass->GetViewport();

	VkViewport vk_viewport{ };
	// TODO: Fix UVs in shader before clamping
	//vk_viewport.x = Max( extent.x, (float)viewport.x );
	//vk_viewport.y = Max( extent.y, (float)viewport.y );
	//vk_viewport.width = Clamp( extent.z, extent.x, (float)viewport.width );
	//vk_viewport.height = Clamp( extent.w, extent.y, (float)viewport.height );
	vk_viewport.x = extent.x;
	vk_viewport.y = extent.y;
	vk_viewport.width = extent.z;
	vk_viewport.height = extent.w;
	vk_viewport.minDepth = 0.0f;
	vk_viewport.maxDepth = 1.0f;
	vkCmdSetViewport( cmdBuffer, 0, 1, &vk_viewport );

	const scissor_t& scissor = pass->GetScissor();

	VkRect2D rect{ };
	rect.offset.x = clipRect.x;
	rect.offset.y = clipRect.y;
	rect.extent.width = clipRect.width;
	rect.extent.height = clipRect.height;
	vkCmdSetScissor( cmdBuffer, 0, 1, &rect );

	pipelineObject_t* pipelineObject = nullptr;
	GetPipelineObject( pipeLineHandle, &pipelineObject );

	if( pipelineObject->pipeline == VK_NULL_HANDLE ) {
		CreateGraphicsPipeline( pipeLineHandle, pipelineObject->state );
	}

	if ( pipelineObject != nullptr )
	{
		const uint32_t descSetCount = 2;
		VkDescriptorSet descSetArray[ descSetCount ] = { cmdContext.GetRenderContext()->globalParms->GetVkObject(), pass->parms->GetVkObject() };

		vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
		vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );

		vkCmdDraw( cmdBuffer, 3, 1, 0, 0 );
	}
}


void vk_RenderImageShader( CommandList& cmdContext, const hdl_t pipeLineHandle, const DrawPass* pass, const renderPassTransition_t& transitionState )
{
	VkCommandBuffer cmdBuffer = cmdContext.CommandBuffer();

	const FrameBuffer* fb = pass->GetFrameBuffer();
	const uint32_t colorAttachmentsCount = fb->ColorLayerCount();

	const VkAttachmentLoadOp  loadOp  = transitionState.flags.clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
	const VkAttachmentStoreOp storeOp = transitionState.flags.store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;

	const VkClearValue clearColor = {};

	const Image* colorImages[ 3 ] = { fb->GetColor(), fb->GetColor1(), fb->GetColor2() };
	VkRenderingAttachmentInfo colorAttachments[ 3 ] = {};
	for ( uint32_t i = 0; i < colorAttachmentsCount; ++i )
	{
		colorAttachments[ i ].sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachments[ i ].imageView   = colorImages[ i ]->gpuImage->GetVkImageView( context.bufferId );
		colorAttachments[ i ].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachments[ i ].loadOp      = loadOp;
		colorAttachments[ i ].storeOp     = storeOp;
		colorAttachments[ i ].clearValue  = clearColor;
	}

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea.offset    = { pass->GetViewport().x, pass->GetViewport().y };
	renderingInfo.renderArea.extent    = { pass->GetViewport().width, pass->GetViewport().height };
	renderingInfo.layerCount           = 1;
	renderingInfo.colorAttachmentCount = colorAttachmentsCount;
	renderingInfo.pColorAttachments    = colorAttachmentsCount > 0 ? colorAttachments : nullptr;

	// Transition render targets from their current layout into attachment-write layout
	const gpuImageStateFlags_t colorPriorState = transitionState.flags.presentBefore ? GPU_IMAGE_PRESENT : GPU_IMAGE_READ;
	for ( uint32_t i = 0; i < colorAttachmentsCount; ++i ) {
		Transition( &cmdContext, *colorImages[ i ], colorPriorState, GPU_IMAGE_WRITE );
	}

	vkCmdBeginRendering( cmdBuffer, &renderingInfo );

	const viewport_t& viewport = pass->GetViewport();

	VkViewport vk_viewport{ };
	vk_viewport.x = static_cast<float>( viewport.x );
	vk_viewport.y = static_cast<float>( viewport.y );
	vk_viewport.width = static_cast<float>( viewport.width );
	vk_viewport.height = static_cast<float>( viewport.height );
	vk_viewport.minDepth = 0.0f;
	vk_viewport.maxDepth = 1.0f;
	vkCmdSetViewport( cmdBuffer, 0, 1, &vk_viewport );

	VkRect2D rect{ };
	rect.extent.width = viewport.width;
	rect.extent.height = viewport.height;
	vkCmdSetScissor( cmdBuffer, 0, 1, &rect );

	pipelineObject_t* pipelineObject = nullptr;
	GetPipelineObject( pipeLineHandle, &pipelineObject );

	if( pipelineObject->pipeline == VK_NULL_HANDLE ) {
		CreateGraphicsPipeline( pipeLineHandle, pipelineObject->state );
	}

	if ( pipelineObject != nullptr )
	{
		const uint32_t descSetCount = 2;
		VkDescriptorSet descSetArray[ descSetCount ] = { cmdContext.GetRenderContext()->globalParms->GetVkObject(), pass->parms->GetVkObject() };

		vkCmdBindPipeline( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipeline );
		vkCmdBindDescriptorSets( cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineObject->pipelineLayout, 0, descSetCount, descSetArray, 0, nullptr );

		vkCmdDraw( cmdBuffer, 3, 1, 0, 0 );
	}

	vkCmdEndRendering( cmdBuffer );

	// Transition render targets back for subsequent sampling (or presentation)
	const gpuImageStateFlags_t colorNextState = transitionState.flags.presentAfter ? GPU_IMAGE_PRESENT : GPU_IMAGE_READ;
	for ( uint32_t i = 0; i < colorAttachmentsCount; ++i ) {
		Transition( &cmdContext, *colorImages[ i ], GPU_IMAGE_WRITE, colorNextState );
	}
}


void vk_CopyImage( VkCommandBuffer cmdBuffer, const Image* src, const copyImageParms_t& srcParms, Image* dst, const copyImageParms_t& dstParms )
{
	bool supportsBlit = true;

	// Check source format properties
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties( context.physicalDevice, vk_GetTextureFormat( src->info.fmt ), &formatProperties );

		if ( ( src->info.tiling == IMAGE_TILING_MORTON ) && ( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == false ) {
			supportsBlit = false;
		}
		if ( ( src->info.tiling == IMAGE_TILING_LINEAR ) && ( formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT ) == false ) {
			supportsBlit = false;
		}
	}

	// Check destination format properties
	{
		VkFormatProperties formatProperties;
		vkGetPhysicalDeviceFormatProperties( context.physicalDevice, vk_GetTextureFormat( dst->info.fmt ), &formatProperties );

		if ( ( dst->info.tiling == IMAGE_TILING_MORTON ) && ( formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == false ) {
			supportsBlit = false;
		}
		if ( ( dst->info.tiling == IMAGE_TILING_LINEAR ) && ( formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT ) == false ) {
			supportsBlit = false;
		}
	}

	const VkImageAspectFlagBits srcAspect = vk_GetColorAspectFlags( src->info.fmt );
	const VkImageAspectFlagBits dstAspect = vk_GetColorAspectFlags( dst->info.fmt );

	VkImageMemoryBarrier srcBarrier{ };
	srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	srcBarrier.image = src->gpuImage->GetVkImage( context.bufferId );
	srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.subresourceRange.aspectMask = srcAspect;
	srcBarrier.subresourceRange.baseArrayLayer = srcParms.baseArray;
	srcBarrier.subresourceRange.layerCount = srcParms.arrayCount;
	srcBarrier.subresourceRange.baseMipLevel = srcParms.baseMip;
	srcBarrier.subresourceRange.levelCount = srcParms.mipLevels;

	VkImageMemoryBarrier dstBarrier{ };
	dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	dstBarrier.image = dst->gpuImage->GetVkImage( context.bufferId );
	dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.subresourceRange.aspectMask = dstAspect;
	dstBarrier.subresourceRange.baseArrayLayer = dstParms.baseArray;
	dstBarrier.subresourceRange.layerCount = dstParms.arrayCount;
	dstBarrier.subresourceRange.baseMipLevel = dstParms.baseMip;
	dstBarrier.subresourceRange.levelCount = dstParms.mipLevels;

	// Transition source and destination to transfer layouts
	{
		srcBarrier.oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		srcBarrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		dstBarrier.oldLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		dstBarrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		VkImageMemoryBarrier preCopy[ 2 ] = { srcBarrier, dstBarrier };
		vkCmdPipelineBarrier( cmdBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 2, preCopy );
	}

	// Perform blit/copy
	if( supportsBlit )
	{
		// Add multiple regions for each mip-level
		assert( ( srcParms.mipLevels == 1 ) && ( dstParms.mipLevels == 1 ) );

		VkImageBlit blit{ };
		blit.srcOffsets[ 0 ] = { srcParms.x, srcParms.y, srcParms.z };
		blit.srcOffsets[ 1 ] = { srcParms.width, srcParms.height, srcParms.depth };
		blit.srcSubresource.aspectMask = srcAspect;
		blit.srcSubresource.mipLevel = srcParms.baseMip;
		blit.srcSubresource.baseArrayLayer = srcParms.baseArray;
		blit.srcSubresource.layerCount = srcParms.arrayCount;
		blit.dstOffsets[ 0 ] = { dstParms.x, dstParms.y, dstParms.z };
		blit.dstOffsets[ 1 ] = { dstParms.width, dstParms.height, dstParms.depth };
		blit.dstSubresource.aspectMask = dstAspect;
		blit.dstSubresource.baseArrayLayer = dstParms.baseArray;
		blit.dstSubresource.layerCount = dstParms.arrayCount;
		blit.dstSubresource.mipLevel = dstParms.baseMip;

		vkCmdBlitImage( cmdBuffer,
						src->gpuImage->GetVkImage( context.bufferId ),
						VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						dst->gpuImage->GetVkImage( context.bufferId ),
						VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
						1,
						&blit,
						VK_FILTER_LINEAR );
	}
	else
	{
		// Add multiple regions for each mip-level
		assert( ( srcParms.mipLevels == 1 ) && ( dstParms.mipLevels == 1 ) );

		VkImageCopy imageCopyRegion{};
		imageCopyRegion.srcSubresource.aspectMask = srcAspect;
		imageCopyRegion.srcSubresource.layerCount = srcParms.arrayCount;
		imageCopyRegion.srcSubresource.baseArrayLayer = srcParms.baseArray;
		imageCopyRegion.srcSubresource.mipLevel = srcParms.baseMip;
		imageCopyRegion.dstSubresource.aspectMask = dstAspect;
		imageCopyRegion.dstSubresource.layerCount = dstParms.arrayCount;
		imageCopyRegion.dstSubresource.baseArrayLayer = dstParms.baseArray;
		imageCopyRegion.dstSubresource.mipLevel = dstParms.baseMip;
		imageCopyRegion.extent.width = srcParms.width;
		imageCopyRegion.extent.height = srcParms.height;
		imageCopyRegion.extent.depth = srcParms.depth;

		// Issue the copy command
		vkCmdCopyImage(
			cmdBuffer,
			src->gpuImage->GetVkImage( context.bufferId ),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dst->gpuImage->GetVkImage( context.bufferId ),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageCopyRegion );
	}

	// Transition source and destination back to shader read
	{
		srcBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		dstBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		dstBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		dstBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		VkImageMemoryBarrier postCopy[ 2 ] = { srcBarrier, dstBarrier };
		vkCmdPipelineBarrier( cmdBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 2, postCopy );
	}
}


void vk_CopyImage( VkCommandBuffer cmdBuffer, const Image& src, Image& dst )
{
	copyImageParms_t srcCopy{};
	srcCopy.baseArray = 0;
	srcCopy.arrayCount = src.subResourceView.arrayCount;
	srcCopy.baseMip = 0;
	srcCopy.mipLevels = src.subResourceView.mipLevels;
	srcCopy.x = 0;
	srcCopy.y = 0;
	srcCopy.z = 0;
	srcCopy.width = src.info.width;
	srcCopy.height = src.info.height;
	srcCopy.depth = 1;

	copyImageParms_t dstCopy{};
	dstCopy.baseArray = 0;
	dstCopy.arrayCount = dst.subResourceView.arrayCount;
	dstCopy.baseMip = 0;
	dstCopy.mipLevels = dst.subResourceView.mipLevels;
	dstCopy.x = 0;
	dstCopy.y = 0;
	dstCopy.z = 0;
	dstCopy.width = dst.info.width;
	dstCopy.height = dst.info.height;
	dstCopy.depth = 1;

	vk_CopyImage( cmdBuffer, &src, srcCopy, &dst, dstCopy );
}


void vk_CopyImage( VkCommandBuffer cmdBuffer, const ImageView& src, ImageView& dst )
{
	copyImageParms_t srcCopy{};
	srcCopy.baseArray = src.subResourceView.baseArray;
	srcCopy.arrayCount = src.subResourceView.arrayCount;
	srcCopy.baseMip = src.subResourceView.baseMip;
	srcCopy.mipLevels = src.subResourceView.mipLevels;
	srcCopy.x = 0;
	srcCopy.y = 0;
	srcCopy.z = 0;
	srcCopy.width = src.info.width;
	srcCopy.height = src.info.height;
	srcCopy.depth = 1;

	copyImageParms_t dstCopy{};
	dstCopy.baseArray = dst.subResourceView.baseArray;
	dstCopy.arrayCount = dst.subResourceView.arrayCount;
	dstCopy.baseMip = dst.subResourceView.baseMip;
	dstCopy.mipLevels = dst.subResourceView.mipLevels;
	dstCopy.x = 0;
	dstCopy.y = 0;
	dstCopy.z = 0;
	dstCopy.width = dst.info.width;
	dstCopy.height = dst.info.height;
	dstCopy.depth = 1;

	vk_CopyImage( cmdBuffer, &src, srcCopy, &dst, dstCopy );
}


void vk_ResolveImage( VkCommandBuffer cmdBuffer, const resolveImageInfo_t& info )
{
	const VkImageAspectFlagBits aspect  = vk_GetColorAspectFlags( info.src->info.fmt );
	const bool                  isDepth = ( aspect & VK_IMAGE_ASPECT_DEPTH_BIT ) != 0;

	// Depth and color images live in different layouts and are owned by different pipeline stages
	const VkImageLayout attachmentLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	const VkImageLayout readLayout = isDepth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	const VkPipelineStageFlags attachmentStage = isDepth ? ( VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT )
	                                                        : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	const VkAccessFlags attachmentWrite = isDepth ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	const VkAccessFlags attachmentRW = isDepth ? ( VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT )
												: ( VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT );

	// Transition source to TRANSFER_SRC_OPTIMAL
	VkImageMemoryBarrier srcBarrier{ };
	srcBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	srcBarrier.image                           = info.src->gpuImage->GetVkImage( context.bufferId );
	srcBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	srcBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
	srcBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.subresourceRange.aspectMask     = aspect;
	srcBarrier.subresourceRange.baseMipLevel   = info.baseMip;
	srcBarrier.subresourceRange.levelCount     = 1;
	srcBarrier.subresourceRange.baseArrayLayer = info.baseArray;
	srcBarrier.subresourceRange.layerCount     = info.arrayCount;

	VkPipelineStageFlags srcStage;
	if ( info.transitionSourceFromWrite )
	{
		srcBarrier.oldLayout     = attachmentLayout;
		srcBarrier.srcAccessMask = attachmentWrite;
		srcStage                 = attachmentStage;
	}
	else
	{
		srcBarrier.oldLayout     = readLayout;
		srcBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		srcStage                 = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}

	// Transition destination to TRANSFER_DST_OPTIMAL
	VkImageMemoryBarrier dstBarrier{ };
	dstBarrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	dstBarrier.image                           = info.dst->gpuImage->GetVkImage( context.bufferId );
	dstBarrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
	dstBarrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dstBarrier.srcAccessMask                   = 0;
	dstBarrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
	dstBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
	dstBarrier.subresourceRange.aspectMask     = aspect;
	dstBarrier.subresourceRange.baseMipLevel   = info.baseMip;
	dstBarrier.subresourceRange.levelCount     = 1;
	dstBarrier.subresourceRange.baseArrayLayer = info.baseArray;
	dstBarrier.subresourceRange.layerCount     = info.arrayCount;

	VkImageMemoryBarrier preBlit[ 2 ] = { srcBarrier, dstBarrier };
	vkCmdPipelineBarrier( cmdBuffer,
		srcStage,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, 0, nullptr, 0, nullptr, 2, preBlit );

	VkImageResolve region{ };
	region.srcSubresource.aspectMask     = aspect;
	region.srcSubresource.mipLevel       = info.baseMip;
	region.srcSubresource.baseArrayLayer = info.baseArray;
	region.srcSubresource.layerCount     = info.arrayCount;
	region.srcOffset                     = { 0, 0, 0 };
	region.dstSubresource.aspectMask     = aspect;
	region.dstSubresource.mipLevel       = info.baseMip;
	region.dstSubresource.baseArrayLayer = info.baseArray;
	region.dstSubresource.layerCount     = info.arrayCount;
	region.dstOffset                     = { 0, 0, 0 };
	region.extent                        = { info.src->info.width, info.src->info.height, 1 };

	vkCmdResolveImage(
		cmdBuffer,
		info.src->gpuImage->GetVkImage( context.bufferId ), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		info.dst->gpuImage->GetVkImage( context.bufferId ), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region );

	// Destination always goes to SHADER_READ_ONLY_OPTIMAL — resolved images are sampled, not written
	dstBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	dstBarrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	dstBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	if ( info.writeSourceAfterResolve )
	{
		// Restore source to its attachment layout so the next render pass can write to it
		srcBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.newLayout     = attachmentLayout;
		srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcBarrier.dstAccessMask = attachmentRW;

		VkImageMemoryBarrier postResolve[ 2 ] = { srcBarrier, dstBarrier };
		vkCmdPipelineBarrier( cmdBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			attachmentStage | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 2, postResolve );
	}
	else
	{
		// Restore source to its read layout for subsequent shader sampling
		srcBarrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		srcBarrier.newLayout     = readLayout;
		srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		srcBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		VkImageMemoryBarrier postResolve[ 2 ] = { srcBarrier, dstBarrier };
		vkCmdPipelineBarrier( cmdBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 2, postResolve );
	}
}


void vk_UploadImageData( VkCommandBuffer cmdBuffer, Image* image, const copyImageParms_t& copyParms, GpuBuffer& buffer )
{
	std::vector<VkBufferImageCopy> regions;
	regions.resize( copyParms.mipLevels );

	const uint64_t bufferSize = buffer.GetSize();
	// bufferOffset must be a multiple of the texel block size (up to 16 bytes for BC/RGBA32F)
	// https://docs.vulkan.org/spec/latest/chapters/copies.html#VUID-vkCmdCopyBufferToImage-dstImage-07975
	const uint32_t bpp = GetBppForFormat( image->info.fmt );
	const uint64_t copyAlignment = Max( context.deviceProperties.limits.optimalBufferCopyOffsetAlignment, static_cast<VkDeviceSize>( bpp ) );
	const uint64_t alignmentOffset = buffer.GetAlignedSize( bufferSize, copyAlignment );
	buffer.SetPos( alignmentOffset );

	for( uint32_t mip = 0; mip < copyParms.mipLevels; ++mip )
	{
		VkBufferImageCopy& region = regions[ mip ];
		memset( &region, 0, sizeof( VkBufferImageCopy ) );

		region.bufferOffset = buffer.GetSize();

		// Just assume continuous layers for now
		for ( uint32_t layer = 0; layer < image->info.layers; ++layer )
		{
			const slice_t imageBuffer = image->cpuImage->GetSlice( layer, mip );
			buffer.CopyData( imageBuffer.ptr, imageBuffer.size );
		}

		region.imageSubresource.aspectMask = vk_GetColorAspectFlags( image->info.fmt );
		region.imageSubresource.mipLevel = copyParms.baseMip + mip;
		region.imageSubresource.baseArrayLayer = copyParms.baseArray;
		region.imageSubresource.layerCount = copyParms.arrayCount;

		uint32_t mipWidth, mipHeight;
		MipDimensions( mip, copyParms.width, copyParms.height, &mipWidth, &mipHeight );

		const int32_t x = static_cast<int32_t>( ( copyParms.x / (float)copyParms.width ) * mipWidth );
		const int32_t y = static_cast<int32_t>( ( copyParms.y / (float)copyParms.height ) * mipHeight );

		region.imageOffset = { x, y, 0 };
		region.imageExtent = {
			static_cast<uint32_t>( mipWidth ),
			static_cast<uint32_t>( mipHeight ),
			static_cast<uint32_t>( 1.0f ),
		};
	}

	vkCmdCopyBufferToImage( cmdBuffer,
							buffer.GetVkObject(),
							image->gpuImage->GetVkImage( context.bufferId ),
							VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							static_cast<uint32_t>( regions.size() ),
							regions.data()
	);
}


imageSamples_t vk_MaxImageSamples()
{
	if( ForceDisableMSAA ) {
		return IMAGE_SMP_1;
	}

	imageSamples_t samples = IMAGE_SMP_1;

	VkSampleCountFlags frameBufferCount = context.deviceProperties.limits.framebufferColorSampleCounts;
	VkSampleCountFlags depthBufferCount = context.deviceProperties.limits.framebufferDepthSampleCounts;
	VkSampleCountFlags counts = ( frameBufferCount & depthBufferCount );

	if ( counts & VK_SAMPLE_COUNT_64_BIT ) { samples = IMAGE_SMP_64; }
	else if ( counts & VK_SAMPLE_COUNT_32_BIT ) { samples = IMAGE_SMP_32; }
	else if ( counts & VK_SAMPLE_COUNT_16_BIT ) { samples = IMAGE_SMP_16; }
	else if ( counts & VK_SAMPLE_COUNT_8_BIT ) { samples = IMAGE_SMP_8; }
	else if ( counts & VK_SAMPLE_COUNT_4_BIT ) { samples = IMAGE_SMP_4; }
	else if ( counts & VK_SAMPLE_COUNT_2_BIT ) { samples = IMAGE_SMP_2; }

	return samples;
}


VkShaderModule vk_CreateShaderModule( const std::vector<char>& code, const char* debugName )
{
	VkShaderModuleCreateInfo createInfo{ };
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>( code.data() );

	VkShaderModule shaderModule;
	VK_CHECK_RESULT( vkCreateShaderModule( context.device, &createInfo, nullptr, &shaderModule ) );

	vk_SetObjectName( (uint64_t)shaderModule, VK_OBJECT_TYPE_SHADER_MODULE, vk_BuildObjectName( "Shader", debugName ).c_str() );

	return shaderModule;
}


std::string vk_BuildObjectName( const char* typeName, const char* baseName, int32_t bufferId )
{
	std::stringstream dbgNameSS;
	dbgNameSS.clear();

	dbgNameSS << typeName << ": " << baseName;
	if( bufferId >= 0 ) {
		dbgNameSS << "(" << bufferId << ")";
	}
	return dbgNameSS.str();
}


void vk_SetObjectName( const uint64_t handle, VkObjectType objectType, const char* name )
{
	if ( context.fnCmdSetDebugUtilsObjectName == nullptr ) {
		return;
	}

	VkDebugUtilsObjectNameInfoEXT nameInfo{};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = objectType;
	nameInfo.objectHandle = handle;
	nameInfo.pObjectName = name;

	context.fnCmdSetDebugUtilsObjectName( context.device, &nameInfo );
}


void vk_MarkerSetObjectTag( uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag )
{
	if ( context.debugMarkersEnabled )
	{
		VkDebugMarkerObjectTagInfoEXT tagInfo = {};
		tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
		tagInfo.objectType = objectType;
		tagInfo.object = object;
		tagInfo.tagName = name;
		tagInfo.tagSize = tagSize;
		tagInfo.pTag = tag;
		context.fnDebugMarkerSetObjectTag( context.device, &tagInfo );
	}
}


void vk_MarkerSetObjectName( uint64_t object, VkDebugReportObjectTypeEXT objectType, const char* name )
{
	if ( context.debugMarkersEnabled )
	{
		VkDebugMarkerObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = objectType;
		nameInfo.object = object;
		nameInfo.pObjectName = name;
		context.fnDebugMarkerSetObjectName( context.device, &nameInfo );
	}
}

#endif // USE_VULKAN
