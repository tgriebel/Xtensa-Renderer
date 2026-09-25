#pragma once

#include <cinttypes>
#include <GfxCore/core/common.h>
#include <GfxCore/math/vector.h>
#include <GfxCore/math/matrix.h>
#include <GfxCore/image/color.h>
#include <SysCore/handle.h>
#include "asset.h"

#define SHADER_STRUCTS_CPP
#include "../../shaders/gpuShared.h"

class Serializer;

struct lightInput_t
{
	vec3f	viewVector;
	vec3f	normal;
};

enum drawPass_t : uint32_t
{
	DRAWPASS_SHADOW,
	DRAWPASS_PREPASS,
	DRAWPASS_OPAQUE,
	DRAWPASS_SKYBOX,
	DRAWPASS_TRANS,
	DRAWPASS_DEBUG_3D,
	DRAWPASS_DEBUG_WIREFRAME,
	DRAWPASS_2D,
	DRAWPASS_DEBUG_2D,
	DRAWPASS_COUNT,

	DRAWPASS_SHADOW_BEGIN = DRAWPASS_SHADOW,
	DRAWPASS_SHADOW_END = DRAWPASS_SHADOW,
	DRAWPASS_OPAQUE_COLOR_BEGIN = DRAWPASS_OPAQUE,
	DRAWPASS_OPAQUE_COLOR_END = DRAWPASS_SKYBOX,
	DRAWPASS_MAIN_BEGIN = DRAWPASS_PREPASS,
	DRAWPASS_MAIN_END = DRAWPASS_DEBUG_WIREFRAME,
	DRAWPASS_2D_BEGIN = DRAWPASS_2D,
	DRAWPASS_2D_END = DRAWPASS_DEBUG_2D,
};

enum textureSlot_t : uint32_t
{
	TEXTURE_SLOT_0,
	TEXTURE_SLOT_1,
	TEXTURE_SLOT_2,
	TEXTURE_SLOT_3,
	TEXTURE_SLOT_4,
	TEXTURE_SLOT_5,
	TEXTURE_SLOT_6,
	TEXTURE_SLOT_7,
};


enum materialUsage_t : uint32_t
{
	MATERIAL_USAGE_UNKNOWN,
	MATERIAL_USAGE_CODE,
	MATERIAL_USAGE_GGX,
	MATERIAL_USAGE_HEIGHT_MAP,
	MATERIAL_USAGE_CUBE,
	MATERIAL_USAGE_BLINN_PHONG,
};


struct materialParms_t
{
	materialParms_t() : opacity( 1.0f ), Ns( 0.0f ), ior( 1.5f ),
						illum( 0.0f ), roughness( 1.0f ), metalness( 0.0f ),
						sheenRoughness( 0.0f ), clearcoatWeight( 0.0f ), clearcoatRoughness( 0.0f ),
						anisotropy( 0.0f ), anisotropyRotation( 0.0f ), transmissionFactor( 0.0f ),
						emissiveStrength( 1.0f ), alphaCutoff( 0.5f )
	{}

	rgb32_t		albedo;				// Diffuse or albedo
	float		opacity;			// Opacity
	rgb32_t		Ka;					// Adhoc blinn-phong ambient (OBJ shading)
	float		Ns;					// Blinn-phong shininess exponenet
	rgb32_t		Ke;					// Emissive color
	float		illum;				// Adhoc blinn-phong illuminance (OBJ shading)
	rgb32_t		Ks;					// Specular color
	float		emissiveStrength;	// Emissive strength
	rgb32_t		Tf;					// Adhoc blinn-phong illuminance (OBJ shading)
	float		alphaCutoff;		// Alpha-mask
	rgb32_t		sheenColor;			// Sheen color
	float		ior;				// Index-of-refraction
	float		sheenRoughness;				// Sheen roughness
	float		roughness;			// GGX roughness
	float		metalness;			// GGX metalness
	float		clearcoatWeight;	// GGX clearcoat intensity
	float		clearcoatRoughness;	// GGX roughness
	float		anisotropy;			// GGX anisotrophy
	float		anisotropyRotation;	// GGX anisotrophy rotation
	float		transmissionFactor;	// Transmission factor
};

class Material
{
public:
	static const uint32_t Version = 1;
	static const uint32_t MaxMaterialShaders = DRAWPASS_COUNT;

	int32_t					uploadId;
	materialUsage_t			usage;

private:
	materialParms_t			p;
	uint32_t				extraData[ MaxMaterialExtraDataBytes ];
	uint32_t				extraByteCount;
	uint16_t				textureBitSet;
	uint16_t				shaderBitSet;

	mat2x2f					uvTransforms[ MaxMaterialTextures ];
	vec2f					uvOffset[ MaxMaterialTextures ];
	uint32_t				uvChannel[ MaxMaterialTextures ];
	hdl_t					textures[ MaxMaterialTextures ];
	hdl_t					shaders[ MaxMaterialShaders ];
	uint32_t				shaderPerms[ MaxMaterialShaders ];	// Per-pass permutation bitmask (maps to shaderPermId_t)

public:
	Material() :
		textureBitSet( 0 ),
		shaderBitSet( 0 )
	{
		p.albedo = rgb32_t( 1.0f, 1.0f, 1.0f );
		for ( int i = 0; i < MaxMaterialTextures; ++i )
		{
			textures[ i ] = INVALID_HDL;
			uvTransforms[ i ] = mat2x2f::Identity();
			uvOffset[ i ] = vec2f( 0.0f, 0.0f );
			uvChannel[ i ] = 0;
		}
		for ( int i = 0; i < MaxMaterialShaders; ++i ) {
			shaders[ i ] = INVALID_HDL;
			shaderPerms[ i ] = 0;
		}
		uploadId = -1;
		usage = MATERIAL_USAGE_UNKNOWN;

		extraByteCount = 0;
		memset( extraData, 0, MaxMaterialExtraDataBytes );
	}

	inline void SetParms( const materialParms_t& parms )
	{
		p = parms;
	}

	inline void SetExtraData( void* data, const uint32_t byteCount )
	{
		extraByteCount = Min( (uint32_t)MaxMaterialExtraDataBytes, byteCount );
		memcpy( extraData, data, extraByteCount );
	}

	inline void CopyExtraData( void* outData, const uint32_t byteCount )
	{
		if( ( byteCount == 0 ) || ( extraByteCount == 0 ) ) {
			return;
		}
		memcpy( outData, extraData, Min( byteCount, extraByteCount ) );
	}

	inline uint32_t GetExtraDataByteCount() const
	{
		return extraByteCount;
	}

	inline const materialParms_t& GetParms() const
	{
		return p;
	}

	inline materialParms_t& GetParms()
	{
		return p;
	}

	inline bool IsTextured() const
	{
		return ( textureBitSet > 0 );
	}

	bool			AddTexture( const uint32_t slot, const hdl_t hdl );
	hdl_t			GetTexture( const uint32_t slot ) const;
	uint32_t		TextureCount() const;
	bool			AssignUvTransform( const uint32_t slot, const uint32_t uvChannel, const vec2f& scale, const vec2f& offset, const float& rotationRadians );
	void			GetUvTransform( const uint32_t slot, uint32_t& channel, mat2x2f& outTransform, vec2f& outOffset ) const;
	void			TranslateToGpuMaterial( gpuMaterial_t& gpuMaterial );
	bool			AddShader( const drawPass_t pass, const hdl_t hdl, const uint32_t perms = 0 );
	hdl_t			GetShader( const drawPass_t pass ) const;
	uint32_t		GetShaderPerms( const drawPass_t pass ) const;
	uint32_t		ShaderCount() const;

	void Serialize( Serializer* serializer );
};

float GGX( float NoH, float roughness );
float SmithGGXCorrelated( float NoV, float NoL, float roughness );
vec3f Schlick( float u, vec3f f0 );
float Lambert();
vec3f BrdfGGX( const vec3f& n, const vec3f& v, const vec3f& l, const Material& m );

class AssetManager;

class BakedMaterialLoader : public LoadHandler<Material>
{
private:
	std::string		m_assetDir;
	std::string		m_textureDir;
	std::string		m_bakedDir;
	std::string		m_fileName;
	std::string		m_ext;
	AssetManager*	m_assets;

	bool Load( Asset<Material>& texture );

public:
	BakedMaterialLoader() {}
	BakedMaterialLoader( AssetManager* assets, const std::string& path, const std::string& ext )
	{
		SetAssetRef( assets );
		SetAssetPath( path );
		SetExtName( ext );
	}

	void SetAssetPath( const std::string& path );
	void SetExtName( const std::string& file );
	void SetAssetRef( AssetManager* assetsPtr );
};

class MaterialLoader : public LoadHandler<Material>
{
private:
	std::string		m_materialPath;
	std::string		m_texturePath;
	std::string		m_fileName;
	AssetManager*	m_assets;

	bool Load( Asset<Material>& materialAsset );

public:
	MaterialLoader() : m_assets( nullptr ) {}

	void SetMaterialPath( const std::string& path );
	void SetTexturePath( const std::string& path );
	void SetFileName( const std::string& fileName );
	void SetAssetRef( AssetManager* assetsPtr );
};

using pMatLoader_t = Asset<Material>::loadHandlerPtr_t;
