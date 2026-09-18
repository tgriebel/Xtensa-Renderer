#ifndef RAYCONE_HLSL_H
#define RAYCONE_HLSL_H

// Ray Cones for texture LOD selection
// Taken from Ray Tracing Gems, 2019 (chapter 20) - Akenine-Moller, et al
struct rayCone_t
{
	float width;		// wi
	float spreadAngle;	// Yi
};


rayCone_t Propagate( const rayCone_t cone, const float surfaceSpreadAngle, const float hitT )
{
	rayCone_t result;
	result.width = cone.spreadAngle * hitT + cone.width;
	result.spreadAngle = cone.spreadAngle + surfaceSpreadAngle;
	return result;
}


float ComputeTextureLOD( const rayCone_t cone, const float triangleWorldArea, const float triangleUvArea, const float3 rayDir, const float3 surfaceNormal, const float textureWidth, const float textureHeight )
{
	const float safeWorldArea = max( triangleWorldArea, 1e-9f );
	const float cosTheta = max( abs( dot( rayDir, surfaceNormal ) ), 1e-4f );

	const float triLodConstant = 0.5f * log2( triangleUvArea / safeWorldArea );

	float lambda = triLodConstant;
	lambda += log2( abs( cone.width ) );
	lambda += 0.5f * log2( textureWidth * textureHeight );
	lambda -= log2( cosTheta );

	return lambda;
}


rayCone_t InitRayCone( const float verticalFovRadians, const uint screenHeightPixels )
{
	rayCone_t cone;
	cone.spreadAngle = atan( ( 2.0f * tan( verticalFovRadians * 0.5f ) ) / (float)screenHeightPixels );
	cone.width = 0.0f; // No width where ray starts
	return cone;
}

#endif // RAYCONE_HLSL_H
