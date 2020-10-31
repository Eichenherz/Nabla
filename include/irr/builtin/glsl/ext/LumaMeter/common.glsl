#ifndef _IRR_GLSL_EXT_LUMA_METER_COMMON_INCLUDED_
#define _IRR_GLSL_EXT_LUMA_METER_COMMON_INCLUDED_

#include <irr/builtin/glsl/macros.glsl>

#ifndef _IRR_GLSL_EXT_LUMA_METER_UNIFORMS_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_UNIFORMS_DEFINED_
struct irr_glsl_ext_LumaMeter_Uniforms_t
{
	vec2 meteringWindowScale;
	vec2 meteringWindowOffset;
};
#endif


#define _IRR_GLSL_EXT_LUMA_METER_MODE_GEOM_MEAN 0
#define _IRR_GLSL_EXT_LUMA_METER_MODE_MEDIAN 1

#if IRR_GLSL_EQUAL(_IRR_GLSL_EXT_LUMA_METER_MODE_DEFINED_,_IRR_GLSL_EXT_LUMA_METER_MODE_MEDIAN)
    #ifndef _IRR_GLSL_EXT_LUMA_METER_BIN_COUNT
        #error "You need to define _IRR_GLSL_EXT_LUMA_METER_BIN_COUNT !"
    #endif
    #if IRR_GLSL_NOT_EQUAL(_IRR_GLSL_WORKGROUP_SIZE_,_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT)
	    #error "_IRR_GLSL_WORKGROUP_SIZE_ does not equal _IRR_GLSL_EXT_LUMA_METER_BIN_COUNT"
    #endif 
    #ifndef _IRR_GLSL_EXT_LUMA_METER_BIN_GLOBAL_REPLICATION
        #error "You need to define _IRR_GLSL_EXT_LUMA_METER_BIN_GLOBAL_REPLICATION !"
    #endif

    #ifdef _IRR_GLSL_EXT_LUMA_METER_FIRST_PASS_DEFINED_
    	#define _IRR_GLSL_EXT_LUMA_METER_PADDED_BIN_COUNT (_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT+1)

	    #ifndef _IRR_GLSL_EXT_LUMA_METER_LOCAL_REPLICATION_POW_DEFINED_
	    #define _IRR_GLSL_EXT_LUMA_METER_LOCAL_REPLICATION_POW_DEFINED_ 3
	    #endif

	    #define _IRR_GLSL_EXT_LUMA_METER_LOCAL_REPLICATION (1<<_IRR_GLSL_EXT_LUMA_METER_LOCAL_REPLICATION_POW_DEFINED_)
	    #define _IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_IMPL_ (_IRR_GLSL_EXT_LUMA_METER_PADDED_BIN_COUNT*_IRR_GLSL_EXT_LUMA_METER_LOCAL_REPLICATION)
    #else
        #define _IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_IMPL_ (_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT*2)
    #endif
    // correct for subgroup emulation stuff
    #if IRR_GLSL_GREATER(_IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_IMPL_,(_IRR_GLSL_WORKGROUP_SIZE_<<1))
        #define _IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_ _IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_IMPL_
    #else
        #define _IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_ (_IRR_GLSL_WORKGROUP_SIZE_<<1)
    #endif

    #if IRR_GLSL_NOT_EQUAL(IRR_GLSL_AND(IRR_GLSL_SUB(_IRR_GLSL_EXT_LUMA_METER_MAX_LUMA_DEFINED_,_IRR_GLSL_EXT_LUMA_METER_MIN_LUMA_DEFINED_),_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT-1),0)
	    #error "The number of bins must evenly divide the histogram range!"
    #endif

    #define _IRR_GLSL_EXT_LUMA_METER_BIN_GLOBAL_COUNT (_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT*_IRR_GLSL_EXT_LUMA_METER_BIN_GLOBAL_REPLICATION)
    struct irr_glsl_ext_LumaMeter_output_t
    {
		uint packedHistogram[_IRR_GLSL_EXT_LUMA_METER_BIN_GLOBAL_COUNT];
    };
#elif _IRR_GLSL_EXT_LUMA_METER_MODE_DEFINED_==_IRR_GLSL_EXT_LUMA_METER_MODE_GEOM_MEAN
    #ifdef _IRR_GLSL_EXT_LUMA_METER_FIRST_PASS_DEFINED_
	    #define _IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_ IRR_GLSL_EVAL(_IRR_GLSL_WORKGROUP_SIZE_)
    #else
        #define _IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_ 0
    #endif

    struct irr_glsl_ext_LumaMeter_output_t
    {
        uint unormAverage;
    };
#else
#error "Unsupported Metering Mode!"
#endif


#define irr_glsl_ext_LumaMeter_output_SPIRV_CROSS_is_dumb_t uint


#ifndef _IRR_GLSL_EXT_LUMA_METER_UNIFORMS_SET_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_UNIFORMS_SET_DEFINED_ 0
#endif

#ifndef _IRR_GLSL_EXT_LUMA_METER_UNIFORMS_BINDING_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_UNIFORMS_BINDING_DEFINED_ 0
#endif


#ifndef _IRR_GLSL_EXT_LUMA_METER_OUTPUT_SET_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_OUTPUT_SET_DEFINED_ 0
#endif

#ifndef _IRR_GLSL_EXT_LUMA_METER_OUTPUT_BINDING_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_OUTPUT_BINDING_DEFINED_ 1
#endif


#ifndef _IRR_GLSL_EXT_LUMA_METER_INPUT_IMAGE_SET_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_INPUT_IMAGE_SET_DEFINED_ 0
#endif

#ifndef _IRR_GLSL_EXT_LUMA_METER_INPUT_IMAGE_BINDING_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_INPUT_IMAGE_BINDING_DEFINED_ 2
#endif


#ifdef _IRR_GLSL_EXT_LUMA_METER_FIRST_PASS_DEFINED_

#ifndef _IRR_GLSL_EXT_LUMA_METER_PUSH_CONSTANTS_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_PUSH_CONSTANTS_DEFINED_
layout(push_constant) uniform PushConstants
{
	int currentFirstPassOutput;
} pc;
#endif

#ifndef _IRR_GLSL_EXT_LUMA_METER_OUTPUT_DESCRIPTOR_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_OUTPUT_DESCRIPTOR_DEFINED_
layout(set=_IRR_GLSL_EXT_LUMA_METER_OUTPUT_SET_DEFINED_, binding=_IRR_GLSL_EXT_LUMA_METER_OUTPUT_BINDING_DEFINED_) restrict coherent buffer OutputBuffer
{
	irr_glsl_ext_LumaMeter_output_t outParams[];
};
#endif

#ifndef _IRR_GLSL_EXT_LUMA_METER_INPUT_IMAGE_DESCRIPTOR_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_INPUT_IMAGE_DESCRIPTOR_DEFINED_
layout(set=_IRR_GLSL_EXT_LUMA_METER_INPUT_IMAGE_SET_DEFINED_, binding=_IRR_GLSL_EXT_LUMA_METER_INPUT_IMAGE_BINDING_DEFINED_) uniform sampler2DArray inputImage;
#endif

#endif // _IRR_GLSL_EXT_LUMA_METER_FIRST_PASS_DEFINED_


#ifdef _IRR_GLSL_SCRATCH_SHARED_DEFINED_
    #if IRR_GLSL_LESS(_IRR_GLSL_SCRATCH_SHARED_SIZE_DEFINED_,_IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_)
        #error "Not enough shared memory declared for ext::LumaMeter!"
    #endif
#else
    #if IRR_GLSL_GREATER(_IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_,0)
        #define _IRR_GLSL_SCRATCH_SHARED_DEFINED_ histogram
        #define _IRR_GLSL_SCRATCH_SHARED_SIZE_DEFINED_ _IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_
        shared uint _IRR_GLSL_SCRATCH_SHARED_DEFINED_[_IRR_GLSL_EXT_LUMA_METER_SHARED_SIZE_NEEDED_];
    #endif
#endif



#if IRR_GLSL_EQUAL(_IRR_GLSL_EXT_LUMA_METER_MODE_DEFINED_,_IRR_GLSL_EXT_LUMA_METER_MODE_MEDIAN)
    #ifdef _IRR_GLSL_EXT_LUMA_METER_FIRST_PASS_DEFINED_
        #ifndef _IRR_GLSL_EXT_LUMA_METER_CLEAR_HISTOGRAM_FUNC_DECLARED_
        #define _IRR_GLSL_EXT_LUMA_METER_CLEAR_HISTOGRAM_FUNC_DECLARED_
        void irr_glsl_ext_LumaMeter_clearHistogram();
        #endif
    #endif

    // TODO: turn `upper_bound__minus_onePoT` into a macro `irr_glsl_parallel_upper_bound__minus_onePoT` and introduce lower_bound, and non-minus one variants
    #if IRR_GLSL_IS_NOT_POT(_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT)
        #error "Parallel Upper Bound requires the Histogram Bin Count to be PoT"
    #endif
    int upper_bound_minus_onePoT(in uint val, int arrayLenPoT)
    {
        arrayLenPoT >>= 1;
        int ret = (val<_IRR_GLSL_SCRATCH_SHARED_DEFINED_[arrayLenPoT]) ? 0:arrayLenPoT;
        for (; arrayLenPoT>0; arrayLenPoT>>=1)
        {
            int right = ret+arrayLenPoT;
            ret = (val<_IRR_GLSL_SCRATCH_SHARED_DEFINED_[right]) ? ret:right;
        }
        return ret;
    }

    struct irr_glsl_ext_LumaMeter_PassInfo_t
    {
        uvec2 percentileRange; // (lowerPercentile,upperPercentile)
    };
#elif _IRR_GLSL_EXT_LUMA_METER_MODE_DEFINED_==_IRR_GLSL_EXT_LUMA_METER_MODE_GEOM_MEAN
    struct irr_glsl_ext_LumaMeter_PassInfo_t
    {
        float rcpFirstPassWGCount;
    };
#endif



#define _IRR_GLSL_EXT_LUMA_METER_GEOM_MEAN_MAX_WG_INCREMENT 0x1000u
#ifdef _IRR_GLSL_EXT_LUMA_METER_FIRST_PASS_DEFINED_
    #ifndef _IRR_GLSL_EXT_LUMA_METER_GET_NEXT_LUMA_OUTPUT_OFFSET_FUNC_DECLARED_
    #define _IRR_GLSL_EXT_LUMA_METER_GET_NEXT_LUMA_OUTPUT_OFFSET_FUNC_DECLARED_
    int irr_glsl_ext_LumaMeter_getNextLumaOutputOffset();
    #endif

    #ifndef _IRR_GLSL_EXT_LUMA_METER_GET_NEXT_LUMA_OUTPUT_OFFSET_FUNC_DEFINED_
    #define _IRR_GLSL_EXT_LUMA_METER_GET_NEXT_LUMA_OUTPUT_OFFSET_FUNC_DEFINED_
    int irr_glsl_ext_LumaMeter_getNextLumaOutputOffset()
    {
        return (pc.currentFirstPassOutput!=0 ? 0:textureSize(inputImage,0).z)+int(gl_WorkGroupID.z);
    }
    #endif


    #ifndef _IRR_GLSL_EXT_LUMA_METER_GET_CURRENT_LUMA_OUTPUT_OFFSET_FUNC_DECLARED_
    #define _IRR_GLSL_EXT_LUMA_METER_GET_CURRENT_LUMA_OUTPUT_OFFSET_FUNC_DECLARED_
    int irr_glsl_ext_LumaMeter_getCurrentLumaOutputOffset();
    #endif

    #ifndef _IRR_GLSL_EXT_LUMA_METER_GET_CURRENT_LUMA_OUTPUT_OFFSET_FUNC_DEFINED_
    #define _IRR_GLSL_EXT_LUMA_METER_GET_CURRENT_LUMA_OUTPUT_OFFSET_FUNC_DEFINED_
    int irr_glsl_ext_LumaMeter_getCurrentLumaOutputOffset()
    {
        return (pc.currentFirstPassOutput!=0 ? textureSize(inputImage,0).z:0)+int(gl_WorkGroupID.z);
    }
    #endif


    #if IRR_GLSL_EQUAL(_IRR_GLSL_EXT_LUMA_METER_MODE_DEFINED_,_IRR_GLSL_EXT_LUMA_METER_MODE_MEDIAN)
        void irr_glsl_ext_LumaMeter_clearHistogram()
        {
	        for (int i=0; i<_IRR_GLSL_EXT_LUMA_METER_LOCAL_REPLICATION; i++)
		        _IRR_GLSL_SCRATCH_SHARED_DEFINED_[gl_LocalInvocationIndex+i*_IRR_GLSL_WORKGROUP_SIZE_] = 0u;
            #if IRR_GLSL_GREATER(_IRR_GLSL_EXT_LUMA_METER_LOCAL_REPLICATION_POW_DEFINED_,0)
	            if (gl_LocalInvocationIndex<_IRR_GLSL_EXT_LUMA_METER_LOCAL_REPLICATION)
		            _IRR_GLSL_SCRATCH_SHARED_DEFINED_[gl_LocalInvocationIndex+_IRR_GLSL_EXT_LUMA_METER_LOCAL_REPLICATION*_IRR_GLSL_WORKGROUP_SIZE_] = 0u;
            #endif
        }
    #endif


    #ifndef _IRR_GLSL_EXT_LUMA_METER_CLEAR_FIRST_PASS_OUTPUT_FUNC_DECLARED_
    #define _IRR_GLSL_EXT_LUMA_METER_CLEAR_FIRST_PASS_OUTPUT_FUNC_DECLARED_
    void irr_glsl_ext_LumaMeter_clearFirstPassOutput();
    #endif

    #ifndef _IRR_GLSL_EXT_LUMA_METER_CLEAR_FIRST_PASS_OUTPUT_FUNC_DEFINED_
    #define _IRR_GLSL_EXT_LUMA_METER_CLEAR_FIRST_PASS_OUTPUT_FUNC_DEFINED_
    void irr_glsl_ext_LumaMeter_clearFirstPassOutput()
    {
        #if IRR_GLSL_EQUAL(_IRR_GLSL_EXT_LUMA_METER_MODE_DEFINED_,_IRR_GLSL_EXT_LUMA_METER_MODE_MEDIAN)
            uint globalIndex = gl_LocalInvocationIndex+gl_WorkGroupID.x*_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT;
            if (globalIndex<_IRR_GLSL_EXT_LUMA_METER_BIN_GLOBAL_COUNT)
            {
    		    outParams[irr_glsl_ext_LumaMeter_getNextLumaOutputOffset()].packedHistogram[globalIndex] = 0u;
            }
        #else
		    if (all(equal(uvec2(0,0),gl_GlobalInvocationID.xy)))
		        outParams[irr_glsl_ext_LumaMeter_getNextLumaOutputOffset()].unormAverage = 0u;
        #endif
    }
    #endif


    #if IRR_GLSL_EQUAL(_IRR_GLSL_EXT_LUMA_METER_MODE_DEFINED_,_IRR_GLSL_EXT_LUMA_METER_MODE_MEDIAN)
        #define WriteOutValue_t uint
    #else
        #define WriteOutValue_t float
    #endif

    #ifndef _IRR_GLSL_EXT_LUMA_METER_SET_FIRST_OUTPUT_FUNC_DECLARED_
    #define _IRR_GLSL_EXT_LUMA_METER_SET_FIRST_OUTPUT_FUNC_DECLARED_
    void irr_glsl_ext_LumaMeter_setFirstPassOutput(in WriteOutValue_t writeOutVal);
    #endif

    #ifndef _IRR_GLSL_EXT_LUMA_METER_SET_FIRST_OUTPUT_FUNC_DEFINED_
    #define _IRR_GLSL_EXT_LUMA_METER_SET_FIRST_OUTPUT_FUNC_DEFINED_
    void irr_glsl_ext_LumaMeter_setFirstPassOutput(in WriteOutValue_t writeOutVal)
    {
        int layerIndex = irr_glsl_ext_LumaMeter_getCurrentLumaOutputOffset();
        #if IRR_GLSL_EQUAL(_IRR_GLSL_EXT_LUMA_METER_MODE_DEFINED_,_IRR_GLSL_EXT_LUMA_METER_MODE_MEDIAN)
            uint globalIndex = gl_LocalInvocationIndex;
            globalIndex += (gl_WorkGroupID.x&uint(_IRR_GLSL_EXT_LUMA_METER_BIN_GLOBAL_REPLICATION-1))*_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT;
		    atomicAdd(outParams[layerIndex].packedHistogram[globalIndex],writeOutVal);
        #else
		    if (gl_LocalInvocationIndex==0u)
		    {
			    float normalizedAvg = writeOutVal/float(_IRR_GLSL_WORKGROUP_SIZE_);
			    uint incrementor = uint(float(_IRR_GLSL_EXT_LUMA_METER_GEOM_MEAN_MAX_WG_INCREMENT)*normalizedAvg+0.5);
			    atomicAdd(outParams[layerIndex].unormAverage,incrementor);
		    }
        #endif
    }
    #endif

    #undef WriteOutValue_t
#endif // _IRR_GLSL_EXT_LUMA_METER_FIRST_PASS_DEFINED_



#ifndef _IRR_GLSL_EXT_LUMA_METER_IMPL_GET_MEASURED_LUMA_FUNC_DECLARED_
#define _IRR_GLSL_EXT_LUMA_METER_IMPL_GET_MEASURED_LUMA_FUNC_DECLARED_
float irr_glsl_ext_LumaMeter_impl_getMeasuredLumaLog2(in irr_glsl_ext_LumaMeter_output_SPIRV_CROSS_is_dumb_t firstPassOutput, in irr_glsl_ext_LumaMeter_PassInfo_t info);
#endif


#ifndef _IRR_GLSL_EXT_LUMA_METER_IMPL_GET_MEASURED_LUMA_FUNC_DEFINED_
#define _IRR_GLSL_EXT_LUMA_METER_IMPL_GET_MEASURED_LUMA_FUNC_DEFINED_

#if IRR_GLSL_EQUAL(_IRR_GLSL_EXT_LUMA_METER_MODE_DEFINED_,_IRR_GLSL_EXT_LUMA_METER_MODE_MEDIAN)
    #include <irr/builtin/glsl/workgroup/arithmetic.glsl>
// TODO: figure out why the irr_glsl_workgroupExclusiveAdd function doesn't work
uint irr_glsl_workgroupExclusiveAdd2(uint val)
{
    uint pingpong = uint(_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT);
    //! Bad INEFFICIENT Kogge-Stone adder, don't implement this way!
    for (int pass = 1; pass < _IRR_GLSL_EXT_LUMA_METER_BIN_COUNT; pass <<= 1)
    {
        uint index = gl_LocalInvocationIndex + pingpong;
        pingpong ^= _IRR_GLSL_EXT_LUMA_METER_BIN_COUNT;

        _IRR_GLSL_SCRATCH_SHARED_DEFINED_[index] = val;
        barrier();
        memoryBarrierShared();
        if (gl_LocalInvocationIndex >= pass)
            val += _IRR_GLSL_SCRATCH_SHARED_DEFINED_[index - pass];
    }
    barrier();
    memoryBarrierShared();
    return val;
}
#endif

float irr_glsl_ext_LumaMeter_impl_getMeasuredLumaLog2(in irr_glsl_ext_LumaMeter_output_SPIRV_CROSS_is_dumb_t firstPassOutput, in irr_glsl_ext_LumaMeter_PassInfo_t info)
{
    #if IRR_GLSL_EQUAL(_IRR_GLSL_EXT_LUMA_METER_MODE_DEFINED_,_IRR_GLSL_EXT_LUMA_METER_MODE_MEDIAN)
        uint histogramVal = firstPassOutput;

        // do the prefix sum stuff
        {
            uint localPrefix = irr_glsl_workgroupExclusiveAdd2(histogramVal);
            // TODO: right now workgroup size must equal _IRR_GLSL_EXT_LUMA_METER_BIN_COUNT, but it would be good if it didn't (we could carry out many prefix sums in serial)
            _IRR_GLSL_SCRATCH_SHARED_DEFINED_[gl_LocalInvocationIndex] = localPrefix;
        }
        barrier();
        memoryBarrierShared();

        // TODO: We can do it better, and without requiring the workgroup size equal the bin count.
        // Assign whole subgroup to do a subgroup_uniform_upper_bound on lower percentile, then do the subgroup_uniform_upper_bound again but in the [previousFound,end) range.
        // a subgroup_uniform bound can be carried out by each subgroup invocation doing an upper_bound on 1/gl_SubgroupSize of the range, then find the first subgroup invocation where the found index is not 1-past-the-end
        float foundPercentiles[2];
        bool lower2Threads = gl_LocalInvocationIndex<2u;
        if (lower2Threads)
        {
            int found = upper_bound_minus_onePoT(info.percentileRange[gl_LocalInvocationIndex],_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT);

            float foundValue = float(found)/float(_IRR_GLSL_EXT_LUMA_METER_BIN_COUNT-1u);
            _IRR_GLSL_SCRATCH_SHARED_DEFINED_[gl_LocalInvocationIndex] = floatBitsToUint(foundValue);
        }
        barrier();
        memoryBarrierShared();

        return (uintBitsToFloat(_IRR_GLSL_SCRATCH_SHARED_DEFINED_[0])+uintBitsToFloat(_IRR_GLSL_SCRATCH_SHARED_DEFINED_[1]))*0.5;
    #else
        return float(firstPassOutput)*info.rcpFirstPassWGCount/float(_IRR_GLSL_EXT_LUMA_METER_GEOM_MEAN_MAX_WG_INCREMENT);
    #endif
}
#endif
#undef _IRR_GLSL_EXT_LUMA_METER_GEOM_MEAN_MAX_WG_INCREMENT



float irr_glsl_ext_LumaMeter_getMeasuredLumaLog2(in irr_glsl_ext_LumaMeter_output_SPIRV_CROSS_is_dumb_t firstPassOutput, in irr_glsl_ext_LumaMeter_PassInfo_t info)
{
    const float MinLuma = intBitsToFloat(_IRR_GLSL_EXT_LUMA_METER_MIN_LUMA_DEFINED_);
    const float MaxLuma = intBitsToFloat(_IRR_GLSL_EXT_LUMA_METER_MAX_LUMA_DEFINED_);
    return irr_glsl_ext_LumaMeter_impl_getMeasuredLumaLog2(firstPassOutput,info)*log2(MaxLuma/MinLuma)+log2(MinLuma);
}


float irr_glsl_ext_LumaMeter_getOptiXIntensity(in float measuredLumaLog2)
{
    return exp2(log2(0.18)-measuredLumaLog2);
}


#endif