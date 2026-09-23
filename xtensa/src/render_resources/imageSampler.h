#pragma once

#include "../render_core/gpuImage.h"
#include "../render_state/deviceContext.h"
#include "../asset_types/image.h"

enum samplerAddress_t
{
	SAMPLER_ADDRESS_WRAP = 0,
	SAMPLER_ADDRESS_CLAMP_EDGE = 1,
	SAMPLER_ADDRESS_CLAMP_BORDER = 2,
	SAMPLER_ADDRESS_MODES,
};


enum samplerFilter_t
{
	SAMPLER_FILTER_NEAREST = 0,
	SAMPLER_FILTER_BILINEAR = 1,
	SAMPLER_FILTER_TRILINEAR = 2,
	SAMPLER_FILTER_MODES,
};


enum samplerBorderColor_t
{
	SAMPLER_BORDER_WHITE = 0,
	SAMPLER_BORDER_BLACK = 1,
};


struct samplerState_t
{
	samplerAddress_t		addrMode;
	samplerFilter_t			filter;
	samplerBorderColor_t	borderColor;
	float					maxAniso; // 0.0f is disabled
	float					minLod;
	float					maxLod;
	bool					borderTransparent;
	bool					borderColorIsFloat;
	bool					pcf;
};


class ImageSampler : public RenderResource
{
private:
	samplerState_t	m_samplerState;
#ifdef USE_VULKAN
	VkSampler		vk_sampler;
#endif

public:
	ImageSampler()
	{
	}

	~ImageSampler()
	{
		// FIXME: TODO:
	}

#ifdef USE_VULKAN
	inline const VkSampler GetVkObject() const { return vk_sampler; }
#endif

	void Init( const samplerState_t& state, const resourceLifeTime_t lifetime );

	void Destroy() override;
};
