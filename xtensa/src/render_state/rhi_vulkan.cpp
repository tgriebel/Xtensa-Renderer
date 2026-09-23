#include "rhi.h"

VkBorderColor vk_GetBorderColor( const samplerBorderColor_t borderColor, const bool isTransparent, const bool isFloat )
{
	if( borderColor == SAMPLER_BORDER_BLACK )
	{
		if( isTransparent )
		{
			if( isFloat ) {
				return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
			} else {
				return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
			}
		}
		else
		{
			if( isFloat ) {
				return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			} else {
				return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
			}
		}
	}
	else if( borderColor == SAMPLER_BORDER_WHITE )
	{
		if( isFloat ) {
			return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		}
		else {
			return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
		}
	}
	assert( 0 );
	return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
}


void vk_GetBorderColor( const VkBorderColor vk_borderColor, samplerBorderColor_t& borderColor, bool& isTransparent, bool& isFloat )
{
	switch( vk_borderColor )
	{
		case VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
		{
			borderColor = SAMPLER_BORDER_BLACK;
			isTransparent = true;
			isFloat = true;
		}
		break;

		case VK_BORDER_COLOR_INT_TRANSPARENT_BLACK:
		{
			borderColor = SAMPLER_BORDER_BLACK;
			isTransparent = true;
			isFloat = false;
		}
		break;

		case VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK:
		{
			borderColor = SAMPLER_BORDER_BLACK;
			isTransparent = false;
			isFloat = true;
		}
		break;

		case VK_BORDER_COLOR_INT_OPAQUE_BLACK:
		{
			borderColor = SAMPLER_BORDER_BLACK;
			isTransparent = false;
			isFloat = false;
		}
		break;

		case VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE:
		{
			borderColor = SAMPLER_BORDER_WHITE;
			isTransparent = false;
			isFloat = true;
		}
		break;

		case VK_BORDER_COLOR_INT_OPAQUE_WHITE:
		{
			borderColor = SAMPLER_BORDER_WHITE;
			isTransparent = false;
			isFloat = false;
		}
		break;
	}
	assert( 0 );
	return;
}
