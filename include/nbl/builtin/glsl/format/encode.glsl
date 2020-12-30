#ifndef _IRR_BUILTIN_GLSL_FORMAT_ENCODE_INCLUDED_
#define _IRR_BUILTIN_GLSL_FORMAT_ENCODE_INCLUDED_

#include <nbl/builtin/glsl/format/constants.glsl>

uvec2 nbl_glsl_encodeRGB19E7(in vec3 col)
{
	const vec3 clamped = clamp(col,vec3(0.0),vec3(nbl_glsl_MAX_RGB19E7));
	const float maxrgb = max(max(clamped.r,clamped.g),clamped.b);

	const int f32_exp = bitfieldExtract(floatBitsToInt(maxrgb),23,8)-127;
	const int shared_exp = clamp(f32_exp,-nbl_glsl_RGB19E7_EXP_BIAS,nbl_glsl_MAX_RGB19E7_EXP);

	const uvec3 mantissas = uvec3(clamped*exp2(nbl_glsl_RGB19E7_MANTISSA_BITS-shared_exp));

	uvec2 encoded;
	encoded.x = bitfieldInsert(mantissas.x,mantissas.y,nbl_glsl_RGB19E7_COMPONENT_BITOFFSETS[1],nbl_glsl_RGB19E7_G_COMPONENT_SPLIT);
	encoded.y = bitfieldInsert(mantissas.y>>nbl_glsl_RGB19E7_G_COMPONENT_SPLIT,mantissas.z,nbl_glsl_RGB19E7_COMPONENT_BITOFFSETS[2],nbl_glsl_RGB19E7_MANTISSA_BITS)|uint((shared_exp+nbl_glsl_RGB19E7_EXP_BIAS)<<nbl_glsl_RGB19E7_COMPONENT_BITOFFSETS[3]);

	return encoded;
}

uint nbl_glsl_encodeRGB10A2(in vec4 col)
{
	const uvec3 rgbMask = uvec3(0x3ffu);
	const vec4 clamped = clamp(col,vec4(0.0),vec4(1.0));
	uvec4 quantized = uvec4(clamped*vec4(vec3(rgbMask),3.0));
	quantized.gba <<= uvec3(10,20,30);
	return quantized.r|quantized.g|quantized.b|quantized.a;
}

#endif