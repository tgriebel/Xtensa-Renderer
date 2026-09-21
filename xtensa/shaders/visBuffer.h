#ifndef VISBUFFER_HLSL_H
#define VISBUFFER_HLSL_H

// See John Hable's blog post for all the core implementation details on visibility buffers
// https://filmicworlds.com/blog/visibility-buffer-rendering-with-material-graphs/

struct visibilitySample_t
{
	uint instanceId;
	uint triangleId;
};


uint2 EncodeVisibility( const uint instanceId, const uint triangleId )
{
	return uint2( instanceId, triangleId );
}


uint2 EncodeVisibility( const visibilitySample_t vis )
{
	return EncodeVisibility( vis.instanceId, vis.triangleId );
}


visibilitySample_t DecodeVisibility( const uint2 packed )
{
	visibilitySample_t vis;
	vis.instanceId = packed.x;
	vis.triangleId = packed.y;
	return vis;
}


sampleAttributes_t ResolveVisibility( const visibilitySample_t vis, const float2 pixelNdc, const gpuView_t view )
{
	sampleAttributes_t result = (sampleAttributes_t)0;
	// TODO: implement
	return result;
}

#endif // VISBUFFER_HLSL_H
