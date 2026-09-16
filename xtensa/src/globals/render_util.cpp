#include "render_util.h"

#include "common.h"
#include "io.h"
#include <gfxcore/primitives/geoBuilder.h>
#include "../scene/assetManager.h"
#include "../asset_types/gpuProgram.h"
#include "../asset_types/image.h"
#include "../asset_types/material.h"
#include "../asset_types/model.h"
#include "../asset_types/assetLib.h"

extern AssetManager g_assets;


mat4x4f MatrixFromVector( const vec3f& v )
{
	vec3f up = vec3f( 0.0f, 0.0f, 1.0f );
	const vec3f u = v.Normalize();
	if ( Dot( v, up ) > 0.99999f ) {
		up = vec3f( 0.0f, 1.0f, 0.0f );
	}
	const vec3f left = Cross( u, up ).Reverse();

	const float values[ 16 ] = {	up[ 0 ], up[ 1 ], up[ 2 ], 0.0f,
									left[ 0 ], left[ 1 ], left[ 2 ], 0.0f,
									v[ 0 ], v[ 1 ], v[ 2 ], 0.0f,
									0.0f, 0.0f, 0.0f, 1.0f };

	return mat4x4f( values );
}


void MatrixToEulerZYX( const mat4x4f& m, float& xDegrees, float& yDegrees, float& zDegrees )
{
	if( m[2][0] < 1.0f )
	{
		if( m[2][0] >= -1.0f )
		{
			yDegrees = asin( -m[2][0] );
			zDegrees = atan2( m[1][0], m[0][0] );
			xDegrees = atan2( m[2][1], m[2][2] );
		}
		else
		{
			yDegrees = 1.570795f;
			zDegrees = -atan2( -m[1][2], m[1][1] );
			xDegrees = 0.0f;
		}
	}
	else
	{
		yDegrees = -1.570795f / 2.0f;
		zDegrees = atan2( -m[1][2], m[1][1] );
		xDegrees = 0.0f;
	}
	xDegrees = Degrees( xDegrees );
	yDegrees = Degrees( yDegrees );
	zDegrees = Degrees( zDegrees );
}


static void CopyGeoBuilderResult( const GeoBuilder& gb, Surface& surf, AABB& bounds )
{
	surf.vertices.reserve( gb.vb.size() );
	for ( const GeoBuilder::vertex_t& v : gb.vb )
	{
		vertex_t vert;
		vert.pos = vec4f( v.pos[ 0 ], v.pos[ 1 ], v.pos[ 2 ], 0.0f );
		vert.color = Color( v.color[ 0 ], v.color[ 1 ], v.color[ 2 ], v.color[ 3 ] );
		vert.normal = vec3f( v.normal[ 0 ], v.normal[ 1 ], v.normal[ 2 ] );
		vert.tangent = vec3f( v.tangent[ 0 ], v.tangent[ 1 ], v.tangent[ 2 ] );
		vert.bitangent = vec3f( v.bitangent[ 0 ], v.bitangent[ 1 ], v.bitangent[ 2 ] );
		vert.uv0 = vec2f( v.texCoord[ 0 ], v.texCoord[ 1 ] );
		vert.uv1 = vec2f( 0.0f, 0.0f );

		surf.vertices.push_back( vert );
		bounds.Expand( vert.pos.xyz );
	}

	surf.indices.reserve( gb.ib.size() );
	for ( uint32_t index : gb.ib )
	{
		surf.indices.push_back( index );
	}
}


bool SkyBoxLoader::Load( Asset<Model>& modelAsset )
{
	Model& model = modelAsset.Get();

	const float cellSize = 2.0f;
	const uint32_t width = 1;
	const uint32_t height = 1;

	const GeoBuilder::winding_t winding = GeoBuilder::WINDING_COUNTER_CLOCKWISE;

	GeoBuilder::planeInfo_t info[ 6 ];
	info[ 0 ].gridSize = vec2f( cellSize );
	info[ 0 ].subDivisionsX = width;
	info[ 0 ].subDivisionsY = height;
	info[ 0 ].uvDy = vec2f( 0.0f, -1.0f );
	info[ 0 ].uvOffset = vec2f( 0.0f, 1.0f );
	info[ 0 ].origin = vec3f( 1.0f, 0.0f, 0.0f );
	info[ 0 ].up = vec3f( 0.0f, 0.0f, 1.0f );
	info[ 0 ].normal = vec3f( 1.0f, 0.0f, 0.0f );
	info[ 0 ].side = Cross( info[ 0 ].normal, info[ 0 ].up ).Normalize();
	info[ 0 ].winding = winding;

	info[ 1 ].gridSize = vec2f( cellSize );
	info[ 1 ].subDivisionsX = width;
	info[ 1 ].subDivisionsY = height;
	info[ 1 ].uvDy = vec2f( 0.0f, -1.0f );
	info[ 1 ].uvOffset = vec2f( 0.0f, 1.0f );
	info[ 1 ].origin = vec3f( 0.0f, -1.0f, 0.0f );
	info[ 1 ].up = vec3f( 0.0f, 0.0f, 1.0f );
	info[ 1 ].normal = vec3f( 0.0f, -1.0f, 0.0f );
	info[ 1 ].side = Cross( info[ 1 ].normal, info[ 1 ].up ).Normalize();
	info[ 1 ].winding = winding;

	info[ 2 ].gridSize = vec2f( cellSize );
	info[ 2 ].subDivisionsX = width;
	info[ 2 ].subDivisionsY = height;
	info[ 2 ].uvDy = vec2f( 0.0f, -1.0f );
	info[ 2 ].uvOffset = vec2f( 0.0f, 1.0f );
	info[ 2 ].origin = vec3f( -1.0f, 0.0f, 0.0f );
	info[ 2 ].up = vec3f( 0.0f, 0.0f, 1.0f );
	info[ 2 ].normal = vec3f( -1.0f, 0.0f, 0.0f );
	info[ 2 ].side = Cross( info[ 2 ].normal, info[ 2 ].up ).Normalize();
	info[ 2 ].winding = winding;

	info[ 3 ].gridSize = vec2f( cellSize );
	info[ 3 ].subDivisionsX = width;
	info[ 3 ].subDivisionsY = height;
	info[ 3 ].uvDy = vec2f( 0.0f, -1.0f );
	info[ 3 ].uvOffset = vec2f( 0.0f, 1.0f );
	info[ 3 ].origin = vec3f( 0.0f, 1.0f, 0.0f );
	info[ 3 ].up = vec3f( 0.0f, 0.0f, 1.0f );
	info[ 3 ].normal = vec3f( 0.0f, 1.0f, 0.0f );
	info[ 3 ].side = Cross( info[ 3 ].normal, info[ 3 ].up ).Normalize();
	info[ 3 ].winding = winding;

	info[ 4 ].gridSize = vec2f( cellSize );
	info[ 4 ].subDivisionsX = width;
	info[ 4 ].subDivisionsY = height;
	info[ 4 ].uvDy = vec2f( 0.0f, -1.0f );
	info[ 4 ].uvOffset = vec2f( 0.0f, 1.0f );
	info[ 4 ].origin = vec3f( 0.0f, 0.0f, 1.0f );
	info[ 4 ].up = vec3f( -1.0f, 0.0f, 0.0f );
	info[ 4 ].normal = vec3f( 0.0f, 0.0f, 1.0f );
	info[ 4 ].side = Cross( info[ 4 ].normal, info[ 4 ].up ).Normalize();
	info[ 4 ].winding = winding;

	info[ 5 ].gridSize = vec2f( cellSize );
	info[ 5 ].subDivisionsX = width;
	info[ 5 ].subDivisionsY = height;
	info[ 5 ].uvDx = vec2f( -1.0f, 0.0f );
	info[ 5 ].uvOffset = vec2f( 1.0f, 0.0f );
	info[ 5 ].origin = vec3f( 0.0f, 0.0f, -1.0f );
	info[ 5 ].up = vec3f( -1.0f, 0.0f, 0.0f );
	info[ 5 ].normal = vec3f( 0.0f, 0.0f, -1.0f );
	info[ 5 ].side = Cross( info[ 5 ].normal, info[ 5 ].up ).Normalize();
	info[ 5 ].winding = winding;

	GeoBuilder gb;
	for ( int i = 0; i < 6; ++i ) {
		gb.AddPlaneSurf( info[ i ] );
	}

	model.surfCount = 1;
	model.surfs.resize( model.surfCount );
	CopyGeoBuilderResult( gb, model.surfs[ 0 ], model.bounds );

	model.surfs[ 0 ].materialHdl = AssetLib<Material>::Handle( "_sky" );

	return true;
}


bool TerrainLoader::Load( Asset<Model>& modelAsset )
{
	Model& model = modelAsset.Get();

	GeoBuilder::planeInfo_t info;
	info.gridSize = vec2f( cellSize );
	info.subDivisionsX = width;
	info.subDivisionsY = height;
	info.normal = vec3f( 0.0f, 0.0f, 1.0f );
	info.up = vec3f( 1.0f, 0.0f, 0.0f );
	info.side = Cross( info.normal, info.up ).Normalize().Reverse();
	info.centerAtOrigin = true;
	info.flipUv = true;
	//info.uvDy = -1.0f;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	model.surfCount = 1;
	model.surfs.resize( model.surfCount );
	CopyGeoBuilderResult( gb, model.surfs[ 0 ], model.bounds );

	model.surfs[ 0 ].materialHdl = handle;

	return true;
}


bool WaterLoader::Load( Asset<Model>& modelAsset )
{
	Model& model = modelAsset.Get();

	const float gridSize = 10.f;
	const uint32_t width = 1;
	const uint32_t height = 1;
	const vec2f uvs[] = { vec2f( 0.0f, 0.0f ), vec2f( 1.0f, 1.0f ) };

	GeoBuilder::planeInfo_t info;
	info.gridSize = vec2f( 10.0f );
	info.subDivisionsX = 1;
	info.subDivisionsY = 1;
	info.origin = vec3f( 0.5f * info.gridSize[ 0 ] * info.subDivisionsX, 0.5f * info.gridSize[ 1 ] * info.subDivisionsY, -0.15f );

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	model.surfCount = 1;
	model.surfs.resize( model.surfCount );
	CopyGeoBuilderResult( gb, model.surfs[ 0 ], model.bounds );

	model.surfs[ 0 ].materialHdl = AssetLib<Material>::Handle( "WATER" );

	return true;
}


void CreateQuadSurface2D( Model& outModel, const std::string& materialName, vec2f& origin, vec2f& size )
{
	GeoBuilder::planeInfo_t info;
	info.gridSize = size;
	info.subDivisionsX = 1;
	info.subDivisionsY = 1;
	info.origin = vec3f( origin[ 0 ], origin[ 1 ], 0.0f );
	info.normal = vec3f( 0.0f, 0.0f, -1.0f );
	info.up = vec3f( 0.0f, 1.0f, 0.0f );
	info.side = Cross( info.normal, info.up ).Normalize();
	info.winding = GeoBuilder::WINDING_COUNTER_CLOCKWISE;
	//info.flipUv = true;

	GeoBuilder gb;
	gb.AddPlaneSurf( info );

	outModel.surfCount = 1;
	outModel.surfs.resize( outModel.surfCount );
	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ], outModel.bounds );

	outModel.surfs[ 0 ].vertices[ 0 ].uv0 = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 1 ].uv0 = vec2f( 1.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 2 ].uv0 = vec2f( 0.0f, 1.0f );
	outModel.surfs[ 0 ].vertices[ 3 ].uv0 = vec2f( 1.0f, 1.0f );

	for( int i = 0; i < 4; ++i ) {
		outModel.surfs[ 0 ].vertices[ i ].uv1 = vec2f( 0.0f, 0.0f );
	}

	outModel.surfs[ 0 ].materialHdl = AssetLib<Material>::Handle( materialName.c_str() );
}


void CreateBoxSurface( Model& outModel, const std::string& materialName, const vec3f& origin, const float size )
{
	GeoBuilder gb;
	gb.AddBoxSurf( origin, size );

	outModel.surfCount = 1;
	outModel.surfs.resize( outModel.surfCount );
	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ], outModel.bounds );

	outModel.surfs[ 0 ].vertices[ 0 ].uv0 = vec2f( 0.0f, 0.0f );

	outModel.surfs[ 0 ].vertices[ 0 ].uv0 = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 1 ].uv0 = vec2f( 1.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 2 ].uv0 = vec2f( 0.0f, 1.0f );
	outModel.surfs[ 0 ].vertices[ 3 ].uv0 = vec2f( 1.0f, 1.0f );
	outModel.surfs[ 0 ].vertices[ 4 ].uv0 = vec2f( 0.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 5 ].uv0 = vec2f( 1.0f, 0.0f );
	outModel.surfs[ 0 ].vertices[ 6 ].uv0 = vec2f( 0.0f, 1.0f );
	outModel.surfs[ 0 ].vertices[ 7 ].uv0 = vec2f( 1.0f, 1.0f );

	for( int i = 0; i < 8; ++i ) {
		outModel.surfs[ 0 ].vertices[ i ].uv1 = vec2f( 0.0f, 0.0f );
	}

	outModel.surfs[ 0 ].materialHdl = AssetLib<Material>::Handle( materialName.c_str() );
}


void CreateSphereSurface( Model& outModel, const std::string& materialName, const vec3f& origin, const float radius )
{
	GeoBuilder::sphereInfo_t info;
	info.origin = origin;
	info.radius = radius;
	info.winding = GeoBuilder::WINDING_COUNTER_CLOCKWISE;

	GeoBuilder gb;
	gb.AddSphereSurf( info );

	outModel.surfCount = 1;
	outModel.surfs.resize( outModel.surfCount );
	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ], outModel.bounds );

	outModel.surfs[ 0 ].materialHdl = AssetLib<Material>::Handle( materialName.c_str() );
}


void CreateTorusSurface( Model& outModel, const std::string& materialName, const vec3f& origin, const float innerRadius, const float outerRadius )
{
	GeoBuilder::torusInfo_t info;
	info.origin      = origin;
	info.innerRadius = innerRadius;
	info.outerRadius = outerRadius;

	GeoBuilder gb;
	gb.AddTorusSurf( info );

	outModel.surfCount = 1;
	outModel.surfs.resize( outModel.surfCount );
	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ], outModel.bounds );

	outModel.surfs[ 0 ].materialHdl = AssetLib<Material>::Handle( materialName.c_str() );
}


void CreateCapsuleSurface( Model& outModel, const std::string& materialName, const vec3f& origin, const float radius, const float cylinderHeight )
{
	GeoBuilder::capsuleInfo_t info;
	info.origin        = origin;
	info.radius        = radius;
	info.cylinderHeight = cylinderHeight;

	GeoBuilder gb;
	gb.AddCapsuleSurf( info );

	outModel.surfCount = 1;
	outModel.surfs.resize( outModel.surfCount );
	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ], outModel.bounds );

	outModel.surfs[ 0 ].materialHdl = AssetLib<Material>::Handle( materialName.c_str() );
}


void CreatePyramidSurface( Model& outModel, const std::string& materialName, const vec3f& origin, const float baseRadius, const float height, const uint32_t sides )
{
	GeoBuilder::pyramidInfo_t info;
	info.origin     = origin;
	info.baseRadius = baseRadius;
	info.height     = height;
	info.sides      = sides;

	GeoBuilder gb;
	gb.AddPyramidSurf( info );

	outModel.surfCount = 1;
	outModel.surfs.resize( outModel.surfCount );
	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ], outModel.bounds );

	outModel.surfs[ 0 ].materialHdl = AssetLib<Material>::Handle( materialName.c_str() );
}


void CreateDiamondSurface( Model& outModel, const std::string& materialName, const vec3f& origin, const float radius, const float topHeight, const float bottomHeight, const uint32_t sides )
{
	GeoBuilder::diamondInfo_t info;
	info.origin       = origin;
	info.radius       = radius;
	info.topHeight    = topHeight;
	info.bottomHeight = bottomHeight;
	info.sides        = sides;

	GeoBuilder gb;
	gb.AddDiamondSurf( info );

	outModel.surfCount = 1;
	outModel.surfs.resize( outModel.surfCount );
	CopyGeoBuilderResult( gb, outModel.surfs[ 0 ], outModel.bounds );

	outModel.surfs[ 0 ].materialHdl = AssetLib<Material>::Handle( materialName.c_str() );
}


bool ModelGenLoader::Load( Asset<Model>& model )
{
	//CreateQuadSurface2D();
	return false;
}
