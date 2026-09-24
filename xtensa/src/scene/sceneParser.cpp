#include "sceneParser.h"

#include <algorithm>
#include <string>

#include "../globals/common.h"
#include "../globals/render_util.h"
#include "../render_resources/gpuImage.h"
#include "entity.h"
#include "sceneBase.h"
#include "../asset_types/gpuProgram.h"
#include "../asset_types/model.h"
#include "../asset_types/material.h"

#include "../io/io.h"
#include "../render_core/log.h"

//#define JSMN_PARENT_LINKS
#include <SysCore/jsmn.h>

static const int TOKEN_LEN = 128;

static uint8_t trashBuffer[ 131072 ];

static int jsoneq( const char* json, jsmntok_t* tok, const char* s )
{
	if ( tok->type == JSMN_STRING && (int)strlen( s ) == tok->end - tok->start &&
		strncmp( json + tok->start, s, tok->end - tok->start ) == 0 ) {
		return 0;
	}
	return -1;
}


struct parseState_t
{
	std::vector<char>*	file;
	jsmntok_t*			tokens;
	int					r;
	int					tx;
	Scene*				scene;
	AssetManager*		assets;
	jsmn_parser*		p;
};


struct enumString_t
{
	const char* name;
	int32_t		value;
};


#define MAKE_ENUM_STRING( e ) enumString_t{ #e, e }

typedef int ParseObjectFunc( parseState_t& st, void* object, uint32_t offset );


struct objectTuple_t
{
	const char*			name;
	void*				ptr;
	uint32_t			elementStride;
	uint32_t			elementCount;
	ParseObjectFunc*	func;
};


struct drawPassShader_t
{
	char		name[ TOKEN_LEN ];
	char		perms[ (uint32_t)shaderPermId_t::COUNT ][ TOKEN_LEN ];
};

void ParseArray( parseState_t& st, const objectTuple_t* objectMap );
void ParseObject( parseState_t& st, const objectTuple_t* objectMap, const uint32_t objectCount );


// Counts the total number of tokens consumed by the token at st.tokens[index],
// including all children for objects/arrays. Used to skip unknown JSON values.
static int CountTokens( const parseState_t& st, int index )
{
	if ( index >= st.r ) {
		return 0;
	}

	const jsmntok_t& tok = st.tokens[ index ];

	if ( tok.type == JSMN_OBJECT )
	{
		int count = 1;
		for ( int i = 0; i < tok.size; ++i )
		{
			count += CountTokens( st, index + count ); // key
			count += CountTokens( st, index + count ); // value
		}
		return count;
	}
	else if ( tok.type == JSMN_ARRAY )
	{
		int count = 1;
		for ( int i = 0; i < tok.size; ++i )
		{
			count += CountTokens( st, index + count ); // element
		}
		return count;
	}

	return 1; // primitive or string
}


std::string ParseCurrentToken( parseState_t& st )
{
	const uint32_t valueLen = ( st.tokens[ st.tx ].end - st.tokens[ st.tx ].start );
	return std::string( st.file->data() + st.tokens[ st.tx ].start, valueLen );
}


int ParseStringObject( parseState_t& st, void* value, uint32_t offset )
{
	assert ( st.tokens[ st.tx ].type == JSMN_STRING );

	char* s = reinterpret_cast<char*>( value ) + offset;
	const uint32_t valueLen = ( st.tokens[ st.tx ].end - st.tokens[ st.tx ].start );

	if( valueLen > 0 )
	{
		assert( valueLen < TOKEN_LEN );
		memset( s, '\0', TOKEN_LEN );
		memcpy( s, st.file->data() + st.tokens[ st.tx ].start, valueLen );
	}
	s[valueLen] = '\0';

	st.tx += 1;
	return 0;
}


int ParseFloatObject( parseState_t& st, void* value, uint32_t offset )
{
	assert( offset == 0 ); // For arrays, but untested for this type
	assert( st.tokens[ st.tx ].type == JSMN_PRIMITIVE );

	float* f = reinterpret_cast<float*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx ].end - st.tokens[ st.tx ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx ].start, valueLen );
	*f = std::stof( s );

	st.tx += 1;
	return 0;
}

struct vectorObject_t
{
	vec3f	v;
	bool	isSet;
};


// Parses one element of a 3-float camera array ( "vector": [ x, y, z ] ).
// ParseArray passes offset = elementIndex * sizeof(float).
int ParseVectorObject( parseState_t& st, void* value, uint32_t offset )
{
	assert( st.tokens[ st.tx ].type == JSMN_PRIMITIVE );

	vectorObject_t* vectorObject = reinterpret_cast< vectorObject_t*>( value );
	const uint32_t elementIndex = offset / sizeof( float );
	assert( elementIndex < 3 );

	const uint32_t valueLen = ( st.tokens[ st.tx ].end - st.tokens[ st.tx ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx ].start, valueLen );
	vectorObject->v[ elementIndex ] = std::stof( s );
	vectorObject->isSet = true;

	st.tx += 1;
	return 0;
}


template <uint32_t BIT>
int ParseFlagObject( parseState_t& st, void* value, uint32_t offset )
{
	assert( offset == 0 ); // For arrays, but untested for this type
	assert( st.tokens[ st.tx ].type == JSMN_PRIMITIVE );

	uint32_t* f = reinterpret_cast<uint32_t*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx ].end - st.tokens[ st.tx ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx ].start, valueLen );

	std::transform( s.begin(), s.end(), s.begin(),
		[]( unsigned char c ) { return std::tolower( c ); } );

	*f |= ( s == "true" || s == "1" ) ? BIT : false;

	st.tx += 1;
	return 0;
}


int ParseBoolObject( parseState_t& st, void* value, uint32_t offset )
{
	assert( offset == 0 ); // For arrays, but untested for this type
	assert( st.tokens[ st.tx ].type == JSMN_PRIMITIVE || st.tokens[ st.tx ].type == JSMN_STRING );

	bool* b = reinterpret_cast<bool*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx ].end - st.tokens[ st.tx ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx ].start, valueLen );

	std::transform( s.begin(), s.end(), s.begin(),
		[]( unsigned char c ) { return std::tolower( c ); } );

	*b = ( s == "true" || s == "1" ) ? true : false;

	st.tx += 1;
	return 0;
}


int ParseIntObject( parseState_t& st, void* value, uint32_t offset )
{
	assert( offset == 0 ); // For arrays, but untested for this type
	assert( st.tokens[ st.tx ].type == JSMN_PRIMITIVE );

	int32_t* i = reinterpret_cast<int32_t*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx ].end - st.tokens[ st.tx ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx ].start, valueLen );

	*i = std::stoi( s );

	st.tx += 1;
	return 0;
}


int ParseUIntObject( parseState_t& st, void* value, uint32_t offset )
{
	assert( offset == 0 ); // For arrays, but untested for this type
	assert( st.tokens[ st.tx ].type == JSMN_PRIMITIVE );

	uint32_t* u = reinterpret_cast<uint32_t*>( value );
	const uint32_t valueLen = ( st.tokens[ st.tx ].end - st.tokens[ st.tx ].start );
	std::string s = std::string( st.file->data() + st.tokens[ st.tx ].start, valueLen );

	*u = std::stoul( s );

	st.tx += 1;
	return 0;
}


int ParseImageObject( parseState_t& st, void* object, uint32_t offset )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return 0;
	}

	AssetLib<Image>& textureLib = *st.assets->GetLib<Image>();

	bool isLinear = false;
	char name[TOKEN_LEN] = "";
	char type[TOKEN_LEN] = "";

	const uint32_t objectCount = 3;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name",		&name,			TOKEN_LEN,		1,	&ParseStringObject },
		{ "type",		&type,			TOKEN_LEN,		1,	&ParseStringObject },
		{ "isLinear",	&isLinear,		sizeof( bool ),	1,	&ParseBoolObject }
	};

	ParseObject( st, objectMap, objectCount );

	ImageLoader* loader = new ImageLoader();
	loader->SetBasePath( TexturePath );
	loader->SetTextureFile( name );
	loader->LoadAsCubemap( strcmp( type, "CUBE" ) == 0 ? true : false );
	loader->LoadAsLinear( isLinear );

	textureLib.AddDeferred( name, Asset<Image>::loadHandlerPtr_t( loader ) );

	return st.tx;
}


int ParseDrawPassShaderObject( parseState_t& st, void* value, uint32_t offset )
{
	st.tx += 1;
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return 0;
	}

	drawPassShader_t* passState = reinterpret_cast<drawPassShader_t*>( value );
	memset( passState, 0, sizeof( drawPassShader_t ) );

	const uint32_t objectCount = 2;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "shader",		&passState->name,	TOKEN_LEN,	1,	&ParseStringObject },
		{ "perms",		&passState->perms,	TOKEN_LEN,	1,	&ParseStringObject },
	};

	ParseObject( st, objectMap, objectCount );
	return 0;
}


int ParseMaterialShaderObject( parseState_t& st, void* object, uint32_t offset )
{
	st.tx += 1;
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return 0;
	}

	Material* material = reinterpret_cast<Material*>( object );

	const uint32_t objectCount = 9;
	drawPassShader_t drawPassShader[ objectCount ] = {};

	static const enumString_t enumMap[ objectCount ] =
	{
		MAKE_ENUM_STRING( DRAWPASS_SHADOW ),
		MAKE_ENUM_STRING( DRAWPASS_PREPASS ),
		MAKE_ENUM_STRING( DRAWPASS_OPAQUE ),
		MAKE_ENUM_STRING( DRAWPASS_TRANS ),
		MAKE_ENUM_STRING( DRAWPASS_TERRAIN ),
		MAKE_ENUM_STRING( DRAWPASS_DEBUG_WIREFRAME ),
		MAKE_ENUM_STRING( DRAWPASS_SKYBOX ),
		MAKE_ENUM_STRING( DRAWPASS_2D ),
		MAKE_ENUM_STRING( DRAWPASS_DEBUG_3D ),
	};

	const objectTuple_t objectMap[ objectCount ] =
	{
		{ enumMap[ 0 ].name,	&drawPassShader[ 0 ],	sizeof( drawPassShader_t ),	1,	&ParseDrawPassShaderObject },
		{ enumMap[ 1 ].name,	&drawPassShader[ 1 ],	sizeof( drawPassShader_t ),	1,	&ParseDrawPassShaderObject },
		{ enumMap[ 2 ].name,	&drawPassShader[ 2 ],	sizeof( drawPassShader_t ),	1,	&ParseDrawPassShaderObject },
		{ enumMap[ 3 ].name,	&drawPassShader[ 3 ],	sizeof( drawPassShader_t ),	1,	&ParseDrawPassShaderObject },
		{ enumMap[ 4 ].name,	&drawPassShader[ 4 ],	sizeof( drawPassShader_t ),	1,	&ParseDrawPassShaderObject },
		{ enumMap[ 5 ].name,	&drawPassShader[ 5 ],	sizeof( drawPassShader_t ),	1,	&ParseDrawPassShaderObject },
		{ enumMap[ 6 ].name,	&drawPassShader[ 6 ],	sizeof( drawPassShader_t ),	1,	&ParseDrawPassShaderObject },
		{ enumMap[ 7 ].name,	&drawPassShader[ 7 ],	sizeof( drawPassShader_t ),	1,	&ParseDrawPassShaderObject },
		{ enumMap[ 8 ].name,	&drawPassShader[ 8 ],	sizeof( drawPassShader_t ),	1,	&ParseDrawPassShaderObject },
	};

	ParseObject( st, objectMap, objectCount );

	for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
	{
		if ( drawPassShader[ mapIx ].name[ 0 ] == '\0' ) {
			continue;
		}

		uint32_t permSet = 0;
		for ( uint32_t i = 0; i < (uint32_t)shaderPermId_t::COUNT; ++i )
		{
			if ( drawPassShader[ mapIx ].perms[ i ][ 0 ] == '\0' ) {
				break;
			}
			permSet |= static_cast<uint32_t>( GetPermId( drawPassShader[ mapIx ].perms[ i ] ) );
		}

		material->AddShader( drawPass_t( enumMap[ mapIx ].value ), AssetLib<GpuProgram>::Handle( drawPassShader[ mapIx ].name ), permSet );
	}
	return 0;
}


int ParseMaterialTextureObject( parseState_t& st, void* object, uint32_t offset )
{
	st.tx += 1;
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return 0;
	}

	Material* material = reinterpret_cast<Material*>( object );

	const uint32_t objectCount = 36;
	char s[ objectCount ][TOKEN_LEN] = {};

	static const enumString_t enumMap[ objectCount ] =
	{
		MAKE_ENUM_STRING( GGX_ALBEDO_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_NORMAL_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_ROUGHNESS_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_METALLIC_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_AO_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_EMISSIVE_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_CC_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_CC_ROUGHNESS_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_CC_NML_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_SHEEN_COLOR_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_SHEEN_ROUGHNESS_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_ANISOTROPY_MAP_SLOT ),
		MAKE_ENUM_STRING( GGX_TRANSMISSION_MAP_SLOT ),
		MAKE_ENUM_STRING( BLINN_PHONG_COLOR_MAP_SLOT ),
		MAKE_ENUM_STRING( BLINN_PHONG_NORMAL_MAP_SLOT ),
		MAKE_ENUM_STRING( BLINN_PHONG_SPEC_MAP_SLOT ),
		MAKE_ENUM_STRING( BLINN_PHONG_GLOSS_MAP_SLOT ),
		MAKE_ENUM_STRING( BLINN_PHONG_EMISSIVE_MAP_SLOT ),
		MAKE_ENUM_STRING( HGT_COLOR_MAP_SLOT0 ),
		MAKE_ENUM_STRING( HGT_COLOR_MAP_SLOT1 ),
		MAKE_ENUM_STRING( HGT_HEIGHT_MAP_SLOT ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_0 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_1 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_2 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_3 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_4 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_5 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_6 ),
		MAKE_ENUM_STRING( TEXTURE_SLOT_7 ),
		MAKE_ENUM_STRING( CUBE_RIGHT_MAP_SLOT ),
		MAKE_ENUM_STRING( CUBE_LEFT_MAP_SLOT ),
		MAKE_ENUM_STRING( CUBE_TOP_MAP_SLOT ),
		MAKE_ENUM_STRING( CUBE_BOTTOM_MAP_SLOT ),
		MAKE_ENUM_STRING( CUBE_FRONT_MAP_SLOT ),
		MAKE_ENUM_STRING( CUBE_BACK_MAP_SLOT ),
	};

	static const objectTuple_t objectMap[ objectCount ] =
	{
		{ enumMap[ 0 ].name,	&s[ 0 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 1 ].name,	&s[ 1 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 2 ].name,	&s[ 2 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 3 ].name,	&s[ 3 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 4 ].name,	&s[ 4 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 5 ].name,	&s[ 5 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 6 ].name,	&s[ 6 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 7 ].name,	&s[ 7 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 8 ].name,	&s[ 8 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 9 ].name,	&s[ 9 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 10 ].name,	&s[ 10 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 11 ].name,	&s[ 11 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 12 ].name,	&s[ 12 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 13 ].name,	&s[ 13 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 14 ].name,	&s[ 14 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 15 ].name,	&s[ 15 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 16 ].name,	&s[ 16 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 17 ].name,	&s[ 17 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 18 ].name,	&s[ 18 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 19 ].name,	&s[ 19 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 20 ].name,	&s[ 20 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 21 ].name,	&s[ 21 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 22 ].name,	&s[ 22 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 23 ].name,	&s[ 23 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 24 ].name,	&s[ 24 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 25 ].name,	&s[ 25 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 26 ].name,	&s[ 26 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 27 ].name,	&s[ 27 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 28 ].name,	&s[ 28 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 29 ].name,	&s[ 29 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 30 ].name,	&s[ 30 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 31 ].name,	&s[ 31 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 32 ].name,	&s[ 32 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 33 ].name,	&s[ 33 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 34 ].name,	&s[ 34 ],	TOKEN_LEN,	1,	&ParseStringObject },
		{ enumMap[ 35 ].name,	&s[ 35 ],	TOKEN_LEN,	1,	&ParseStringObject },
	};

	ParseObject( st, objectMap, objectCount );

	for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
	{
		if ( s[ mapIx ][0] == '\0' ) {
			continue;
		}
		material->AddTexture( enumMap[ mapIx ].value, AssetLib<Image>::Handle( s[ mapIx ] ) );
	}
	return 0;
}


int ParseModelObject( parseState_t& st, void* object, uint32_t offset )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	using loader_t = Asset<Model>::loadHandlerPtr_t;

	struct modelObjectData_t
	{
		char		name[ TOKEN_LEN ];
		char		modelName[ TOKEN_LEN ];
		char		s[3][ TOKEN_LEN ];
		float		f[3];
		int			i[3];
	};

	modelObjectData_t od;

	const uint32_t objectCount = 11;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name",		&od.name,		TOKEN_LEN,		1,	&ParseStringObject },
		{ "model",		&od.modelName,	TOKEN_LEN,		1,	&ParseStringObject },
		{ "argStr0",	&od.s[0],		TOKEN_LEN,		1,	&ParseStringObject },
		{ "argStr1",	&od.s[1],		TOKEN_LEN,		1,	&ParseStringObject },
		{ "argStr2",	&od.s[2],		TOKEN_LEN,		1,	&ParseStringObject },
		{ "argInt0",	&od.i[0],		sizeof( int ),	1,	&ParseIntObject },
		{ "argInt1",	&od.i[1],		sizeof( int ),	1,	&ParseIntObject },
		{ "argInt2",	&od.i[2],		sizeof( int ),	1,	&ParseIntObject },
		{ "argFlt0",	&od.f[0],		sizeof( float ),1,	&ParseFloatObject },
		{ "argFlt1",	&od.f[1],		sizeof( float ),1,	&ParseFloatObject },
		{ "argFlt2",	&od.f[2],		sizeof( float ),1,	&ParseFloatObject },
	};

	ParseObject( st, objectMap, objectCount );

	if ( strcmp( od.modelName, "_skybox" ) == 0 ) {
		st.assets->GetLib<Model>()->AddDeferred( od.name, loader_t( new SkyBoxLoader() ) );
	}
	else if ( strcmp( od.modelName, "_terrain" ) == 0 )
	{
		hdl_t handle = AssetLib<Material>::Handle( od.s[0] );
		st.assets->GetLib<Model>()->AddDeferred( od.name, loader_t( new TerrainLoader( od.i[0], od.i[1], od.f[0], od.f[1], handle ) ) );
	}
	else 
	{
		ModelLoader* loader = new ModelLoader();
		loader->SetModelPath( ModelPath );
		loader->SetTexturePath( TexturePath );
		loader->SetModelName( od.modelName );
		loader->SetAssetRef( st.assets );
		st.assets->GetLib<Model>()->AddDeferred( od.name, loader_t( loader ) );
	}
	return st.tx;
}


int ParseEntityObject( parseState_t& st, void* object, uint32_t offset )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	char name[ TOKEN_LEN ] = "";
	char modelName[ TOKEN_LEN ] = "";
	char materialName[ TOKEN_LEN ] = "";
	std::vector<Entity*>& entities = st.scene->entities;

	bool hidden = false, wireframe = false;

	vectorObject_t xyz;
	vectorObject_t rotation;
	vectorObject_t scale;

	scale.v = vec3f( 1.0f, 1.0f, 1.0f );

	const uint32_t objectCount = 12;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name",		&name,			TOKEN_LEN,			1,	&ParseStringObject },
		{ "model",		&modelName,		TOKEN_LEN,			1,	&ParseStringObject },
		{ "material",	&materialName,	TOKEN_LEN,			1,	&ParseStringObject },
		{ "xyz",		&xyz,			sizeof( float ),	3,	&ParseVectorObject },
		{ "rotation",	&rotation,		sizeof( float ),	3,	&ParseVectorObject },
		{ "scale",		&scale,			sizeof( float ),	3,	&ParseVectorObject },
		{ "hidden",		&hidden,		sizeof( bool ),		1,	&ParseBoolObject },
		{ "wireframe",	&wireframe,		sizeof( bool ),		1,	&ParseBoolObject },
	};

	ParseObject( st, objectMap, objectCount );

	Entity* ent = new Entity();
	ent->name = name;
	ent->modelHdl = AssetLib<Model>::Handle( modelName );
	
	if ( strcmp( modelName, "_skybox" ) == 0 ) {
		ent->modelHdl = st.assets->GetLib<Model>()->AddDeferred( modelName, loader_t( new SkyBoxLoader() ) );
	}
	else if( st.assets->GetLib<Model>()->Find( ent->modelHdl ) == st.assets->GetLib<Model>()->GetDefault() )
	{
		ModelLoader* loader = new ModelLoader();
		loader->SetModelPath( ModelPath );
		loader->SetTexturePath( TexturePath );
		loader->SetModelName( modelName );
		loader->SetAssetRef( st.assets );
		ent->modelHdl = st.assets->GetLib<Model>()->AddDeferred( modelName, loader_t( loader ) );
	}
	ent->SetOrigin( xyz.v );
	ent->SetRotation( rotation.v );
	ent->SetScale( scale.v );
	if ( strcmp( materialName, "" ) != 0 ) {
		ent->materialHdl = AssetLib<Material>::Handle( materialName );
	}
	if ( hidden ) {
		ent->SetFlag( ENT_FLAG_NO_DRAW );
	}
	if ( wireframe ) {
		ent->SetFlag( ENT_FLAG_WIREFRAME );
	}
	st.scene->entities.push_back( ent );

	return st.tx;
}


int ParseMaterialObject( parseState_t& st, void* object, uint32_t offset )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	AssetLib<Material>* materials = reinterpret_cast<AssetLib<Material>*>( object );

	Material m;
	materialParms_t mParms;
	char name[ TOKEN_LEN ] = "";

	// TODO: allow parse functions that aren't just primitives. Need objects/array reading
	const uint32_t objectCount = 35;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name",				&name,						TOKEN_LEN,			1,	&ParseStringObject },
		{ "KaR",				&mParms.Ka.r,				sizeof( float ),	1,	&ParseFloatObject },
		{ "KaG",				&mParms.Ka.g,				sizeof( float ),	1,	&ParseFloatObject },
		{ "KaB",				&mParms.Ka.b,				sizeof( float ),	1,	&ParseFloatObject },
		{ "KeR",				&mParms.Ke.r,				sizeof( float ),	1,	&ParseFloatObject },
		{ "KeG",				&mParms.Ke.g,				sizeof( float ),	1,	&ParseFloatObject },
		{ "KeB",				&mParms.Ke.b,				sizeof( float ),	1,	&ParseFloatObject },
		{ "KdR",				&mParms.albedo.r,			sizeof( float ),	1,	&ParseFloatObject },
		{ "KdG",				&mParms.albedo.g,			sizeof( float ),	1,	&ParseFloatObject },
		{ "KdB",				&mParms.albedo.b,			sizeof( float ),	1,	&ParseFloatObject },
		{ "KsR",				&mParms.Ks.r,				sizeof( float ),	1,	&ParseFloatObject },
		{ "KsG",				&mParms.Ks.g,				sizeof( float ),	1,	&ParseFloatObject },
		{ "KsB",				&mParms.Ks.b,				sizeof( float ),	1,	&ParseFloatObject },
		{ "TfR",				&mParms.Tf.r,				sizeof( float ),	1,	&ParseFloatObject },
		{ "TfG",				&mParms.Tf.g,				sizeof( float ),	1,	&ParseFloatObject },
		{ "TfB",				&mParms.Tf.b,				sizeof( float ),	1,	&ParseFloatObject },
		{ "opacity",			&mParms.opacity,			sizeof( float ),	1,	&ParseFloatObject },
		{ "ns",					&mParms.Ns,					sizeof( float ),	1,	&ParseFloatObject },
		{ "ior",				&mParms.ior,				sizeof( float ),	1,	&ParseFloatObject },
		{ "illum",				&mParms.illum,				sizeof( float ),	1,	&ParseFloatObject },
		{ "roughness",			&mParms.roughness,			sizeof( float ),	1,	&ParseFloatObject },
		{ "metalness",			&mParms.metalness,			sizeof( float ),	1,	&ParseFloatObject },
		{ "clearcoatWeight",	&mParms.clearcoatWeight,	sizeof( float ),	1,	&ParseFloatObject },
		{ "clearcoatRoughness",	&mParms.clearcoatRoughness,	sizeof( float ),	1,	&ParseFloatObject },
		{ "anisotropy",			&mParms.anisotropy,			sizeof( float ),	1,	&ParseFloatObject },
		{ "anisotropyRotation",	&mParms.anisotropyRotation,	sizeof( float ),	1,	&ParseFloatObject },
		{ "alphaCutoff",		&mParms.alphaCutoff,		sizeof( float ),	1,	&ParseFloatObject },
		{ "sheenColorR",		&mParms.sheenColor.r,		sizeof( float ),	1,	&ParseFloatObject },
		{ "sheenColorG",		&mParms.sheenColor.g,		sizeof( float ),	1,	&ParseFloatObject },
		{ "sheenColorB",		&mParms.sheenColor.b,		sizeof( float ),	1,	&ParseFloatObject },
		{ "sheen",				&mParms.sheenRoughness,				sizeof( float ),	1,	&ParseFloatObject },
		{ "transmissionFactor",	&mParms.transmissionFactor,	sizeof( float ),	1,	&ParseFloatObject },
		{ "emissiveStrength",	&mParms.emissiveStrength,	sizeof( float ),	1,	&ParseFloatObject },
		{ "shaders",			&m,							sizeof( Material ),	1,	&ParseMaterialShaderObject },
		{ "textures",			&m,							sizeof( Material ),	1,	&ParseMaterialTextureObject },
	};

	ParseObject( st, objectMap, objectCount );

	std::string fullname = name;
	std::string fileName;
	std::string ext;
	SysCore::SplitFileName( name, fileName, ext );

	const bool isMtlFile = ( ext == "mtl" );
	if ( isMtlFile )
	{
		MaterialLoader* loader = new MaterialLoader();
		loader->SetAssetRef( st.assets );
		loader->SetMaterialPath( ModelPath );
		loader->SetTexturePath( TexturePath );
		loader->SetFileName( fullname );
		materials->AddDeferred( fullname.c_str(), pMatLoader_t( loader ) );
	}
	else
	{
		m.SetParms( mParms );
		materials->Add( name, m );
	}

	return st.tx;
}


int ParseShaderObject( parseState_t& st, void* object, uint32_t offset )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return -1;
	}

	constexpr static uint32_t UniquePermCount = static_cast<uint32_t>( shaderPermId_t::COUNT );

	char name[ TOKEN_LEN ] = "";
	char shaderNames[ shaderType_t::COUNT ][ TOKEN_LEN ] = {};
	char bindSet[ TOKEN_LEN ] = "";
	char perms[ UniquePermCount ][ TOKEN_LEN ] = {};
	shaderFlags_t shaderFlags = shaderFlags_t::NONE;
	AssetLib<GpuProgram>* shaders = reinterpret_cast<AssetLib<GpuProgram>*>( object );

	const uint32_t objectCount = 13;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "name",			&name,											TOKEN_LEN,					1,	&ParseStringObject },
		{ "vs",				&shaderNames[ shaderType_t::VERTEX ],			TOKEN_LEN,					1,	&ParseStringObject },
		{ "ps",				&shaderNames[ shaderType_t::PIXEL ],			TOKEN_LEN,					1,	&ParseStringObject },
		{ "cs",				&shaderNames[ shaderType_t::COMPUTE ],			TOKEN_LEN,					1,	&ParseStringObject },
		{ "rgen",			&shaderNames[ shaderType_t::RT_GEN ],			TOKEN_LEN,					1,	&ParseStringObject },
		{ "rmiss",			&shaderNames[ shaderType_t::RT_MISS ],			TOKEN_LEN,					1,	&ParseStringObject },
		{ "rint",			&shaderNames[ shaderType_t::RT_INTERSECTION ],	TOKEN_LEN,					1,	&ParseStringObject },
		{ "rchit",			&shaderNames[ shaderType_t::RT_CLOSEST_HIT ],	TOKEN_LEN,					1,	&ParseStringObject },
		{ "rahit",			&shaderNames[ shaderType_t::RT_ANY_HIT ],		TOKEN_LEN,					1,	&ParseStringObject },
		{ "rcall",			&shaderNames[ shaderType_t::RT_CALLABLE ],		TOKEN_LEN,					1,	&ParseStringObject },
		{ "bindset",		&bindSet,										TOKEN_LEN,					1,	&ParseStringObject },
		{ "perms",			&perms,											TOKEN_LEN,					1,	&ParseStringObject },	// NOTE: works for arrays via ParseArray string-element path
		{ "no_vb",			&shaderFlags,									sizeof( shaderFlags_t ),	1,	&ParseFlagObject<(uint32_t)shaderFlags_t::NO_VERTEX_BUFFER> }
	};

	ParseObject( st, objectMap, objectCount );

	if ( strcmp( bindSet, "bindset_imageShader" ) == 0 ) {
		shaderFlags |= shaderFlags_t::IMAGE_SHADER;
	}

	GpuProgramLoader* loader = new GpuProgramLoader();
	loader->SetSourcePath( "shaders/" );
	loader->SetBinPath( "shaders/bin/" );
	loader->SetCompilerPath( "scripts/" );
	shaderFileNames_t fileNames;
	for ( uint32_t i = 0; i < shaderType_t::COUNT; ++i ) {
		fileNames.names[ i ] = shaderNames[ i ];
	}
	loader->AddFilePaths( fileNames );
	loader->SetBindSet( bindSet );

	for( uint32_t i = 0; i < UniquePermCount; ++i ) {
		loader->AddPerm( perms[ i ] );
	}

	loader->SetFlags( shaderFlags );
	shaders->AddDeferred( name, Asset<GpuProgram>::loadHandlerPtr_t( loader ) );

	return st.tx;
}


void ParseObject( parseState_t& st, const objectTuple_t* objectMap, const uint32_t objectCount )
{
	if ( st.tokens[ st.tx ].type != JSMN_OBJECT ) {
		return;
	}

	int itemsFound = 0;
	int itemsCount = st.tokens[ st.tx ].size;

	st.tx += 1;

	while ( ( itemsFound < itemsCount ) && ( st.tx < st.r ) )
	{
		bool found = false;
		for ( uint32_t mapIx = 0; mapIx < objectCount; ++mapIx )
		{
			if ( jsoneq( st.file->data(), &st.tokens[ st.tx ], objectMap[ mapIx ].name ) != 0 ) {
				continue;
			}

			const jsmntype_t elemType = st.tokens[ st.tx + 1 ].type;

			if ( elemType == JSMN_ARRAY )
			{
				st.tx += 1; // Move past key
				ParseArray( st, &objectMap[ mapIx ] );
			}
			else if( ( elemType == JSMN_PRIMITIVE ) || ( elemType == JSMN_STRING ) )
			{
				st.tx += 1; // Move past key
				( *objectMap[ mapIx ].func )( st, objectMap[ mapIx ].ptr, 0 );
			}
			else
			{
				( *objectMap[ mapIx ].func )( st, objectMap[ mapIx ].ptr, 0 );
			}

			++itemsFound;
			found = true;
			break;
		}

		if( found == false )
		{
			const std::string s = ParseCurrentToken( st );
			LogMsg( "Scene", logSeverity_t::Warning, "No matching parse function for '%s'", s.c_str() );
			st.tx += 1; // skip the key
			st.tx += CountTokens( st, st.tx ); // skip the value (handles nested objects/arrays)
			++itemsFound;
		}
	}
}


void ParseArray( parseState_t& st, const objectTuple_t* objectMap )
{
	if ( st.tokens[ st.tx ].type != JSMN_ARRAY ) {
		return;
	}

	ParseObjectFunc* readFunc = objectMap->func;
	void* object = objectMap->ptr;

	int itemsFound = 0;
	int itemsCount = st.tokens[ st.tx ].size;

	std::string arrayString = ParseCurrentToken( st );

	st.tx += 1;

	while ( ( itemsFound < itemsCount ) && ( st.tx < st.r ) )
	{
		const jsmntype_t elemType = st.tokens[ st.tx ].type;

		if ( ( elemType == JSMN_STRING ) || ( elemType == JSMN_PRIMITIVE ) )
		{
			( *readFunc )( st, object, itemsFound * objectMap->elementStride );
		}
		else
		{
			int32_t ret = ( *readFunc )( st, object, itemsFound * objectMap->elementStride );
			if ( ret > 0 )
			{
				st.tx = ret;
			}
			else
			{
				LogMsg( "Scene", logSeverity_t::Warning, "ParseArray: skipping unparseable object element in '%s'", arrayString.c_str() );
				st.tx += CountTokens( st, st.tx );
			}
		}
		++itemsFound;
	}
}


static void CleanupParseState( parseState_t& st )
{
	delete st.p;
	st.p = nullptr;
	delete[] st.tokens;
	st.tokens = nullptr;
}


void ParseJson( const std::string& fileName, Scene** scene, AssetManager* assets, sceneInitializerCallback_t* sceneInitializer )
{
	LOG_SCOPE_SYSTEM( Scene );

	assert( assets != nullptr );
	assert( scene != nullptr );

	std::vector<char> file = SysCore::ReadTextFile( ScenePath + fileName );

	const int maxTokens = 8192;

	parseState_t st;
	st.file = &file;
	st.p = new jsmn_parser;
	st.tokens = new jsmntok_t[ maxTokens ];
	st.tx = 1;
	st.assets = assets;

	jsmn_init( st.p );
	st.r = jsmn_parse( st.p, st.file->data(), static_cast<uint32_t>( st.file->size() ), st.tokens, maxTokens );
	if ( st.r < 0 ) {
		LogMsg( logSeverity_t::Error, "Failed to parse JSON: %s (error: %d)", fileName.c_str(), st.r );
		CleanupParseState( st );
		return;
	}

	if ( st.r < 1 || st.tokens[ 0 ].type != JSMN_OBJECT ) {
		LogMsg( logSeverity_t::Error, "Object expected: %s", fileName.c_str() );
		CleanupParseState( st );
		return;
	}

	if( sceneInitializer != nullptr )
	{
		char stringBuffer[ TOKEN_LEN ] = "";

		while( st.tx < st.r )
		{
			const int items = st.tokens[ st.tx ].size;
			if ( jsoneq( file.data(), &st.tokens[ st.tx ], "sceneClass" ) == 0 )
			{
				st.tx += 1;
				sceneInitializer( ParseCurrentToken( st ), scene );
			}
			else if ( jsoneq( file.data(), &st.tokens[ st.tx ], "reflink" ) == 0 )
			{
				st.tx += 1;
				ParseJson( ParseCurrentToken( st ), scene, assets, nullptr );
			}
			else {
				st.tx += 1;
			}
			st.tx += items;
		}
	}

	st.tx = 0;
	st.scene = *scene;

	char skyName[ TOKEN_LEN ] = "";

	vectorObject_t cameraPosition = { vec3f( 0.0f, 0.0f, 0.0f ), false };

	const uint32_t trashBufferSize = COUNTARRAY( trashBuffer );

	assert( trashBufferSize >= file.size() );

	const uint32_t objectCount = 10;
	const objectTuple_t objectMap[ objectCount ] =
	{
		{ "sceneClass",		&trashBuffer,						trashBufferSize,					1,										&ParseStringObject },
		{ "type",			&trashBuffer,						trashBufferSize,					1,										&ParseStringObject },
		{ "reflink",		&trashBuffer,						trashBufferSize,					1,										&ParseStringObject },
		{ "skyName",		&skyName,							TOKEN_LEN,							1,										&ParseStringObject },
		{ "camera",			&cameraPosition,					sizeof( float ),					3,										&ParseVectorObject },
		{ "shaders",		st.assets->GetLib<GpuProgram>(),	sizeof( AssetLib<GpuProgram>* ),	1,										&ParseShaderObject },
		{ "images",			st.assets->GetLib<Image>(),			sizeof( AssetLib<Image>* ),			1,										&ParseImageObject },
		{ "materials",		st.assets->GetLib<Material>(),		sizeof( AssetLib<Material>* ),		1,										&ParseMaterialObject },
		{ "models",			st.assets->GetLib<Model>(),			sizeof( AssetLib<Model>* ),			1,										&ParseModelObject },
		{ "entities",		&st.scene->entities,				sizeof( Entity ),					(uint32_t)st.scene->entities.size(),	&ParseEntityObject },
	};

	ParseObject( st, objectMap, objectCount );

	if( sceneInitializer != nullptr )
	{
		std::string skyBaseName = std::string( skyName );

		const bool hasSky = ( skyBaseName.empty() == false );

		if( hasSky )
		{
			const uint32_t skyImageCount = 3;

			const std::string codeImagePath = TexturePath + CodeAssetPath;
			const std::string skyImages[ skyImageCount ] =
			{
				skyBaseName + "_env.img",
				skyBaseName + "_diffuseIbl.img",
				skyBaseName + "_specIbl.img"
			};

			( *scene )->envMap = skyImages[ 0 ];
			( *scene )->diffuseIblMap = skyImages[ 1 ];
			( *scene )->specIblMap = skyImages[ 2 ];

			for( uint32_t skyImageIx = 0; skyImageIx < skyImageCount; ++skyImageIx )
			{
				st.assets->GetLib<Image>()->AddDeferred(
					skyImages[ skyImageIx ].c_str(),
					pImgLoader_t( new ImageLoader( codeImagePath, skyImages[ skyImageIx ], false ) )
				);
			}
		}
		
		if( cameraPosition.isSet && ( ( *scene )->mainCamera != nullptr ) )
		{
			( *scene )->mainCamera->SetPosition( cameraPosition.v );
		}
	}

	CleanupParseState( st );
}


std::string SceneJsonPath( const std::string& sceneName )
{
	return sceneName + "/" + sceneName + ".json";
}


void LoadScene( std::string fileName, Scene** scene, AssetManager* assets, sceneInitializerCallback_t* sceneInitializer )
{
	{
		SCOPED_TIMER_PRINT( ParseScene, MILLISECOND );
		ParseJson( fileName, scene, assets, sceneInitializer );
	}

	{
		SCOPED_TIMER_PRINT( LoadAssets, MILLISECOND );
		g_assets.RunLoadLoop();
	}
}
