#include "transPass.h"
#include "../render_binding/bindings.h"
#include "../globals/renderConstants.h"
#include "../render_core/renderer.h"

extern renderConstants_t rc;

void TransPass::Init( RenderContext* renderContext, FrameBuffer* frameBuffer )
{
	m_name = "Trans Pass";
	m_passId = DRAWPASS_TRANS;

	m_stateBits = GFX_STATE_NONE;
	m_stateBits |= GFX_STATE_DEPTH_TEST;
	m_stateBits |= GFX_STATE_CULL_MODE_BACK;
	m_stateBits |= GFX_STATE_BLEND_ENABLE;
	m_stateBits |= GFX_STATE_MRT_ENABLE;

	codeImages.SetRenderContext( renderContext );
	codeCubeImages.SetRenderContext( renderContext );

	codeImages.Resize( 5 );

	SetFrameBuffer( frameBuffer );
}

void TransPass::FrameBegin( const ResourceContext* resources )
{
	codeImages.BindIndex( 0, resources->shadowMapImage[ 0 ] );
	codeImages.BindIndex( 1, resources->shadowMapImage[ 1 ] );
	codeImages.BindIndex( 2, resources->shadowMapImage[ 2 ] );
	codeImages.BindIndex( 3, resources->mainColorResolvedImage );	// Can be previous the frame depending on scheduling order and that's ok
	codeImages.BindIndex( 4, resources->rtReflectionsOutputImage );

	parms->Bind( BINDING_NAME( lightBuffer ),			&resources->lightParms );
	parms->Bind( BINDING_NAME( imageCodeArray ),		&codeImages );
	parms->Bind( BINDING_NAME( imageCodeCubeArray ),	&codeCubeImages );
	parms->Bind( BINDING_NAME( imageStencil ),			rc.whiteImage );
}


void TransPass::FrameEnd()
{

}