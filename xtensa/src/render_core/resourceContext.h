#pragma once

#include <cstdint>

#include "../render_resources/gpuBuffer.h"
#include "../render_resources/imageSampler.h"
#include "../render_resources/imageArray.h"
#include "../render_resources/imageView.h"
#include "../render_resources/aliasableImageHeap.h"

class Image;
class GpuAccelerationStructure;

// Resources that are globally accessible for shader binding
class ResourceContext
{
private:
	uint32_t outputImageCount = 0;
	uint32_t renderViewCount = 0;

	Image* NextImage()
	{
		assert( outputImageCount < MaxFrameImages );
		return &gpuOutput2D[ outputImageCount++ ];
	}

public:
	GpuBuffer				globalConstants;
	GpuBuffer				surfParms;
	GpuBuffer				materialBuffers;
	GpuBuffer				lightParms;
	GpuBuffer				particleBuffer;
	GpuAccelerationStructure* tlas = nullptr;

	ImageSampler			nearestSampler;
	ImageSampler			bilinearSamplers[ SAMPLER_ADDRESS_MODES ];
	ImageSampler			trilinearSamplers[ SAMPLER_ADDRESS_MODES ];
	ImageSampler			depthShadowSampler;

	// TODO: Views should have a final output image assigned to them
	// Intermediate images can be reused across views however
	// Synchronization can avoid potential hazards when necessary
	GpuBuffer				viewParms;
	Image*					mainColorImage;
	Image*					cubeFbColorImage;
	Image*					cubeFbDepthImage;
	Image*					gBufferLayerImage0;
	Image*					gBufferLayerImage1;
	Image*					depthStencilImage;
	Image*					ssaoImage;
	Image*					dofCocImage;
	Image*					dofTileCocImage;
	Image*					dofBokeh;
	Image*					dofBlur;
	Image*					ssaoBlurImage;
	ImageView				depthImageView;
	ImageView				stencilImageView;

	// Code images
	AliasableImageHeap		tempColorImageHeap;
	ImageView				depthResolvedImageView;
	ImageView				stencilResolvedImageView;
	Image*					shadowMapImage[ MaxShadowViews ];
	Image*					mainColorResolvedImage;
	Image*					gBufferLayerResolvedImage0;
	Image*					gBufferLayerResolvedImage1;
	Image*					bloom;
	Image*					blurredImage;
	Image*					tempColorImage;
	Image*					previousLum;
	Image*					currentLum;
	Image*					depthStencilResolvedImage;
	Image*					rtOutputImage;
	Image*					rtReflectionsOutputImage;
	Image*					visBufferImage;
	Image					gpuOutput2D[ MaxFrameImages ];

	// Data images
	ImageArray				gpuImages2D;
	ImageArray				gpuImagesCube;

	void RegisterOutputImages()
	{
		outputImageCount = 0;

		mainColorImage = NextImage();
		cubeFbColorImage = NextImage();
		cubeFbDepthImage = NextImage();
		gBufferLayerImage0 = NextImage();
		gBufferLayerImage1 = NextImage();
		depthStencilImage = NextImage();
		ssaoImage = NextImage();
		ssaoBlurImage = NextImage();
		dofCocImage = NextImage();
		dofTileCocImage = NextImage();
		dofBokeh = NextImage();
		dofBlur = NextImage();

		for( uint32_t i = 0; i < MaxShadowViews; ++i )
		{
			shadowMapImage[ i ] = NextImage();
		}
		mainColorResolvedImage = NextImage();
		gBufferLayerResolvedImage0 = NextImage();
		gBufferLayerResolvedImage1 = NextImage();
		blurredImage = NextImage();
		bloom = NextImage();
		tempColorImage = NextImage();
		previousLum = NextImage();
		currentLum = NextImage();
		depthStencilResolvedImage = NextImage();
		rtOutputImage = NextImage();
		rtReflectionsOutputImage = NextImage();
		visBufferImage = NextImage();
	}

	uint32_t OutputImageCount()
	{
		return outputImageCount;
	}

	uint32_t NextRenderView( GpuBufferView& renderViewBuffer );
};