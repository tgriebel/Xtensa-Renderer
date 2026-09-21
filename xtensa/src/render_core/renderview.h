#pragma once
#include <cstdint>
#include "../scene/sceneBase.h"
#include "../globals/common.h"
#include "../globals/drawGroup.h"
#include "../draw_passes/drawpass.h"

#define SHADER_STRUCTS_CPP
#include "../../shaders/gpuShared.h"

class ResourceContext;
class RenderContext;

static const uint32_t MaxMultiViews = 6; // Max of 6 because of cubemaps

// Defines how a render view draws geometry to an image
// These are mutually exclusive since they define specific data-flows
enum class renderViewMode_t : uint32_t
{
	SHADOW		= 0,	// Rasterize once (depth only)
	FORWARD		= 1,	// Rasterize opaques twice (depth prepass + forward pass)
	DEFERRED	= 2,	// Rasterize once (gbuffer prepass / transparent pass)
	VISIBILITY	= 3,	// Rasterize once (visibility buffer prepass / transparent pass)
	DRAW_2D		= 4,	// Draw only 2D elements (UI, overlays, etc)
	COUNT,
	UNKNOWN,
};

// How many unique framebuffers can be used by a single view
// Allows for complex workflows such as deferred and half-resolution passes
static const uint32_t MaxFrameBuffersPerView = 2;

struct renderViewCreateInfo_t
{
	renderViewMode_t			viewMode;
	renderPassTransition_t		transition;
	frameBufferCreateInfo_t		fbImages[ MaxFrameBuffersPerView ];

	const char*					name;
	ResourceContext*			resources;
	RenderContext*				context;

	vec4f						clearColor;
	float						clearDepth;
	bool						clear;
	uint8_t						clearStencil;
	bool						finalize;

	bool						isCubeView;
	int							viewId;
};


class RenderView
{
private:
	using debugMenuArray_t = Array<debugMenuFuncPtr, 12>;

	ResourceContext*		m_resources;
	RenderContext*			m_context;
	FrameBuffer*			m_framebuffers[ MaxFrameBuffersPerView ][ MaxMultiViews ];
	ImageView*				m_colorViews[ MaxFrameBuffersPerView ][ MaxMultiViews ];
	ImageView*				m_gBuffer0Views[ MaxFrameBuffersPerView ][ MaxMultiViews ];
	ImageView*				m_gBuffer1Views[ MaxFrameBuffersPerView ][ MaxMultiViews ];
	ImageView*				m_depthViews[ MaxFrameBuffersPerView ][ MaxMultiViews ];
	ImageView*				m_stencilViews[ MaxFrameBuffersPerView ][ MaxMultiViews ];
	uint32_t				m_uploadViewIds[ MaxMultiViews ];
	GpuBufferView			m_viewParmeters[ MaxMultiViews ];
	GpuBufferView			m_surfParmeters;
	gpuSurface_t			m_surfBuffer[ MaxSurfaces ];

	ShaderBindParms*		m_viewParms;
	vec4f					m_clearColor;
	float					m_clearDepth;
	vec3f					m_viewOrigin;
	uint32_t				m_clearStencil;
	renderPassTransition_t	m_transitionState;
	viewport_t				m_viewport;
	mat4x4f					m_viewMatrices[ MaxMultiViews ];
	mat4x4f					m_projMatrices[ MaxMultiViews ];
	mat4x4f					m_invProjMatrices[ MaxMultiViews ];
	mat4x4f					m_viewProjMatrices[ MaxMultiViews ];
	mat4x4f					m_previousViewProjMatrices[ MaxMultiViews ];
	frameBufferCreateInfo_t	m_fbSourceImages[ MaxFrameBuffersPerView ];
	const char*				m_name;
	renderViewMode_t		m_region;
	uint32_t				m_multiViewCount;
	int32_t					m_viewBufferId;
	int32_t					m_surfaceBufferId;
	uint64_t				m_lastUpdateFrame = ~0u;
	uint64_t				m_lastResizeFrame = ~0u;
	bool					m_committed;
	bool					m_isCubeView;
	bool					m_clearImage;
	bool					m_finalizeImage;

public:

	RenderView()
	{
		m_viewport = viewport_t( 0, 0, DefaultDisplayWidth, DefaultDisplayHeight, 0.0f, 1.0f );

		m_multiViewCount = 1;

		m_viewBufferId = -1;
		m_surfaceBufferId = -1;
		m_committed = false;

		numLights = 0;
		skyboxImage = nullptr;
		memset( drawGroupOffset, 0, sizeof( drawGroupOffset ) );

		for( uint32_t multiViewIndex = 0; multiViewIndex < MaxMultiViews; ++multiViewIndex )
		{
			m_viewMatrices[ multiViewIndex ] = mat4x4f( 1.0f );
			m_projMatrices[ multiViewIndex ] = mat4x4f( 1.0f );
			m_invProjMatrices[ multiViewIndex ] = mat4x4f( 1.0f );
			m_viewProjMatrices[ multiViewIndex ] = mat4x4f( 1.0f );

			for ( uint32_t fbIndex = 0; fbIndex < MaxFrameBuffersPerView; ++fbIndex ) {
				m_framebuffers[ fbIndex ][ multiViewIndex ] = nullptr;
			}

			for ( uint32_t passIndex = 0; passIndex < DRAWPASS_COUNT; ++passIndex ) {
				passes[ multiViewIndex ][ passIndex ] = nullptr;
			}
		}
		m_region = renderViewMode_t::UNKNOWN;
	}

	~RenderView()
	{
		for ( uint32_t multiViewIndex = 0; multiViewIndex < MaxMultiViews; ++multiViewIndex )
		{
			for ( uint32_t passIndex = 0; passIndex < DRAWPASS_COUNT; ++passIndex )
			{
				if ( passes[ multiViewIndex ] != nullptr )
				{
					delete passes[ multiViewIndex ][ passIndex ];
					passes[ multiViewIndex ][ passIndex ] = nullptr;
				}
			}
			for ( uint32_t fbIndex = 0; fbIndex < MaxFrameBuffersPerView; ++fbIndex )
			{
				if( m_framebuffers[ fbIndex ][ multiViewIndex ] != nullptr )
				{
					delete m_framebuffers[ fbIndex ][ multiViewIndex ];
					m_framebuffers[ fbIndex ][ multiViewIndex ] = nullptr;

					delete m_colorViews[ fbIndex ][ multiViewIndex ];
					m_colorViews[ fbIndex ][ multiViewIndex ] = nullptr;

					delete m_gBuffer0Views[ fbIndex ][ multiViewIndex ];
					m_gBuffer0Views[ fbIndex ][ multiViewIndex ] = nullptr;

					delete m_gBuffer1Views[ fbIndex ][ multiViewIndex ];
					m_gBuffer1Views[ fbIndex ][ multiViewIndex ] = nullptr;

					delete m_depthViews[ fbIndex ][ multiViewIndex ];
					m_depthViews[ fbIndex ][ multiViewIndex ] = nullptr;

					delete m_stencilViews[ fbIndex ][ multiViewIndex ];
					m_stencilViews[ fbIndex ][ multiViewIndex ] = nullptr;
				}
			}
		}
	}

	void					Init( const renderViewCreateInfo_t& info );
	void					CreateFrameBuffers( const frameBufferCreateInfo_t createInfos[ MaxFrameBuffersPerView ], const uint32_t fbIndex);
	void					FrameBegin( const drawPass_t begin, const drawPass_t end );
	void					FrameEnd( const drawPass_t begin, const drawPass_t end );
	void					Resize();

	drawPass_t				ViewRegionPassBegin() const;
	drawPass_t				ViewRegionPassEnd() const;
	renderPassTransition_t	TransitionState() const;
	bool					Finalize() const;
	bool					Clear() const;
	const vec4f&			ClearColor() const;
	float					ClearDepth() const;
	uint32_t				ClearStencil() const;
	const ShaderBindParms*	BindParms() const;

	void					SetCamera( const Camera& camera, const bool reverseZ = true, const uint32_t multiView = 0 );
	void					SetCamera2D( const Camera& camera, const vec4f& frame, const uint32_t multiView = 0 );
	void					SetViewRect( const int32_t x, const int32_t y, const uint32_t width, const uint32_t height );
	const viewport_t&		GetViewport() const;
	vec2i					GetFrameSize() const;
	const mat4x4f&			GetViewMatrix( const uint32_t multiViewIndex = 0 ) const;
	const mat4x4f&			GetProjMatrix( const uint32_t multiViewIndex = 0 ) const;
	const mat4x4f&			GetInvProjMatrix( const uint32_t multiViewIndex = 0 ) const;
	const mat4x4f&			GetViewProjMatrix( const uint32_t multiViewIndex = 0 ) const;
	const mat4x4f&			GetPreviousViewProjMatrix( const uint32_t multiViewIndex = 0 ) const;
	int32_t					GetViewBufferUploadId( const int multiViewIndex = 0 ) const;
	int32_t					GetSurfaceBufferId() const; // TODO: Have view own it's surface buffer. Eliminates indexing

	uint32_t				GetMultiViewCount() const;

	inline const vec3f&		GetViewOrigin( const uint32_t multiView = 0 ) const { return m_viewOrigin; }

	const char*				GetName() const;
	const renderViewMode_t	GetViewMode() const;
	const bool				CanRenderSurface( const Entity& ent, const Material& material, const renderFlags_t renderFlags ) const;

	const void				Commit();
	bool					IsCommitted() const;
	bool					NeedsResize() const;

	void					AttachDebugMenu( const debugMenuFuncPtr funcPtr );

	uint32_t				lights[ MaxLights ];
	uint32_t				numLights;
	Image*					skyboxImage;
	uint32_t				drawGroupOffset[ DRAWPASS_COUNT ];
	DrawPass*				passes[ MaxMultiViews ][ DRAWPASS_COUNT ];
	DrawGroup				drawGroup[ DRAWPASS_COUNT ];
	debugMenuArray_t		debugMenus;
};
