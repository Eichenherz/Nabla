#ifndef _NBL_BUILTIN_HLSL_CENTRAL_LIMIT_BLUR_COMMON_INCLUDED_
#define _NBL_BUILTIN_HLSL_CENTRAL_LIMIT_BLUR_COMMON_INCLUDED_

#include <nbl/builtin/hlsl/cpp_compat.hlsl>
#include <nbl/builtin/hlsl/cpp_compat/intrinsics.h>
#include <nbl/builtin/hlsl/glsl_compat/core.hlsl>

namespace nbl 
{
namespace hlsl
{
namespace central_limit_blur
{

enum WrapMode : uint32_t
{
	WRAP_MODE_CLAMP_TO_EDGE,
	WRAP_MODE_CLAMP_TO_BORDER,
	WRAP_MODE_REPEAT,
	WRAP_MODE_MIRROR,
};

enum BorderColor : uint32_t
{
	BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
	BORDER_COLOR_INT_TRANSPARENT_BLACK,
	BORDER_COLOR_FLOAT_OPAQUE_BLACK,
	BORDER_COLOR_INT_OPAQUE_BLACK,
	BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	BORDER_COLOR_INT_OPAQUE_WHITE
};

struct BoxBlurParams
{
	nbl::hlsl::uint32_t4 inputDimensions;
	nbl::hlsl::uint32_t2 chosenAxis;
	float radius;

	uint32_t getDirection()
	{
		return nbl::hlsl::glsl::bitfieldExtract( inputDimensions.w, 30, 2 );
	}
	uint32_t getChannelCount()
	{
		return nbl::hlsl::glsl::bitfieldExtract( inputDimensions.w, 27, 3 );
	}

	uint32_t getWrapMode()
	{
		return nbl::hlsl::glsl::bitfieldExtract( inputDimensions.w, 25, 2 );
	}

	uint32_t getBorderColorType()
	{
		return nbl::hlsl::glsl::bitfieldExtract( inputDimensions.w, 22, 3 );
	}
	nbl::hlsl::float32_t4 getBorderColor()
	{
		nbl::hlsl::float32_t4 borderColor = nbl::hlsl::float32_t4( 1.f, 0.f, 1.f, 1.f );
		switch( getBorderColorType() )
		{
		case BORDER_COLOR_FLOAT_TRANSPARENT_BLACK:
		case BORDER_COLOR_INT_TRANSPARENT_BLACK:
			borderColor = nbl::hlsl::float32_t4( 0.f, 0.f, 0.f, 0.f );
			break;

		case BORDER_COLOR_FLOAT_OPAQUE_BLACK:
		case BORDER_COLOR_INT_OPAQUE_BLACK:
			borderColor = nbl::hlsl::float32_t4( 0.f, 0.f, 0.f, 1.f );
			break;

		case BORDER_COLOR_FLOAT_OPAQUE_WHITE:
		case BORDER_COLOR_INT_OPAQUE_WHITE:
			borderColor = nbl::hlsl::float32_t4( 1.f, 1.f, 1.f, 1.f );
			break;
		}
		return borderColor;
	}
};

}
}
}
#endif


//struct BufferAccessor;
/*
{
	nbl::hlsl::float32_t getPaddedData( const nbl::hlsl::uint32_t3 coordinate, const uint32_t channel );
	void setData( const nbl::hlsl::uint32_t3 coordinate, const uint32_t channel, const nbl::hlsl::float32_t val );
};
*/