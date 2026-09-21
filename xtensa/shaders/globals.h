#ifndef GLOBALS_HLSL_H
#define GLOBALS_HLSL_H

#define SHADER_STRUCTS_HLSL
#include "gpuShared.h"

// ============================================================
// Constants
// ============================================================
#define AMBIENT         float4( 0.03f, 0.03f, 0.03f, 1.0f )

// ============================================================
// Resource binding macros
// ============================================================

#define IMAGE_CONSTANT_LAYOUT( S, N, TYPE, NAME )                                                                   \
    BIND_SET( S, N ) cbuffer imageShaderConstants_t                                                                 \
    {                                                                                                               \
        float4  dimensions;                                                                                         \
        uint    pass;                                                                                               \
        uint    previousImageId;                                                                                    \
        uint    level;                                                                                              \
        uint    layer;                                                                                              \
        uint    mipCount;                                                                                           \
        uint    layerCount;                                                                                         \
        uint    viewId;																								\
        uint    _sc_pad1;                                                                                           \
        TYPE    NAME;                                                                                               \
    };

#define CONSTANT_LAYOUT( S, N, TYPE, NAME )				BIND_SET( S, N ) cbuffer _ShaderConstants { TYPE NAME; };

#define MODEL_LAYOUT( S, N )                            BIND_SET( S, N ) StructuredBuffer<gpuSurface_t> surfaces;

#define GLOBALS_LAYOUT( S, N )                          BIND_SET( S, N ) ConstantBuffer<gpuGlobals_t> globals;

#define VIEW_LAYOUT( S, N )                             BIND_SET( S, N ) StructuredBuffer<gpuView_t> views;

#define READ_BUFFER_LAYOUT( S, N, TYPE, NAME )          BIND_SET( S, N ) StructuredBuffer<TYPE> NAME;

#define WRITE_BUFFER_LAYOUT( S, N, TYPE, NAME )         BIND_SET( S, N ) RWStructuredBuffer<TYPE> NAME;

#define WRITE_IMAGE_LAYOUT( S, N, TEXTYPE, NAME )       BIND_SET( S, N ) TEXTYPE NAME;

#define SAMPLER( S, N, NAME )							BIND_SET( S, N ) SamplerState NAME;

#define SAMPLER_COMP( S, N, NAME )						BIND_SET( S, N ) SamplerComparisonState NAME;

#define SAMPLER_2D_LAYOUT( S, N )                       BIND_SET( S, N ) Texture2D globalTextures[];

#define SAMPLER_CUBE_LAYOUT( S, N )                     BIND_SET( S, N ) TextureCube globalCubemaps[];

#define CODE_IMAGE_LAYOUT( S, N, TEXTYPE )              BIND_SET( S, N ) TEXTYPE localTextures[];

#define CODE_IMAGE_CUBE_LAYOUT( S, N )                  BIND_SET( S, N ) TextureCube localCubemaps[];

#define STENCIL_LAYOUT( S, N, TEXTYPE )                 BIND_SET( S, N ) TEXTYPE stencilImage;

#define MATERIAL_LAYOUT( S, N )                         BIND_SET( S, N ) StructuredBuffer<gpuMaterial_t> materials;

#define LIGHT_LAYOUT( S, N )                            BIND_SET( S, N ) StructuredBuffer<gpuLight_t> lights;

#define PASS_LAYOUT( S, N )                             BIND_SET( S, N ) StructuredBuffer<gpuPass_t> passData;

#define MATERIAL_PUSH_CONSTANTS                         BIND_INLINE gpuPushConstants_t pushConstants;

#define RT_ACCELERATION_STRUCTURE( S, N, NAME )			BIND_SET( S, N ) RaytracingAccelerationStructure NAME;

#define RT_OUTPUT( S, N, NAME )							BIND_SET( S, N ) RWTexture2D<float4> NAME;

// ============================================================
// Compound bind macros
// ============================================================

#define GLOBAL_BINDS( SET )                                                                                         \
                                                        GLOBALS_LAYOUT( SET, 0 )                                    \
                                                        VIEW_LAYOUT( SET, 1 )                                       \
                                                        SAMPLER_2D_LAYOUT( SET, 2 )                                 \
                                                        SAMPLER_CUBE_LAYOUT( SET, 3 )                               \
                                                        MATERIAL_LAYOUT( SET, 4 )									\
                                                        SAMPLER( SET, 5, nearestSampler )							\
                                                        SAMPLER( SET, 6, bilinearSamplerWrap )						\
														SAMPLER( SET, 7, bilinearSamplerClampEdge )					\
                                                        SAMPLER( SET, 7, bilinearSamplerClampBorder );				\
                                                        SAMPLER_COMP( SET, 9, depthShadowSampler );

#define VIEW_BINDS( SET )                               MODEL_LAYOUT( SET, 0 )

#define PASS_BINDS( SET, TEXTYPE )                                                                                  \
                                                        LIGHT_LAYOUT( SET, 0 )                                      \
                                                        CODE_IMAGE_LAYOUT( SET, 1, TEXTYPE )                        \
                                                        CODE_IMAGE_CUBE_LAYOUT( SET, 2 )                            \
                                                        STENCIL_LAYOUT( SET, 3, TEXTYPE )

// ============================================================
// Vertex shader I/O
// ============================================================

struct vsToPsInterpolators
{
					float4 pos						: SV_Position;
	BIND_SLOT( 0 )	float4 color					: COLOR0;
	BIND_SLOT( 1 )	float3 normal					: NORMAL;
	BIND_SLOT( 2 )	float3 tangent					: TEXCOORD2;
	BIND_SLOT( 3 )	float3 bitangent				: TEXCOORD3;
	BIND_SLOT( 4 )	float3 TBN2						: TEXCOORD4;
	BIND_SLOT( 5 )	float2 uv0						: TEXCOORD5;
	BIND_SLOT( 6 )	float2 uv1						: TEXCOORD6;
	BIND_SLOT( 7 )	float3 objectPosition			: TEXCOORD7;
	BIND_SLOT( 8 )	float4 clipPosition				: TEXCOORD8;
	BIND_SLOT( 9 )	float4 prevClipPosition			: TEXCOORD9;
	BIND_SLOT( 10 )	float4 worldPosition			: TEXCOORD10;
	BIND_SLOT( 11 )	nointerpolation uint objectId	: TEXCOORD11;
};

#define VS_LAYOUT_STANDARD( TEXTYPE )                                                                               \
                                                        GLOBAL_BINDS( 0 )                                           \
                                                        VIEW_BINDS( 1 )                                             \
                                                        PASS_BINDS( 2, TEXTYPE )                                    \
                                                        MATERIAL_PUSH_CONSTANTS

// ============================================================
// Pixel shader I/O
// ============================================================

struct psOutput_t
{
    float4 outColor : SV_Target0;
#ifdef USE_MRT
	float4 outColor1 : SV_Target1;
	float4 outColor2 : SV_Target2;
#endif
};

#define PS_LAYOUT_STANDARD( TEXTYPE )                                                                               \
                                                        GLOBAL_BINDS( 0 )                                           \
                                                        VIEW_BINDS( 1 )                                             \
                                                        PASS_BINDS( 2, TEXTYPE )                                    \
                                                        MATERIAL_PUSH_CONSTANTS

#define PS_LAYOUT_IMAGE_SHADER( TEXTYPE, USERTYPE )                                                                \
                                                        GLOBAL_BINDS( 0 )                                           \
                                                        CODE_IMAGE_LAYOUT( 1, 0, TEXTYPE )                          \
                                                        CODE_IMAGE_CUBE_LAYOUT( 1, 1 )                              \
                                                        STENCIL_LAYOUT( 1, 2, TEXTYPE )                             \
                                                        IMAGE_CONSTANT_LAYOUT( 1, 3, USERTYPE, imageProcess )
#endif // GLOBALS_HLSL_H
