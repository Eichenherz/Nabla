// Copyright (C) 2018-2024 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h
#ifndef _NBL_ASSET_C_SPIRV_INTROSPECTOR_H_INCLUDED_
#define _NBL_ASSET_C_SPIRV_INTROSPECTOR_H_INCLUDED_


#include "nbl/core/declarations.h"

#include <cstdint>
#include <memory>

#include "nbl/asset/ICPUShader.h"
#include "nbl/asset/ICPUImageView.h"
#include "nbl/asset/ICPUComputePipeline.h"
#include "nbl/asset/ICPURenderpassIndependentPipeline.h"
//#include "nbl/asset/utils/ShaderRes.h"
#include "nbl/asset/utils/CGLSLCompiler.h"

#include "nbl/core/definitions.h"

namespace spirv_cross
{
    class ParsedIR;
    class Compiler;
	class Resource;
    struct SPIRType;
}

// TODO: put this somewhere useful in a separate header
namespace nbl::core
{

template<typename T>
struct based_offset
{
		constexpr static inline bool IsConst = std::is_const_v<T>;

	public:
		using element_type = T;

		constexpr based_offset() {}

		constexpr explicit based_offset(T* basePtr, T* ptr) : m_byteOffset(ptrdiff_t(ptr)-ptrdiff_t(basePtr)) {}
		constexpr based_offset(const size_t _byteOffset) : m_byteOffset(_byteOffset) {}

		inline explicit operator bool() const {return m_byteOffset!=InvalidOffset;}
		inline T* operator()(std::conditional_t<IsConst,const void*,void*> newBase) const
		{
			std::conditional_t<IsConst,const uint8_t,uint8_t>* retval = nullptr;
			if (bool(*this))
				retval = reinterpret_cast<decltype(retval)>(newBase)+m_byteOffset;
			return reinterpret_cast<T*>(retval);
		}
		
		inline based_offset<T> operator+(const size_t extraOff) const {return {sizeof(T)*extraOff+m_byteOffset};}

		inline auto byte_offset() const {return m_byteOffset;}

	private:
		constexpr static inline size_t InvalidOffset = ~0ull;
		size_t m_byteOffset = InvalidOffset;
};

template<typename T, size_t Extent=std::dynamic_extent>
struct based_span
{
		constexpr static inline bool IsConst = std::is_const_v<T>;

	public:
		using element_type = T;

		constexpr based_span()
		{
			static_assert(sizeof(based_span<T,Extent>)==sizeof(std::span<T,Extent>));
		}

		constexpr explicit based_span(T* basePtr, std::span<T,Extent> span) : m_byteOffset(ptrdiff_t(span.data())-ptrdiff_t(basePtr)), m_size(span.size()) {}
		constexpr based_span(size_t byteOffset, size_t size) : m_byteOffset(byteOffset), m_size(size) {}

		inline bool empty() const {return m_size==0ull;}
		
		inline std::span<T> operator()(std::conditional_t<IsConst,const void*,void*> newBase) const
		{
			std::conditional_t<IsConst,const uint8_t,uint8_t>* retval = nullptr;
			if (!empty())
				retval = reinterpret_cast<decltype(retval)>(newBase)+m_byteOffset;
			return {reinterpret_cast<T*>(retval),m_size};
		}

		inline auto byte_offset() const {return m_byteOffset;}

	private:
		size_t m_byteOffset = ~0ull;
		size_t m_size = 0ull;
};

}

namespace nbl::asset
{

class NBL_API2 CSPIRVIntrospector : public core::Uncopyable
{
	public:
		constexpr static inline uint16_t MaxPushConstantsSize = 256;
		//
		class CIntrospectionData : public core::IReferenceCounted
		{
			public:
				struct SArrayInfo
				{
					union
					{
						uint32_t value = 0;
						struct
						{
							uint32_t specID : 31;
							uint32_t isSpecConstant : 1;
						};
					};

					// illegal for push constant block members
					inline bool isRuntimeSized() const {return !isSpecConstant && value==0;}
				};
				struct SDescriptorInfo
				{
					inline bool operator<(const SDescriptorInfo& _rhs) const
					{
						return binding<_rhs.binding;
					}

					uint32_t binding = ~0u;
					IDescriptor::E_TYPE type = IDescriptor::E_TYPE::ET_COUNT;
				};
		};
		// Forward declare for friendship
		class CPipelineIntrospectionData;
		class CStageIntrospectionData : public CIntrospectionData
		{
			public:
				//! General
				enum class VAR_TYPE : uint8_t
				{
					UNKNOWN_OR_STRUCT,
					U64,
					I64,
					U32,
					I32,
					U16,
					I16,
					U8,
					I8,
					F64,
					F32,
					F16
				};
				struct SInterface
				{
					uint32_t location;
					uint32_t elements; // of array
					VAR_TYPE basetype;

					inline bool operator<(const SInterface& _rhs) const
					{
						return location<_rhs.location;
					}
				};
				struct SInputInterface final : SInterface {};
				struct SOutputInterface : SInterface {};
				struct SFragmentOutputInterface final : SOutputInterface
				{
					//! for dual source blending
					uint8_t colorIndex;
				};
				struct STypeInfo
				{
					inline bool isScalar() const {return lastRow==0 && lastCol==0;}
					inline bool isVector() const {return lastRow>0 && lastCol==0;}
					inline bool isMatrix() const {return lastRow>0 && stride>0;}

					uint16_t lastRow : 2 = 0;
					uint16_t lastCol : 2 = 0;
					//! rowMajor=false implies col-major
					uint16_t rowMajor : 1 = true;
					//! stride==0 implies not matrix
					uint16_t stride : 11 = 0;
					VAR_TYPE type : 6 = VAR_TYPE::UNKNOWN_OR_STRUCT;
					uint8_t restrict_ : 1 = false;
					uint8_t aliased : 1 = false;
				};
				//
				template<typename T, bool Mutable>
				using ptr_t = std::conditional_t<Mutable,core::based_offset<T>,const T*>;
				//
				template<typename T, bool Mutable>
				using span_t = std::conditional_t<Mutable,core::based_span<T>,std::span<const T>>;
				//
				template<bool Mutable=false>
				struct SType
				{
					public:
						inline bool isArray() const {return !count.empty();}

						//! children
						using member_type_t = ptr_t<SType<Mutable>,Mutable>;
						inline ptr_t<member_type_t,Mutable> memberTypes() const {return reinterpret_cast<const ptr_t<member_type_t,Mutable>&>(memberInfoStorage);}
						using member_name_t = span_t<char,Mutable>;
						inline ptr_t<member_name_t,Mutable> memberNames() const {return reinterpret_cast<const ptr_t<member_name_t,Mutable>&>(memberTypes()+memberCount);}

						//! SPIR-V is a bit dumb and types of the same struct can have variable size depending on std140, std430, or scalar layouts
						//! it would have been much easier if the layout was baked into the struct, so there'd be a separate type
						//! (like a template instantation) of each free-floating struct in different layouts or at least like cv-qualifiers baked into a type.
						using member_size_t = uint32_t;
						// This is the size of the entire member, so for an array it includes everything
						inline ptr_t<member_size_t,Mutable> memberSizes() const {return reinterpret_cast<const ptr_t<member_size_t,Mutable>&>(memberNames()+memberCount);}
						using member_offset_t = member_size_t;
						inline ptr_t<member_offset_t,Mutable> memberOffsets() const {return memberSizes()+memberCount;}
						// `memberStrides[i]` only relevant if `memberTypes[i]->isArray()`
						using member_stride_t = uint32_t;
						inline ptr_t<member_stride_t,Mutable> memberStrides() const {return memberOffsets()+memberCount;}

						constexpr static inline size_t StoragePerMember = sizeof(member_type_t)+sizeof(member_name_t)+sizeof(member_size_t)+sizeof(member_offset_t)+sizeof(member_stride_t);

						//! self
						span_t<char,Mutable> typeName = {};
						span_t<SArrayInfo,Mutable> count = {};
						STypeInfo info = {};

					//private:
						uint32_t memberCount = 0;
						ptr_t<char,Mutable> memberInfoStorage = {};
				};
				template<bool Mutable=false>
				using type_ptr = SType<Mutable>::member_type_t;
				template<bool Mutable=false>
				struct SMemoryBlock
				{
					type_ptr<Mutable> type = nullptr;
				};
				template<bool Mutable, class Pre>
				inline void visitMemoryBlockPreOrderDFS(SMemoryBlock<Mutable>& block, Pre& pre)
				{
					auto* const basePtr = m_memPool.data();

					std::stack<type_ptr<Mutable>> stk;
					if (block.type)
						stk.push(block.type);
					while (!stk.empty())
					{
						const auto& entry = stk.top();
						std::conditional_t<Mutable,SType<true>,const SType<false>>* type;
						if constexpr (Mutable)
							type = entry(basePtr);
						else
							type = entry;

						stk.pop();

						const type_ptr<Mutable>* members;
						if constexpr(Mutable)
							members = type->memberTypes()(basePtr);
						else
							members = type->memberTypes();
						for (auto i=0u; i<type->memberCount; i++)
							stk.push(members[i]);

						pre(type);
					}
				}
				//! Maybe one day in the future they'll use memory blocks, but not now
				template<bool Mutable=false>
				struct SSpecConstant final
				{
					span_t<char,Mutable> name = {};
					union {
						uint64_t u64;
						int64_t i64;
						uint32_t u32;
						int32_t i32;
						double f64;
						float f32;
					} defaultValue;
					uint32_t id;
					uint32_t byteSize;
					VAR_TYPE type;
				};

				//! Push Constants
				template<bool Mutable=false>
				struct SPushConstantInfo : SMemoryBlock<Mutable>
				{
					span_t<char,Mutable> name = {};
					// believe it or not you can declare an empty PC block
					inline bool present() const {return SMemoryBlock<Mutable>::type;}
				};

				//! Descriptors
				template<bool Mutable=false>
				struct SDescriptorVarInfo final : SDescriptorInfo
				{
					struct SRWDescriptor
					{
						uint8_t readonly : 1 = false;
						uint8_t writeonly : 1 = false;
					};
					struct SImage
					{
						IImageView<ICPUImage>::E_TYPE viewType : 3 = IImageView<ICPUImage>::E_TYPE::ET_2D;
						uint8_t shadow : 1 = false;
					};
					struct SCombinedImageSampler final : SImage
					{
						uint8_t multisample : 1;
					};
					struct SStorageImage final : SRWDescriptor, SImage
					{
						// `EF_UNKNOWN` means that Shader will use the StoreWithoutFormat or LoadWithoutFormat capability
						E_FORMAT format = EF_UNKNOWN;
					};
					struct SUniformTexelBuffer final
					{
					};
					struct SStorageTexelBuffer final : SRWDescriptor
					{
					};
					struct SUniformBuffer final : SMemoryBlock<Mutable>
					{
						size_t size = 0;
					};
					struct SStorageBuffer final : SRWDescriptor, SMemoryBlock<Mutable>
					{
						template<bool C=!Mutable>
						inline std::enable_if_t<C,bool> isLastMemberRuntimeSized() const
						{
							if (type->memberCount)
								return type->memberTypes()[type->memberCount-1].count.isRuntimeSized();
							return false;
						}
						template<bool C=!Mutable>
						inline std::enable_if_t<C,size_t> getRuntimeSize(const size_t lastMemberElementCount) const
						{
							if (isLastMemberRuntimeSized())
							{
								const auto& lastMember = type->memberTypes()[type->memberCount-1];
								assert(!lastMember.count.isSpecConstantID);
								return sizeWithoutLastMember+lastMemberElementCount*type->memberStrides()[type->memberCount-1];
							}
							return sizeWithoutLastMember;
						}

						//! Use `getRuntimeSize` for size of the struct with assumption of passed number of elements.
						//! Need special handling if last member is rutime-sized array (e.g. buffer SSBO `buffer { float buf[]; }`)
						size_t sizeWithoutLastMember;
					};
					struct SInputAttachment final
					{
						uint32_t index;
					};

					inline bool isArray() const {return count.empty();}

					//! Note: for SSBOs and UBOs it's the block name
					span_t<char,Mutable> name = {};
					span_t<SArrayInfo,Mutable> count = {};
					uint8_t restrict_ : 1 = false;
					uint8_t aliased : 1 = false;
					//
					union
					{
						SCombinedImageSampler combinedImageSampler;
						SStorageImage storageImage;
						SUniformTexelBuffer uniformTexelBuffer;
						SStorageTexelBuffer storageTexelBuffer;
						SUniformBuffer uniformBuffer;
						SStorageBuffer storageBuffer;
						SInputAttachment inputAttachment;
						// TODO: acceleration structure?
					};
				};

				//! For the Factory Creation
				struct SParams
				{
					std::string entryPoint;
					core::smart_refctd_ptr<const ICPUShader> shader;

					bool operator==(const SParams& rhs) const
					{
						if (entryPoint != rhs.entryPoint)
							return false;
						if (!rhs.shader)
							return false;
						if (shader->getStage() != rhs.shader->getStage())
							return false;
						if (shader->getContentType() != rhs.shader->getContentType())
							return false;
						if (shader->getContent()->getSize() != rhs.shader->getContent()->getSize())
							return false;
						return memcmp(shader->getContent()->getPointer(), rhs.shader->getContent()->getPointer(), shader->getContent()->getSize()) == 0;
					}
				};
				inline const auto& getParams() const {return m_params;}

				//! TODO: Add getters for all the other members!
				inline const auto& getDescriptorSetInfo(const uint8_t set) const {return m_descriptorSetBindings[set];}

				/*inline bool canSpecializationlesslyCreateDescSetFrom() const
				{
					for (const auto& descSet : m_descriptorSetBindings)
					{
						auto found = std::find_if(descSet.begin(),descSet.end(),[](const SDescriptorVarInfo<>& bnd)->bool{ return bnd.count.isSpecConstant();});
						if (found!=descSet.end())
							return false;
					}
					return true;
				}*/

				// all members are set-up outside the ctor
				inline CStageIntrospectionData()
				{
					// TODO: add these up-front
					//m_typenames[][] = addString("float64_t");
				}

				// We don't need to do anything, all the data was allocated from vector pools
				inline ~CStageIntrospectionData() {}

			protected:
				friend CSPIRVIntrospector;

				//! Only call these during construction!
				inline size_t allocOffset(const size_t bytes)
				{
					const size_t off = m_memPool.size();
					m_memPool.resize(off+bytes);
					return off;
				}
				template<typename T>
				inline core::based_span<T> alloc(const size_t count)
				{
					const auto off = allocOffset(sizeof(T)*count);
					return {off,count};
				}
				inline core::based_span<SArrayInfo> addCounts(const size_t count, const uint32_t* sizes, const bool* size_is_literal)
				{
					if (count)
					{
						const auto range = alloc<SArrayInfo>(count);
						auto arraySizes = range(m_memPool.data());
						for (size_t i=0; i<count; i++)
						{
							// the API for this spec constant checking is truly messed up
							if (size_is_literal[i])
								arraySizes[i].value = sizes[i];
							else
							{
								arraySizes[i].specID = sizes[i]; // ID of spec constant if size is spec constant
								arraySizes[i].isSpecConstant = true;
							}
						}
						return range;
					}
					else
						return {};
				}
				inline core::based_span<char> addString(const std::string_view str)
				{
					const auto range = alloc<char>(str.size()+1);
					const auto out = range(m_memPool.data());
					memcpy(out.data(),str.data(),str.size());
					out.back() = '\0';
					return range;
				}
				SDescriptorVarInfo<true>* addResource(const spirv_cross::Compiler& comp, const spirv_cross::Resource& r, IDescriptor::E_TYPE restype);
				inline core::based_offset<SType<true>> addType(const size_t memberCount)
				{
					const auto memberStorage = allocOffset(SType<true>::StoragePerMember*memberCount);
					auto retval = alloc<SType<true>>(1);
					// allocated everything before touching any data stores, to avoid pointer invalidation
					auto pType = retval(m_memPool.data()).data();
					pType->memberCount = memberCount;
					pType->memberInfoStorage = {memberStorage};
					return {retval.byte_offset()};
				}
				void shaderMemBlockIntrospection(const spirv_cross::Compiler& comp, SMemoryBlock<true>* root, const spirv_cross::Resource& r);
				void finalizeShaderMemBlocks();
				std::string printMemBlock(const SMemoryBlock<false>& block) const;
				
				// Parameters it was created with
				SParams m_params;
				//! Sorted by `id`
				core::vector<SSpecConstant<>> m_specConstants; // TODO: maybe unordered_set?
				//! Sorted by `location`
				core::vector<SInputInterface> m_input;
				std::variant<
					core::vector<SFragmentOutputInterface>, // when `params.shader->getStage()==ESS_FRAGMENT`
					core::vector<SOutputInterface> // otherwise
				> m_output;
				//!
				SPushConstantInfo<> m_pushConstants = {};
				//! Each vector is sorted by `binding`
				constexpr static inline uint8_t DescriptorSetCount = 4;
				core::vector<SDescriptorVarInfo<>> m_descriptorSetBindings[DescriptorSetCount];
				// The idea is that we construct with based_span (so we can add `.data()` when accessing)
				// then just convert from `SMemoryBlock<true>` to `SMemoryBlock<false>`
				// in-place when filling in the `descriptorSetBinding` vector which we pass to ctor
				core::vector<char> m_memPool;
		};
		class CPipelineIntrospectionData final : public CIntrospectionData
		{
			public:
				struct SDescriptorInfo final : CIntrospectionData::SDescriptorInfo
				{
					inline bool isArray() const {return count || isRuntimeSized;}

					uint32_t count : 31;
					uint32_t isRuntimeSized : 1;
					// Which shader stages touch it
					core::bitflag<ICPUShader::E_SHADER_STAGE> stageMask = ICPUShader::ESS_UNKNOWN;
				};
				//
				inline CPipelineIntrospectionData()
				{
					std::fill(m_pushConstantBytes.begin(),m_pushConstantBytes.end(),ICPUShader::ESS_UNKNOWN);
				}

				// returns true if successfully added all the info to self, false if incompatible with what's already in our pipeline or incomplete (e.g. missing spec constants)
				NBL_API2 bool merge(const CStageIntrospectionData* stageData, const ICPUShader::SSpecInfoBase::spec_constant_map_t* specConstants=nullptr);

				//
				NBL_API2 core::smart_refctd_dynamic_array<SPushConstantRange> createPushConstantRangesFromIntrospection();
				NBL_API2 core::smart_refctd_ptr<ICPUDescriptorSetLayout> createApproximateDescriptorSetLayoutFromIntrospection(const uint32_t setID);
				NBL_API2 core::smart_refctd_ptr<ICPUPipelineLayout> createApproximatePipelineLayoutFromIntrospection();

			protected:
				// ESS_UNKNOWN on a byte means its not declared in any shader merged so far
				std::array<core::bitflag<ICPUShader::E_SHADER_STAGE>,MaxPushConstantsSize> m_pushConstantBytes;
				//
				struct Hash
				{
					inline size_t operator()(const SDescriptorInfo& item) const {return item.binding;}
				};
				struct KeyEqual
				{
					inline bool operator()(const SDescriptorInfo& lhs, const SDescriptorInfo& rhs) const
					{
						return lhs.binding==rhs.binding;
					}
				};
				core::unordered_set<SDescriptorInfo,Hash,KeyEqual> m_descriptorSetBindings[4];
		};

		// 
		CSPIRVIntrospector() = default;

		//! params.cpuShader.contentType should be ECT_SPIRV
		//! the compiled SPIRV must be compiled with IShaderCompiler::SCompilerOptions::debugInfoFlags enabling EDIF_SOURCE_BIT implicitly or explicitly, with no `spirvOptimizer` used in order to include names in introspection data
		inline core::smart_refctd_ptr<const CStageIntrospectionData> introspect(const CStageIntrospectionData::SParams& params, bool insertToCache=true)
		{
			if (!params.shader)
				return nullptr;
    
			if (params.shader->getContentType()!=IShader::E_CONTENT_TYPE::ECT_SPIRV)
				return nullptr;

			// TODO: templated find!
			//auto introspectionData = m_introspectionCache.find(params);
			//if (introspectionData != m_introspectionCache.end())
			//	return *introspectionData;

			auto introspection = doIntrospection(params);

			//if (insertToCache)
			//	m_introspectionCache.insert(introspectionData,introspection);

			return introspection;
		}
		
		inline core::smart_refctd_ptr<ICPUComputePipeline> createApproximateComputePipelineFromIntrospection(const ICPUShader::SSpecInfo& info, core::smart_refctd_ptr<ICPUPipelineLayout>&& layout=nullptr)
		{
			if (info.shader->getStage()!=IShader::ESS_COMPUTE)
				return nullptr;

			// TODO: 
			// 1. find or perform introspection using `info`
			// 2. if `layout` then just check for compatiblity
			// 3. if `!layout` then create `CPipelineIntrospectionData` from the stage introspection and create a Layout

			ICPUComputePipeline::SCreationParams params = {{.layout = layout.get()}};
			params.shader = info;
			return ICPUComputePipeline::create(params);
		}

#if 0 // wait until Renderpass Indep completely gone and Graphics Pipeline is used in a new way
		core::smart_refctd_ptr<ICPURenderpassIndependentPipeline> createApproximateRenderpassIndependentPipelineFromIntrospection(const std::span<const ICPUShader::SSpecInfo> _infos);
		struct CShaderStages
		{
			const CStageIntrospectionData* vertex = nullptr;
			const CStageIntrospectionData* fragment = nullptr;
			const CStageIntrospectionData* control = nullptr;
			const CStageIntrospectionData* evaluation = nullptr;
			const CStageIntrospectionData* geometry = nullptr;
		}
		core::smart_refctd_ptr<ICPUGraphicsPipeline> createApproximateGraphicsPipeline(const CShaderStages& shaderStages);
#endif	
	private:
		core::smart_refctd_ptr<const CStageIntrospectionData> doIntrospection(const CStageIntrospectionData::SParams& params);
		size_t calcBytesizeforType(spirv_cross::Compiler& comp, const spirv_cross::SPIRType& type) const;

		struct KeyHasher
		{
			using is_transparent = void;

			inline size_t operator()(const CStageIntrospectionData::SParams& params) const
			{
				auto stringViewHasher = std::hash<std::string_view>();

				auto code = std::string_view(reinterpret_cast<const char*>(params.shader->getContent()->getPointer()),params.shader->getContent()->getSize());
				size_t hash = stringViewHasher(code);

				core::hash_combine<std::string_view>(hash, std::string_view(params.entryPoint));
				core::hash_combine<uint32_t>(hash, static_cast<uint32_t>(params.shader->getStage()));

				return hash;
			}
			inline size_t operator()(const core::smart_refctd_ptr<const CStageIntrospectionData>& data) const
			{
				return operator()(data->getParams());
			}
		};
		struct KeyEquals
		{
			using is_transparent = void;

			inline bool operator()(const CStageIntrospectionData::SParams& lhs, const core::smart_refctd_ptr<const CStageIntrospectionData>& rhs) const
			{
				return lhs==rhs->getParams();
			}
			inline bool operator()(const core::smart_refctd_ptr<const CStageIntrospectionData>& lhs, const CStageIntrospectionData::SParams& rhs) const
			{
				return lhs->getParams()==rhs;
			}
			inline bool operator()(const core::smart_refctd_ptr<const CStageIntrospectionData>& lhs, const core::smart_refctd_ptr<const CStageIntrospectionData>& rhs) const
			{
				return operator()(lhs,rhs->getParams());
			}
		};

		//using ParamsToDataMap = core::unordered_set<core::smart_refctd_ptr<const CStageIntrospectionData>,KeyHasher,KeyEquals>;
		//ParamsToDataMap m_introspectionCache;
};

} // nbl::asset
#endif
