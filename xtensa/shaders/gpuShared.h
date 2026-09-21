#ifndef GPU_STRUCTS_HLSL_H
#define GPU_STRUCTS_HLSL_H

// This file is shared between C++ and HLSL.
// The languages are syntactically similar so basic things like structs can be shared with some finesse
// It's a common problem keeping parallel structs in sync; this resolves that without needing a reflection system

// `SHADER_STRUCTS_CPP` and `SHADER_STRUCTS_HLSL` can be used where language differ
// Alignment, minimum struct sizes, padding need to be considered--always. Stick to 16-byte alignment generally

#ifdef SHADER_STRUCTS_CPP
#include <cassert>
#define float4x4 mat4x4f
#define float3x3 mat3x3f
#define float2x2 mat2x2f
#define float2x4 mat2x4f
#define float4 vec4f
#define float3 vec3f
#define float2 vec2f
#define uint uint32_t
#define int int32_t

static_assert( sizeof( mat4x4f ) == 64 );
static_assert( sizeof( mat3x3f ) == 36 );
static_assert( sizeof( mat2x2f ) == 16 );
static_assert( sizeof( mat2x4f ) == 32 );
static_assert( sizeof( vec4f ) == 16 );
static_assert( sizeof( vec3f ) == 12 );
static_assert( sizeof( vec2f ) == 8 );

#define BIND_SLOT( x )
#define SEMANTIC( x )
#define row_major
#endif


#ifdef SHADER_STRUCTS_HLSL
#define BIND_SLOT( x )		[[vk::location( x )]]
#define BIND_SET( S, N )    [[vk::binding( N, S )]]
#define BIND_INLINE         [[vk::push_constant]]
#define SEMANTIC( x ) : x
#endif


#define MaxLights					128
#define MaxMaterials				256
#define MaxViews					15
#define MaxSurfaces					1000
#define MaxMaterialTextures			16
#define MaxMaterialExtraDataBytes	256
#define MaxTextureUVs				2

enum ggxTextureSlot_t
{
	GGX_ALBEDO_MAP_SLOT				= 0,	// sRGB
	GGX_NORMAL_MAP_SLOT				= 1,	// linear
	GGX_ROUGHNESS_MAP_SLOT			= 2,	// linear
	GGX_METALLIC_MAP_SLOT			= 3,	// linear
	GGX_AO_MAP_SLOT					= 4,	// linear
	GGX_EMISSIVE_MAP_SLOT			= 5,	// sRGB
	GGX_CC_MAP_SLOT					= 6,	// linear
	GGX_CC_ROUGHNESS_MAP_SLOT		= 7,	// linear
	GGX_CC_NML_MAP_SLOT				= 8,	// linear
	GGX_SHEEN_COLOR_MAP_SLOT		= 9,	// sRGB
	GGX_SHEEN_ROUGHNESS_MAP_SLOT	= 10,	// linear
	GGX_ANISOTROPY_MAP_SLOT			= 11,	// linear
	GGX_TRANSMISSION_MAP_SLOT		= 12,	// linear
};


enum blinnPhongTextureSlot_t
{
	BLINN_PHONG_COLOR_MAP_SLOT		= 0,	// sRGB
	BLINN_PHONG_NORMAL_MAP_SLOT		= 1,	// linear
	BLINN_PHONG_SPEC_MAP_SLOT		= 2,	// linear — specular color
	BLINN_PHONG_GLOSS_MAP_SLOT		= 3,	// linear — per-pixel shininess (Ns)
	BLINN_PHONG_EMISSIVE_MAP_SLOT	= 4,	// sRGB
};


enum cubeTextureSlot_t
{
	CUBE_FRONT_MAP_SLOT				= 0,
	CUBE_BACK_MAP_SLOT				= 1,
	CUBE_TOP_MAP_SLOT				= 2,
	CUBE_BOTTOM_MAP_SLOT			= 3,
	CUBE_RIGHT_MAP_SLOT				= 4,
	CUBE_LEFT_MAP_SLOT				= 5,
};


enum hgtTextureSlot_t
{
	HGT_HEIGHT_MAP_SLOT					= 0,
	HGT_COLOR_MAP_SLOT0					= 1,
	HGT_COLOR_MAP_SLOT1					= 2,
};


enum gpuDebugLightingMode_t : uint
{
	DEBUG_NONE,
	DEBUG_ALBEDO,
	DEBUG_ROUGHNESS,
	DEBUG_METALLIC,
	DEBUG_TBN_NORMAL,
	DEBUG_NORMAL,
	DEBUG_INPUT_UV,
	DEBUG_EMISSIVE,
	DEBUG_SHEENCOLOR,
	DEBUG_SHEENROUGHNESS,
	DEBUG_AO,
	DEBUG_BRDF_LUT,
	DEBUG_POSITION,
	DEBUG_LIGHTING_COUNT,
};


struct gpuGlobals_t
{
	float4	time;
	float4	generic;
	float4	shadowParms;
	float4	toneMapTint;
	float4	bloom;
	float4	exposure;
	float4	exposure2;
	float4	dof;
	float4	chromaticAberration;
	uint	numSamples;
	uint	whiteId;
	uint	blackId;
	uint	defaultAlbedoId;
	uint	defaultNormalId;
	uint	defaultRoughnessId;
	uint	defaultMetalId;
	uint	defaultImageId;
	uint	brdfLutId;
	uint	isTextured;
	uint	shadow2dCount;
	uint	shadowCubeCount;
	uint	textureCount;
	uint	materialCount;
	uint	useDiffuseIBL;
	uint	useSpecularIBL;
	gpuDebugLightingMode_t debugLightingMode;
};


struct gpuMaterial_t
{
	float3				albedo;
	float				opacity;
	float3				Ka;
	float				Ns;
	float3				Ke;
	float				illum;
	float3				Ks;
	float				emissiveStrength;
	float3				Tf;
	float				alphaCutoff;
	float3				sheenColor;
	float				ior;
	float				sheenRoughness;
	float				roughness;
	float				metalness;
	float				clearcoatWeight;
	float				clearcoatRoughness;
	float				anisotropy;
	float				anisotropyRotation;
	float				transmissionFactor;
	// 128 bytes
	uint				textured;
	uint				pad[ 3 ];
	// 144 bytes
	int					textureId[ MaxMaterialTextures ];
	// 208 bytes
	float2x2			uvTransform[ MaxMaterialTextures ];
	// 464 bytes
	float2				uvOffset[ MaxMaterialTextures ];
	// 592 bytes
	uint				uvChannel[ MaxMaterialTextures ];
	// 656
	uint				extraData[ MaxMaterialExtraDataBytes / 4 ]; // HLSL doesn't have an 8-bit type
};
#ifdef SHADER_STRUCTS_CPP
static_assert( sizeof( gpuMaterial_t ) == 912 );
#endif


struct gpuLight_t
{
	float4 lightPos;
	float4 intensity;
	float4 lightDir;
	uint   shadowViewId;
	uint   pad0;
	uint   pad1;
	uint   pad2;
};


struct gpuView_t
{
	float4x4	viewMat;
	float4x4	projMat;
	float4x4	invProjMat;
	float4x4	viewProjMat;
	float4x4	prevViewProjMat;
	float4		dimensions;
	float3		viewOrigin;
	uint		numLights;
	uint		skyboxCubeId;
};


struct gpuPass_t
{
	uint codeImageCount;
};


struct gpuSurface_t
{
	float4x4	model;
	float4x4	prevModel;
	uint		diffuseIblCubeId;
	uint		envCubeId;
	uint		pad[ 14 ];
};


struct gpuParticle_t
{
	float2	position;
	float2	velocity;
	float4	color;
};


struct gpuPushConstants_t
{
	uint objectId;
	uint materialId;
	uint viewId;
};


// Per-instance data (i.e. TLAS instance)
// RT analogue to `gpuSurface_t`
struct gpuRtSurface_t
{
	uint	vertexOffset;
	uint	firstIndex;
	uint	materialId;
	uint	diffuseIblCubeId;
	uint	envCubeId;
	uint	pad0;
};


struct vsInput_t
{ 
	BIND_SLOT( 0 ) float4 inPosition	SEMANTIC( POSITION );
	BIND_SLOT( 1 ) float4 inColor		SEMANTIC( COLOR0 );
	BIND_SLOT( 2 ) float3 inNormal		SEMANTIC( NORMAL );
	BIND_SLOT( 3 ) float3 inTangent		SEMANTIC( TANGENT );
	BIND_SLOT( 4 ) float3 inBitangent	SEMANTIC( BINORMAL );
	BIND_SLOT( 5 ) float2 uv0			SEMANTIC( TEXCOORD0 );
	BIND_SLOT( 6 ) float2 uv1			SEMANTIC( TEXCOORD1 );
};


struct rtVertex_t
{
	float4	position;	// offset  0, 16 bytes
	float4	color;		// offset 16, 16 bytes
	float3	normal;		// offset 32, 12 bytes
	float3	tangent;	// offset 44, 12 bytes
	float3	bitangent;	// offset 56, 12 bytes
	float4	uv;			// offset 68, 16 bytes
	// stride: 84 bytes
};


#ifdef SHADER_STRUCTS_CPP
#undef float4x4
#undef float3x3
#undef float2x2
#undef float2x4
#undef float4
#undef float3
#undef float2
#undef uint
#undef int
#undef BIND_SLOT
#undef SEMANTIC
#undef row_major
#endif

#endif
