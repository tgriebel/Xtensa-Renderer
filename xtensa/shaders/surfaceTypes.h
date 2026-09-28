#ifndef SURFACETYPES_HLSL_H
#define SURFACETYPES_HLSL_H

// These structs represent data respective to the lighting equation
// surfaceInput_t: Material data from the current surface/pixel sample (one per shader invocation)
// lightingInput_t: Incoming light data to the surface sample (multiple / shader)
// brdfSample_t: Data from what happens when light interacts with the surface material sample

// Surface sample data
struct surfaceInput_t
{
	float3	N;
	float3	V;
	float3	F;
	float3	F0;
	float3	T;
	float3	B;
	float3	position;
	float3	albedo;
	float3	ccNormal;
	float3	emissive;
	float3	sheenColor;
	float3	tangentNormal;
	float	NoV;
	float	roughness;
	float	metallic;
	float	ccStrength;	// cc: clear-coat
	float	ccRoughness;
	float	ao;
	float	sheenRoughness;
	float	aniso;
	float	anisoRotation;
	bool	useClearCoat;
	bool	useSheen;
	bool	useAniso;
};


// Surface sample-to-light data
struct lightingInput_t
{
	float3	lightRay;
	float3	intensity;
	float3	L;
	float3	H;
	float3	Li;
	float	lightDistance;
	float	NoL;
	float	NoH;
	float	LoH;
	float	HoV;
};


// BRDF surface sample
struct brdfSample_t
{
	float3 Fd;	// Diffuse
	float3 Fr;	// Specular
	float3 F;	// Fresnel
};


// Generalized structure representing a sample from a given triangle (or geometry generally)
// Can correspond 1:1 with pixel shader interpolators, a ray-intersection payload, or compute shader computation (e.g. visibility buffer)
struct geometryAttributes_t
{
	float3	worldPosition;
	float3	T;	// world-space tangent
	float3	B;	// world-space bitangent
	float3	N;	// world-space geometric normal ( pre-normal-map ), == vsToPsInterpolators.TBN2
	float2	uv0;
	float2	uv1;
};

#endif // SURFACETYPES_HLSL_H
