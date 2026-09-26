#include "codeAssets.h"

#include <sstream>
#include <string>

#include <gfxcore/core/util.h>
#include <gfxcore/image/color.h>
#include <gfxcore/image/image.h>

#include "../globals/assetDefs.h"
#include "../globals/render_util.h"
#include "../asset_types/model.h"
#include "../asset_types/material.h"
#include "../asset_types/image.h"
#include "../asset_types/gpuProgram.h"


void CreateCodeAssets()
{
	// ----------------- TEXTURES ----------------- //
	{
		for( uint32_t t = 0; t < 4; ++t )
		{
			const rgba8_t rgba = Swizzle( Color( Color::Gold ).AsRgba8(), RGBA_A, RGBA_B, RGBA_G, RGBA_R );

			std::stringstream ss;
			ss << "CODE_COLOR_" << t;
			std::string s = ss.str();

			hdl_t handle = ImageLib().Add( s.c_str(), Image() );
			Image& texture = ImageLib().Find( handle )->Get();

			imageInfo_t info = DefaultImage2dInfo( 256, 240 );

			texture.Create( info );

			ImageBuffer<rgba8_t>* imageBuffer = reinterpret_cast<ImageBuffer<rgba8_t>*>( texture.cpuImage );

			for ( uint32_t y = 0; y < texture.info.height; ++y ) {
				for ( uint32_t x = 0; x < texture.info.width; ++x ) {
					imageBuffer->SetPixel( x, y, rgba );
				}
			}
		}

		// Solid Colors
		{
			const uint32_t debugColorCount = 18;

			struct dbgColorImageInfo_t
			{
				char*		name;
				Color		color;
				imageFmt_t	format;
			};

			static const dbgColorImageInfo_t colorInfo[ debugColorCount ] =
			{
				{ "_red",		ColorRed,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_green",		ColorGreen,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_blue",		ColorBlue,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_white",		ColorWhite,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_black",		ColorBlack,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_lightGrey",	ColorLGrey,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_darkGrey",	ColorDGrey,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_brown",		ColorBrown,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_cyan",		ColorCyan,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_yellow",	ColorYellow,				imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_purple",	ColorPurple,				imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_orange",	ColorOrange,				imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_pink",		ColorPink,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_gold",		ColorGold,					imageFmt_t::IMAGE_FMT_RGBA_8 },
				{ "_alb",		Color( 1.0f, 1.0f, 1.0f ),	imageFmt_t::IMAGE_FMT_RGBA_8_UNORM },
				{ "_nml",		Color( 0.5f, 0.5f, 1.0f ),	imageFmt_t::IMAGE_FMT_RGBA_8_UNORM },
				{ "_rgh",		Color( 1.0f, 0.0f, 0.0f ),	imageFmt_t::IMAGE_FMT_RGBA_8_UNORM },
				{ "_mtl",		Color( 0.6f, 0.0f, 0.0f ),	imageFmt_t::IMAGE_FMT_RGBA_8_UNORM },
			};

			imageInfo_t defaultInfo = DefaultImage2dInfo( 1, 1 );

			for ( uint32_t t = 0; t < debugColorCount; ++t )
			{
				hdl_t handle = ImageLib().Add( colorInfo[ t ].name, Image() );
				Image& texture = ImageLib().Find( handle )->Get();

				rgba8_t pixel = Swizzle( colorInfo[ t ].color.AsRgba8(), RGBA_A, RGBA_B, RGBA_G, RGBA_R );

				defaultInfo.fmt = colorInfo[ t ].format;

				texture.Create( defaultInfo, (uint8_t*)&pixel, sizeof( rgba8_t ) );
			}
		}

		// Default Image - Checkerboard
		{
			hdl_t handle = ImageLib().Add( "_defaultImage", Image() );
			Image& texture = ImageLib().Find( handle )->Get();

			const uint32_t cellSize = 16;

			imageInfo_t info = DefaultImage2dInfo( 128, 128 );

			texture.Create( info );

			ImageBuffer<rgba8_t>* imageBuffer = reinterpret_cast<ImageBuffer<rgba8_t>*>( texture.cpuImage );

			for ( uint32_t y = 0; y < info.height; ++y )
			{
				const uint32_t cellY = y / cellSize;
				for ( uint32_t x = 0; x < info.width; ++x )
				{
					const uint32_t cellX = x / cellSize;
					const float cellGradient = static_cast<float>( Max( 4, Max( abs(int( x % cellSize ) - 8), abs( int( y % cellSize ) - 8 ) ) ) / (0.5f * cellSize) );
					const Color color = ( ( cellX % 2 ) == ( cellY % 2 ) ) ? Lerp( ColorBlack, ColorLGrey, cellGradient ) : Lerp( ColorDGrey, ColorWhite, cellGradient );
					const rgba8_t pixel = Swizzle( color.AsRgba8(), RGBA_A, RGBA_B, RGBA_G, RGBA_R );
					imageBuffer->SetPixel( x, y, pixel );
				}
			}
		}
		ImageLib().SetDefault( "_defaultImage" );

		// Default Image Cube - Rainbow
		{
			hdl_t handle = ImageLib().Add( "_defaultCube", Image() );
			Image& texture = ImageLib().Find( handle )->Get();

			imageInfo_t info = DefaultImage2dInfo( 1, 1 );
			info.width = 8;
			info.height = 8;
			info.layers = 6;
			info.type = imageType_t::IMAGE_TYPE_CUBE;

			const Color* colors[ 6 ] = {
				&ColorYellow,
				&ColorGreen,
				&ColorBlue,
				&ColorCyan,
				&ColorRed,
				&ColorPink
			};

			texture.Create( info );

			ImageBuffer<rgba8_t>* imageBuffer = reinterpret_cast<ImageBuffer<rgba8_t>*>( texture.cpuImage );

			for ( uint32_t faceId = 0; faceId < 6; ++faceId ) {
				const Color* color = colors[ faceId ];
				for ( uint32_t y = 0; y < info.height; ++y ) {
					for ( uint32_t x = 0; x < info.width; ++x )
					{
						const rgba8_t pixel = Swizzle( color->AsRgba8(), RGBA_A, RGBA_B, RGBA_G, RGBA_R );
						imageBuffer->SetPixel( x, y, faceId, pixel );
					}
				}
			}
		}
	}

	// ----------------- MATERIALS ----------------- //
	{
		{
			Material material;
			material.usage = MATERIAL_USAGE_GGX;
			material.AddShader( DRAWPASS_PREPASS, AssetLibGpuProgram::Handle( "Prepass" ) );
			material.AddShader( DRAWPASS_SHADOW, AssetLibGpuProgram::Handle( "Shadow" ) );
			material.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLibGpuProgram::Handle( "Debug" ) );
			material.AddShader( DRAWPASS_DEBUG_3D, AssetLibGpuProgram::Handle( "DebugSolid" ) );
			material.AddShader( DRAWPASS_OPAQUE, AssetLibGpuProgram::Handle( "LitDiffuse" ) );
			MaterialLib().Add( "_defaultMaterial", material );
		}

		{
			Material material;
			material.usage = MATERIAL_USAGE_CODE;
			material.AddShader( DRAWPASS_DEBUG_2D, AssetLibGpuProgram::Handle( "Basic" ) );
			MaterialLib().Add( "IMAGE2D", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_WIREFRAME, AssetLibGpuProgram::Handle( "Debug" ) );
			MaterialLib().Add( "DEBUG_WIRE", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_DEBUG_3D, AssetLibGpuProgram::Handle( "DebugSolid" ) );
			MaterialLib().Add( "DEBUG_3D", material );
		}

		{
			Material material;
			material.AddShader( DRAWPASS_SKYBOX, AssetLibGpuProgram::Handle( "Sky" ), (uint32_t)shaderPermId_t::SKY_CUBE_SAMPLER );
			MaterialLib().Add( "_sky", material );
		}

		MaterialLib().SetDefault( "_defaultMaterial" );
	}

	// ----------------- MODELS ----------------- //
	{
		{
			Model model;
			CreateQuadSurface2D( model, "IMAGE2D", vec2f( 0.0f, 0.0f ), vec2f( 1.0f, 1.0f ) );
			ModelLib().Add( "_quadTexDebug", model );
		}
		{
			Model model;
			CreateBoxSurface( model, "_defaultMaterial", vec3f( 0.0f, 0.0f, 0.0f ), 1.0f );
			ModelLib().Add( "_modelDefault", model );
		}
		ModelLib().SetDefault( "_modelDefault" );
	}
}
