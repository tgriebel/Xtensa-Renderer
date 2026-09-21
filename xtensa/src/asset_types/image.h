#pragma once

#include <cstdint>
#include <functional>

#include "asset.h"
#include "../render_core/renderResource.h"

class GpuImage;

enum gpuImageStateFlags_t : uint8_t;


// Reordering enums will break content
enum imageType_t : uint8_t
{
	IMAGE_TYPE_UNKNOWN			= 0,
	IMAGE_TYPE_2D				= 1,
	IMAGE_TYPE_2D_ARRAY			= 2,
	IMAGE_TYPE_3D				= 3,
	IMAGE_TYPE_3D_ARRAY			= 4,
	IMAGE_TYPE_CUBE				= 5,
	IMAGE_TYPE_CUBE_ARRAY		= 6,
	IMAGE_TYPE_DEPTH			= 7,
	IMAGE_TYPE_STENCIL			= 8,
	IMAGE_TYPE_DEPTH_STENCIL	= 9,
};


enum imageAspectFlags_t : uint8_t
{
	IMAGE_ASPECT_NONE			= 0,
	IMAGE_ASPECT_COLOR_FLAG		= ( 1 << 0 ),
	IMAGE_ASPECT_DEPTH_FLAG		= ( 1 << 1 ),
	IMAGE_ASPECT_STENCIL_FLAG	= ( 1 << 2 ),
	IMAGE_ASPECT_PLANE0			= ( 1 << 3 ),
	IMAGE_ASPECT_PLANE1			= ( 1 << 4 ),
	IMAGE_ASPECT_PLANE2			= ( 1 << 5 ),
	IMAGE_ASPECT_ALL			= ( 1 << 6 ) - 1
};
static_assert( IMAGE_ASPECT_ALL == 0x3F, "Reordering will break content" );


// Reordering enums will break content
enum imageTiling_t : uint8_t
{
	IMAGE_TILING_LINEAR	= 0,
	IMAGE_TILING_MORTON	= 1,
};


enum imageFmt_t : uint8_t
{
	IMAGE_FMT_UNKNOWN			= 0,
	IMAGE_FMT_R_8				= 1,
	IMAGE_FMT_R_16				= 2,
	IMAGE_FMT_R_32				= 3,
	IMAGE_FMT_D_16				= 4,
	IMAGE_FMT_D24S8				= 5,
	IMAGE_FMT_D_32				= 6,
	IMAGE_FMT_D_32_S8			= 7,
	IMAGE_FMT_RGB_8				= 8,
	IMAGE_FMT_RGBA_8			= 9,
	IMAGE_FMT_RGBA_8_UNORM		= 10, // Unsigned
	IMAGE_FMT_ABGR_8			= 11,
	IMAGE_FMT_BGR_8				= 12,
	IMAGE_FMT_BGRA_8			= 13,
	IMAGE_FMT_RG_16				= 14,
	IMAGE_FMT_RGB_16			= 15,
	IMAGE_FMT_RGBA_16			= 16,
	IMAGE_FMT_RG_32				= 17,
	IMAGE_FMT_RGB_32			= 18,
	IMAGE_FMT_RGBA_32			= 19,
	IMAGE_FMT_R11G11B10_US		= 20, // Unsigned
	IMAGE_FMT_RG_16_UINT		= 21, // Appended at the end to preserve legacy files
	IMAGE_FMT_RG_32_UINT		= 22, // Appended at the end to preserve legacy files
	IMAGE_FMT_COUNT				= 23,
};
static_assert( IMAGE_FMT_COUNT == 23, "Reordering will break content" );


enum imageSamples_t : uint8_t
{
	IMAGE_SMP_1 = (1 << 0),
	IMAGE_SMP_2 = ( 1 << 1 ),
	IMAGE_SMP_4 = ( 1 << 2 ),
	IMAGE_SMP_8 = ( 1 << 3 ),
	IMAGE_SMP_16 = ( 1 << 4 ),
	IMAGE_SMP_32 = ( 1 << 5 ),
	IMAGE_SMP_64 = ( 1 << 6 ),
};


struct imageInfo_t
{
	uint32_t				width;
	uint32_t				height;
	uint32_t				channels;
	uint32_t				mipLevels;
	uint32_t				layers;
	imageSamples_t			subsamples;
	imageType_t				type;
	imageFmt_t				fmt;
	imageTiling_t			tiling;
	bool					unused; // Deprecated v2
};


// Reordering enums will break content
enum imageCubeFace : uint8_t
{
	IMAGE_CUBE_FACE_X_POS	= 0,
	IMAGE_CUBE_FACE_X_NEG	= 1,
	IMAGE_CUBE_FACE_Y_POS	= 2,
	IMAGE_CUBE_FACE_Y_NEG	= 3,
	IMAGE_CUBE_FACE_Z_POS	= 4,
	IMAGE_CUBE_FACE_Z_NEG	= 5,
};


struct imageSubResourceView_t
{
	uint32_t			baseMip;
	uint32_t			mipLevels;
	uint32_t			baseArray;
	uint32_t			arrayCount;
	imageAspectFlags_t	aspect;
};


enum samplerAddress_t
{
	SAMPLER_ADDRESS_WRAP			= 0,
	SAMPLER_ADDRESS_CLAMP_EDGE		= 1,
	SAMPLER_ADDRESS_CLAMP_BORDER	= 2,
	SAMPLER_ADDRESS_MODES,
};


enum samplerFilter_t
{
	SAMPLER_FILTER_NEAREST		= 0,
	SAMPLER_FILTER_BILINEAR		= 1,
	SAMPLER_FILTER_TRILINEAR	= 2,
	SAMPLER_FILTER_MODES,
};


enum samplerBorderColor_t
{
	SAMPLER_BORDER_WHITE	= 0,
	SAMPLER_BORDER_BLACK	= 1,
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


struct colorAspectTableEntry_t
{
	imageFmt_t			fmt;
	imageAspectFlags_t	aspect;
};


static const colorAspectTableEntry_t s_formatAspectTable[] =
{
	{ IMAGE_FMT_UNKNOWN,		IMAGE_ASPECT_NONE		},
	{ IMAGE_FMT_R_8,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_R_16,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_R_32,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_D_16,			IMAGE_ASPECT_DEPTH_FLAG	},
	{ IMAGE_FMT_D24S8,			imageAspectFlags_t( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG ) },
	{ IMAGE_FMT_D_32,			IMAGE_ASPECT_DEPTH_FLAG	},
	{ IMAGE_FMT_D_32_S8,		imageAspectFlags_t( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG ) },
	{ IMAGE_FMT_RGB_8,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_RGBA_8,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_RGBA_8_UNORM,	IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_ABGR_8,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_BGR_8,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_BGRA_8,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_RG_16,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_RG_16_UINT,		IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_RGB_16,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_RGBA_16,		IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_RG_32,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_RG_32_UINT,		IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_RGB_32,			IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_RGBA_32,		IMAGE_ASPECT_COLOR_FLAG	},
	{ IMAGE_FMT_R11G11B10_US,	IMAGE_ASPECT_COLOR_FLAG	},
};
static_assert( COUNTARRAY( s_formatAspectTable ) == IMAGE_FMT_COUNT );


static inline constexpr imageAspectFlags_t GetColorAspectFlags( const imageFmt_t fmt )
{
	for( uint32_t i = 0; i < COUNTARRAY( s_formatAspectTable ); ++i )
	{
		if( s_formatAspectTable[ i ].fmt == fmt )
		{
			return s_formatAspectTable[ i ].aspect;
		}
	}
	return IMAGE_ASPECT_NONE;
}


static inline constexpr bool IsDepthStencilCompatible( const imageFmt_t fmt )
{
	const imageAspectFlags_t aspect = GetColorAspectFlags( fmt );
	return ( aspect & ( IMAGE_ASPECT_DEPTH_FLAG | IMAGE_ASPECT_STENCIL_FLAG ) ) != 0;
}


inline uint32_t GetBppForFormat( const imageFmt_t format )
{
	switch( format )
	{
		case IMAGE_FMT_R_8:				return 1;
		case IMAGE_FMT_R_16:			return 2;
		case IMAGE_FMT_R_32:			return 4;
		case IMAGE_FMT_D_16:			return 2;
		case IMAGE_FMT_D24S8:			return 4;
		case IMAGE_FMT_D_32:			return 4;
		case IMAGE_FMT_D_32_S8:			return 8; // Padded
		case IMAGE_FMT_RGB_8:			return 3;
		case IMAGE_FMT_RGBA_8:			return 4;
		case IMAGE_FMT_RGBA_8_UNORM:	return 4;
		case IMAGE_FMT_ABGR_8:			return 4;
		case IMAGE_FMT_BGR_8:			return 3;
		case IMAGE_FMT_BGRA_8:			return 4;
		case IMAGE_FMT_RG_16:			return 4;
		case IMAGE_FMT_RG_16_UINT:		return 4;
		case IMAGE_FMT_RGB_16:			return 6;
		case IMAGE_FMT_RGBA_16:			return 8;
		case IMAGE_FMT_RG_32:			return 8;
		case IMAGE_FMT_RG_32_UINT:		return 8;
		case IMAGE_FMT_RGB_32:			return 12;
		case IMAGE_FMT_RGBA_32:			return 16;
		case IMAGE_FMT_R11G11B10_US:	return 4;

		default:
		{
			assert( 0 );
			return 4;
		}
	}
}


inline uint32_t GetChannelsForFormat( const imageFmt_t format )
{
	switch( format )
	{
		case IMAGE_FMT_R_8:				return 1;
		case IMAGE_FMT_R_16:			return 1;
		case IMAGE_FMT_R_32:			return 1;
		case IMAGE_FMT_D_16:			return 1;
		case IMAGE_FMT_D24S8:			return 2;
		case IMAGE_FMT_D_32:			return 1;
		case IMAGE_FMT_D_32_S8:			return 2;
		case IMAGE_FMT_RGB_8:			return 3;
		case IMAGE_FMT_RGBA_8:			return 4;
		case IMAGE_FMT_RGBA_8_UNORM:	return 4;
		case IMAGE_FMT_ABGR_8:			return 4;
		case IMAGE_FMT_BGR_8:			return 3;
		case IMAGE_FMT_BGRA_8:			return 4;
		case IMAGE_FMT_RG_16:			return 2;
		case IMAGE_FMT_RG_16_UINT:		return 2;
		case IMAGE_FMT_RGB_16:			return 3;
		case IMAGE_FMT_RGBA_16:			return 4;
		case IMAGE_FMT_RG_32:			return 2;
		case IMAGE_FMT_RG_32_UINT:		return 2;
		case IMAGE_FMT_RGB_32:			return 3;
		case IMAGE_FMT_RGBA_32:			return 4;
		case IMAGE_FMT_R11G11B10_US:	return 3;

		default:
		{
			assert( 0 );
			return 4;
		}
	}
}


inline bool operator==( const imageInfo_t& info0, const imageInfo_t& info1 )
{
	bool equal =
		( info0.width == info1.width ) &&
		( info0.height == info1.height ) &&
		( info0.channels == info1.channels ) &&
		( info0.mipLevels == info1.mipLevels ) &&
		( info0.layers == info1.layers ) &&
		( info0.subsamples == info1.subsamples ) &&
		( info0.type == info1.type ) &&
		( info0.fmt == info1.fmt ) &&
		( info0.tiling == info1.tiling );
	return equal;
}


inline bool operator!=( const imageInfo_t& info0, const imageInfo_t& info1 )
{
	return !( info0 == info1 );
}


inline imageInfo_t DefaultImage2dInfo( uint32_t w, uint32_t h )
{
	imageInfo_t info {};
	info.width = w;
	info.height = h;
	info.layers = 1;
	info.channels = 4;
	info.mipLevels = MipCount( w, h );
	info.subsamples = IMAGE_SMP_1;
	info.type = IMAGE_TYPE_2D;
	info.fmt = IMAGE_FMT_RGBA_8;
	info.tiling = IMAGE_TILING_MORTON;

	return info;
}


class AliasableImageHeap;

class Image : public RenderResource
{
public:
	using ResizeFn = std::function<imageInfo_t( uint32_t width, uint32_t height )>;

private:
	static const uint32_t	Version = 2;
	ResizeFn				m_resizeFn;
	AliasableImageHeap*		m_heap = nullptr; // Optional. For aliased images

public:

	imageInfo_t				info;
	imageSubResourceView_t	subResourceView;
	bool					generateMips;

	// FIXME: Ownership has been tricky to resolve since Images do *a lot*, but should be resolvable after numerous refactorings
	// `cpuImage` is loaded from disk, passed as a pointer to avoid slow copies. It can be explicitly deleted once uploaded
	// Ownership for `cpuImage` needs to be clearer--whatever does the allocation should also delete
	ImageBufferInterface*	cpuImage; // Memory lifetime is not tied to the object for now
	GpuImage*				gpuImage;

	Image()
	{
		info = DefaultImage2dInfo( 1, 1 );

		subResourceView.baseArray = 0;
		subResourceView.arrayCount = 1;
		subResourceView.baseMip = 0;
		subResourceView.mipLevels = 1;
		subResourceView.aspect = IMAGE_ASPECT_ALL;

		generateMips = true;

		cpuImage = nullptr;
		gpuImage = nullptr;
	}

	Image( const imageInfo_t& _info ) : Image( _info, nullptr ) {}

	Image( const imageInfo_t& _info, ImageBufferInterface* _cpuImage )
	{
		Create( _info, _cpuImage );
	}

	Image( const imageInfo_t& _info, const char* _name, const gpuImageStateFlags_t _flags, const resourceLifeTime_t _lifetime )
	{
		Create( _info, _name, _flags, _lifetime );
	}

	~Image()
	{
		cpuImage = nullptr;
		gpuImage = nullptr;
	}

	void Create( const imageInfo_t& _info )
	{
		Create( _info, nullptr, 0u );
	}

	void Create( const imageInfo_t& _info, uint8_t* pixelBytes, const uint32_t byteCount );

	void Create( const imageInfo_t& _info, ImageBufferInterface* _cpuImage );

	void Create( const imageInfo_t& _info, const char* _name, const gpuImageStateFlags_t _flags, const resourceLifeTime_t _lifetime );

	void CreateAliased( const imageInfo_t& _info, const char* _name, const gpuImageStateFlags_t _flags, const resourceLifeTime_t _lifetime, AliasableImageHeap& heap );

	void Destroy() override;

	void DestroyCpuData();

	bool OnResize( const uint32_t w, const uint32_t h ) override;

	virtual bool IsView() const { return false; }

	void RegisterResize( ResizeFn fn ) { m_resizeFn = std::move( fn ); }
	static ResizeFn FullDimensionResizeFn( const imageInfo_t& info );

	void Serialize( Serializer* serializer );
};


class ImageLoader : public LoadHandler<Image>
{
private:
	std::string		m_basePath;
	std::string		m_fileName;
	std::string		m_ext;
	bool			m_hdr;
	bool			m_cubemap;
	bool			m_linearColor;

	bool Load( Asset<Image>& texture );

public:
	ImageLoader() : m_cubemap( false ), m_hdr( false ), m_linearColor( false )
	{
	}

	ImageLoader( const std::string& path, const std::string& file, const bool linearColor ) : m_cubemap( false ), m_hdr( false ), m_linearColor( linearColor )
	{
		SetBasePath( path );
		SetTextureFile( file );
	}

	void SetBasePath( const std::string& path );
	void SetTextureFile( const std::string& file );
	void LoadAsCubemap( const bool isCubemap );
	void LoadAsLinear( const bool isLinear );
};

class BakedImageLoader : public LoadHandler<Image>
{
private:
	std::string m_basePath;
	std::string m_fileName;
	std::string m_ext;

	bool Load( Asset<Image>& texture );

public:
	BakedImageLoader() {}
	BakedImageLoader( const std::string& path, const std::string& ext )
	{
		SetBasePath( path );
		SetFileExt( ext );
	}

	void SetBasePath( const std::string& path );
	void SetFileExt( const std::string& ext );
};

using pImgLoader_t = Asset<Image>::loadHandlerPtr_t;
