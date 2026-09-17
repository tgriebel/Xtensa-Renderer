#define USE_RT

#include "globals.h"

// ============================================================
// Shared payload — must be identical in rgen, rchit, and rmiss
// ============================================================

struct hitPayload_t
{
	float4	color;
};


// ============================================================
// Push-constant layout — must match RayTracingTask::baseConstants_t
// ============================================================

struct rtBaseConstants_t
{
	uint	viewId;
	uint	width;
	uint	height;
	uint	pad;
};

#define RT_PUSH_CONSTANTS			BIND_INLINE rtBaseConstants_t rtConstants;


// ============================================================
// Binding macros
// ============================================================

#define RT_ACCELERATION_STRUCTURE( S, N, NAME )		BIND_SET( S, N ) RaytracingAccelerationStructure NAME;
#define RT_OUTPUT( S, N, NAME )						BIND_SET( S, N ) RWTexture2D<float4> NAME;
#define RT_VERTEX_BUFFER( S, N, NAME )				BIND_SET( S, N ) ByteAddressBuffer NAME;
#define RT_INDEX_BUFFER( S, N, NAME )				BIND_SET( S, N ) StructuredBuffer<uint> NAME;
#define RT_SURFACE_INFO( S, N, NAME )				BIND_SET( S, N ) StructuredBuffer<gpuRtSurface_t> NAME;


// ByteAddressBuffer allows for better control over padding and alignment so the vertices can be shared between RT and rasterization pipelines
rtVertex_t LoadRtVertex( ByteAddressBuffer buf, uint index )
{
	static const uint VertexStride = 84;
	const uint base = index * VertexStride;

	rtVertex_t v;
	v.position = asfloat( buf.Load4( base + 0 ) );
	v.color = asfloat( buf.Load4( base + 16 ) );
	v.normal = asfloat( buf.Load3( base + 32 ) );
	v.tangent = asfloat( buf.Load3( base + 44 ) );
	v.bitangent = asfloat( buf.Load3( base + 56 ) );
	v.uv = asfloat( buf.Load4( base + 68 ) );
	return v;
}
