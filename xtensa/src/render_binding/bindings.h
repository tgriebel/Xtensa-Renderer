#pragma once

#include "shaderBinding.h"

// ***************************** IMPORTANT ***********************************
//																			\\
//	Must mirror changes and recompile all shaders when adjusting bindings	\\
//																			\\
// ***************************************************************************

#define BINDING( NAME, TYPE, COUNT, FLAGS )	static const ShaderBinding bind_##NAME( #NAME, bindType_t::TYPE, COUNT, FLAGS )
#define BINDING_NAME( NAME ) bind_##NAME

BINDING( globalsBuffer, CONSTANT_BUFFER, 1, BIND_STATE_ALL );

// Compute Resources
BINDING( particleWriteBuffer,	WRITE_BUFFER,		1,	BIND_STATE_CS );
BINDING( computeParms,			CONSTANT_BUFFER,	1,	BIND_STATE_CS );
BINDING( computeWrite,			WRITE_BUFFER,		1,	BIND_STATE_CS );
BINDING( computeImage,			IMAGE_2D_ARRAY,		8,	BIND_STATE_CS );
BINDING( computeWriteImage,		WRITE_IMAGE_BUFFER,	1,	BIND_STATE_CS );

// Post Effect Resources
BINDING( imageProcess,			CONSTANT_BUFFER,	1,						BIND_STATE_PS );
BINDING( sourceImages,			IMAGE_2D_ARRAY,		7,						BIND_STATE_PS );
BINDING( sourceCubeImages,		IMAGE_CUBE,			1,						BIND_STATE_PS );

// Raster Resources
BINDING( viewBuffer,					READ_BUFFER,		1,						BIND_STATE_ALL );
BINDING( modelBuffer,					READ_BUFFER,		1,						BIND_STATE_ALL_GFX );
BINDING( image2DArray,					IMAGE_2D_ARRAY,		MaxImageDescriptors,	BIND_STATE_ALL );
BINDING( imageCubeArray,				IMAGE_CUBE_ARRAY,	MaxImageDescriptors,	BIND_STATE_ALL );
BINDING( materialBuffer,				READ_BUFFER,		1,						BIND_STATE_ALL );
BINDING( lightBuffer,					READ_BUFFER,		1,						BIND_STATE_ALL );
BINDING( passBuffer,					READ_BUFFER,		1,						BIND_STATE_ALL_GFX );
BINDING( imageCodeArray,				IMAGE_2D_ARRAY,		MaxCodeImages,			BIND_STATE_ALL_GFX );
BINDING( imageCodeCubeArray,			IMAGE_CUBE_ARRAY,	MaxCodeImages,			BIND_STATE_ALL_GFX );
BINDING( imageStencil,					IMAGE_2D,			1,						BIND_STATE_ALL_GFX );
BINDING( nearestSampler,				IMAGE_SAMPLER,		1,						BIND_STATE_ALL );
BINDING( bilinearSamplerWrap,			IMAGE_SAMPLER,		1,						BIND_STATE_ALL );
BINDING( bilinearSamplerClampEdge,		IMAGE_SAMPLER,		1,						BIND_STATE_ALL );
BINDING( bilinearSamplerClampBorder,	IMAGE_SAMPLER,		1,						BIND_STATE_ALL );
BINDING( depthShadowSampler,			IMAGE_SAMPLER,		1,						BIND_STATE_ALL );

// Ray-tracing Resources
BINDING( tlas,							ACCELERATION_STRUCTURE,	1,					BIND_STATE_ALL_RTX );
BINDING( rtOutputImage,					WRITE_IMAGE_BUFFER,		1,					BIND_STATE_RAYGEN );
BINDING( rtVertexBuffer,				READ_BUFFER,			1,					BIND_STATE_ALL_RTX );
BINDING( rtIndexBuffer,					READ_BUFFER,			1,					BIND_STATE_ALL_RTX );
BINDING( rtSurfaceInfos,				READ_BUFFER,			1,					BIND_STATE_ALL_RTX );
BINDING( rtCodeImageArray,				IMAGE_2D_ARRAY,			MaxCodeImages,		BIND_STATE_RAYGEN );

static const ShaderBinding g_globalBindings[] =
{
	BINDING_NAME( globalsBuffer ),
	BINDING_NAME( viewBuffer ),
	BINDING_NAME( image2DArray ),
	BINDING_NAME( imageCubeArray ),
	BINDING_NAME( materialBuffer ),
	BINDING_NAME( nearestSampler ),
	BINDING_NAME( bilinearSamplerWrap ),
	BINDING_NAME( bilinearSamplerClampEdge ),
	BINDING_NAME( bilinearSamplerClampBorder ),
	BINDING_NAME( depthShadowSampler ),
};
const uint64_t bindset_global = Hash( "bindset_global" );


static const ShaderBinding g_viewBindings[] =
{
	BINDING_NAME( modelBuffer ),
};
const uint64_t bindset_view = Hash( "bindset_view" );


static const ShaderBinding g_passBindings[] =
{
	BINDING_NAME( lightBuffer ),
	BINDING_NAME( imageCodeArray ),
	BINDING_NAME( imageCodeCubeArray ),
	BINDING_NAME( imageStencil ),
};
const uint64_t bindset_pass = Hash( "bindset_pass" );


static const ShaderBinding g_particleBindings[] =
{
	BINDING_NAME( globalsBuffer ),
	BINDING_NAME( particleWriteBuffer ),
};
const uint64_t bindset_particle = Hash( "bindset_particle" );


static const ShaderBinding g_computeBindings[] =
{
	BINDING_NAME( globalsBuffer ),
	BINDING_NAME( computeImage ),
	BINDING_NAME( computeParms ),
	BINDING_NAME( computeWrite ),
};
const uint64_t bindset_compute = Hash( "bindset_compute" );


static const ShaderBinding g_imageProcessBindings[] =
{
	BINDING_NAME( sourceImages ),
	BINDING_NAME( sourceCubeImages ),
	BINDING_NAME( imageStencil ),
	BINDING_NAME( imageProcess ),
};
const uint64_t bindset_imageShader = Hash( "bindset_imageShader" );


static const ShaderBinding g_rtBindings[] =
{
	BINDING_NAME( tlas ),
	BINDING_NAME( rtOutputImage ),
	BINDING_NAME( rtVertexBuffer ),
	BINDING_NAME( rtIndexBuffer ),
	BINDING_NAME( rtSurfaceInfos ),
	BINDING_NAME( lightBuffer ),
	BINDING_NAME( rtCodeImageArray ),
};
const uint64_t bindset_rayTracing = Hash( "bindset_rayTracing" );
