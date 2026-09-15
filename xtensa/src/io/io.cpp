#include "io.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

#include <syscore/common.h>
#include <syscore/serializer.h>

#include <GfxCore/image/image.h>

#include "../asset_types/material.h"
#include "../asset_types/model.h"
#include "../asset_types/gpuProgram.h"
#include "../scene/assetManager.h"

#include "../asset_types/image.h"
#include "../render_core/log.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "../../external/tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb_image.h"

#define CGLTF_IMPLEMENTATION
#pragma warning( push )
#pragma warning( disable : 4996 )
#include "../../external/cgltf.h"
#pragma warning( pop )

#define STB_IMAGE_WRITE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4996) // sprintf
#include "../../external/stb_image_write.h"
#pragma warning(pop)

#define USE_MIKKT

#ifdef USE_MIKKT
#include "../scene/mtInterface.h"
#endif


bool LoadImage( const char* texturePath, const bool isLinearColor, Image& texture )
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load( texturePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );

	if( !pixels )
	{
		stbi_image_free( pixels );
		return false;
	}

	//assert( texChannels == 4 );
	imageInfo_t info = DefaultImage2dInfo( texWidth, texHeight );
	if( isLinearColor ) {
		info.fmt = imageFmt_t::IMAGE_FMT_RGBA_8_UNORM;
	}
	texture.Create( info, pixels, info.width * info.height * 4 );

	stbi_image_free( pixels );
	return true;
}


bool LoadImageHDR( const char* texturePath, Image& texture )
{
	int texWidth, texHeight, texChannels;
	float* elements = stbi_loadf( texturePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha );

	if( !elements )
	{
		stbi_image_free( elements );
		return false;
	}

	//assert( texChannels == 4 );
	imageInfo_t info = DefaultImage2dInfo( texWidth, texHeight );
	info.fmt = IMAGE_FMT_RGBA_16;

	assert( texture.cpuImage == nullptr );

	ImageBuffer<rgba16_t>* imageBuffer = new ImageBuffer<rgba16_t>( info.width, info.height, info.layers );

	const uint32_t elementCount = 4 * imageBuffer->GetPixelCount();

	uint16_t* buffer = reinterpret_cast< uint16_t* >( imageBuffer->Ptr() );
	for( uint32_t i = 0; i < elementCount; ++i ) {
		buffer[ i ] = PackFloat32( elements[ i ] );
	}

	texture.Create( info, imageBuffer );

	stbi_image_free( elements );
	return true;
}


bool LoadCubeMapImage( const char* textureBasePath, const char* ext, Image& texture )
{
	std::string paths[ 6 ] = {
		( std::string( textureBasePath ) + "_right." + ext ),
		( std::string( textureBasePath ) + "_left." + ext ),
		( std::string( textureBasePath ) + "_top." + ext ),
		( std::string( textureBasePath ) + "_bottom." + ext ),
		( std::string( textureBasePath ) + "_front." + ext ),
		( std::string( textureBasePath ) + "_back." + ext ),
	};

	int sizeBytes = 0;
	Image textures2D[ 6 ];
	for( int i = 0; i < 6; ++i )
	{
		if( LoadImage( paths[ i ].c_str(), false, textures2D[ i ] ) == false )
		{
			sizeBytes = 0;
			break;
		}
		assert( textures2D[ i ].cpuImage != nullptr );
		assert( textures2D[ i ].cpuImage->GetByteCount() > 0 );
		sizeBytes += textures2D[ i ].cpuImage->GetByteCount();
	}

	if( sizeBytes == 0 )
	{
		for( int i = 0; i < 6; ++i )
		{
			delete textures2D[ i ].cpuImage;
			textures2D[ i ].cpuImage = nullptr;
		}
		return false;
	}

	const int texWidth = textures2D[ 0 ].info.width;
	const int texHeight = textures2D[ 0 ].info.height;
	const int texChannels = textures2D[ 0 ].info.channels;

	uint8_t* bytes = new uint8_t[ sizeBytes ];

	int byteOffset = 0;
	for( int i = 0; i < 6; ++i )
	{
		if( ( texWidth != textures2D[ i ].info.width ) ||
			( texHeight != textures2D[ i ].info.height ) ||
			( texChannels != textures2D[ i ].info.channels ) )
		{
			if( bytes != nullptr ) {
				delete[] bytes;
			}
			for( int j = 0; j < 6; ++j )
			{
				delete textures2D[ i ].cpuImage;
				textures2D[ i ].cpuImage = nullptr;
			}
			return false;
		}

		memcpy( bytes + byteOffset, textures2D[ i ].cpuImage->Ptr(), textures2D[ i ].cpuImage->GetByteCount() );
		byteOffset += textures2D[ i ].cpuImage->GetByteCount();
	}

	assert( sizeBytes == byteOffset );
	imageInfo_t info = DefaultImage2dInfo( texWidth, texHeight );
	info.channels = texChannels;
	info.layers = 6;
	info.type = IMAGE_TYPE_CUBE;

	texture.Create( info, bytes, sizeBytes );

	delete[] bytes;

	return true;
}


bool WriteImage( const char* path, const Image& image )
{
	std::string fileName;
	std::string ext;
	SysCore::SplitFileName( path, fileName, ext );

	if( ext == "png" )
	{
		const int ret = stbi_write_png( path, image.info.width, image.info.height, image.info.channels, image.cpuImage->Ptr(), image.info.width * image.cpuImage->GetBpp() );
		return ret == 1;
	}
	else if( ext == "jpg" )
	{
		const int ret = stbi_write_jpg( path, image.info.width, image.info.height, image.info.channels, image.cpuImage->Ptr(), 100 );
		return ret == 1;
	}
	else if( ext == "bmp" )
	{
		const int ret = stbi_write_bmp( path, image.info.width, image.info.height, image.info.channels, image.cpuImage->Ptr() );
		return ret == 1;
	}
	else if( ext == "hdr" )
{
		const int ret = stbi_write_hdr( path, image.info.width, image.info.height, image.info.channels, ( float* )image.cpuImage->Ptr() );
		return ret == 1;
	}
	return false;
}


static Material TranslateObjMaterial( AssetManager& assets, const tinyobj::material_t& material, const std::string& texturePath )
{
	Material outMaterial;

	const bool isPbr = material.roughness || material.metallic || !material.roughness_texname.empty() || !material.metallic_texname.empty();

	struct loadInfo_t
	{
		const std::string& name;
		bool isLinear;
	};

	std::vector<loadInfo_t> supportedTextures;

	if ( isPbr )
	{
		supportedTextures.push_back( loadInfo_t{ material.diffuse_texname, false } );
		supportedTextures.push_back( loadInfo_t{ material.bump_texname, true } );
		supportedTextures.push_back( loadInfo_t{ material.roughness_texname, true } );
		supportedTextures.push_back( loadInfo_t{ material.metallic_texname, true } );
	}
	else
	{
		supportedTextures.push_back( loadInfo_t{ material.diffuse_texname, false } );
		supportedTextures.push_back( loadInfo_t{ material.bump_texname, true } );
		supportedTextures.push_back( loadInfo_t{ material.specular_texname, true } );
	}

	const uint32_t textureCount = static_cast<uint32_t>( supportedTextures.size() );
	for ( uint32_t i = 0; i < textureCount; ++i )
	{
		const std::string& name = supportedTextures[ i ].name;
		if ( name.length() == 0 ) {
			continue;
		}
		assets.GetLib<Image>()->AddDeferred( name.c_str(), pImgLoader_t( new ImageLoader( texturePath, name, supportedTextures[ i ].isLinear ) ) );
	}

	if ( material.dissolve == 1.0f )
	{
		outMaterial.AddShader( DRAWPASS_SHADOW, AssetLib<GpuProgram>::Handle( "Shadow" ) );
		outMaterial.AddShader( DRAWPASS_DEPTH, AssetLib<GpuProgram>::Handle( "Prepass" ) );
		outMaterial.AddShader( DRAWPASS_OPAQUE, AssetLib<GpuProgram>::Handle( "LitOpaque" ) );
	}
	else
	{
		outMaterial.AddShader( DRAWPASS_TRANS, AssetLib<GpuProgram>::Handle( "LitTrans" ) );
	}
	outMaterial.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLib<GpuProgram>::Handle( "Debug" ) );
	outMaterial.AddShader( DRAWPASS_DEBUG_3D, AssetLib<GpuProgram>::Handle( "DebugSolid" ) );

	if ( isPbr )
	{
		outMaterial.usage = materialUsage_t::MATERIAL_USAGE_GGX;
		outMaterial.AddTexture( GGX_ALBEDO_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 0 ].name.c_str() ) );
		outMaterial.AddTexture( GGX_NORMAL_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 1 ].name.c_str() ) );
		outMaterial.AddTexture( GGX_ROUGHNESS_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 2 ].name.c_str() ) );
		outMaterial.AddTexture( GGX_METALLIC_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 3 ].name.c_str() ) );
	}
	else
	{
		outMaterial.usage = materialUsage_t::MATERIAL_USAGE_GGX;
		outMaterial.AddTexture( GGX_ALBEDO_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 0 ].name.c_str() ) );
		outMaterial.AddTexture( GGX_NORMAL_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 1 ].name.c_str() ) );
		outMaterial.AddTexture( GGX_ROUGHNESS_MAP_SLOT, assets.GetLib<Image>()->RetrieveHdl( supportedTextures[ 2 ].name.c_str() ) );
	}

	materialParms_t& parms = outMaterial.GetParms();
	parms.albedo = rgb32_t( material.diffuse[ 0 ], material.diffuse[ 1 ], material.diffuse[ 2 ] );
	parms.Ks = rgb32_t( material.specular[ 0 ], material.specular[ 1 ], material.specular[ 2 ] );
	parms.Ka = rgb32_t( material.ambient[ 0 ], material.ambient[ 1 ], material.ambient[ 2 ] );
	parms.Ke = rgb32_t( material.emission[ 0 ], material.emission[ 1 ], material.emission[ 2 ] );
	parms.Tf = rgb32_t( material.transmittance[ 0 ], material.transmittance[ 1 ], material.transmittance[ 2 ] );
	parms.ior = material.ior;
	parms.Ns = material.shininess;
	parms.opacity = material.dissolve;
	parms.illum = static_cast<float>( material.illum );

	if ( isPbr )
	{
		parms.roughness				= material.roughness;
		parms.metalness				= material.metallic;
		parms.sheenRoughness					= material.sheen;
		parms.clearcoatWeight		= material.clearcoat_thickness;
		parms.clearcoatRoughness	= material.clearcoat_roughness;
		parms.anisotropy			= material.anisotropy;
		parms.anisotropyRotation	= material.anisotropy_rotation;
	}
	return outMaterial;
}


bool LoadMaterialObj( AssetManager& assets, const std::string& fileName, const std::string& materialPath, const std::string& texturePath, Material& material )
{
	LOG_SCOPE_SYSTEM( Asset );

	std::ifstream matStream( materialPath + fileName );
	if ( matStream.fail() == true ) {
		LogMsg( logSeverity_t::Error, "LoadMaterialFile: failed to open %s", ( materialPath + fileName ).c_str() );
		return false;
	}

	std::map<std::string, int> matMap;
	std::vector<tinyobj::material_t> materials;
	std::string warn;

	tinyobj::LoadMtl( &matMap, &materials, &matStream, nullptr, &warn );
	if ( warn.empty() == false ) {
		LogMsg( logSeverity_t::Warning, "LoadMaterialFile warning: %s", warn.c_str() );
	}

	// TODO: Load all materials. There's currently just a one-to-one relationship since adding to the asset lib only adds a single material
	material = TranslateObjMaterial( assets, materials[ 0 ], texturePath );

	return true;
}


bool LoadRawModelModelObj( AssetManager& assets, const std::string& fileName, const std::string& modelPath, const std::string& texturePath, Model& model )
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if( !tinyobj::LoadObj( &attrib, &shapes, &materials, &warn, &err, ( modelPath + fileName ).c_str(), modelPath.c_str() ) ) {
		throw std::runtime_error( warn + err );
	}

	// Add Materials
	for( const auto& objMaterial : materials )
	{
		const Material material = TranslateObjMaterial( assets, objMaterial, texturePath );
		assets.GetLib<Material>()->Add( objMaterial.name.c_str(), material );
	}

	uint32_t vertexCnt = 0;
	//model.surfs[ 0 ].vertices.reserve( attrib.vertices.size() );
	model.surfCount = 0;
	model.surfs.resize( shapes.size() );
	for( const auto& shape : shapes )
	{
		bool hasUv = true;

		std::unordered_map<vertex_t, uint32_t> uniqueVertices {};
		std::unordered_map< uint32_t, uint32_t > indexFaceCount {};

		for( const auto& index : shape.mesh.indices )
		{
			vertex_t vertex { };

			vertex.pos[ 0 ] = attrib.vertices[ 3 * index.vertex_index + 0 ];
			vertex.pos[ 1 ] = attrib.vertices[ 3 * index.vertex_index + 1 ];
			vertex.pos[ 2 ] = attrib.vertices[ 3 * index.vertex_index + 2 ];

			model.surfs[ model.surfCount ].centroid += vec3f( vertex.pos.xyz );

			model.bounds.Expand( vec3f( vertex.pos[ 0 ], vertex.pos[ 1 ], vertex.pos[ 2 ] ) );

			vertex.uv0[ 0 ] = 0.0f;
			vertex.uv0[ 1 ] = 0.0f;
			vertex.uv1[ 0 ] = 0.0f;
			vertex.uv1[ 1 ] = 0.0f;

			// This is written this way for backwards compatibility
			// GLTF is the default import format now
			// UV channel should be selected by the material, not vertex index

			hasUv = ( index.texcoord_index < 0 );

			if( index.texcoord_index >= 0 )
			{
				vertex.uv0[ 0 ] = attrib.texcoords[ 2 * index.texcoord_index + 0 ];
				vertex.uv0[ 1 ] = 1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ];
			}
			if( index.texcoord_index >= 1 )
			{
				vertex.uv1[ 0 ] = attrib.texcoords[ 2 * index.texcoord_index + 0 ];
				vertex.uv1[ 1 ] = 1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ];
			}

			vertex.normal[ 0 ] = attrib.normals[ 3 * index.normal_index + 0 ];
			vertex.normal[ 1 ] = attrib.normals[ 3 * index.normal_index + 1 ];
			vertex.normal[ 2 ] = attrib.normals[ 3 * index.normal_index + 2 ];

			vertex.color[ 0 ] = attrib.colors[ 3 * index.vertex_index + 0 ];
			vertex.color[ 1 ] = attrib.colors[ 3 * index.vertex_index + 1 ];
			vertex.color[ 2 ] = attrib.colors[ 3 * index.vertex_index + 2 ];
			vertex.color[ 3 ] = 1.0f;

			if( uniqueVertices.count( vertex ) == 0 )
			{
				model.surfs[ model.surfCount ].vertices.push_back( vertex );
				uniqueVertices[ vertex ] = static_cast< uint32_t >( model.surfs[ model.surfCount ].vertices.size() - 1 );
			}

			const uint32_t index = uniqueVertices[ vertex ];
			model.surfs[ model.surfCount ].indices.push_back( index );
			indexFaceCount[ uniqueVertices[ vertex ] ]++;
		}

		const int indexCount = static_cast< int >( model.surfs[ model.surfCount ].indices.size() );
		assert( ( indexCount % 3 ) == 0 );

#ifdef USE_MIKKT
		Surface& surface = model.surfs[ model.surfCount ];

		GenerateMikkTangents( surface.vertices, surface.indices );
#else
		// Eric Lengyel "Computing Tangent Basis Vectors for an Arbitrary Mesh"
		for( int i = 0; i < indexCount; i += 3 )
		{
			int indices[ 3 ];
			float weights[ 3 ];
			indices[ 0 ] = model.surfs[ model.surfCount ].indices[ i + 0 ];
			indices[ 1 ] = model.surfs[ model.surfCount ].indices[ i + 1 ];
			indices[ 2 ] = model.surfs[ model.surfCount ].indices[ i + 2 ];

			assert( indexFaceCount[ indices[ 0 ] ] > 0 );
			assert( indexFaceCount[ indices[ 1 ] ] > 0 );
			assert( indexFaceCount[ indices[ 2 ] ] > 0 );

			weights[ 0 ] = ( 1.0f / indexFaceCount[ indices[ 0 ] ] );
			weights[ 1 ] = ( 1.0f / indexFaceCount[ indices[ 1 ] ] );
			weights[ 2 ] = ( 1.0f / indexFaceCount[ indices[ 2 ] ] );

			vertex_t& v0 = model.surfs[ model.surfCount ].vertices[ indices[ 0 ] ];
			vertex_t& v1 = model.surfs[ model.surfCount ].vertices[ indices[ 1 ] ];
			vertex_t& v2 = model.surfs[ model.surfCount ].vertices[ indices[ 2 ] ];

			const vec3f edge0 = vec3f( v1.pos.xyz - v0.pos.xyz );
			const vec3f edge1 = vec3f( v2.pos.xyz - v0.pos.xyz );

			const vec3f faceNormal = ( v0.normal + v1.normal + v2.normal ).Normalize();
			vec3f faceTangent;
			vec3f faceBitangent;

			vec2f uvEdgeDt0;
			vec2f uvEdgeDt1;

			if( hasUv )
			{
				uvEdgeDt0 = ( v1.uv - v0.uv );
				uvEdgeDt1 = ( v2.uv - v0.uv );
			}
			else
			{
				uvEdgeDt0 = vec2f( 1.0f, 0.0f );
				uvEdgeDt1 = vec2f( 0.0f, 1.0f );
			}

			const float denom = ( uvEdgeDt0[ 0 ] * uvEdgeDt1[ 1 ] - uvEdgeDt1[ 0 ] * uvEdgeDt0[ 1 ] ) + 0.00001f;
			if( denom != 0.0f )
			{
				const float r = 1.0f / denom;
				faceTangent = ( edge0 * uvEdgeDt1[ 1 ] - edge1 * uvEdgeDt0[ 1 ] ) * r;
				faceBitangent = ( edge1 * uvEdgeDt0[ 0 ] - edge0 * uvEdgeDt1[ 0 ] ) * r;

				v0.tangent += weights[ 0 ] * faceTangent;
				v0.bitangent += weights[ 0 ] * faceBitangent;

				v1.tangent += weights[ 1 ] * faceTangent;
				v1.bitangent += weights[ 1 ] * faceBitangent;

				v2.tangent += weights[ 2 ] * faceTangent;
				v2.bitangent += weights[ 2 ] * faceBitangent;
			}
		}

		const int vertexCount = static_cast< int >( model.surfs[ model.surfCount ].vertices.size() );
		for( int i = 0; i < vertexCount; ++i )
		{
			vertex_t& v = model.surfs[ model.surfCount ].vertices[ i ];
			FlushDenorms( v.tangent );
			FlushDenorms( v.bitangent );
			FlushDenorms( v.normal );

			// Gram-Schmidt orthogonalize
			v.normal = v.normal.Normalize();
			v.tangent = v.tangent.Normalize();
			v.tangent = ( v.tangent - v.normal * Dot( v.normal, v.tangent ) ).Normalize();
			v.bitangent = v.bitangent.Normalize();

			const uint32_t signBit = ( Dot( Cross( v.tangent, v.bitangent ), v.normal ) > 0.0f ) ? 0 : 1;
			union tangentBitPack_t
			{
				struct
				{
					uint32_t signBit : 1;
					uint32_t vecBits : 31;
				};
				float value;
			};
			tangentBitPack_t packed;
			packed.value = v.tangent[ 0 ];
			packed.signBit = signBit;
			v.tangent[ 0 ] = packed.value;

			//assert( fabs( v.normal.Length() - 1.0f ) < 0.001f );
			//assert( fabs( v.tangent.Length() - 1.0f ) < 0.001f );
			//assert( fabs( v.bitangent.Length() - 1.0f ) < 0.001f );

			float tsValues[ 9 ] = { v.tangent[ 0 ], v.tangent[ 1 ], v.tangent[ 2 ],
									v.bitangent[ 0 ], v.bitangent[ 1 ], v.bitangent[ 2 ],
									v.normal[ 0 ], v.normal[ 1 ], v.normal[ 2 ] };
			mat3x3f tsMatrix = mat3x3f( tsValues );
			//assert( tsMatrix.IsOrthonormal( 0.01f ) );
		}
#endif

		model.surfs[ model.surfCount ].materialHdl = assets.GetLib<Material>()->GetDefault()->Handle();
		if( ( materials.size() > 0 ) && ( shape.mesh.material_ids.size() > 0 ) )
		{
			const int shapeMaterial = shape.mesh.material_ids[ 0 ];
			if( shapeMaterial == -1 ) {
				continue;
			}
			const hdl_t materialHdl = AssetLib<Material>::Handle( materials[ shapeMaterial ].name.c_str() );
			if( materialHdl.IsValid() ) {
				model.surfs[ model.surfCount ].materialHdl = materialHdl;
			}
		}
		++model.surfCount;
	}
	return true;
}


static bool LoadImageFromMemory( const cgltf_image& img, const bool isLinear, Image& outImage )
{
	const cgltf_buffer_view* bv = img.buffer_view;
	if ( bv == nullptr || bv->buffer == nullptr || bv->buffer->data == nullptr ) {
		return false;
	}

	const uint8_t* buffer  = static_cast<const uint8_t*>( bv->buffer->data ) + bv->offset;
	const int      bufSize = static_cast<int>( bv->size );

	int width, height, channels;
	stbi_uc* pixels = stbi_load_from_memory( buffer, bufSize, &width, &height, &channels, STBI_rgb_alpha );
	if ( pixels == nullptr ) {
		return false;
	}

	imageInfo_t info = DefaultImage2dInfo( width, height );
	if ( isLinear ) {
		info.fmt = imageFmt_t::IMAGE_FMT_RGBA_8_UNORM;
	}
	outImage.Create( info, pixels, width * height * 4 );
	stbi_image_free( pixels );
	return true;
}


static void AddGltfTexture( Material& outMaterial, AssetManager& assets,
                             const cgltf_texture_view& view, const uint32_t slot,
                             const cgltf_data* data, const std::vector<std::string>& imageKeys )
{
	if ( view.texture == nullptr || view.texture->image == nullptr ) {
		return;
	}

	const cgltf_size imgIdx = static_cast<cgltf_size>( view.texture->image - data->images );
	outMaterial.AddTexture( slot, assets.GetLib<Image>()->RetrieveHdl( imageKeys[ imgIdx ].c_str() ) );

	if( view.has_transform != 0 )
	{
		vec2f offset;
		vec2f scale;
		float rotation; // Radians - CCW

		offset.x = view.transform.offset[ 0 ];
		offset.y = view.transform.offset[ 1 ];
		scale.x = view.transform.scale[ 0 ];
		scale.y = view.transform.scale[ 1 ];
		rotation = view.transform.rotation;

		const uint32_t uvChannel = ( view.transform.has_texcoord ) ? view.transform.texcoord : 0;

		outMaterial.AssignUvTransform( slot, uvChannel, scale, offset, rotation );
	}
}


static Material TranslateGltfMaterial( AssetManager& assets, const cgltf_material& mat,
                                        const cgltf_data* data, const std::vector<std::string>& imageKeys )
{
	Material outMaterial;

	if ( mat.alpha_mode == cgltf_alpha_mode_blend )
	{
		outMaterial.AddShader( DRAWPASS_TRANS, AssetLib<GpuProgram>::Handle( "LitTrans" ) );
	}
	else
	{
		outMaterial.AddShader( DRAWPASS_SHADOW, AssetLib<GpuProgram>::Handle( "Shadow" ) );
		outMaterial.AddShader( DRAWPASS_DEPTH,  AssetLib<GpuProgram>::Handle( "Prepass" ) );
		outMaterial.AddShader( DRAWPASS_OPAQUE, AssetLib<GpuProgram>::Handle( "LitOpaque" ) );
	}
	outMaterial.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLib<GpuProgram>::Handle( "Debug" ) );
	outMaterial.AddShader( DRAWPASS_DEBUG_3D,        AssetLib<GpuProgram>::Handle( "DebugSolid" ) );

	outMaterial.usage = materialUsage_t::MATERIAL_USAGE_GGX;

	if ( mat.has_pbr_metallic_roughness )
	{
		AddGltfTexture( outMaterial, assets, mat.pbr_metallic_roughness.base_color_texture,         GGX_ALBEDO_MAP_SLOT,   data, imageKeys );
		AddGltfTexture( outMaterial, assets, mat.pbr_metallic_roughness.metallic_roughness_texture, GGX_METALLIC_MAP_SLOT, data, imageKeys );
		AddGltfTexture( outMaterial, assets, mat.pbr_metallic_roughness.metallic_roughness_texture, GGX_ROUGHNESS_MAP_SLOT, data, imageKeys );
	}

	AddGltfTexture( outMaterial, assets, mat.normal_texture,     GGX_NORMAL_MAP_SLOT,   data, imageKeys );
	AddGltfTexture( outMaterial, assets, mat.occlusion_texture,  GGX_AO_MAP_SLOT,       data, imageKeys );
	AddGltfTexture( outMaterial, assets, mat.emissive_texture,   GGX_EMISSIVE_MAP_SLOT, data, imageKeys );

	if ( mat.has_clearcoat )
	{
		AddGltfTexture( outMaterial, assets, mat.clearcoat.clearcoat_texture,           GGX_CC_MAP_SLOT,           data, imageKeys );
		AddGltfTexture( outMaterial, assets, mat.clearcoat.clearcoat_roughness_texture, GGX_CC_ROUGHNESS_MAP_SLOT, data, imageKeys );
		AddGltfTexture( outMaterial, assets, mat.clearcoat.clearcoat_normal_texture,    GGX_CC_NML_MAP_SLOT,       data, imageKeys );
	}

	if ( mat.has_sheen )
	{
		AddGltfTexture( outMaterial, assets, mat.sheen.sheen_color_texture,     GGX_SHEEN_COLOR_MAP_SLOT,     data, imageKeys );
		AddGltfTexture( outMaterial, assets, mat.sheen.sheen_roughness_texture, GGX_SHEEN_ROUGHNESS_MAP_SLOT, data, imageKeys );
	}

	if ( mat.has_anisotropy ) {
		AddGltfTexture( outMaterial, assets, mat.anisotropy.anisotropy_texture, GGX_ANISOTROPY_MAP_SLOT, data, imageKeys );
	}

	if ( mat.has_transmission ) {
		AddGltfTexture( outMaterial, assets, mat.transmission.transmission_texture, GGX_TRANSMISSION_MAP_SLOT, data, imageKeys );
	}

	materialParms_t& parms = outMaterial.GetParms();

	if ( mat.has_pbr_metallic_roughness )
	{
		parms.albedo	= rgb32_t( mat.pbr_metallic_roughness.base_color_factor[ 0 ],
		                           mat.pbr_metallic_roughness.base_color_factor[ 1 ],
		                           mat.pbr_metallic_roughness.base_color_factor[ 2 ] );
		parms.opacity	= mat.pbr_metallic_roughness.base_color_factor[ 3 ];
		parms.roughness	= mat.pbr_metallic_roughness.roughness_factor;
		parms.metalness	= mat.pbr_metallic_roughness.metallic_factor;
	}

	parms.Ke = rgb32_t( mat.emissive_factor[ 0 ], mat.emissive_factor[ 1 ], mat.emissive_factor[ 2 ] );

	if ( mat.has_ior ) {
		parms.ior = mat.ior.ior;
	}

	if ( mat.has_clearcoat )
	{
		parms.clearcoatWeight = mat.clearcoat.clearcoat_factor;
		parms.clearcoatRoughness = mat.clearcoat.clearcoat_roughness_factor;
	}

	if ( mat.has_sheen )
	{
		parms.sheenColor = rgb32_t( mat.sheen.sheen_color_factor[ 0 ],
									mat.sheen.sheen_color_factor[ 1 ],
									mat.sheen.sheen_color_factor[ 2 ] );
		parms.sheenRoughness = mat.sheen.sheen_roughness_factor;
	}

	if ( mat.has_anisotropy )
	{
		parms.anisotropy         = mat.anisotropy.anisotropy_strength;
		parms.anisotropyRotation = mat.anisotropy.anisotropy_rotation;
	}

	return outMaterial;
}


bool LoadRawModelGLTF( AssetManager& assets, const std::string& fileName, const std::string& modelPath, const std::string& texturePath, Model& model )
{
	cgltf_options options = {};
	cgltf_data* data = nullptr;

	const std::string filePath = modelPath + fileName;
	if ( cgltf_parse_file( &options, filePath.c_str(), &data ) != cgltf_result_success )
	{
		return false;
	}

	if ( cgltf_load_buffers( &options, data, filePath.c_str() ) != cgltf_result_success )
	{
		cgltf_free( data );
		return false;
	}

	// Pre-scan materials to tag which images are sRGB (base color, emissive).
	// All others (normal, roughness, metallic, occlusion, clearcoat) are linear.
	// Build a stable key for every image: uri → name → asset-name-based fallback.
	// Used consistently in both loading and material texture lookup.
	std::string modelName, modelExt;
	SysCore::SplitFileName( fileName, modelName, modelExt );

	// Pre-scan textures to find a name per image (first named texture wins).
	// Many glTF exports leave image.name empty but set texture.name, so this
	// provides a more semantic fallback than the "_img<idx>" convention.
	std::vector<const char*> textureNameForImage( data->images_count, nullptr );
	for( cgltf_size texIx = 0; texIx < data->textures_count; ++texIx )
	{
		const cgltf_texture& tex = data->textures[ texIx ];
		if( tex.image == nullptr || tex.name == nullptr ) {
			continue;
		}
		const cgltf_size imgIx = static_cast<cgltf_size>( tex.image - data->images );
		if( textureNameForImage[ imgIx ] == nullptr ) {
			textureNameForImage[ imgIx ] = tex.name;
		}
	}

	std::vector<std::string> imageKeys( data->images_count );
	for( cgltf_size imgIx = 0; imgIx < data->images_count; ++imgIx )
	{
		const cgltf_image& img = data->images[ imgIx ];

		if( img.uri != nullptr ) {
			imageKeys[ imgIx ] = img.uri;
		} else if( img.name != nullptr ) {
			imageKeys[ imgIx ] = img.name;
		} else if( textureNameForImage[ imgIx ] != nullptr ) {
			imageKeys[ imgIx ] = textureNameForImage[ imgIx ];
		} else {
			imageKeys[ imgIx ] = modelName + "_img" + std::to_string( imgIx );
		}
	}

	// Pre-scan materials to tag which images are sRGB (base color, emissive).
	// All others (normal, roughness, metallic, occlusion, clearcoat) are linear.
	std::unordered_set<const cgltf_image*> srgbImages;
	for ( cgltf_size matIx = 0; matIx < data->materials_count; ++matIx )
	{
		const cgltf_material& mat = data->materials[ matIx ];
		if ( mat.has_pbr_metallic_roughness && mat.pbr_metallic_roughness.base_color_texture.texture != nullptr ) {
			srgbImages.insert( mat.pbr_metallic_roughness.base_color_texture.texture->image );
		}
		if ( mat.emissive_texture.texture != nullptr ) {
			srgbImages.insert( mat.emissive_texture.texture->image );
		}
	}

	// Images
	for ( cgltf_size imgIx = 0; imgIx < data->images_count; ++imgIx )
	{
		const cgltf_image& img     = data->images[ imgIx ];
		const std::string& key     = imageKeys[ imgIx ];

		const bool isLinear = ( srgbImages.count( &img ) == 0 );

		if ( img.uri != nullptr )
		{
			assets.GetLib<Image>()->AddDeferred( key.c_str(), pImgLoader_t( new ImageLoader( modelPath, img.uri, isLinear ) ) );
		}
		else if ( img.buffer_view != nullptr )
		{
			Image embeddedImage;
			if ( LoadImageFromMemory( img, isLinear, embeddedImage ) ) {
				assets.GetLib<Image>()->Add( key.c_str(), embeddedImage );
			}
		}
	}

	// Materials
	for ( cgltf_size matIx = 0; matIx < data->materials_count; ++matIx )
	{
		const cgltf_material& mat = data->materials[ matIx ];
		const std::string matName = ( mat.name != nullptr ) ? mat.name : ( "gltf_material_" + std::to_string( matIx ) );
		assets.GetLib<Material>()->Add( matName.c_str(), TranslateGltfMaterial( assets, mat, data, imageKeys ) );
	}

	for ( cgltf_size meshIx = 0; meshIx < data->meshes_count; ++meshIx )
	{
		const cgltf_mesh& mesh = data->meshes[ meshIx ];

		for ( cgltf_size primIx = 0; primIx < mesh.primitives_count; ++primIx )
		{
			const cgltf_primitive& prim = mesh.primitives[ primIx ];

			if ( prim.type != cgltf_primitive_type_triangles ) {
				continue;
			}

			if ( prim.indices == nullptr ) {
				continue;
			}

			// Locate attribute accessors
			const cgltf_accessor* posAccessor      = nullptr;
			const cgltf_accessor* normalAccessor   = nullptr;
			const cgltf_accessor* tangentAccessor  = nullptr;
			const cgltf_accessor* texcoordAccessor  = nullptr;
			const cgltf_accessor* texcoord1Accessor = nullptr;
			const cgltf_accessor* colorAccessor     = nullptr;

			for ( cgltf_size attrIx = 0; attrIx < prim.attributes_count; ++attrIx )
			{
				const cgltf_attribute& attr = prim.attributes[ attrIx ];
				switch ( attr.type )
				{
					case cgltf_attribute_type_position:
					{
						posAccessor = attr.data;
					} break;

					case cgltf_attribute_type_normal:
					{
						normalAccessor = attr.data;
					} break;

					case cgltf_attribute_type_tangent:
					{
						tangentAccessor = attr.data;
					} break;

					case cgltf_attribute_type_texcoord:
					{
						if( attr.index == 0 ) {
							texcoordAccessor = attr.data;
						} else if ( attr.index == 1 ) {
							texcoord1Accessor = attr.data;
						}
					} break;

					case cgltf_attribute_type_color:
					{
						if( attr.index == 0 ) {
							colorAccessor = attr.data;
						}
					} break;

					default:
						break;
				}
			}

			if ( posAccessor == nullptr ) {
				continue;
			}

			Surface surf;

			const cgltf_size vertexCount = posAccessor->count;
			surf.vertices.resize( vertexCount );

			// Positions
			{
				std::vector<float> tmp( vertexCount * 3 );
				cgltf_accessor_unpack_floats( posAccessor, tmp.data(), vertexCount * 3 );
				for ( cgltf_size i = 0; i < vertexCount; ++i )
				{
					surf.vertices[ i ].pos = vec4f( tmp[ i * 3 + 0 ], tmp[ i * 3 + 1 ], tmp[ i * 3 + 2 ], 1.0f );
					surf.centroid += vec3f( surf.vertices[ i ].pos.xyz );
					model.bounds.Expand( vec3f( surf.vertices[ i ].pos[ 0 ], surf.vertices[ i ].pos[ 1 ], surf.vertices[ i ].pos[ 2 ] ) );
				}
			}

			// Normals
			if ( normalAccessor != nullptr )
			{
				std::vector<float> tmp( vertexCount * 3 );
				cgltf_accessor_unpack_floats( normalAccessor, tmp.data(), vertexCount * 3 );
				for ( cgltf_size i = 0; i < vertexCount; ++i )
				{
					surf.vertices[ i ].normal = vec3f( tmp[ i * 3 + 0 ], tmp[ i * 3 + 1 ], tmp[ i * 3 + 2 ] );
				}
			}

			// UVs (glTF origin is top-left, same as Vulkan — no Y-flip)
			if ( texcoordAccessor != nullptr )
			{
				std::vector<float> tmp( vertexCount * 2 );
				cgltf_accessor_unpack_floats( texcoordAccessor, tmp.data(), vertexCount * 2 );
				for ( cgltf_size i = 0; i < vertexCount; ++i )
				{
					surf.vertices[ i ].uv0 = vec2f( tmp[ i * 2 + 0 ], tmp[ i * 2 + 1 ] );
				}
			}

			if ( texcoord1Accessor != nullptr )
			{
				std::vector<float> tmp( vertexCount * 2 );
				cgltf_accessor_unpack_floats( texcoord1Accessor, tmp.data(), vertexCount * 2 );
				for ( cgltf_size i = 0; i < vertexCount; ++i )
				{
					surf.vertices[ i ].uv1 = vec2f( tmp[ i * 2 + 0 ], tmp[ i * 2 + 1 ] );
				}
			}

			// Vertex colors (vec3 or vec4)
			if ( colorAccessor != nullptr )
			{
				const cgltf_size numComponents = cgltf_num_components( colorAccessor->type );
				std::vector<float> tmp( vertexCount * numComponents );

				cgltf_accessor_unpack_floats( colorAccessor, tmp.data(), vertexCount * numComponents );

				for ( cgltf_size i = 0; i < vertexCount; ++i )
				{
					surf.vertices[ i ].color[ 0 ] = tmp[ i * numComponents + 0 ];
					surf.vertices[ i ].color[ 1 ] = tmp[ i * numComponents + 1 ];
					surf.vertices[ i ].color[ 2 ] = tmp[ i * numComponents + 2 ];
					surf.vertices[ i ].color[ 3 ] = ( numComponents == 4 ) ? tmp[ i * numComponents + 3 ] : 1.0f;
				}
			}
			else
			{
				for ( cgltf_size i = 0; i < vertexCount; ++i )
				{
					surf.vertices[ i ].color[ 0 ] = 1.0f;
					surf.vertices[ i ].color[ 1 ] = 1.0f;
					surf.vertices[ i ].color[ 2 ] = 1.0f;
					surf.vertices[ i ].color[ 3 ] = 1.0f;
				}
			}

			// Indices
			{
				const cgltf_size indexCount = prim.indices->count;
				surf.indices.resize( indexCount );

				cgltf_accessor_unpack_indices( prim.indices, surf.indices.data(), sizeof( uint32_t ), indexCount );
			}

			// Tangents
			if ( tangentAccessor != nullptr )
			{
				// glTF tangents are vec4: xyz = tangent direction, w = bitangent sign
				std::vector<float> tmp( vertexCount * 4 );

				cgltf_accessor_unpack_floats( tangentAccessor, tmp.data(), vertexCount * 4 );

				for ( cgltf_size i = 0; i < vertexCount; ++i )
				{
					const float tx   = tmp[ i * 4 + 0 ];
					const float ty   = tmp[ i * 4 + 1 ];
					const float tz   = tmp[ i * 4 + 2 ];
					const float sign = tmp[ i * 4 + 3 ];

					vertex_t& v = surf.vertices[ i ];

					v.bitangent[ 0 ] = sign * ( v.normal[ 1 ] * tz - v.normal[ 2 ] * ty );
					v.bitangent[ 1 ] = sign * ( v.normal[ 2 ] * tx - v.normal[ 0 ] * tz );
					v.bitangent[ 2 ] = sign * ( v.normal[ 0 ] * ty - v.normal[ 1 ] * tx );

					union tangentBitPack_t
					{
						struct { uint32_t signBit : 1; uint32_t vecBits : 31; };
						float value;
					};
					tangentBitPack_t packed;
					packed.value   = tx;
					packed.signBit = ( sign >= 0.0f ) ? 0x00 : 0x01;
					v.tangent[ 0 ] = packed.value;
					v.tangent[ 1 ] = ty;
					v.tangent[ 2 ] = tz;
				}
			}
			else if ( normalAccessor != nullptr )
			{
				GenerateMikkTangents( surf.vertices, surf.indices );
			}

			surf.materialHdl = assets.GetLib<Material>()->GetDefault()->Handle();

			if ( prim.material != nullptr )
			{
				const cgltf_size matIndex = static_cast<cgltf_size>( prim.material - data->materials );
				const std::string matName = ( prim.material->name != nullptr ) ? prim.material->name : ( "gltf_material_" + std::to_string( matIndex ) );
				const hdl_t matHdl = AssetLib<Material>::Handle( matName.c_str() );

				if ( matHdl.IsValid() ) {
					surf.materialHdl = matHdl;
				}
			}

			model.surfs.push_back( std::move( surf ) );
			++model.surfCount;
		}
	}

	cgltf_free( data );
	return true;
}


bool LoadModel( Model& model, const hdl_t& hdl, const std::string& bakePath, const std::string& modelPath, const std::string& ext )
{
	Serializer* s = new Serializer( MB( 8 ), serializeMode_t::LOAD );
	std::string fileName = bakePath + modelPath + hdl.String() + ext;

	if( !s->ReadFile( fileName ) ) {
		return false;
	}

	uint8_t name[ 256 ];
	uint32_t nameLength = 0;
	memset( name, 0, 256 );
	s->Next( nameLength );
	assert( nameLength < 256 );
	s->NextArray( name, nameLength );

	name[ nameLength ] = '2'; // FIXME: test

	model.Serialize( s );
	return true;
}


bool WriteModel( Asset<Model>* model, const std::string& fileName )
{
	if( model == nullptr ) {
		return false;
	}
	std::string name = model->GetName();
	Serializer* s = new Serializer( MB( 8 ), serializeMode_t::STORE );

	uint8_t buffer[ 256 ];
	assert( name.length() < 256 );
	uint32_t nameLength = static_cast< uint32_t >( name.length() );
	memcpy( buffer, name.c_str(), nameLength );
	s->Next( nameLength );
	s->NextArray( buffer, nameLength );

	model->Get().Serialize( s );
	s->WriteFile( fileName );
	return true;
}
