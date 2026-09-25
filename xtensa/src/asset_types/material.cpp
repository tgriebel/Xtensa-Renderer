#include "material.h"
#include <Syscore/serializer.h>
#include <Syscore/systemUtils.h>
#include <gfxcore/io/serializeClasses.h>
#include "../scene/assetManager.h"
#include "../scene/assetBaker.h"
#include "../io/serializeClasses.h"
#include "../io/io.h"
#include "../globals/common.h"
#include "image.h"

bool Material::AddTexture( const uint32_t slot, const hdl_t hdl )
{
	if ( slot >= MaxMaterialTextures ) {
		return false;
	}
	if ( hdl.IsValid() == false ) {
		return false;
	}
	textures[ slot ] = hdl;
	textureBitSet |= ( 1 << slot );
	return true;
}


hdl_t Material::GetTexture( const uint32_t slot ) const
{
	if ( slot >= MaxMaterialTextures ) {
		return INVALID_HDL;
	}
	return textures[ slot ];
}


uint32_t Material::TextureCount() const
{
	uint32_t count = 0;
	uint32_t bits = textureBitSet;
	while ( bits )
	{
		bits &= ( bits - 1 );
		count++;
	}
	return count;
}


bool Material::AssignUvTransform( const uint32_t slot, const uint32_t channel, const vec2f& scale, const vec2f& offset, const float& rotationRadians )
{
	if( slot >= MaxMaterialTextures )
	{
		return false;
	}
	
	const float c = cosf( rotationRadians );
	const float s = sinf( rotationRadians );

	mat2x2f& t = uvTransforms[ slot ];
	vec2f& o = uvOffset[ slot ];

	t[ 0 ][ 0 ] = c * scale.x;
	t[ 0 ][ 1 ] = -s * scale.y;
	t[ 1 ][ 0 ] = s * scale.x;
	t[ 1 ][ 1 ] = c * scale.y;

	o[ 0 ] = offset.x;
	o[ 1 ] = offset.y;

	uvChannel[ slot ] = channel;
	
	return true;
}


void Material::GetUvTransform( const uint32_t slot, uint32_t& channel, mat2x2f& outTransform, vec2f& outOffset ) const
{
	if( slot >= MaxMaterialTextures )
	{
		outTransform = mat2x2f::Identity();
		outOffset = vec2f( 0.0f, 0.0f );
		channel = 0;
		return;
	}
	outTransform = uvTransforms[ slot ];
	outOffset = uvOffset[ slot ];
	channel = uvChannel[ slot ];
}


void Material::TranslateToGpuMaterial( gpuMaterial_t& gpuMaterial )
{
	for( uint32_t t = 0; t < MaxMaterialTextures; ++t )
	{
		GetUvTransform( t, gpuMaterial.uvChannel[ t ], gpuMaterial.uvTransform[ t ], gpuMaterial.uvOffset[ t ] );
	}

	const materialParms_t& parms = GetParms();

	gpuMaterial.albedo = vec3f( parms.albedo.r, parms.albedo.g, parms.albedo.b );
	gpuMaterial.Ks = vec3f( parms.Ks.r, parms.Ks.g, parms.Ks.b );
	gpuMaterial.Ka = vec3f( parms.Ka.r, parms.Ka.g, parms.Ka.b );
	gpuMaterial.Ke = vec3f( parms.Ke.r, parms.Ke.g, parms.Ke.b );
	gpuMaterial.Tf = vec3f( parms.Tf.r, parms.Tf.g, parms.Tf.b );
	gpuMaterial.sheenColor = vec3f( parms.sheenColor.r, parms.sheenColor.g, parms.sheenColor.b );
	gpuMaterial.opacity = parms.opacity;
	gpuMaterial.Ns = parms.Ns;
	gpuMaterial.illum = parms.illum;
	gpuMaterial.emissiveStrength = parms.emissiveStrength;
	gpuMaterial.alphaCutoff = parms.alphaCutoff;
	gpuMaterial.ior = parms.ior;
	gpuMaterial.sheenRoughness = parms.sheenRoughness;
	gpuMaterial.roughness = parms.roughness;
	gpuMaterial.metalness = parms.metalness;
	gpuMaterial.clearcoatWeight = parms.clearcoatWeight;
	gpuMaterial.clearcoatRoughness = parms.clearcoatRoughness;
	gpuMaterial.anisotropy = parms.anisotropy;
	gpuMaterial.anisotropyRotation = parms.anisotropyRotation;
	gpuMaterial.transmissionFactor = parms.transmissionFactor;

	gpuMaterial.textured = IsTextured();

	CopyExtraData( gpuMaterial.extraData, GetExtraDataByteCount() );
}


bool Material::AddShader( const drawPass_t pass, const hdl_t hdl, const uint32_t perms )
{
	const uint32_t slot = uint32_t( pass );
	if ( slot >= MaxMaterialShaders ) {
		return false;
	}
	if ( hdl.IsValid() == false ) {
		return false;
	}
	assert( pass < 16 ); // Bitset overrun

	shaders[ slot ] = hdl;
	shaderPerms[ slot ] = perms;
	shaderBitSet |= ( 1 << slot );
	return true;
}


hdl_t Material::GetShader( const drawPass_t pass ) const
{
	const uint32_t slot = uint32_t( pass );
	if ( slot >= MaxMaterialShaders ) {
		return INVALID_HDL;
	}
	return shaders[ slot ];
}


uint32_t Material::GetShaderPerms( const drawPass_t pass ) const
{
	const uint32_t slot = uint32_t( pass );
	if ( slot >= MaxMaterialShaders ) {
		return 0;
	}
	return shaderPerms[ slot ];
}


uint32_t Material::ShaderCount() const
{
	uint32_t count = 0;
	uint32_t bits = shaderBitSet;
	while ( bits )
	{
		bits &= ( bits - 1 );
		count++;
	}
	return count;
}


bool BakedMaterialLoader::Load( Asset<Material>& materialAsset )
{
	Material& material = materialAsset.Get();

	sourceFile_t matSource {};
	matSource.isBakedAsset = true;

	bakedAssetInfo_t info = {};
	const bool loadedBaked = LoadBaked( materialAsset, info, matSource, BakePath + m_assetDir, "mtl.bin" );
	if ( loadedBaked )
	{
		assert( m_assets != nullptr );

		for ( uint32_t imageIx = 0; imageIx < MaxMaterialTextures; ++imageIx )
		{
			const hdl_t imgHandle = material.GetTexture( imageIx );

			if( imgHandle == INVALID_HDL ) {
				continue;
			}
			m_assets->GetLib<Image>()->AddDeferred( imgHandle, pImgLoader_t( new BakedImageLoader( BakePath + TexturePath, "img.bin" ) ) );
		}
		return true;
	}
	return false;
}


void BakedMaterialLoader::SetAssetPath( const std::string& path )
{
	m_assetDir = path;
}


void BakedMaterialLoader::SetExtName( const std::string& ext )
{
	m_ext = ext;
}


void BakedMaterialLoader::SetAssetRef( AssetManager* assetsPtr )
{
	m_assets = assetsPtr;
}


bool MaterialLoader::Load( Asset<Material>& materialAsset )
{
	assert( m_assets != nullptr );
	return LoadMaterialObj( *m_assets, m_fileName, m_materialPath, m_texturePath, materialAsset.Get() );
}


void MaterialLoader::SetMaterialPath( const std::string& path )
{
	m_materialPath = path;
}


void MaterialLoader::SetTexturePath( const std::string& path )
{
	m_texturePath = path;
}


void MaterialLoader::SetFileName( const std::string& fileName )
{
	m_fileName = fileName;
}


void MaterialLoader::SetAssetRef( AssetManager* assetsPtr )
{
	m_assets = assetsPtr;
}

// BRDF functions are adapted from "https://google.github.io/filament/Filament.md.html#overview/physicallybasedrendering"
float GGX( float NoH, float roughness )
{
	float a = NoH * roughness;
	float k = roughness / ( 1.0f - NoH * NoH + a * a );
	return k * k * ( 1.0f / PI );
}


float SmithGGXCorrelated( float NoV, float NoL, float roughness )
{
	float a2 = roughness * roughness;
	float GGXV = NoL * sqrt( NoV * NoV * ( 1.0f - a2 ) + a2 );
	float GGXL = NoV * sqrt( NoL * NoL * ( 1.0f - a2 ) + a2 );
	return 0.5f / ( GGXV + GGXL );
}


vec3f Schlick( float u, vec3f f0 )
{
	return f0 + ( vec3f( 1.0f ) - f0 ) * pow( 1.0f - u, 5.0f );
}


float Lambert()
{
	return 1.0f / PI;
}


vec3f BrdfGGX( const vec3f& n, const vec3f& v, const vec3f& l, const Material& m )
{
	float perceptualRoughness = 1.0f;
	float f0 = 0.1f;

	vec3f h = ( v + l ).Normalize();

	float NoV = abs( Dot( n, v ) ) + 1e-5f;
	float NoL = Clamp( Dot( n, l ), 0.0f, 1.0f );
	float NoH = Clamp( Dot( n, h ), 0.0f, 1.0f );
	float LoH = Clamp( Dot( l, h ), 0.0f, 1.0f );

	// perceptually linear roughness to roughness (see parameterization)
	float roughness = perceptualRoughness * perceptualRoughness;

	float D = GGX( NoH, roughness );
	vec3f  F = Schlick( LoH, f0 );
	float V = SmithGGXCorrelated( NoV, NoL, roughness );

	// specular BRDF
	vec3f Fr = ( D * V ) * F;

	// diffuse BRDF
	vec3f Fd = vec3f( 1.0f, 0.0f, 0.0f ) * Lambert();

	return ( Fr + Fd ) * std::max( 0.0f, Dot( n, l ) ); // TODO:
}
