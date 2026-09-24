#pragma once



#include <SysCore/bitArray.h>
#include "gpuImage.h"
#include "../render_state/deviceContext.h"
#include "../asset_types/image.h"

using BaseImageArray = Array<const Image*, MaxImageDescriptors>;

class ImageArray : public BaseImageArray
{
private:
	uint64_t		m_lastFrameUpdate[MaxImageDescriptors];
	RenderContext* m_context;

public:
	ImageArray()
	{
		m_context = nullptr;

		memset(m_lastFrameUpdate, 0, MaxImageDescriptors * sizeof(uint64_t));
	}

	const Image* operator[](uint32_t index) const
	{
		return BaseImageArray::operator[](index);
	}

	// This name makes the intent more explicit than [] and allows additional book-keeping easily
	void BindIndex( const uint32_t index, const Image* image, const bool forceUpdate = false );

	void SetRenderContext(RenderContext* context);

	[[nodiscard]]
	bool HasPossibleUpdates() const;
};
