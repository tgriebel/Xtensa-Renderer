#include <type_traits>
#include "../asset_types/model.h"
#include "../asset_types/material.h"
#include "../asset_types/gpuProgram.h"
#include <syscore/serializer.h>

#define SERIALIZE_IMPLEMENTATIONS

static bool g_supportBaked = true;

void ToggleBakedLoading( const bool enabled )
{
	g_supportBaked = enabled;
}

bool AreBakedAssetsEnabled()
{
	return g_supportBaked;
}

#ifdef SERIALIZE_IMPLEMENTATIONS

template<size_t D, typename T, typename S>
void Serialize( Serializer* serializer, Vector<D, T, S>& v )
{
	Serializer* s = reinterpret_cast<Serializer*>( serializer );
	uint32_t length = D;
	s->Next( length );
	if ( length != D ) {
		THROW_ERROR( "Wrong vector length." );
	}
	for ( size_t i = 0; i < D; ++i ) {
		s->Next( v[ i ] );
	}
}


void SerializeStruct( Serializer* s, vertex_t& v )
{
	static_assert( sizeof( vertex_t ) == 84, "Serialization out-of-date" );
	v.pos.Serialize( s );
	v.normal.Serialize( s );
	v.tangent.Serialize( s );
	v.bitangent.Serialize( s );
	v.uv0.Serialize( s );
	v.uv1.Serialize( s );
	v.color.Serialize( s );
}


void SerializeStruct( Serializer* s, rgbaTuple_t<float>& rgba )
{
	static_assert( sizeof( rgbaTuple_t<float> ) == 16, "Serialization out-of-date" );
	s->Next( rgba.r );
	s->Next( rgba.g );
	s->Next( rgba.b );
	s->Next( rgba.a );
}


void SerializeStruct( Serializer* s, rgbTuple_t<float>& rgb )
{
	static_assert( sizeof( rgbTuple_t<float> ) == 12, "Serialization out-of-date" );
	s->Next( rgb.r );
	s->Next( rgb.g );
	s->Next( rgb.b );
}


void SerializeStruct( Serializer* s, materialParms_t& p )
{
	static_assert( sizeof( materialParms_t ) == 128, "Serialization out-of-date" );
	SerializeStruct( s, p.albedo );
	s->Next( p.opacity );
	SerializeStruct( s, p.Ka );
	s->Next( p.Ns );
	SerializeStruct( s, p.Ke );
	s->Next( p.illum );
	SerializeStruct( s, p.Ks );
	s->Next( p.emissiveStrength );
	SerializeStruct( s, p.Tf );
	s->Next( p.alphaCutoff );
	SerializeStruct( s, p.sheenColor );
	s->Next( p.ior );
	s->Next( p.sheenRoughness );
	s->Next( p.roughness ); 
	s->Next( p.metalness ); 
	s->Next( p.clearcoatWeight );
	s->Next( p.clearcoatRoughness );
	s->Next( p.anisotropy );
	s->Next( p.anisotropyRotation );
	s->Next( p.transmissionFactor );
}


void Color::Serialize( Serializer* s )
{
	uint32_t version = Version;
	s->Next( version );
	if ( version != Version ) {
		THROW_ERROR( "Wrong version number." );
	}
	SerializeStruct( s, rgba );
}


void AABB::Serialize( Serializer* s )
{
	uint32_t version = Version;
	s->Next( version );
	if ( version != Version ) {
		THROW_ERROR( "Wrong version number." );
	}
	min.Serialize( s );
	max.Serialize( s );
}


void ImageBufferInterface::Serialize( Serializer* s )
{
	uint32_t version = Version;
	s->Next( version );
	s->Next( width );
	s->Next( height );
	s->Next( layers );	
	s->Next( length );
	s->Next( bpp );
	s->Next( mipCount );

	if ( version == 5 )
	{
		s->Next( byteCount );
	}

	if ( s->GetMode() == serializeMode_t::LOAD )
	{
		imageBufferInfo_t info{};
		info.width = width;
		info.height = height;
		info.layers = layers;
		info.mipCount = mipCount > 0 ? mipCount : 1;
		info.bpp = bpp;

		const uint32_t storedLength = length; // TODO: replace with byteCount
		_Init( info );
		assert( storedLength == length );
	}

	if( version == 5 )
	{
		assert( buffer != nullptr );
		SerializeArray( s, buffer, byteCount );
	}
	else
	{
		assert( buffer != nullptr );
		SerializeArray( s, buffer, bpp * length );
	}
}


void Material::Serialize( Serializer* s )
{
	uint32_t version = Version;
	s->Next( version );
	if ( version != Version ) {
		THROW_ERROR( "Wrong version number." );
	}
	SerializeStruct( s, usage );
	SerializeStruct( s, p );
	s->Next( textureBitSet );
	s->Next( shaderBitSet );
	SerializeArray( s, uvTransforms, MaxMaterialTextures );
	SerializeArray( s, textures, MaxMaterialTextures );
	SerializeArray( s, shaders, MaxMaterialShaders );
}


void Surface::Serialize( Serializer* s )
{
	uint32_t vertexCount = 0;
	if ( s->GetMode() == serializeMode_t::LOAD )
	{
		s->Next( vertexCount );
		vertices.resize( vertexCount );
	}
	else if ( s->GetMode() == serializeMode_t::STORE )
	{
		vertexCount = static_cast<uint32_t>( vertices.size() );
		s->Next( vertexCount );
	}

	for ( uint32_t i = 0; i < vertexCount; ++i ) {
		SerializeStruct( s, vertices[ i ] );
	}

	uint32_t indexCount = 0;
	if ( s->GetMode() == serializeMode_t::LOAD )
	{
		s->Next( indexCount );
		indices.resize( indexCount );
	}
	else if ( s->GetMode() == serializeMode_t::STORE )
	{
		indexCount = static_cast<uint32_t>( indices.size() );
		s->Next( indexCount );
	}

	for ( uint32_t i = 0; i < indexCount; ++i ) {
		s->Next( indices[ i ] );
	}
	uint64_t hash = materialHdl.Get();
	s->Next( hash );
	materialHdl = hdl_t( hash );
}


void Model::Serialize( Serializer* s )
{
	uint32_t version = Version;
	s->Next( version );
	if ( version != Version ) {
		THROW_ERROR( "Wrong version number." );
	}
	bounds.Serialize( s );

	s->Next( surfCount );
	if ( s->GetMode() == serializeMode_t::LOAD ) {
		surfs.resize( surfCount );
	}

	for ( uint32_t i = 0; i < surfCount; ++i ) {
		surfs[ i ].Serialize( s );
	}
}
#endif
