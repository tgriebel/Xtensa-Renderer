#pragma once

#include <vector>
#include <chrono>

#include "camera.h"
#include <gfxcore/core/common.h>
#include <gfxcore/math/vector.h>
#include <gfxcore/image/color.h>
#include "../asset_types/image.h"
#include "../asset_types/material.h"
#include "../asset_types/gpuProgram.h"
#include "../asset_types/model.h"
#include "../asset_types/assetLib.h"
#include "assetManager.h"

class Entity;


enum lightType_t : uint32_t
{
	LIGHT_TYPE_POINT		= 0,
	LIGHT_TYPE_DIRECTIONAL	= 1,
	LIGHT_TYPE_SPOT			= 2,
//	LIGHT_TYPE_AREA			= 3,
};

enum lightFlags_t : uint16_t
{
	LIGHT_FLAGS_NONE	= 0,
	LIGHT_FLAGS_HIDDEN	= ( 1 << 0 ),
	LIGHT_FLAGS_SHADOW	= ( 1 << 1 ),
	LIGHT_FLAGS_POINT	= ( 1 << 2 ),
	LIGHT_FLAGS_ALL		= 0XFF,
};
DEFINE_ENUM_OPERATORS( lightFlags_t, uint16_t )


struct light_t
{
	vec4f			pos;
	vec4f			dir;
	Color			color;
	float			intensity;
	lightType_t		type;
	lightFlags_t	flags;
};

struct Ray;

extern AssetManager g_assets;

class Scene
{
private:
	using chronoClock_t = std::chrono::high_resolution_clock;
	chronoClock_t::time_point	prevTime;
	std::chrono::nanoseconds	dt;
	std::chrono::nanoseconds	totalTime;
	uint64_t					frameNumber;
public:
	Camera*						mainCamera;
	Camera						camera2D;
	Camera						cameras[ 7 ]; // TODO: There isn't much value having a generic set of cameras. This should be defined by the child-class scene instead
	std::string					envMap;
	std::string					diffuseIblMap;
	std::string					specIblMap;
	std::vector<Entity*>		entities;
	std::vector<light_t>		lights;
	float						defaultNear = 0.1f;
	float						defaultFar = 1000.0f;
	Entity*						selectedEntity = nullptr;

	virtual void Update() {}
	virtual void Init() {}
	virtual void Shutdown() {}

	inline void AdvanceFrame()
	{
		const chronoClock_t::time_point currentTime = chronoClock_t::now();
		dt = ( currentTime - prevTime );
		prevTime = currentTime;

		totalTime += dt;

		++frameNumber;
	}

	inline float DeltaTime() const
	{
		return std::chrono::duration<float, std::chrono::seconds::period>( dt ).count();
	}

	inline std::chrono::nanoseconds DeltaNano() const
	{
		return dt;
	}

	inline float TotalTimeSeconds() const
	{
		return std::chrono::duration<float, std::chrono::seconds::period>( totalTime ).count();
	}

	inline uint64_t CurrentFrame() const
	{
		return frameNumber;
	}

	Scene()
	{
		for( uint32_t i = 0; i < 7; ++i )
		{
			cameras[ i ] = Camera( vec4f( 0.0f, 0.0f, 0.0f, 0.0f ) );
			cameras[ i ].SetClip( defaultNear, defaultFar );
			cameras[ i ].SetFov( Radians( 90.0f ), 1.0f );
		}
		cameras[ IMAGE_CUBE_FACE_X_POS + 1 ].Pan( 0.0f * PI );
		cameras[ IMAGE_CUBE_FACE_Y_POS + 1 ].Pan( 0.5f * PI );
		cameras[ IMAGE_CUBE_FACE_X_NEG + 1 ].Pan( 1.0f * PI );
		cameras[ IMAGE_CUBE_FACE_Y_NEG + 1 ].Pan( 1.5f * PI );
		cameras[ IMAGE_CUBE_FACE_Z_POS + 1 ].Tilt( -0.5f * PI );
		cameras[ IMAGE_CUBE_FACE_Z_NEG + 1 ].Tilt( 0.5f * PI );

		mainCamera = &cameras[ 0 ];

		prevTime = chronoClock_t::now();
		dt = std::chrono::nanoseconds( 0 );
		totalTime = std::chrono::nanoseconds( 0 );
	}

	void			CreateEntityBounds( const hdl_t modelHdl, Entity& entity );
	Entity*			GetTracedEntity( const Ray& ray );

	uint32_t		EntityCount() const;
	Entity*			FindEntity( const uint32_t entityIx );
	const Entity*	FindEntity( const uint32_t entityIx ) const ;
	Entity*			FindEntity( const char* name );
	const Entity*	FindEntity( const char* name ) const;

	Image*			GetSkyBoxImage() const;
};
