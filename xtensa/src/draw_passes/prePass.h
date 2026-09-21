#pragma once

#include "drawpass.h"

class PrePass : public DrawPass
{
public:
	PrePass( RenderContext* renderContext, FrameBuffer* fb )
	{
		Init( renderContext, fb );
	}

	virtual void Init( RenderContext* renderContext, FrameBuffer* fb );
	virtual void FrameBegin( const ResourceContext* resources );
	virtual void FrameEnd();
};
