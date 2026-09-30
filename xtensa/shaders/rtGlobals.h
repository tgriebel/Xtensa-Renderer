#define USE_RT

#include "globals.h"
#include "rayCone.h"

// Shared payload (must be identical in rgen, rchit, and rmiss)
struct hitPayload_t
{
	float4		color;
	rayCone_t	cone;
	uint		depth;	// Current depth-level (0 = primary ray, 1 = first bounce, etc)
};

static const uint MaxRtRecursionDepth = 3;


// Push-constant (see RayTracingTask::baseConstants_t)
struct rtBaseConstants_t
{
	uint	viewId;
	uint	width;
	uint	height;
	uint	pad;
};

#define RT_PUSH_CONSTANTS			BIND_INLINE rtBaseConstants_t rtConstants;


// Bindings
#define RT_ACCELERATION_STRUCTURE( S, N, NAME )		BIND_SET( S, N ) RaytracingAccelerationStructure NAME;
#define RT_OUTPUT( S, N, NAME )						BIND_SET( S, N ) RWTexture2D<float4> NAME;
#define RT_SURFACE_INFO( S, N, NAME )				BIND_SET( S, N ) StructuredBuffer<gpuRtSurface_t> NAME;
