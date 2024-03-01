#ifndef _NBL_VIDEO_C_RESIZABLE_SURFACE_H_INCLUDED_
#define _NBL_VIDEO_C_RESIZABLE_SURFACE_H_INCLUDED_


#include "nbl/video/utilities/ISimpleManagedSurface.h"


namespace nbl::video
{

// For this whole thing to work, you CAN ONLY ACQUIRE ONE IMAGE AT A TIME BEFORE CALLING PRESENT!
class NBL_API2 IResizableSurface : public ISimpleManagedSurface
{
	public:
		// Simple callback to facilitate detection of window being closed
		class ICallback : public ISimpleManagedSurface::ICallback
		{
			protected:
				// remember to call this when overriding it further down!
				inline virtual bool onWindowResized_impl(uint32_t w, uint32_t h) override
				{
					if (m_recreator)
						m_recreator->explicitRecreateSwapchain(w,h);
					return true;
				}

			private:
				friend class IResizableSurface;
				// `recreator` owns the `ISurface`, which refcounts the `IWindow` which refcounts the callback, so fumb pointer to avoid cycles 
				inline void setSwapchainRecreator(IResizableSurface* recreator) { m_recreator = recreator; }

				IResizableSurface* m_recreator = nullptr;
		};

		//
		struct SPresentSource
		{
			IGPUImage* image;
			VkRect2D rect;
		};

		//
		class NBL_API2 ISwapchainResources : public core::IReferenceCounted, public ISimpleManagedSurface::ISwapchainResources
		{
			protected:
				friend class IResizableSurface;

				// Returns what stage the submit signal semaphore should signal from for the presentation to wait on.
				// The `cmdbuf` is already begun when given to the callback, and will be ended outside.
				// User is responsible for transitioning the image layouts (most notably the swapchain), acquiring ownership etc.
				// Performance Tip: DO NOT transition the layout of `source` inside this callback, have it already in the correct Layout you need!
				// However, if `qFamToAcquireSrcFrom!=IQueue::FamilyIgnored`, you need to acquire the ownership of the `source.image`
				virtual asset::PIPELINE_STAGE_FLAGS tripleBufferPresent(IGPUCommandBuffer* cmdbuf, const SPresentSource& source, const uint8_t dstIx, const uint32_t qFamToAcquireSrcFrom) = 0;
		};

		//
		inline CThreadSafeQueueAdapter* pickQueue(ILogicalDevice* device) const override final
		{
			const auto fam = pickQueueFamily(device);
			return device->getThreadSafeQueue(fam,device->getQueueCount(fam)-1);
		}

		// This is basically a poll, the extent CAN change between a call to this and `present`
		inline VkExtent2D getCurrentExtent()
		{
			std::unique_lock guard(m_swapchainResourcesMutex);
			// because someone might skip an acquire when area==0, handle window closing
			if (m_cb->isWindowOpen())
			while (true)
			{
				auto resources = getSwapchainResources();
				if (resources && resources->getStatus()==ISwapchainResources::STATUS::USABLE)
				{
					auto swapchain = resources->getSwapchain();
					if (swapchain)
					{
						const auto& params = swapchain->getCreationParameters().sharedParams;
						if (params.width>0 && params.height>0)
							return {params.width,params.height};
					}
				}
				// if got some weird invalid extent, try to recreate and retry once
				if (!recreateSwapchain())
					break;
			}
			else
				becomeIrrecoverable();
			return {0,0};
		}

		struct SPresentInfo
		{
			inline operator bool() const {return source.image;}

			SPresentSource source;
			uint8_t mostRecentFamilyOwningSource;
			// only allow waiting for one semaphore, because there's only one source to present!
			IQueue::SSubmitInfo::SSemaphoreInfo wait;
			core::IReferenceCounted* frameResources;
		};		
		// This is a present that you should regularly use from the main rendering thread or something.
		// Due to the constraints and mutexes on everything, its impossible to split this into a separate acquire and present call so this does both.
		// So DON'T USE `acquireNextImage` for frame pacing, it was bad Vulkan practice anyway!
		inline bool present(const SPresentInfo& presentInfo)
		{
			std::unique_lock guard(m_swapchainResourcesMutex);
			// The only thing we want to do under the mutex, is just enqueue a blit and a present, its not a lot.
			// Only acquire ownership if the Blit&Present queue is different to the current one.
			return present_impl(presentInfo,getAssignedQueue()->getFamilyIndex()!=presentInfo.mostRecentFamilyOwningSource);
		}

		// Call this when you want to recreate the swapchain with new extents
		inline bool explicitRecreateSwapchain(const uint32_t w, const uint32_t h, CThreadSafeQueueAdapter* blitAndPresentQueue=nullptr)
		{
			// recreate the swapchain under a mutex
			std::unique_lock guard(m_swapchainResourcesMutex);

			// quick sanity check
			core::smart_refctd_ptr<ISwapchain> oldSwapchain(getSwapchainResources() ? getSwapchainResources()->getSwapchain():nullptr);
			if (oldSwapchain)
			{
				const auto& params = oldSwapchain->getCreationParameters().sharedParams;
				if (w==params.width && h==params.height)
					return true;
			}

			bool retval = recreateSwapchain(w,h);
			auto current = getSwapchainResources();
			// no point racing to present to old SC
			if (current->getSwapchain()==oldSwapchain.get())
				return true;

			// The blit enqueue operations are fast enough to be done under a mutex, this is safer on some platforms. You need to "race to present" to avoid a flicker.
			// Queue family ownership acquire not needed, done by the the very first present when `m_lastPresentSource` wasset.
			return present_impl({.source=m_lastPresentSource,.wait=m_lastPresentWait,.frameResources=nullptr},false);
		}

	protected:
		using ISimpleManagedSurface::ISimpleManagedSurface;
		virtual inline ~IResizableSurface()
		{
			static_cast<ICallback*>(m_cb)->setSwapchainRecreator(nullptr);
		}

		//
		inline void deinit_impl() override final
		{
			// stop any calls into explicit resizes
			std::unique_lock guard(m_swapchainResourcesMutex);
			static_cast<ICallback*>(m_cb)->setSwapchainRecreator(nullptr);

			m_lastPresentWait = {};
			m_lastPresentSource = {};
			m_lastPresentSemaphore = nullptr;
			m_lastPresentSourceImage = nullptr;

			if (m_blitSemaphore)
			{
				auto device = const_cast<ILogicalDevice*>(m_blitSemaphore->getOriginDevice());
				const ISemaphore::SWaitInfo info[1] = {{
					.semaphore = m_blitSemaphore.get(), .value = getAcquireCount()
				}};
				device->blockForSemaphores(info);
			}

			std::fill(m_cmdbufs.begin(),m_cmdbufs.end(),nullptr);
			m_blitSemaphore = nullptr;
		}

		//
		inline bool init_impl(const ISwapchain::SSharedCreationParams& sharedParams) override final
		{
			// swapchain callback already deinitialized, so no mutex needed here

			auto queue = getAssignedQueue();
			auto device = const_cast<ILogicalDevice*>(queue->getOriginDevice());

			m_sharedParams = sharedParams;
			if (!m_sharedParams.deduce(device->getPhysicalDevice(),getSurface()))
				return false;

			// want to keep using the same semaphore throughout the lifetime to not run into sync issues
			if (!m_blitSemaphore)
			{
				m_blitSemaphore = device->createSemaphore(0u);
				if (!m_blitSemaphore)
					return false;
			}

			// transient commandbuffer and pool to perform the blits or copies to SC images
			auto pool = device->createCommandPool(queue->getFamilyIndex(),IGPUCommandPool::CREATE_FLAGS::RESET_COMMAND_BUFFER_BIT);
			if (!pool || !pool->createCommandBuffers(IGPUCommandPool::BUFFER_LEVEL::PRIMARY,{m_cmdbufs.data(),getMaxFramesInFlight()}))
				return false;

			if (!createSwapchainResources())
				return false;
			
			static_cast<ICallback*>(m_cb)->setSwapchainRecreator(this);
			return true;
		}

		//
		inline bool recreateSwapchain(const uint32_t w=0, const uint32_t h=0)
		{
			auto* surface = getSurface();
			auto device = const_cast<ILogicalDevice*>(getAssignedQueue()->getOriginDevice());

			auto swapchainResources = getSwapchainResources();
			// dont assign straight to `m_swapchainResources` because of complex refcounting and cycles
			core::smart_refctd_ptr<ISwapchain> newSwapchain;
			{
				m_sharedParams.width = w;
				m_sharedParams.height = h;
				// Question: should we re-query the supported queues, formats, present modes, etc. just-in-time??
				auto* swapchain = swapchainResources->getSwapchain();
				if (swapchain ? swapchain->deduceRecreationParams(m_sharedParams):m_sharedParams.deduce(device->getPhysicalDevice(),surface))
				{
					// super special case, we can't re-create the swapchain but its possible to recover later on
					if (m_sharedParams.width==0 || m_sharedParams.height==0)
					{
						// we need to keep the old-swapchain around, but can drop the rest
						swapchainResources->invalidate();
						return false;
					}
					// now lets try to create a new swapchain
					if (swapchain)
						newSwapchain = swapchain->recreate(m_sharedParams);
					else
					{
						ISwapchain::SCreationParams params = {
							.surface = core::smart_refctd_ptr<ISurface>(surface),
							.surfaceFormat = {},
							.sharedParams = m_sharedParams
							// we're not going to support concurrent sharing in this simple class
						};
						if (params.deduceFormat(device->getPhysicalDevice()))
							newSwapchain = CVulkanSwapchain::create(core::smart_refctd_ptr<const ILogicalDevice>(device),std::move(params));
					}
				}
				else // parameter deduction failed
					return false;
			}

			if (newSwapchain)
			{
				swapchainResources->invalidate();
				return createSwapchainResources()->onCreateSwapchain(getAssignedQueue()->getFamilyIndex(),std::move(newSwapchain));
			}
			else
			{
				becomeIrrecoverable();
				return false;
			}
		}

		// handlers for acquisition exceptions (will get called under mutexes)
		inline uint8_t handleOutOfDate() override final
		{
			// try again, will re-create swapchain
			return ISimpleManagedSurface::acquireNextImage();
		}

		//
		inline bool present_impl(const SPresentInfo& presentInfo, const bool acquireOwnership)
		{
			// irrecoverable or bad input
			if (!presentInfo || !getSwapchainResources())
				return false;

			// delayed init of our swapchain
			if (getSwapchainResources()->getStatus()!=ISwapchainResources::STATUS::USABLE && !recreateSwapchain())
				return false;

			// now pointer won't change until we get out from under the lock
			auto swapchainResources = static_cast<ISwapchainResources*>(getSwapchainResources());
			assert(swapchainResources);
			
			const uint8_t imageIx = acquireNextImage();
			if (imageIx==ISwapchain::MaxImages)
				return false;

			// now that an image is acquired, we HAVE TO present
			bool willBlit = true;
			const auto acquireCount = getAcquireCount();
			const IQueue::SSubmitInfo::SSemaphoreInfo waitSemaphores[2] = {
				presentInfo.wait,
				{
					.semaphore = getAcquireSemaphore(),
					.value = acquireCount,
					.stageMask = asset::PIPELINE_STAGE_FLAGS::NONE // presentation engine usage isn't a stage
				}
			};
			m_lastPresentSourceImage = core::smart_refctd_ptr<IGPUImage>(presentInfo.source.image);
			m_lastPresentSemaphore = core::smart_refctd_ptr<ISemaphore>(presentInfo.wait.semaphore);
			m_lastPresentSource = presentInfo.source;
			m_lastPresentWait = presentInfo.wait;
			
			//
			auto queue = getAssignedQueue();
			auto device = const_cast<ILogicalDevice*>(queue->getOriginDevice());
			
			// need to wait before resetting a commandbuffer
			const auto maxFramesInFlight = getMaxFramesInFlight();
			if (acquireCount>maxFramesInFlight)
			{
				const ISemaphore::SWaitInfo cmdbufDonePending[1] = {
					{ 
						.semaphore = m_blitSemaphore.get(),
						.value = acquireCount-maxFramesInFlight
					}
				};
				device->blockForSemaphores(cmdbufDonePending);
			}


			// Maybe tie the cmbufs to the Managed Surface instead?
			const auto cmdBufIx = acquireCount%maxFramesInFlight;
			auto cmdbuf = m_cmdbufs[cmdBufIx].get();

			willBlit &= cmdbuf->begin(IGPUCommandBuffer::USAGE::ONE_TIME_SUBMIT_BIT);
			// now enqueue the mini-blit
			const IQueue::SSubmitInfo::SSemaphoreInfo blitted[1] = {
				{
					.semaphore = m_blitSemaphore.get(),
					.value = acquireCount,
					// don't need to predicate with `willBlit` because if `willBlit==false` cmdbuf not properly begun and validation will fail
					.stageMask = swapchainResources->tripleBufferPresent(cmdbuf,presentInfo.source,imageIx,acquireOwnership ? queue->getFamilyIndex():IQueue::FamilyIgnored)
				}
			};
			willBlit &= bool(blitted[1].stageMask.value);
			willBlit &= cmdbuf->end();
			
			const IQueue::SSubmitInfo::SCommandBufferInfo commandBuffers[1] = {{.cmdbuf=cmdbuf}};
			const IQueue::SSubmitInfo submitInfos[1] = {{
				.waitSemaphores = waitSemaphores,
				.commandBuffers = commandBuffers,
				.signalSemaphores = blitted
			}};
			willBlit &= queue->submit(submitInfos)==IQueue::RESULT::SUCCESS;

			// handle two cases of present
			if (willBlit)
			{
				auto frameResources = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<ISwapchain::void_refctd_ptr>>(2);
				frameResources->front() = core::smart_refctd_ptr<core::IReferenceCounted>(presentInfo.frameResources);
				frameResources->back() = core::smart_refctd_ptr<IGPUCommandBuffer>(cmdbuf);
				return ISimpleManagedSurface::present(imageIx,blitted,frameResources.get());
			}
			else
				return ISimpleManagedSurface::present(imageIx,{waitSemaphores+1,1},presentInfo.frameResources);
		}

		// Assume it will execute under a mutex
		virtual ISwapchainResources* createSwapchainResources() = 0;

		// Because the surface can start minimized (extent={0,0}) we might not be able to create the swapchain right away, so store creation parameters until we can create it.
		ISwapchain::SSharedCreationParams m_sharedParams = {};

	private:
		// Have to use a second semaphore to make acquire-present pairs independent of each other, also because there can be no ordering ensured between present->acquire
		core::smart_refctd_ptr<ISemaphore> m_blitSemaphore;
		// Command Buffers for blitting/copying to 
		std::array<core::smart_refctd_ptr<IGPUCommandBuffer>,ISwapchain::MaxImages> m_cmdbufs = {};

		// used to protect access to swapchain resources during present and recreateExplicit
		std::mutex m_swapchainResourcesMutex;
		// Why do we delay the swapchain recreate present until the rendering of the most recent present source is done? Couldn't we present whatever latest Triple Buffer is done?
		// No because there can be presents enqueued whose wait semaphores have not signalled yet, meaning there could be images presented in the future.
		// Unless you like your frames to go backwards in time in a special "rewind glitch" you need to blit the frame that has not been presented yet or is the same as most recently enqueued.
		IQueue::SSubmitInfo::SSemaphoreInfo m_lastPresentWait = {};
		SPresentSource m_lastPresentSource = {};
		core::smart_refctd_ptr<ISemaphore> m_lastPresentSemaphore = {};
		core::smart_refctd_ptr<IGPUImage> m_lastPresentSourceImage = {};
};

// The use of this class is supposed to be externally synchronized
template<typename SwapchainResources> requires std::is_base_of_v<IResizableSurface::ISwapchainResources,SwapchainResources>
class CResizableSurface final : public IResizableSurface
{
	public:
		using this_t = CResizableSurface<SwapchainResources>;		
		// Factory method so we can fail, requires a `_surface` created from a window and with a callback that inherits from `ICallback` declared just above
		static inline core::smart_refctd_ptr<this_t> create(core::smart_refctd_ptr<ISurface>&& _surface)
		{
			if (!_surface)
				return nullptr;

			auto _window = _surface->getWindow();
			if (!_window)
				return nullptr;

			auto cb = dynamic_cast<ICallback*>(_window->getEventCallback());
			if (!cb)
				return nullptr;

			return core::smart_refctd_ptr<this_t>(new this_t(std::move(_surface),cb),core::dont_grab);
		}

	protected:
		using IResizableSurface::IResizableSurface;

		inline bool checkQueueFamilyProps(const IPhysicalDevice::SQueueFamilyProperties& props) const override {return props.queueFlags.hasFlags(SwapchainResources::RequiredQueueFlags);}

		// All of the below are called under a mutex
		inline ISwapchainResources* createSwapchainResources() override
		{
			m_swapchainResources = core::make_smart_refctd_ptr<SwapchainResources>();
			return m_swapchainResources.get();
		}
		inline ISimpleManagedSurface::ISwapchainResources* getSwapchainResources() override {return m_swapchainResources.get();}
		inline void becomeIrrecoverable() override {m_swapchainResources = nullptr;}

	private:
		// As per the above, the swapchain might not be possible to create or recreate right away, so this might be
		// either nullptr before the first successful acquire or the old to-be-retired swapchain.
		core::smart_refctd_ptr<SwapchainResources> m_swapchainResources = {};
};

}
#endif