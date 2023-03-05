// Copyright (C) 2018-2020 - DevSH Graphics Programming Sp. z O.O.
// This file is part of the "Nabla Engine".
// For conditions of distribution and use, see copyright notice in nabla.h

#ifndef __NBL_ASSET_I_CPU_IMAGE_VIEW_H_INCLUDED__
#define __NBL_ASSET_I_CPU_IMAGE_VIEW_H_INCLUDED__

#include "nbl/asset/IAsset.h"
#include "nbl/asset/ICPUImage.h"
#include "nbl/asset/IImageView.h"

namespace nbl
{
namespace asset
{

class ICPUImageView final : public IImageView<ICPUImage>, public IAsset
{
	public:
		static core::smart_refctd_ptr<ICPUImageView> create(SCreationParams&& params)
		{
			if (!validateCreationParameters(params))
				return nullptr;

			return core::make_smart_refctd_ptr<ICPUImageView>(std::move(params));
		}
		ICPUImageView(SCreationParams&& _params) : IImageView<ICPUImage>(std::move(_params)) {}

		//!
		size_t conservativeSizeEstimate() const override
		{
			return sizeof(SCreationParams);
		}

        core::smart_refctd_ptr<IAsset> clone(uint32_t _depth = ~0u) const override
        {
            auto par = params;
            if (_depth > 0u && par.image)
                par.image = core::smart_refctd_ptr_static_cast<ICPUImage>(par.image->clone(_depth-1u));

            auto cp = core::make_smart_refctd_ptr<ICPUImageView>(std::move(par));
            clone_common(cp.get());

            return cp;
        }

		//!
		void convertToDummyObject(uint32_t referenceLevelsBelowToConvert=0u) override
		{
            convertToDummyObject_common(referenceLevelsBelowToConvert);

			if (referenceLevelsBelowToConvert)
				params.image->convertToDummyObject(referenceLevelsBelowToConvert-1u);
		}

		//!
		_NBL_STATIC_INLINE_CONSTEXPR auto AssetType = ET_IMAGE_VIEW;
		inline IAsset::E_TYPE getAssetType() const override { return AssetType; }

		//!
		const SComponentMapping& getComponents() const { return params.components; }
		SComponentMapping&	getComponents() 
		{ 
			assert(!isImmutable_debug());
			return params.components;
		}

		inline void setAspectFlags(core::bitflag<IImage::E_ASPECT_FLAGS> aspect)
		{
			params.subresourceRange.aspectMask = aspect.value;
		}

		bool canBeRestoredFrom(const IAsset* _other) const override
		{
			if (!compatible(_other))
				return false;
			auto* other = static_cast<const ICPUImageView*>(_other);
			const auto& rhs = other->params;
			if (!params.image->canBeRestoredFrom(other->params.image.get()))
				return false;
			return true;
		}

		bool equals(const IAsset* _other) const override
		{
			if (!compatible(_other))
				return false;
			auto* other = static_cast<const ICPUImageView*>(_other);
			if (!params.image->equals(other->params.image.get()))
				return false;
			return true;
		}

		size_t hash(std::unordered_map<IAsset*, size_t>* temporary_hash_cache = nullptr) const override
		{
			size_t seed = AssetType;
			core::hash_combine(seed, params.flags);
			core::hash_combine(seed, params.format);
			core::hash_combine(seed, params.components);
			core::hash_combine(seed, params.viewType);
			core::hash_combine(seed, params.subresourceRange);
			return seed;
		}

	protected:

		bool compatible(const IAsset* _other) const override {
			if (!IAsset::compatible(_other))
				return false;
			auto* other = static_cast<const ICPUImageView*>(_other);
			const auto& rhs = other->params;
			if (params.flags != rhs.flags)
				return false;
			if (params.format != rhs.format)
				return false;
			if (params.components != rhs.components)
				return false;
			if (params.viewType != rhs.viewType)
				return false;
			if (memcmp(&params.subresourceRange, &rhs.subresourceRange, sizeof(params.subresourceRange)))
				return false;
			return true;
		}

		void restoreFromDummy_impl(IAsset* _other, uint32_t _levelsBelow) override
		{
			auto* other = static_cast<ICPUImageView*>(_other);

			if (_levelsBelow)
			{
				restoreFromDummy_impl_call(params.image.get(), other->params.image.get(), _levelsBelow - 1u);
			}
		}

		bool isAnyDependencyDummy_impl(uint32_t _levelsBelow) const override
		{
			--_levelsBelow;
			return params.image->isAnyDependencyDummy(_levelsBelow);
		}

		virtual ~ICPUImageView() = default;
};

}
}

#endif