#include "../globals/common.h"
#include "gpuImage.h"
#include "debugMenu.h"
#include "../globals/assetDefs.h"
#include "../scene/sceneBase.h"
#include "../scene/entity.h"
#include <sstream>

renderDebugData_t g_renderDebugData;

#include "../globals/render_util.h"

#if defined( USE_IMGUI )
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/backends/imgui_impl_glfw.h"
#include "../../../external/imgui/backends/imgui_impl_vulkan.h"

#include "debugMenu.h"

#include "../app/imguiInterface.h"
#include "renderer.h"

extern imguiControls_t  g_imguiControls;
extern Renderer         g_renderer;

extern void AddImageViewerCallback( ImDrawList* dl, const imageViewerCallbackData_t& callbackData );
extern bool QueryImageViewerSample( const vec2u& pickLocation, vec4f& outColor );
extern bool QueryImageViewerStat( imageViewerStatistics_t& outStats );

const char* FormatByteSize( const uint64_t bytes )
{
	static char buffer[ 64 ];
	if ( bytes >= MB_1 ) {
		snprintf( buffer, sizeof( buffer ), "%.2f MB", BYTES_TO_MB( bytes ) );
	} else if ( bytes >= KB_1 ) {
		snprintf( buffer, sizeof( buffer ), "%.1f KB", BYTES_TO_KB( bytes ) );
	} else {
		snprintf( buffer, sizeof( buffer ), "%llu bytes", bytes );
	}
	return buffer;
}

static const int defaultWidth = 100;

struct ImguiStyle
{
	static const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;
};


#define EditFlagValue( NAME, FLAGS, CHECKFLAG )	{																					\
													bool NAME = HasFlags( FLAGS, CHECKFLAG );										\
													if ( ImGui::Checkbox( "##" #NAME, &NAME ) )										\
													{																				\
														if ( NAME ) {																\
															SetFlags( FLAGS, CHECKFLAG );											\
														} else {																	\
															ClearFlags( FLAGS, CHECKFLAG );											\
														}																			\
													}																				\
												}

void EditVector3Field( vec3f& v, const std::string& label, const float speedSlow = 0.1f, const float speedFast = 1.0f )
{
	float vec[ 3 ] = { v[ 0 ], v[ 1 ], v[ 2 ] };

	static ImGuiTableFlags vectorFieldFlags = ImGuiTableFlags_Hideable;

	if ( ImGui::BeginTable( ( "##" + label ).c_str(), 3, vectorFieldFlags ) )
	{
		ImGui::TableNextColumn();
		ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.5f, 0.0f, 0.0f, 1.0f ) );
		ImGui::PushItemWidth( -1 );
		ImGui::InputFloat( ( "##X" + label ).c_str(), &vec[ 0 ], speedSlow, speedFast );
		ImGui::PopItemWidth();
		ImGui::PopStyleColor();

		ImGui::TableNextColumn();
		ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.0f, 0.5f, 0.0f, 1.0f ) );
		ImGui::PushItemWidth( -1 );
		ImGui::InputFloat( ( "##Y" + label ).c_str(), &vec[ 1 ], speedSlow, speedFast );
		ImGui::PopItemWidth();
		ImGui::PopStyleColor();

		ImGui::TableNextColumn();
		ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.0f, 0.0f, 0.5f, 1.0f ) );
		ImGui::PushItemWidth( -1 );
		ImGui::InputFloat( ( "##Z" + label ).c_str(), &vec[ 2 ], speedSlow, speedFast );
		ImGui::PopItemWidth();
		ImGui::PopStyleColor();

		ImGui::EndTable();
	}
	v = vec;
}

static bool EditFloat( float& f )
{
	bool edited = false;
	ImGui::PushID( "##editFloat" );
	ImGui::PushItemWidth( defaultWidth );
	edited = edited || ImGui::InputFloat( "##editFloat", &f );
	ImGui::PopItemWidth();
	ImGui::PopID();

	return edited;
}


static bool EditRgb( rgb32_t& rgb )
{
	ImGui::PushItemWidth( 3 * defaultWidth );

	float col[3] = { rgb.r, rgb.g, rgb.b };
	const bool edited = ImGui::ColorEdit3( "##RGB0", col );
	if( edited )
	{
		rgb.r = col[0];
		rgb.g = col[1];
		rgb.b = col[2];
	}

	ImGui::PopItemWidth();

	return edited;
}


void DebugMenuMaterial( const Material& mat )
{
	const materialParms_t& parms = mat.GetParms();

	ImGui::Text( "Albedo: (%1.2f, %1.2f, %1.2f)", parms.albedo.r, parms.albedo.g, parms.albedo.b );
	ImGui::Text( "Ks: (%1.2f, %1.2f, %1.2f)", parms.Ks.r, parms.Ks.g, parms.Ks.b );
	ImGui::Text( "Ke: (%1.2f, %1.2f, %1.2f)", parms.Ke.r, parms.Ke.g, parms.Ke.b );
	ImGui::Text( "Ka: (%1.2f, %1.2f, %1.2f)", parms.Ka.r, parms.Ka.g, parms.Ka.b );
	ImGui::Text( "ior: %1.2f", parms.ior );
	ImGui::Text( "Tf: %1.2f", parms.Tf );
	ImGui::Text( "Opacity: %1.2f", parms.opacity );
	ImGui::Text( "illum: %1.2f", parms.illum );
	ImGui::Separator();
	for ( uint32_t t = 0; t < MaxMaterialTextures; ++t )
	{
		hdl_t texHdl = mat.GetTexture( t );
		if ( texHdl.IsValid() == false )
		{
			ImGui::Text( "<none>" );
		}
		else
		{
			const char* texName = ImageLib().FindName( texHdl );
			ImGui::Text( texName );
		}
	}
}

template<class AssetType>
void DebugMenuLibComboEdit( const std::string label, hdl_t& currentHdl, const AssetLib<AssetType>& lib )
{
	const Asset<AssetType>* selectedAsset = lib.Find( currentHdl );
	const char* previewName = ( currentHdl.IsValid() ) ? selectedAsset->GetName().c_str() : "<none>";
	if ( ImGui::BeginCombo( label.c_str(), previewName ) )
	{
		if ( ImGui::Selectable( "<none>", !currentHdl.IsValid() ) ) {
			currentHdl = INVALID_HDL;
		}

		std::vector<AssetListEntry> assetList;
		lib.GetAssetList( assetList );

		for ( const AssetListEntry& entry : assetList )
		{
			const bool selected = ( currentHdl == entry.handle );
			if ( ImGui::Selectable( entry.name.c_str(), selected ) ) {
				currentHdl = entry.handle;
			}

			if ( selected ) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
}


std::string GetShaderTextureName( const Material& material, const uint32_t textureSlot )
{
	static const char* ggxDebugName[ 13 ] =
	{
		"alb",	// GGX_ALBEDO_MAP_SLOT
		"nml",	// GGX_NORMAL_MAP_SLOT
		"rgh",	// GGX_CC_ROUGHNESS_MAP_SLOT
		"mtl",	// GGX_METALLIC_MAP_SLOT
		"ao ",	// GGX_AO_MAP_SLOT
		"em ",	// GGX_EMISSIVE_MAP_SLOT
		"cc ",	// GGX_CC_MAP_SLOT
		"ccr",	// GGX_CC_ROUGHNESS_MAP_SLOT
		"ccn",	// GGX_CC_NML_MAP_SLOT
		"sh ",	// GGX_SHEEN_COLOR_MAP_SLOT
		"shr",	// GGX_SHEEN_ROUGHNESS_MAP_SLOT
		"ani",	// GGX_ANISOTROPY_MAP_SLOT
		"tr ",	// GGX_TRANSMISSION_MAP_SLOT
	};

	static const char* blinnPhongDebugName[ 5 ] =
	{
		"clr",	// BLINN_PHONG_COLOR_MAP_SLOT
		"nml",	// BLINN_PHONG_NORMAL_MAP_SLOT
		"spc",	// BLINN_PHONG_SPEC_MAP_SLOT
		"gls",	// BLINN_PHONG_GLOSS_MAP_SLOT
		"em ",	// BLINN_PHONG_EMISSIVE_MAP_SLOT
	};
	
	std::string name = std::to_string( textureSlot ) + ": ";

	switch( material.usage )
	{
	case MATERIAL_USAGE_GGX:			name = ( textureSlot >= 13 ? "   " : ggxDebugName[ textureSlot ] ); break;
	case MATERIAL_USAGE_BLINN_PHONG:	name = ( textureSlot >= 5 ? "   " : blinnPhongDebugName[ textureSlot ] ); break;
	}

	return name;
}


std::string GetLightingDebugModeName( const gpuDebugLightingMode_t mode )
{
	switch( mode )
	{
		case DEBUG_NONE:			return "defaultLit";
		case DEBUG_ALBEDO:			return "albedo";
		case DEBUG_ROUGHNESS:		return "roughness";
		case DEBUG_METALLIC:		return "metallic";
		case DEBUG_TBN_NORMAL:		return "Tangent Normal";
		case DEBUG_NORMAL:			return "Normal";
		case DEBUG_INPUT_UV:		return "UV";
		case DEBUG_EMISSIVE:		return "Emissive";
		case DEBUG_SHEENCOLOR:		return "SheenColor";
		case DEBUG_SHEENROUGHNESS:	return "SheenRoughness";
		case DEBUG_AO:				return "AO";
		case DEBUG_BRDF_LUT:		return "BRDF";
		case DEBUG_POSITION:		return "Position";
	}
	return "<unknown>";
}


void ChannelButton( const char* label, float& channelValue, ImVec4 onColor )
{
	static const ImVec4 offColor = ImVec4( 0.25f, 0.25f, 0.25f, 1.0f );

	const bool active = channelValue > 0.5f;
	ImGui::PushStyleColor( ImGuiCol_Button, active ? onColor : offColor );
	if( ImGui::SmallButton( label ) ) {
		channelValue = active ? 0.0f : 1.0f;
	}
	ImGui::PopStyleColor();
}


void DebugMenuMaterialEdit( Asset<Material>* matAsset )
{
#define EditRgbValue( VALUE )	{															\
									ImGui::PushID( ( matAsset->GetName() + "." + #VALUE ).c_str() ); \
									ImGui::Text( #VALUE );									\
									ImGui::SameLine();										\
									rgb32_t rgb = mat.GetParms().##VALUE;					\
									if( EditRgb( rgb ) ) {									\
										mat.GetParms().##VALUE = rgb;						\
										matAsset->RefreshUpload();							\
									}														\
									ImGui::PopID();											\
								}

#define EditFloatValue( VALUE )	{															\
									ImGui::PushID( ( matAsset->GetName() + "." + #VALUE ).c_str() ); \
									ImGui::Text( #VALUE );									\
									ImGui::SameLine();										\
									float value = mat.GetParms().##VALUE;					\
									if( EditFloat( value ) ) {								\
										mat.GetParms().##VALUE = value;						\
										matAsset->RefreshUpload();							\
									}														\
									ImGui::PopID();											\
								}

	Material& mat = matAsset->Get();

	EditRgbValue( albedo );
	EditRgbValue( Ks );
	EditRgbValue( Ke );
	EditRgbValue( Ka );
	EditRgbValue( Tf );
	EditRgbValue( sheenColor );
	EditFloatValue( opacity );
	EditFloatValue( Ns );
	EditFloatValue( illum );
	EditFloatValue( emissiveStrength );
	EditFloatValue( alphaCutoff );
	EditFloatValue( ior );
	EditFloatValue( roughness );
	EditFloatValue( metalness );
	EditFloatValue( sheenRoughness );
	EditFloatValue( clearcoatWeight );
	EditFloatValue( clearcoatRoughness );
	EditFloatValue( anisotropy );
	EditFloatValue( anisotropyRotation );
	EditFloatValue( transmissionFactor );

	if ( ImGui::TreeNode( "Textures" ) )
	{
		for ( uint32_t t = 0; t < MaxMaterialTextures; ++t )
		{
			if( mat.GetTexture( t ) == INVALID_HDL ) {
				continue;
			}

			std::string imageName = GetShaderTextureName( mat, t );
			std::string label = "##" + imageName;

			ImGui::Text( imageName.c_str() );
			ImGui::SameLine();

			hdl_t originalHdl = mat.GetTexture( t );
			hdl_t texHdl = originalHdl;
			DebugMenuLibComboEdit( label, texHdl, ImageLib() );

			if( texHdl != originalHdl )
			{
				mat.AddTexture( t, texHdl );
				matAsset->RefreshUpload();
			}
		}
		ImGui::TreePop();
	}

	if ( ImGui::TreeNode( "Shaders" ) )
	{
		for ( uint32_t s = 0; s < Material::MaxMaterialShaders; ++s )
		{
			ImGui::Text( "%i:", s );
			ImGui::SameLine();
			hdl_t shaderHdl = mat.GetShader( drawPass_t( s ) );
			if ( shaderHdl.IsValid() == false )
			{
				ImGui::Text( "<none>" );
			}
			else
			{
				const char* shaderName = GpuProgramLib().FindName( shaderHdl );
				ImGui::Text( shaderName );
			}
		}
		ImGui::TreePop();
	}

#undef EditRgbValue
#undef EditFloatValue
}


void DebugMenuModelTreeNode( Asset<Model>* modelAsset )
{
	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

	const char* modelName = modelAsset->GetName().c_str();
	if ( ImGui::TreeNode( modelName ) )
	{
		Model& model = modelAsset->Get();
		const vec3f& min = model.bounds.GetMin();
		const vec3f& max = model.bounds.GetMax();
		if ( ImGui::BeginTable( "Bounds", 4, tableFlags ) )
		{
			ImGui::TableSetupColumn( "" );
			ImGui::TableSetupColumn( "X" );
			ImGui::TableSetupColumn( "Y" );
			ImGui::TableSetupColumn( "Z" );
			ImGui::TableHeadersRow();

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex( 0 );
			ImGui::Text( "Min" );
			ImGui::TableSetColumnIndex( 1 );
			ImGui::Text( "%4.3f", min[ 0 ] );
			ImGui::TableSetColumnIndex( 2 );
			ImGui::Text( "%4.3f", min[ 1 ] );
			ImGui::TableSetColumnIndex( 3 );
			ImGui::Text( "%4.3f", min[ 2 ] );

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex( 0 );
			ImGui::Text( "Max" );
			ImGui::TableSetColumnIndex( 1 );
			ImGui::Text( "%4.3f", max[ 0 ] );
			ImGui::TableSetColumnIndex( 2 );
			ImGui::Text( "%4.3f", max[ 1 ] );
			ImGui::TableSetColumnIndex( 3 );
			ImGui::Text( "%4.3f", max[ 2 ] );

			ImGui::EndTable();
		}

		if ( ImGui::TreeNode( "##Surfaces", "Surfaces (%u)", model.surfCount ) )
		{
			if ( ImGui::BeginTable( "Surface", 5, tableFlags ) )
			{
				ImGui::TableSetupColumn( "Number" );
				ImGui::TableSetupColumn( "Material" );
				ImGui::TableSetupColumn( "Vertices" );
				ImGui::TableSetupColumn( "Indices" );
				ImGui::TableSetupColumn( "Centroid" );
				ImGui::TableHeadersRow();

				for ( uint32_t s = 0; s < model.surfCount; ++s )
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::Text( "%u", s );
					ImGui::TableSetColumnIndex( 1 );
					hdl_t& handle = model.surfs[ s ].materialHdl;
					std::string modelName = "##" + std::string( MaterialLib().FindName( handle ) );
					DebugMenuLibComboEdit( modelName, handle, MaterialLib() );
					ImGui::TableSetColumnIndex( 2 );
					ImGui::Text( "%i", (int)model.surfs[ s ].vertices.size() );
					ImGui::TableSetColumnIndex( 3 );
					ImGui::Text( "%i", (int)model.surfs[ s ].indices.size() );
					ImGui::TableSetColumnIndex( 4 );
					ImGui::Text( "(%.2f %.2f %.2f)", model.surfs[ s ].centroid[ 0 ], model.surfs[ s ].centroid[ 1 ], model.surfs[ s ].centroid[ 2 ] );
				}
				ImGui::EndTable();
			}
			ImGui::TreePop();
		}
		ImGui::TreePop();
	}
}


void DebugMenuTextureTreeNode( Asset<Image>* texAsset )
{
	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

	const char* texName = texAsset->GetName().c_str();

	if ( ImGui::TreeNode( texName ) )
	{
		Image& texture = texAsset->Get();
		if ( ImGui::BeginTable( "Info", 2, tableFlags ) )
		{
			ImGui::TableNextColumn();	ImGui::Text( "Width" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.info.width );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Height" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.info.height );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Layers" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.info.layers );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Channels" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.info.channels );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Mips" );
			ImGui::TableNextColumn();
			ImGui::Text( "%u", texture.info.mipLevels );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Layers" );
			ImGui::TableNextColumn();
			switch ( texture.info.type )
			{
			case IMAGE_TYPE_2D:
				ImGui::Text( "2D" );
				break;
			case IMAGE_TYPE_CUBE:
				ImGui::Text( "CUBE" );
				break;
			default:
				ImGui::Text( "Unknown" );
			}
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "Upload Id" );
			ImGui::TableNextColumn();	ImGui::Text( "%u", texture.gpuImage->GetId() );
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "CPU Size" );
			if( texture.cpuImage != nullptr ) {
				ImGui::TableNextColumn();	ImGui::Text( "%s", FormatByteSize( texture.cpuImage->GetByteCount() ) );
			} else {
				ImGui::TableNextColumn();	ImGui::Text( "No CPU Mem" );
			}
			ImGui::TableNextRow();

			ImGui::TableNextColumn();	ImGui::Text( "GPU Size" );
			if( texture.gpuImage != nullptr ) {
				ImGui::TableNextColumn();	ImGui::Text( "%s", FormatByteSize( texture.gpuImage->GetByteCount() ) );
			} else {
				ImGui::TableNextColumn();	ImGui::Text( "No GPU Mem" );
			}
			ImGui::TableNextRow();

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}
}


void DebugMenuShaderTreeNode( Asset<GpuProgram>* progAsset )
{
	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

	GpuProgram& prog = progAsset->Get();
	const char* progName = progAsset->GetName().c_str();
	if ( ImGui::TreeNode( progName ) )
	{
		if ( ImGui::BeginTable( progName, 3, tableFlags ) )
		{
			ImGui::TableSetupColumn( "Name" );
			ImGui::TableSetupColumn( "Type" );
			ImGui::TableSetupColumn( "Bytes" );
			ImGui::TableHeadersRow();

			for ( uint32_t i = 0; i < prog.shaderCount; ++i )
			{
				ShaderBin& bin = prog.shaderBins[ i ].begin()->second;
				ShaderSource& source = prog.shaders[ i ];

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex( 0 );
				ImGui::Text( source.name.c_str() );
				ImGui::TableSetColumnIndex( 1 );
				switch ( bin.type )
				{
					case VERTEX:			ImGui::Text( "Vertex" ); break;
					case PIXEL:				ImGui::Text( "Pixel" ); break;
					case COMPUTE:			ImGui::Text( "Compute" ); break;
					case RT_GEN:			ImGui::Text( "Ray Generation" ); break;
					case RT_ANY_HIT:		ImGui::Text( "Ray Hit" ); break;
					case RT_CLOSEST_HIT:	ImGui::Text( "Ray Closest Hit" ); break;
					case RT_INTERSECTION:	ImGui::Text( "Ray Intersection" ); break;
					case RT_MISS:			ImGui::Text( "Ray Miss" ); break;
				}
				ImGui::TableSetColumnIndex( 2 );
				ImGui::Text( "%u", bin.blob.size() );
			}
			if( ImGui::Button( "Reload" ) ) {
				g_imguiControls.shaderHdl = progAsset->Handle();
			}
			ImGui::EndTable();
		}
		ImGui::TreePop();
	}
}


void DebugMenuEntityEdit( Scene* scene )
{
	static ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

	struct outlinerEntry_t
	{
		std::string		label;
		int32_t			id;
		bool			isEntity;
	};

	const uint32_t entityCount = scene->EntityCount();
	const uint32_t lightCount = static_cast<uint32_t>( scene->lights.size() );

	std::vector<outlinerEntry_t> entries;
	entries.reserve( entityCount + lightCount );

	ImGui::BeginChild( "Scene", ImVec2( 0, 260 ) );
	
	for ( uint32_t i = 0; i < lightCount; ++i )
	{
		if ( i >= scene->lights.size() ) {
			break;
		}
		light_t& light = scene->lights[ i ];

		std::stringstream ss;
		ss << "Light" << i;

		outlinerEntry_t entry;
		entry.label = ss.str();
		entry.isEntity = false;
		entry.id = i;

		entries.push_back( entry );
	}

	for ( uint32_t i = 0; i < entityCount; ++i )
	{
		Entity* ent = scene->FindEntity( i );
		if ( ent == nullptr ) {
			continue;
		}
		
		std::stringstream ss;

		outlinerEntry_t entry;
		entry.label = scene->entities[ i ]->name;
		entry.isEntity = true;
		entry.id = i;

		entries.push_back( entry );
	}

	static ImVector<int32_t> selection;

	if ( ImGui::BeginTable( "Entities", 2, flags ) )
	{
		const uint32_t entryCount = static_cast<uint32_t>( entries.size() );
		for ( uint32_t i = 0; i < entryCount; ++i )
		{
			const int32_t itemId = i;

			const outlinerEntry_t& entry = entries[ i ];
			const bool isSelected = selection.contains( itemId );

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID( i );
			if( ImGui::Selectable( entry.label.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns ) )
			{
				if ( ImGui::GetIO().KeyCtrl )
				{
					if ( isSelected )
						selection.find_erase_unsorted( itemId );
					else
						selection.push_back( itemId );
				}
				else
				{
					selection.clear();
					selection.push_back( itemId );
				}
			}
			ImGui::TableNextColumn();
			ImGui::Text( entry.isEntity ? "Entity" : "Light" );

			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	ImGui::EndChild();

	ImGui::Separator();

	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

	if( selection.empty() ) {
		return;
	}

	const int32_t currentEntityId = selection[ 0 ];

	if ( currentEntityId < 0 ) {
		return;
	}

	const outlinerEntry_t& entry = entries[ currentEntityId ];

	ImGui::BeginChild( "##PropertyGrid", ImVec2( 0, 260 ), true );
	ImGui::Separator();
	ImGui::Text( "Property Grid" );
	ImGui::Separator();

	static ImGuiTableFlags propertyGridFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

	ImGui::PushItemWidth( -1 );
	ImGui::PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0.2f, 0.2f, 0.2f, 1.0f ) );

	if( entry.isEntity )
	{
		Entity* ent = scene->FindEntity( entry.id );

		if ( ImGui::BeginTable( "##EntityProperties", 2, propertyGridFlags ) )
		{
			vec3f origin = ent->GetOrigin();
			vec3f scale = ent->GetScale();
			const mat4x4f R = ent->GetRotation();

			vec3f rotation;
			MatrixToEulerZYX( R, rotation[ 0 ], rotation[ 1 ], rotation[ 2 ] );

			ImGui::TableNextColumn();
			ImGui::Text( "Origin" );
			ImGui::TableNextColumn();
			EditVector3Field( origin, "Origin", 0.1f, 1.0f );

			ImGui::TableNextColumn();
			ImGui::Text( "Scale" );
			ImGui::TableNextColumn();
			EditVector3Field( scale, "Scale", 0.1f, 1.0f );

			ImGui::TableNextColumn();
			ImGui::Text( "Rotation" );
			ImGui::TableNextColumn();
			EditVector3Field( rotation, "Rotation", 1.0f, 10.0f );

			ent->SetOrigin( origin );
			ent->SetScale( scale );
			ent->SetRotation( rotation );

			const char* currentModelName = ModelLib().FindName( ent->modelHdl );
			ImGui::TableNextColumn();
			ImGui::Text( "Model" );
			ImGui::TableNextColumn();
			if ( ImGui::BeginCombo( "##model", currentModelName != nullptr ? currentModelName : "<none>" ) )
			{
				const uint32_t modelCount = ModelLib().Count();
				for ( uint32_t m = 0; m < modelCount; ++m )
				{
					const char* modelName = ModelLib().FindName( m );
					const hdl_t modelHdl  = ModelLib().RetrieveHdl( modelName );
					const bool selected   = ( modelHdl == ent->modelHdl );

					if ( ImGui::Selectable( modelName, selected ) && !selected )
					{
						ent->modelHdl = modelHdl;
						scene->CreateEntityBounds( modelHdl, *ent );
					}

					if ( selected ) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::EndTable();
		}
	}
	else if ( entry.id >= 0 && entry.id < static_cast<int32_t>( scene->lights.size() ) )
	{
		light_t& light = scene->lights[ entry.id ];

		if ( ImGui::BeginTable( "##LightProperties", 2, propertyGridFlags ) )
		{
			vec3f origin = light.pos.xyz;
			vec3f dir = light.dir.xyz;

			ImGui::TableNextColumn();
			ImGui::Text( "Origin" );
			ImGui::TableNextColumn();
			EditVector3Field( origin, "Origin", 0.1f, 1.0f );

			ImGui::TableNextColumn();
			ImGui::Text( "Direction" );
			ImGui::TableNextColumn();
			EditVector3Field( dir, "Direction", 0.1f, 1.0f );

			light.pos = vec4f( origin, 0.0f );
			light.dir = vec4f( dir, 0.0f );

			ImGui::TableNextColumn();
			ImGui::Text( "Intensity" );
			ImGui::TableNextColumn();
			ImGui::InputFloat( "##lightIntensity", &light.intensity, 0.1f, 1.0f );

			ImGui::TableNextColumn();
			ImGui::Text( "Shadow" );
			ImGui::TableNextColumn();
			EditFlagValue( Shadow, light.flags, lightFlags_t::LIGHT_FLAGS_SHADOW );

			ImGui::TableNextColumn();
			ImGui::Text( "Point Light" );
			ImGui::TableNextColumn();
			EditFlagValue( PointLight, light.flags, lightFlags_t::LIGHT_FLAGS_POINT );

			rgb32_t rgb = light.color.AsRgb32();

			ImGui::TableNextColumn();
			ImGui::Text( "Color" );
			ImGui::TableNextColumn();
			EditRgb( rgb );
			light.color = rgb;

			ImGui::EndTable();
		}
	}
	ImGui::PopStyleColor();
	ImGui::PopItemWidth();
	
	ImGui::EndChild();
	ImGui::Separator();
}


#define LimitUint( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );												\
									ImGui::TableNextColumn();	ImGui::Text( "%u", deviceProperties.limits.##FIELD );

#define LimitDeviceSize( FIELD )	ImGui::TableNextColumn();	ImGui::Text( #FIELD );												\
									ImGui::TableNextColumn();	ImGui::Text( "%u", deviceProperties.limits.##FIELD );

#define LimitSizeT( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );												\
									ImGui::TableNextColumn();	ImGui::Text( "%ull", deviceProperties.limits.##FIELD );

#define LimitBool( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );												\
									ImGui::TableNextColumn();	ImGui::Text( "%u", deviceProperties.limits.##FIELD );

#define LimitFloat( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );												\
									ImGui::TableNextColumn();	ImGui::Text( "%f", deviceProperties.limits.##FIELD );

#define LimitInt( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );												\
									ImGui::TableNextColumn();	ImGui::Text( "%i", deviceProperties.limits.##FIELD );

#define LimitHex( FIELD )			ImGui::TableNextColumn();	ImGui::Text( #FIELD );												\
									ImGui::TableNextColumn();	ImGui::Text( "%X", deviceProperties.limits.##FIELD );

#define FeatureBool( FIELD )		ImGui::TableNextColumn();	ImGui::Text( #FIELD );												\
									ImGui::TableNextColumn();	ImGui::Text( "%u", deviceFeatures.features.##FIELD );


void DebugMenuDeviceProperties( VkPhysicalDeviceProperties deviceProperties, VkPhysicalDeviceFeatures2 deviceFeatures )
{
	static ImGuiTableFlags tableFlags = ImguiStyle::TableFlags;

	if ( ImGui::BeginTable( "Info", 2, tableFlags ) )
	{
		ImGui::TableNextColumn();	ImGui::Text( "Device" );
		ImGui::TableNextColumn();	ImGui::Text( deviceProperties.deviceName );

		ImGui::TableNextColumn();	ImGui::Text( "API Version" );
		ImGui::TableNextColumn();	ImGui::Text( "%u", deviceProperties.apiVersion );

		ImGui::TableNextColumn();	ImGui::Text( "Driver Version" );
		ImGui::TableNextColumn();	ImGui::Text( "%u", deviceProperties.driverVersion );

		ImGui::TableNextColumn();
		ImGui::Text( "Type" );
		ImGui::TableNextColumn();
		switch( deviceProperties.deviceType )
		{
			case VK_PHYSICAL_DEVICE_TYPE_OTHER:				ImGui::Text( "Unknown" );	break;
			case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:	ImGui::Text( "Integrated" );break;
			case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:		ImGui::Text( "Discrete" );	break;
			case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:		ImGui::Text( "Virtual" );	break;
			case VK_PHYSICAL_DEVICE_TYPE_CPU:				ImGui::Text( "CPU" );		break;
		}

		ImGui::EndTable();
	}

	if ( ImGui::TreeNode( "Limits" ) )
	{
		if ( ImGui::BeginTable( "Info", 2, tableFlags ) )
		{
			LimitUint( maxImageDimension1D )
			LimitUint( maxImageDimension2D )
			LimitUint( maxImageDimension3D )
			LimitUint( maxImageDimensionCube )
			LimitUint( maxImageArrayLayers )
			LimitUint( maxTexelBufferElements )
			LimitUint( maxUniformBufferRange )
			LimitUint( maxStorageBufferRange )
			LimitUint( maxPushConstantsSize )
			LimitUint( maxMemoryAllocationCount )
			LimitUint( maxSamplerAllocationCount )
			LimitUint( maxBoundDescriptorSets )
			LimitUint( maxPerStageDescriptorSamplers )
			LimitUint( maxPerStageDescriptorUniformBuffers )
			LimitUint( maxPerStageDescriptorStorageBuffers )
			LimitUint( maxPerStageDescriptorSampledImages )
			LimitUint( maxPerStageDescriptorStorageImages )
			LimitUint( maxPerStageDescriptorInputAttachments )
			LimitUint( maxPerStageResources )
			LimitUint( maxDescriptorSetSamplers )
			LimitUint( maxDescriptorSetUniformBuffers )
			LimitUint( maxDescriptorSetUniformBuffersDynamic )
			LimitUint( maxDescriptorSetStorageBuffers )
			LimitUint( maxDescriptorSetStorageBuffersDynamic )
			LimitUint( maxDescriptorSetSampledImages )
			LimitUint( maxDescriptorSetStorageImages )
			LimitUint( maxDescriptorSetInputAttachments )
			LimitUint( maxVertexInputAttributes )
			LimitUint( maxVertexInputBindings )
			LimitUint( maxVertexInputAttributeOffset )
			LimitUint( maxVertexInputBindingStride )
			LimitUint( maxVertexOutputComponents )
			LimitUint( maxTessellationGenerationLevel )
			LimitUint( maxTessellationPatchSize )
			LimitUint( maxTessellationControlPerVertexInputComponents )
			LimitUint( maxTessellationControlPerVertexOutputComponents )
			LimitUint( maxTessellationControlPerPatchOutputComponents )
			LimitUint( maxTessellationControlTotalOutputComponents )
			LimitUint( maxTessellationEvaluationInputComponents )
			LimitUint( maxTessellationEvaluationOutputComponents )
			LimitUint( maxGeometryShaderInvocations )
			LimitUint( maxGeometryInputComponents )
			LimitUint( maxGeometryOutputComponents )
			LimitUint( maxGeometryOutputVertices )
			LimitUint( maxGeometryTotalOutputComponents )
			LimitUint( maxFragmentInputComponents )
			LimitUint( maxFragmentOutputAttachments )
			LimitUint( maxFragmentDualSrcAttachments )
			LimitUint( maxFragmentCombinedOutputResources )
			LimitUint( maxComputeSharedMemorySize )
			LimitUint( maxComputeWorkGroupCount[0] )
			LimitUint( maxComputeWorkGroupCount[1] )
			LimitUint( maxComputeWorkGroupCount[2] )
			LimitUint( maxComputeWorkGroupInvocations )
			LimitUint( maxComputeWorkGroupSize[0] )
			LimitUint( maxComputeWorkGroupSize[1] )
			LimitUint( maxComputeWorkGroupSize[2] )
			LimitUint( subPixelPrecisionBits )
			LimitUint( subTexelPrecisionBits )
			LimitUint( mipmapPrecisionBits )
			LimitUint( maxDrawIndexedIndexValue )
			LimitUint( maxDrawIndirectCount )
			LimitUint( maxViewports )
			LimitUint( maxViewportDimensions[0] )
			LimitUint( maxViewportDimensions[1] )
			LimitUint( viewportSubPixelBits )
			LimitUint( maxTexelOffset )
			LimitUint( maxTexelGatherOffset )
			LimitUint( subPixelInterpolationOffsetBits )
			LimitUint( maxFramebufferWidth )
			LimitUint( maxFramebufferHeight )
			LimitUint( maxFramebufferLayers )
			LimitUint( maxColorAttachments )
			LimitUint( maxSampleMaskWords )
			LimitUint( maxClipDistances )
			LimitUint( maxCullDistances )
			LimitUint( maxCombinedClipAndCullDistances )
			LimitUint( discreteQueuePriorities )
			LimitDeviceSize( bufferImageGranularity )
			LimitDeviceSize( sparseAddressSpaceSize )
			LimitDeviceSize( minTexelBufferOffsetAlignment )
			LimitDeviceSize( minUniformBufferOffsetAlignment )
			LimitDeviceSize( minStorageBufferOffsetAlignment )
			LimitDeviceSize( optimalBufferCopyOffsetAlignment )
			LimitDeviceSize( optimalBufferCopyRowPitchAlignment )
			LimitDeviceSize( nonCoherentAtomSize )
			LimitBool( timestampComputeAndGraphics )
			LimitBool( strictLines )
			LimitBool( standardSampleLocations )
			LimitFloat( maxSamplerLodBias )
			LimitFloat( maxSamplerAnisotropy )
			LimitFloat( viewportBoundsRange[0] )
			LimitFloat( viewportBoundsRange[1] )
			LimitFloat( minInterpolationOffset )
			LimitFloat( maxInterpolationOffset )
			LimitFloat( timestampPeriod )
			LimitFloat( pointSizeRange[0] )
			LimitFloat( pointSizeRange[1] )
			LimitFloat( lineWidthRange[0] )
			LimitFloat( lineWidthRange[1] )
			LimitFloat( pointSizeGranularity )
			LimitFloat( lineWidthGranularity )
			LimitInt( minTexelOffset )
			LimitInt( minTexelGatherOffset )
			LimitSizeT( minMemoryMapAlignment )
			LimitHex( framebufferColorSampleCounts )
			LimitHex( framebufferDepthSampleCounts )
			LimitHex( framebufferStencilSampleCounts )
			LimitHex( framebufferNoAttachmentsSampleCounts )
			LimitHex( sampledImageColorSampleCounts )
			LimitHex( sampledImageIntegerSampleCounts )
			LimitHex( sampledImageDepthSampleCounts )
			LimitHex( sampledImageStencilSampleCounts )
			LimitHex( storageImageSampleCounts )

			ImGui::EndTable();
		}

		ImGui::TreePop();
	}
	if ( ImGui::TreeNode( "Features" ) )
	{
		if ( ImGui::BeginTable( "Info", 2, tableFlags ) )
		{
			FeatureBool( robustBufferAccess )
			FeatureBool( fullDrawIndexUint32 )
			FeatureBool( imageCubeArray )
			FeatureBool( independentBlend )
			FeatureBool( geometryShader )
			FeatureBool( tessellationShader )
			FeatureBool( sampleRateShading )
			FeatureBool( dualSrcBlend )
			FeatureBool( logicOp )
			FeatureBool( multiDrawIndirect )
			FeatureBool( drawIndirectFirstInstance )
			FeatureBool( depthClamp )
			FeatureBool( depthBiasClamp )
			FeatureBool( fillModeNonSolid )
			FeatureBool( depthBounds )
			FeatureBool( wideLines )
			FeatureBool( largePoints )
			FeatureBool( alphaToOne )
			FeatureBool( multiViewport )
			FeatureBool( samplerAnisotropy )
			FeatureBool( textureCompressionETC2 )
			FeatureBool( textureCompressionASTC_LDR )
			FeatureBool( textureCompressionBC )
			FeatureBool( occlusionQueryPrecise )
			FeatureBool( pipelineStatisticsQuery )
			FeatureBool( vertexPipelineStoresAndAtomics )
			FeatureBool( fragmentStoresAndAtomics )
			FeatureBool( shaderTessellationAndGeometryPointSize )
			FeatureBool( shaderImageGatherExtended )
			FeatureBool( shaderStorageImageExtendedFormats )
			FeatureBool( shaderStorageImageMultisample )
			FeatureBool( shaderStorageImageReadWithoutFormat )
			FeatureBool( shaderStorageImageWriteWithoutFormat )
			FeatureBool( shaderUniformBufferArrayDynamicIndexing )
			FeatureBool( shaderSampledImageArrayDynamicIndexing )
			FeatureBool( shaderStorageBufferArrayDynamicIndexing )
			FeatureBool( shaderStorageImageArrayDynamicIndexing )
			FeatureBool( shaderClipDistance )
			FeatureBool( shaderCullDistance )
			FeatureBool( shaderFloat64 )
			FeatureBool( shaderInt64 )
			FeatureBool( shaderInt16 )
			FeatureBool( shaderResourceResidency )
			FeatureBool( shaderResourceMinLod )
			FeatureBool( sparseBinding )
			FeatureBool( sparseResidencyBuffer )
			FeatureBool( sparseResidencyImage2D )
			FeatureBool( sparseResidencyImage3D )
			FeatureBool( sparseResidency2Samples )
			FeatureBool( sparseResidency4Samples )
			FeatureBool( sparseResidency8Samples )
			FeatureBool( sparseResidency16Samples )
			FeatureBool( sparseResidencyAliased )
			FeatureBool( variableMultisampleRate )
			FeatureBool( inheritedQueries )
			ImGui::EndTable();
		}
		ImGui::TreePop();
	}
}

#undef LimitUint
#undef LimitDeviceSize
#undef LimitBool
#undef LimitFloat
#undef LimitInt
#undef LimitHex
#undef FeatureBool


void DrawImageViewerDebugMenu()
{
	// --- Set-up ---
	// Build image list from both the asset library and renderer output images
	std::vector<const Image*> images;

	const uint32_t imageCount = ImageLib().Count();
	for ( uint32_t i = 0; i < imageCount; ++i )
	{
		const Image* img = &ImageLib().Find( i )->Get();
		if ( img != nullptr ) {
			images.push_back( img );
		}
	}

	const uint32_t outputImageCount = g_renderer.OutputImageCount();
	for ( uint32_t i = 0; i < outputImageCount; ++i )
	{
		const Image* img = g_renderer.FindOutputImage( i );
		if ( img != nullptr && img->gpuImage != nullptr ) {
			images.push_back( img );
		}
	}

	std::sort( images.begin(), images.end() );

	std::vector<std::string> imageNames;
	const uint32_t dbgImageCount = static_cast<uint32_t>( images.size() );
	for ( uint32_t i = 0; i < dbgImageCount; ++i ) {
		imageNames.push_back( std::to_string( i ) + ": " + images[ i ]->gpuImage->GetDebugName() );
	}

	const char* debugImageName = ( g_imguiControls.dbgImageId >= 0 ) ? imageNames[ g_imguiControls.dbgImageId ].c_str() : "";

	if ( ImGui::BeginCombo( "Images", debugImageName ) )
	{
		for ( uint32_t i = 0; i < dbgImageCount; ++i )
		{
			const char* name = imageNames[ i ].c_str();
			if ( _stricmp( name, "" ) == 0 ) {
				continue;
			}
			const bool selected = ( i == (uint32_t)g_imguiControls.dbgImageId );
			if ( ImGui::Selectable( name, selected ) ) {
				g_imguiControls.dbgImageId = i;
			}
			if ( selected ) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if ( g_imguiControls.dbgImageId < 0 ) {
		return;
	}

	const Image* image = images[ g_imguiControls.dbgImageId ];
	const float  aspect = image->info.width / (float)image->info.height;

	static bool  autoScale = true;
	static float scale = 1.0f;

	static float tint[ 4 ] = { 1.0f, 1.0f, 1.0f, 0.0f };
	static bool gammaEnabled = false;
	static int selectedMip = 0;
	static int selectedLayer = 0;
	static int selectedSample = -1;  // -1 = average
	static float intensity = 1.0f;
	static float intensityMin = 0.0f;
	static float intensityMax = 2.0f;
	static float rangeMin = 0.0f;
	static float rangeMax = 1.0f;// INFINITY;

	ImGui::Begin( "Image Viewer" );

	ImGuiIO& io = ImGui::GetIO();

	// --- Toolbar ---
	const float totalAvailW = ImGui::GetContentRegionAvail().x;
	const float totalAvailH = ImGui::GetContentRegionAvail().y;
	const float rowH = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
	const bool isMsaa = ( image->info.subsamples != IMAGE_SMP_1 );
	const int toolbarRows = 3 + ( image->info.layers > 1 ? 1 : 0 ) + ( isMsaa ? 1 : 0 );
	const float toolbarH = toolbarRows * rowH;
	const float separatorH = ImGui::GetStyle().ItemSpacing.y + 1.0f;
	const float imageAreaH = totalAvailH - toolbarH - separatorH;

	ChannelButton( "R", tint[ 0 ], ImVec4(0.8f, 0.2f, 0.2f, 1.0f) );
	ImGui::SameLine();
	ChannelButton( "G", tint[ 1 ], ImVec4( 0.2f, 0.7f, 0.2f, 1.0f ) );
	ImGui::SameLine();
	ChannelButton( "B", tint[ 2 ], ImVec4( 0.2f, 0.4f, 0.9f, 1.0f ) );
	ImGui::SameLine();
	ChannelButton( "A", tint[ 3 ], ImVec4( 0.7f, 0.7f, 0.7f, 1.0f ) );
	ImGui::SameLine();

	{
		static const ImVec4 offColor = ImVec4( 0.25f, 0.25f, 0.25f, 1.0f );
		ImGui::PushStyleColor( ImGuiCol_Button, gammaEnabled ? ImVec4( 0.9f, 0.7f, 0.1f, 1.0f ) : offColor );
		if ( ImGui::SmallButton( "sRGB" ) ) {
			gammaEnabled = !gammaEnabled;
		}
		ImGui::PopStyleColor();
	}
	ImGui::SameLine();

	ImGui::Separator();
	ImGui::SameLine();

	ImGui::Text( "Zoom" );
	ImGui::SameLine();

	if ( ImGui::SmallButton( "1:1" ) )
	{
		autoScale = false;
		scale     = 1.0f;
	}
	ImGui::SameLine();

	if ( ImGui::SmallButton( "Fit" ) )
	{
		autoScale = true;
	}
	ImGui::SameLine();

	static const float presetScales[] = { 0.1f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f, 4.0f, 8.0f };
	static const char* presetLabels[] = { "10%", "25%", "50%", "75%", "100%", "200%", "400%", "800%", "Auto" };
	static const int   presetCount    = 9;

	char comboPreview[ 16 ];
	if ( autoScale )
	{
		sprintf_s( comboPreview, "Auto" );
	}
	else
	{
		bool matched = false;
		for ( int i = 0; i < presetCount - 1; ++i )
		{
			if ( fabsf( scale - presetScales[ i ] ) < 0.001f )
			{
				sprintf_s( comboPreview, "%s", presetLabels[ i ] );
				matched = true;
				break;
			}
		}
		if ( !matched )
		{
			sprintf_s( comboPreview, "%d%%", (int)roundf( scale * 100.0f ) );
		}
	}

	ImGui::SetNextItemWidth( 80.0f );
	if ( ImGui::BeginCombo( "##zoom", comboPreview ) )
	{
		for ( int i = 0; i < presetCount; ++i )
		{
			const bool isAuto = ( i == presetCount - 1 );
			const bool isSelected = isAuto
				? autoScale
				: ( !autoScale && fabsf( scale - presetScales[ i ] ) < 0.001f );

			if ( ImGui::Selectable( presetLabels[ i ], isSelected ) )
			{
				autoScale = isAuto;
				if ( !isAuto ) {
					scale = presetScales[ i ];
				}
			}
		}
		ImGui::EndCombo();
	}

	// --- Row: Intensity ---
	{
		const float fieldW = 50.0f;

		ImGui::Text( "Intensity" );
		ImGui::SameLine();

		ImGui::SetNextItemWidth( fieldW );
		ImGui::InputFloat( "##intMin", &intensityMin, 0.0f, 0.0f, "%.2f" );
		ImGui::SameLine();

		ImGui::SetNextItemWidth( -fieldW - ImGui::GetStyle().ItemSpacing.x );
		ImGui::SliderFloat( "##intensity", &intensity, intensityMin, intensityMax, "%.3f" );
		ImGui::SameLine();

		ImGui::SetNextItemWidth( fieldW );
		ImGui::InputFloat( "##intMax", &intensityMax, 0.0f, 0.0f, "%.2f" );

		intensityMin = Min( intensityMin, intensityMax - 0.001f );
		intensity    = Max( Min( intensity, intensityMax ), intensityMin );
	}

	// --- Row: Range ---
	{
		const float fieldW = 50.0f;

		ImGui::Text( "Range" );
		ImGui::SameLine();

		ImGui::SetNextItemWidth( fieldW );
		ImGui::InputFloat( "##rangeMin", &rangeMin, 0.0f, 0.0f, "%.2f" );
		ImGui::SameLine();

		ImGui::SetNextItemWidth( fieldW );
		ImGui::InputFloat( "##rangeMax", &rangeMax, 0.0f, 0.0f, "%.2f" );

		rangeMin = Min( rangeMin, rangeMax - 0.001f );

		ImGui::SameLine();
		if( ImGui::Button( "Auto Range" ) )
		{
			imageViewerStatistics_t stats{};
			if( QueryImageViewerStat( stats ) )
			{
				const vec3f maskedMin = Multiply( vec4f( tint ).xyz, stats.minSample.xyz );
				const vec3f maskedMax = Multiply( vec4f( tint ).xyz, stats.maxSample.xyz );

				rangeMin = Min( maskedMin );
				rangeMax = Max( maskedMax );
			}
		}
	}

	// --- Row: MIP level ---
	{
		const int mipCount = (int)image->info.mipLevels;
		selectedMip = Max( 0, Min( selectedMip, mipCount - 1 ) );

		auto MipW = [&]( int mip ) { return Max( 1u, image->info.width  >> mip ); };
		auto MipH = [&]( int mip ) { return Max( 1u, image->info.height >> mip ); };

		const char* msSuffix = isMsaa ? " MS" : "";

		char preview[ 32 ];
		snprintf( preview, sizeof( preview ), "%d  (%u x %u%s)", selectedMip, MipW( selectedMip ), MipH( selectedMip ), msSuffix );

		ImGui::Text( "MIP" );
		ImGui::SameLine();

		ImGui::BeginDisabled( mipCount <= 1 );
		ImGui::SetNextItemWidth( -1 );
		if ( ImGui::BeginCombo( "##mip", preview ) )
		{
			for ( int i = 0; i < mipCount; ++i )
			{
				char label[ 32 ];
				snprintf( label, sizeof( label ), "%d  (%u x %u%s)", i, MipW( i ), MipH( i ), msSuffix );
				if ( ImGui::Selectable( label, selectedMip == i ) ) {
					selectedMip = i;
				}
				if ( selectedMip == i ) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndDisabled();
	}

	// --- Row: Slice / Face (only when the image has multiple layers) ---
	if ( image->info.layers > 1 )
	{
		static const char* cubeFaceLabels[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
		const bool isCube = ( image->info.type == IMAGE_TYPE_CUBE );
		const int numLayers = (int)image->info.layers;

		selectedLayer = Max( 0, Min( selectedLayer, numLayers - 1 ) );

		auto LayerLabel = [&]( int i, char* buf, int bufSize )
		{
			if ( isCube ) {
				snprintf( buf, bufSize, "%s", cubeFaceLabels[ i % 6 ] );
			} else {
				snprintf( buf, bufSize, "Layer %d", i );
			}
		};

		char preview[ 16 ];
		LayerLabel( selectedLayer, preview, sizeof( preview ) );

		ImGui::Text( isCube ? "Face" : "Slice" );
		ImGui::SameLine();

		ImGui::SetNextItemWidth( -1 );
		if ( ImGui::BeginCombo( "##layer", preview ) )
		{
			for ( int i = 0; i < numLayers; ++i )
			{
				char label[ 16 ];
				LayerLabel( i, label, sizeof( label ) );
				if ( ImGui::Selectable( label, selectedLayer == i ) ) {
					selectedLayer = i;
				}
				if ( selectedLayer == i ) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}

	// --- Row: Sample (MSAA only) ---
	if ( isMsaa )
	{
		const int numSamples = (int)image->info.subsamples;
		selectedSample = Max( -1, Min( selectedSample, numSamples - 1 ) );

		const char* preview = ( selectedSample < 0 ) ? "Average" : nullptr;
		char singlePreview[ 16 ];
		if ( selectedSample >= 0 ) {
			snprintf( singlePreview, sizeof( singlePreview ), "Sample %d", selectedSample );
			preview = singlePreview;
		}

		ImGui::Text( "Sample" );
		ImGui::SameLine();

		ImGui::SetNextItemWidth( -1 );
		if ( ImGui::BeginCombo( "##sample", preview ) )
		{
			for ( int i = 0; i < numSamples; ++i )
			{
				char label[ 16 ];
				snprintf( label, sizeof( label ), "Sample %d", i );
				if ( ImGui::Selectable( label, selectedSample == i ) ) {
					selectedSample = i;
				}
				if ( selectedSample == i ) {
					ImGui::SetItemDefaultFocus();
				}
			}
			if ( ImGui::Selectable( "Average", selectedSample < 0 ) ) {
				selectedSample = -1;
			}
			if ( selectedSample < 0 ) {
				ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}

	ImGui::Separator();

	// --- Image ---
	ImGui::BeginChild( "##image", ImVec2( ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y ), 0, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse );

	const float availW   = ImGui::GetContentRegionAvail().x;
	const float displayW = autoScale ? availW : ( image->info.width  * scale );
	const float displayH = autoScale ? displayW / aspect : ( image->info.height * scale );

	ImGui::ColorButton( "button", ImVec4( 1.0f, 1.0f, 1.0f, 0.0f ), ImGuiColorEditFlags_NoTooltip, ImVec2( displayW, displayH ) );
	const ImVec2 imageMin = ImGui::GetItemRectMin();
	// Use the full intended extent, not GetItemRectMax() which is clipped to the visible window.
	const ImVec2 imageMax = ImVec2( imageMin.x + displayW, imageMin.y + displayH );

	vec2u  pixelLocation  = vec2u( 0, 0 );
	bool   isHovered      = false;
	ImVec2 pixelBoxMin    = {};
	ImVec2 pixelBoxMax    = {};

	if( ImGui::IsItemHovered() )
	{
		isHovered = true;
		ImGui::SetMouseCursor( ImGuiMouseCursor_None );

		const float wheelDelta = io.MouseWheel;
		if( wheelDelta != 0.0f )
		{
			if( autoScale )
			{
				// Preserve apparent size when transitioning from auto to fixed
				autoScale = false;
				scale = availW / (float)image->info.width;
			}
			scale *= powf( 1.1f, wheelDelta );
			scale = Max( scale, 0.01f );
		}

		// UV inside the drawn image, then map to the MIP level's pixel grid.
		const ImVec2 mouse = ImGui::GetMousePos();
		const float rangeX = imageMax.x - imageMin.x;
		const float rangeY = imageMax.y - imageMin.y;
		const float uvX = ( rangeX > 0.0f ) ? ( mouse.x - imageMin.x ) / rangeX : 0.0f;
		const float uvY = ( rangeY > 0.0f ) ? ( mouse.y - imageMin.y ) / rangeY : 0.0f;

		uint32_t w, h;
		MipDimensions( static_cast<uint32_t>( selectedMip ), image->info.width, image->info.height, &w, &h );
		pixelLocation.x = Clamp( static_cast<uint32_t>( uvX * w ), 0u, w - 1u );
		pixelLocation.y = Clamp( static_cast<uint32_t>( uvY * h ), 0u, h - 1u );

		// Screen size of one MIP pixel at current scale.
		const float pixelScreenSize = displayW / (float)w;
		pixelBoxMin = ImVec2( imageMin.x + pixelLocation.x * pixelScreenSize,
		                      imageMin.y + pixelLocation.y * pixelScreenSize );
		pixelBoxMax = ImVec2( pixelBoxMin.x + pixelScreenSize,
		                      pixelBoxMin.y + pixelScreenSize );

		vec4f color;		
		if( QueryImageViewerSample( pixelLocation, color ) )
		{
			ImGui::BeginTooltip();
			ImGui::ColorButton( "#pixelSample", ImVec4( color.x, color.y, color.z, color.w ), ImGuiColorEditFlags_NoTooltip, ImVec2( 50.0f, 50.0f ) );
			ImGui::SameLine();
			ImGui::BeginGroup();
			ImGui::Text( "x:%u, y:%u (%1.2g, %1.2g)", pixelLocation.x, pixelLocation.y, uvX, uvY  );
			ImGui::Text( "(%g, %g, %g, %g)", color.x, color.y, color.z, color.w );
			ImGui::EndGroup();
			ImGui::EndTooltip();
		}
	}

	imageViewerCallbackData_t data{};
	data.image			= image;
	data.pixelX			= pixelLocation.x;
	data.pixelY			= pixelLocation.y;
	data.x				= imageMin.x;
	data.y				= imageMin.y;
	data.width			= displayW;
	data.height			= displayH;
	data.tint			= ((vec4f)tint) * intensity;
	data.rangeMin		= rangeMin;
	data.rangeMax		= rangeMax;
	data.flags			= gammaEnabled ? 0x02 : 0x00;
	data.mipLevel		= (uint32_t)selectedMip;
	data.layer			= (uint32_t)selectedLayer;
	data.msaaSampleIndex= ( selectedSample < 0 ) ? ~0u : (uint32_t)selectedSample;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	AddImageViewerCallback( dl, data );

	// Draw pixel cursor box on top of the image (added after the callback so it renders last).
	if ( isHovered ) {
		dl->AddRect( pixelBoxMin, pixelBoxMax, IM_COL32( 255, 255, 255, 255 ) );
	}

	ImGui::EndChild(); // End image region

	ImGui::End();
}

#endif
