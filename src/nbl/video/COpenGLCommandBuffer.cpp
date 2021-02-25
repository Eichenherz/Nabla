#include "nbl/video/COpenGLCommandBuffer.h"

namespace nbl {
namespace video
{

    void COpenGLCommandBuffer::copyBufferToImage(const SCmd<ECT_COPY_BUFFER_TO_IMAGE>& c, IOpenGL_FunctionTable* gl, SOpenGLContextLocalCache* ctxlocal)
    {
        IGPUImage* dstImage = c.dstImage.get();
        IGPUBuffer* srcBuffer = c.srcBuffer.get();
        if (!dstImage->validateCopies(c.regions, c.regions + c.regionCount, srcBuffer))
            return;

        const auto params = dstImage->getCreationParameters();
        const auto type = params.type;
        const auto format = params.format;
        const bool compressed = asset::isBlockCompressionFormat(format);
        auto dstImageGL = static_cast<COpenGLImage*>(dstImage);
        GLuint dst = dstImageGL->getOpenGLName();
        GLenum glfmt, gltype;
        getOpenGLFormatAndParametersFromColorFormat(format, glfmt, gltype);

        const auto bpp = asset::getBytesPerPixel(format);
        const auto blockDims = asset::getBlockDimensions(format);

        ctxlocal->nextState.pixelUnpack.buffer = core::smart_refctd_ptr<const COpenGLBuffer>(static_cast<COpenGLBuffer*>(srcBuffer));
        for (auto it = c.regions; it != c.regions + c.regionCount; it++)
        {
            // TODO: check it->bufferOffset is aligned to data type of E_FORMAT
            //assert(?);

            uint32_t pitch = ((it->bufferRowLength ? it->bufferRowLength : it->imageExtent.width) * bpp).getIntegerApprox();
            int32_t alignment = 0x1 << core::min(core::max(core::findLSB(it->bufferOffset), core::findLSB(pitch)), 3u);
            ctxlocal->nextState.pixelUnpack.alignment = alignment;
            ctxlocal->nextState.pixelUnpack.rowLength = it->bufferRowLength;
            ctxlocal->nextState.pixelUnpack.imgHeight = it->bufferImageHeight;

            if (compressed)
            {
                ctxlocal->nextState.pixelUnpack.BCwidth = blockDims[0];
                ctxlocal->nextState.pixelUnpack.BCheight = blockDims[1];
                ctxlocal->nextState.pixelUnpack.BCdepth = blockDims[2];
                // TODO flush
                //ctx->flushStateGraphics(GSB_PIXEL_PACK_UNPACK);

                uint32_t imageSize = pitch;
                switch (type)
                {
                case IGPUImage::ET_1D:
                    imageSize *= it->imageSubresource.layerCount;
                    gl->extGlCompressedTextureSubImage2D(dst, GL_TEXTURE_1D_ARRAY, it->imageSubresource.mipLevel,
                        it->imageOffset.x, it->imageSubresource.baseArrayLayer,
                        it->imageExtent.width, it->imageSubresource.layerCount,
                        dstImageGL->getOpenGLSizedFormat(), imageSize, reinterpret_cast<const void*>(it->bufferOffset));
                    break;
                case IGPUImage::ET_2D:
                    imageSize *= (it->bufferImageHeight ? it->bufferImageHeight : it->imageExtent.height);
                    imageSize *= it->imageSubresource.layerCount;
                    gl->extGlCompressedTextureSubImage3D(dst, GL_TEXTURE_2D_ARRAY, it->imageSubresource.mipLevel,
                        it->imageOffset.x, it->imageOffset.y, it->imageSubresource.baseArrayLayer,
                        it->imageExtent.width, it->imageExtent.height, it->imageSubresource.layerCount,
                        dstImageGL->getOpenGLSizedFormat(), imageSize, reinterpret_cast<const void*>(it->bufferOffset));
                    break;
                case IGPUImage::ET_3D:
                    imageSize *= (it->bufferImageHeight ? it->bufferImageHeight : it->imageExtent.height);
                    imageSize *= it->imageExtent.depth;
                    gl->extGlCompressedTextureSubImage3D(dst, GL_TEXTURE_3D, it->imageSubresource.mipLevel,
                        it->imageOffset.x, it->imageOffset.y, it->imageOffset.z,
                        it->imageExtent.width, it->imageExtent.height, it->imageExtent.depth,
                        dstImageGL->getOpenGLSizedFormat(), imageSize, reinterpret_cast<const void*>(it->bufferOffset));
                    break;
                }
            }
            else
            {
                // TODO flush
                //ctx->flushStateGraphics(GSB_PIXEL_PACK_UNPACK);
                switch (type)
                {
                case IGPUImage::ET_1D:
                    gl->extGlTextureSubImage2D(dst, GL_TEXTURE_1D_ARRAY, it->imageSubresource.mipLevel,
                        it->imageOffset.x, it->imageSubresource.baseArrayLayer,
                        it->imageExtent.width, it->imageSubresource.layerCount,
                        glfmt, gltype, reinterpret_cast<const void*>(it->bufferOffset));
                    break;
                case IGPUImage::ET_2D:
                    gl->extGlTextureSubImage3D(dst, GL_TEXTURE_2D_ARRAY, it->imageSubresource.mipLevel,
                        it->imageOffset.x, it->imageOffset.y, it->imageSubresource.baseArrayLayer,
                        it->imageExtent.width, it->imageExtent.height, it->imageSubresource.layerCount,
                        glfmt, gltype, reinterpret_cast<const void*>(it->bufferOffset));
                    break;
                case IGPUImage::ET_3D:
                    gl->extGlTextureSubImage3D(dst, GL_TEXTURE_3D, it->imageSubresource.mipLevel,
                        it->imageOffset.x, it->imageOffset.y, it->imageOffset.z,
                        it->imageExtent.width, it->imageExtent.height, it->imageExtent.depth,
                        glfmt, gltype, reinterpret_cast<const void*>(it->bufferOffset));
                    break;
                }
            }
        }
    }

    void COpenGLCommandBuffer::copyImageToBuffer(const SCmd<ECT_COPY_IMAGE_TO_BUFFER>& c, IOpenGL_FunctionTable* gl, SOpenGLContextLocalCache* ctxlocal)
    {
        auto* srcImage = c.srcImage.get();
        auto* dstBuffer = c.dstBuffer.get();
        if (!srcImage->validateCopies(c.regions, c.regions + c.regionCount, dstBuffer))
            return;

        const auto params = srcImage->getCreationParameters();
        const auto type = params.type;
        const auto format = params.format;
        const bool compressed = asset::isBlockCompressionFormat(format);
        GLuint src = static_cast<COpenGLImage*>(srcImage)->getOpenGLName();
        GLenum glfmt, gltype;
        getOpenGLFormatAndParametersFromColorFormat(format, glfmt, gltype);

        const auto bpp = asset::getBytesPerPixel(format);
        const auto blockDims = asset::getBlockDimensions(format);

        ctxlocal->nextState.pixelPack.buffer = core::smart_refctd_ptr<const COpenGLBuffer>(static_cast<COpenGLBuffer*>(dstBuffer));
        for (auto it = c.regions; it != c.regions + c.regionCount; it++)
        {
            // TODO: check it->bufferOffset is aligned to data type of E_FORMAT
            //assert(?);

            uint32_t pitch = ((it->bufferRowLength ? it->bufferRowLength : it->imageExtent.width) * bpp).getIntegerApprox();
            int32_t alignment = 0x1 << core::min(core::max(core::findLSB(it->bufferOffset), core::findLSB(pitch)), 3u);
            ctxlocal->nextState.pixelPack.alignment = alignment;
            ctxlocal->nextState.pixelPack.rowLength = it->bufferRowLength;
            ctxlocal->nextState.pixelPack.imgHeight = it->bufferImageHeight;

            auto yStart = type == IGPUImage::ET_1D ? it->imageSubresource.baseArrayLayer : it->imageOffset.y;
            auto yRange = type == IGPUImage::ET_1D ? it->imageSubresource.layerCount : it->imageExtent.height;
            auto zStart = type == IGPUImage::ET_2D ? it->imageSubresource.baseArrayLayer : it->imageOffset.z;
            auto zRange = type == IGPUImage::ET_2D ? it->imageSubresource.layerCount : it->imageExtent.depth;
            if (compressed)
            {
                ctxlocal->nextState.pixelPack.BCwidth = blockDims[0];
                ctxlocal->nextState.pixelPack.BCheight = blockDims[1];
                ctxlocal->nextState.pixelPack.BCdepth = blockDims[2];
                // TODO flush
                //ctx->flushStateGraphics(GSB_PIXEL_PACK_UNPACK);

                // TODO impl in func table
                //gl->extGlGetCompressedTextureSubImage(src, it->imageSubresource.mipLevel, it->imageOffset.x, yStart, zStart, it->imageExtent.width, yRange, zRange,
                //    dstBuffer->getSize() - it->bufferOffset, reinterpret_cast<void*>(it->bufferOffset));
            }
            else
            {
                // TODO flush
                //ctx->flushStateGraphics(GSB_PIXEL_PACK_UNPACK);

                // TODO impl in func table
                //gl->extGlGetTextureSubImage(src, it->imageSubresource.mipLevel, it->imageOffset.x, yStart, zStart, it->imageExtent.width, yRange, zRange,
                //    glfmt, gltype, dstBuffer->getSize() - it->bufferOffset, reinterpret_cast<void*>(it->bufferOffset));
            }
        }
    }

    void COpenGLCommandBuffer::beginRenderpass_clearAttachments(IOpenGL_FunctionTable* gl, const SRenderpassBeginInfo& info, GLuint fbo)
    {
        auto& rp = info.framebuffer->getCreationParameters().renderpass;
        auto& sub = rp->getSubpasses().begin()[0];
        auto* color = sub.colorAttachments;
        auto* depthstencil = sub.depthStencilAttachment;
        auto* descriptions = rp->getAttachments().begin();

        for (uint32_t i = 0u; i < sub.colorAttachmentCount; ++i)
        {
            // TODO how do i set clear color in vulkan ???
            GLfloat colorf[4]{ 0.f, 0.f, 0.f, 0.f };
            GLint colori[4]{ 0,0,0,0 };
            GLuint coloru[4]{ 0u,0u,0u,0u };

            uint32_t a = color[i].attachment;
            if (descriptions[a].loadOp == asset::IRenderpass::ELO_CLEAR)
            {
                asset::E_FORMAT fmt = descriptions[a].format;

                if (asset::isFloatingPointFormat(fmt))
                {
                    gl->extGlClearNamedFramebufferfv(fbo, GL_COLOR, GL_COLOR_ATTACHMENT0 + i, colorf);
                }
                else if (asset::isIntegerFormat(fmt))
                {
                    if (asset::isSignedFormat(fmt))
                    {
                        gl->extGlClearNamedFramebufferiv(fbo, GL_COLOR, GL_COLOR_ATTACHMENT0 + i, colori);
                    }
                    else
                    {
                        gl->extGlClearNamedFramebufferuiv(fbo, GL_COLOR, GL_COLOR_ATTACHMENT0 + i, coloru);
                    }
                }
            }
        }
        if (depthstencil)
        {
            auto* depthstencilDescription = descriptions + depthstencil->attachment;
            if (depthstencilDescription->loadOp == asset::IRenderpass::ELO_CLEAR)
            {
                asset::E_FORMAT fmt = depthstencilDescription->format;

                // isnt there a way in vulkan to clear only depth or only stencil part?? TODO

                // how do i set clear values in vulkan ??? TODO
                GLfloat depth = 1.f;
                GLint stencil = 0;
                if (asset::isDepthOnlyFormat(fmt))
                {
                    gl->extGlClearNamedFramebufferfv(fbo, GL_DEPTH, 0, &depth);
                }
                else if (asset::isStencilOnlyFormat(fmt))
                {
                    gl->extGlClearNamedFramebufferiv(fbo, GL_STENCIL, 0, &stencil);
                }
                else if (asset::isDepthOrStencilFormat(fmt))
                {
                    gl->extGlClearNamedFramebufferfi(fbo, GL_DEPTH_STENCIL, 0, depth, stencil);
                }
            }
        }
    }

    bool COpenGLCommandBuffer::pushConstants_validate(const IGPUPipelineLayout* _layout, uint32_t _stages, uint32_t _offset, uint32_t _size, const void* _values)
    {
        if (!_layout || !_values)
            return false;
        if (!_size)
            return false;
        if (!_stages)
            return false;
        if (!core::is_aligned_to(_offset, 4u))
            return false;
        if (!core::is_aligned_to(_size, 4u))
            return false;
        if (_offset >= IGPUMeshBuffer::MAX_PUSH_CONSTANT_BYTESIZE)
            return false;
        if ((_offset + _size) > IGPUMeshBuffer::MAX_PUSH_CONSTANT_BYTESIZE)
            return false;

        asset::SPushConstantRange updateRange;
        updateRange.offset = _offset;
        updateRange.size = _size;

#ifdef _NBL_DEBUG
        //TODO validation:
        /*
        For each byte in the range specified by offset and size and for each shader stage in stageFlags,
        there must be a push constant range in layout that includes that byte and that stage
        */
        for (const auto& rng : _layout->getPushConstantRanges())
        {
            /*
            For each byte in the range specified by offset and size and for each push constant range that overlaps that byte,
            stageFlags must include all stages in that push constant ranges VkPushConstantRange::stageFlags
            */
            if (updateRange.overlap(rng) && ((_stages & rng.stageFlags) != rng.stageFlags))
                return false;
        }
#endif//_NBL_DEBUG

        return true;
    }

    void COpenGLCommandBuffer::executeAll(IOpenGL_FunctionTable* gl, SOpenGLContextLocalCache* ctxlocal, uint32_t ctxid) const
    {
        for (const SCommand& cmd : m_commands)
        {
            switch (cmd.type)
            {
            case ECT_BIND_INDEX_BUFFER:
            {
                auto& c = cmd.get<ECT_BIND_INDEX_BUFFER>();
                auto* buffer = static_cast<COpenGLBuffer*>(c.buffer.get());
                ctxlocal->nextState.vertexInputParams.vao.idxBinding = { c.offset, core::smart_refctd_ptr<const COpenGLBuffer>(buffer) };
                ctxlocal->nextState.vertexInputParams.vao.idxType = c.indexType;
            }
            break;
            case ECT_DRAW:
            {
                auto& c = cmd.get<ECT_DRAW>();

                // TODO flush state

                const asset::E_PRIMITIVE_TOPOLOGY primType = ctxlocal->currentState.pipeline.graphics.pipeline->getPrimitiveAssemblyParams().primitiveType;
                GLenum glpt = getGLprimitiveType(primType);

                gl->extGlDrawArraysInstancedBaseInstance(glpt, c.firstVertex, c.vertexCount, c.instanceCount, c.firstInstance);
            }
            break;
            case ECT_DRAW_INDEXED:
            {
                auto& c = cmd.get<ECT_DRAW_INDEXED>();

                // TODO flush state

                const asset::E_PRIMITIVE_TOPOLOGY primType = ctxlocal->currentState.pipeline.graphics.pipeline->getPrimitiveAssemblyParams().primitiveType;
                GLenum glpt = getGLprimitiveType(primType);
                GLenum idxType = GL_INVALID_ENUM;
                switch (ctxlocal->currentState.vertexInputParams.vao.idxType)
                {
                case asset::EIT_16BIT:
                    idxType = GL_UNSIGNED_SHORT;
                    break;
                case asset::EIT_32BIT:
                    idxType = GL_UNSIGNED_INT;
                    break;
                default: break;
                }

                if (idxType != GL_INVALID_ENUM)
                {
                    GLuint64 idxBufOffset = ctxlocal->currentState.vertexInputParams.vao.idxBinding.offset;
                    static_assert(sizeof(idxBufOffset) == sizeof(void*), "Bad reinterpret_cast");
                    gl->extGlDrawElementsInstancedBaseVertexBaseInstance(glpt, c.indexCount, idxType, reinterpret_cast<void*>(idxBufOffset), c.instanceCount, c.firstIndex, c.firstInstance);
                }
            }
            break;
            case ECT_DRAW_INDIRECT:
            {
                auto& c = cmd.get<ECT_DRAW_INDIRECT>();

                ctxlocal->nextState.vertexInputParams.indirectDrawBuf = core::smart_refctd_ptr_static_cast<const COpenGLBuffer>(c.buffer);
                const asset::E_PRIMITIVE_TOPOLOGY primType = ctxlocal->currentState.pipeline.graphics.pipeline->getPrimitiveAssemblyParams().primitiveType;
                GLenum glpt = getGLprimitiveType(primType);

                // TODO flush

                if (c.drawCount)
                {
                    GLuint64 offset = c.offset;
                    static_assert(sizeof(offset) == sizeof(void*), "Bad reinterpret_cast");
                    gl->extGlMultiDrawArraysIndirect(glpt, reinterpret_cast<void*>(offset), c.drawCount, c.stride);
                }
            }
            break;
            case ECT_DRAW_INDEXED_INDIRECT:
            {
                auto& c = cmd.get<ECT_DRAW_INDEXED_INDIRECT>();

                ctxlocal->nextState.vertexInputParams.indirectDrawBuf = core::smart_refctd_ptr_static_cast<const COpenGLBuffer>(c.buffer);

                // TODO flush

                const asset::E_PRIMITIVE_TOPOLOGY primType = ctxlocal->currentState.pipeline.graphics.pipeline->getPrimitiveAssemblyParams().primitiveType;
                GLenum glpt = getGLprimitiveType(primType);

                GLenum idxType = GL_INVALID_ENUM;
                switch (ctxlocal->currentState.vertexInputParams.vao.idxType)
                {
                case asset::EIT_16BIT:
                    idxType = GL_UNSIGNED_SHORT;
                    break;
                case asset::EIT_32BIT:
                    idxType = GL_UNSIGNED_INT;
                    break;
                default: break;
                }

                if (c.drawCount && idxType != GL_INVALID_ENUM)
                {
                    GLuint64 offset = c.offset;
                    static_assert(sizeof(offset) == sizeof(void*), "Bad reinterpret_cast");
                    gl->extGlMultiDrawElementsIndirect(glpt, idxType, reinterpret_cast<void*>(offset), c.drawCount, c.stride);
                }
            }
            break;
            case ECT_SET_VIEWPORT:
            {
                auto& c = cmd.get<ECT_SET_VIEWPORT>();
                if (c.firstViewport < SOpenGLState::MAX_VIEWPORT_COUNT)
                {
                    uint32_t count = std::min(c.viewportCount, SOpenGLState::MAX_VIEWPORT_COUNT);
                    if (c.firstViewport + count > SOpenGLState::MAX_VIEWPORT_COUNT)
                        count = SOpenGLState::MAX_VIEWPORT_COUNT - c.firstViewport;

                    uint32_t first = c.firstViewport;
                    for (uint32_t i = 0u; i < count; ++i)
                    {
                        auto& vp = ctxlocal->nextState.rasterParams.viewport[first + i];

                        vp.x = c.viewports[i].x;
                        vp.y = c.viewports[i].y;
                        vp.width = c.viewports[i].width;
                        vp.height = c.viewports[i].height;
                        vp.minDepth = c.viewports[i].minDepth;
                        vp.maxDepth = c.viewports[i].maxDepth;
                    }
                }
            }
            break;
            case ECT_SET_LINE_WIDTH:
            {
                auto& c = cmd.get<ECT_SET_LINE_WIDTH>();
                ctxlocal->nextState.rasterParams.lineWidth = c.lineWidth;
            }
            break;
            case ECT_SET_DEPTH_BIAS:
            {
                auto& c = cmd.get<ECT_SET_DEPTH_BIAS>();
                // TODO what about c.depthBiasClamp
                ctxlocal->nextState.rasterParams.polygonOffset.factor = c.depthBiasSlopeFactor;
                ctxlocal->nextState.rasterParams.polygonOffset.units = c.depthBiasConstantFactor;
            }
            break;
            case ECT_SET_BLEND_CONSTANTS:
            {
                auto& c = cmd.get<ECT_SET_BLEND_CONSTANTS>();
                // TODO, cant see such thing in opengl
            }
            break;
            case ECT_COPY_BUFFER:
            {
                auto& c = cmd.get<ECT_COPY_BUFFER>();
                // TODO flush some state?
                GLuint readb = static_cast<COpenGLBuffer*>(c.srcBuffer.get())->getOpenGLName();
                GLuint writeb = static_cast<COpenGLBuffer*>(c.dstBuffer.get())->getOpenGLName();
                for (uint32_t i = 0u; i < c.regionCount; ++i)
                {
                    const asset::SBufferCopy& cp = c.regions[i];
                    gl->extGlCopyNamedBufferSubData(readb, writeb, cp.srcOffset, cp.dstOffset, cp.size);
                }
            }
            break;
            case ECT_COPY_IMAGE:
            {
                auto& c = cmd.get<ECT_COPY_IMAGE>();
                // TODO flush some state?
                IGPUImage* dstImage = c.dstImage.get();
                IGPUImage* srcImage = c.srcImage.get();
                if (!dstImage->validateCopies(c.regions, c.regions + c.regionCount, srcImage))
                    return;

                auto src = static_cast<COpenGLImage*>(srcImage);
                auto dst = static_cast<COpenGLImage*>(dstImage);
                IGPUImage::E_TYPE srcType = srcImage->getCreationParameters().type;
                IGPUImage::E_TYPE dstType = dstImage->getCreationParameters().type;
                constexpr GLenum type2Target[3u] = { GL_TEXTURE_1D_ARRAY,GL_TEXTURE_2D_ARRAY,GL_TEXTURE_3D };
                for (auto it = c.regions; it != c.regions + c.regionCount; it++)
                {
                    gl->extGlCopyImageSubData(src->getOpenGLName(), type2Target[srcType], it->srcSubresource.mipLevel,
                        it->srcOffset.x, srcType == IGPUImage::ET_1D ? it->srcSubresource.baseArrayLayer : it->srcOffset.y, srcType == IGPUImage::ET_2D ? it->srcSubresource.baseArrayLayer : it->srcOffset.z,
                        dst->getOpenGLName(), type2Target[dstType], it->dstSubresource.mipLevel,
                        it->dstOffset.x, dstType == IGPUImage::ET_1D ? it->dstSubresource.baseArrayLayer : it->dstOffset.y, dstType == IGPUImage::ET_2D ? it->dstSubresource.baseArrayLayer : it->dstOffset.z,
                        it->extent.width, dstType == IGPUImage::ET_1D ? it->dstSubresource.layerCount : it->extent.height, dstType == IGPUImage::ET_2D ? it->dstSubresource.layerCount : it->extent.depth);
                }
            }
            break;
            case ECT_COPY_BUFFER_TO_IMAGE:
            {
                auto& c = cmd.get<ECT_COPY_BUFFER_TO_IMAGE>();

                copyBufferToImage(c, gl, ctxlocal);
            }
            break;
            case ECT_COPY_IMAGE_TO_BUFFER:
            {
                auto& c = cmd.get<ECT_COPY_IMAGE_TO_BUFFER>();

                copyImageToBuffer(c, gl, ctxlocal);
            }
            break;
            case ECT_BLIT_IMAGE:
            {
                auto& c = cmd.get<ECT_BLIT_IMAGE>();

                // no way to do it easily in GL
                // idea: have one 1 fbo per function table (effectively per context) just to do blits and resolves
            }
            break;
            case ECT_RESOLVE_IMAGE:
            {
                auto& c = cmd.get<ECT_RESOLVE_IMAGE>();

                // no way to do it easily in GL
                // idea: have one 1 fbo per function table (effectively per context) just to do blits and resolves
            }
            break;
            case ECT_BIND_VERTEX_BUFFERS:
            {
                auto& c = cmd.get<ECT_BIND_VERTEX_BUFFERS>();

                for (uint32_t i = 0u; i < c.count; ++i)
                {
                    auto& binding = ctxlocal->nextState.vertexInputParams.vao.vtxBindings[c.first + i];
                    binding.buffer = core::smart_refctd_ptr_static_cast<const COpenGLBuffer>(c.buffers[i]);
                    binding.offset = c.offsets[i];
                }
            }
            break;
            case ECT_SET_SCISSORS:
            {
                auto& c = cmd.get<ECT_SET_SCISSORS>();
                // TODO ?
            }
            break;
            case ECT_SET_DEPTH_BOUNDS:
            {
                auto& c = cmd.get<ECT_SET_DEPTH_BOUNDS>();
                // TODO ?
            }
            break;
            case ECT_SET_STENCIL_COMPARE_MASK:
            {
                auto& c = cmd.get<ECT_SET_STENCIL_COMPARE_MASK>();
                if (c.faceMask & asset::ESFF_FRONT_BIT)
                    ctxlocal->nextState.rasterParams.stencilFunc_front.mask = c.cmpMask;
                if (c.faceMask & asset::ESFF_BACK_BIT)
                    ctxlocal->nextState.rasterParams.stencilFunc_back.mask = c.cmpMask;;
            }
            break;
            case ECT_SET_STENCIL_WRITE_MASK:
            {
                auto& c = cmd.get<ECT_SET_STENCIL_WRITE_MASK>();
                if (c.faceMask & asset::ESFF_FRONT_BIT)
                    ctxlocal->nextState.rasterParams.stencilWriteMask_front = c.writeMask;
                if (c.faceMask & asset::ESFF_BACK_BIT)
                    ctxlocal->nextState.rasterParams.stencilWriteMask_back = c.writeMask;
            }
            break;
            case ECT_SET_STENCIL_REFERENCE:
            {
                auto& c = cmd.get<ECT_SET_STENCIL_REFERENCE>();
                if (c.faceMask & asset::ESFF_FRONT_BIT)
                    ctxlocal->nextState.rasterParams.stencilFunc_front.ref = c.reference;
                if (c.faceMask & asset::ESFF_BACK_BIT)
                    ctxlocal->nextState.rasterParams.stencilFunc_back.ref = c.reference;
            }
            break;
            case ECT_DISPATCH:
            {
                auto& c = cmd.get<ECT_DISPATCH>();
                // TODO flush some state
                gl->glCompute.pglDispatchCompute(c.groupCountX, c.groupCountY, c.groupCountZ);
            }
            break;
            case ECT_DISPATCH_INDIRECT:
            {
                auto& c = cmd.get<ECT_DISPATCH_INDIRECT>();
                ctxlocal->nextState.dispatchIndirect.buffer = core::smart_refctd_ptr_static_cast<const COpenGLBuffer>(c.buffer);
                // TODO flush
                gl->glCompute.pglDispatchComputeIndirect(static_cast<GLintptr>(c.offset));
            }
            break;
            case ECT_DISPATCH_BASE:
            {
                auto& c = cmd.get<ECT_DISPATCH_BASE>();
                // no such thing in opengl (easy to emulate tho)
                // maybe spirv-cross emits some uniforms for this?
            }
            break;
            case ECT_SET_EVENT:
            {
                auto& c = cmd.get<ECT_SET_EVENT>();
                // TODO
            }
            break;
            case ECT_RESET_EVENT:
            {
                auto& c = cmd.get<ECT_RESET_EVENT>();
                // TODO
            }
            break;
            case ECT_WAIT_EVENTS:
            {
                auto& c = cmd.get<ECT_WAIT_EVENTS>();
                // TODO
            }
            break;
            case ECT_PIPELINE_BARRIER:
            {
                auto& c = cmd.get<ECT_PIPELINE_BARRIER>();
                // TODO
            }
            break;
            case ECT_BEGIN_RENDERPASS:
            {
                auto& c = cmd.get<ECT_BEGIN_RENDERPASS>();
                //c.renderpassBegin.framebuffer
                // TODO bind framebuffer
                GLuint fbo = 0; // TODO get fbo name!!!!!!!!!
                beginRenderpass_clearAttachments(gl, c.renderpassBegin, fbo);
            }
            break;
            case ECT_NEXT_SUBPASS:
            {
                auto& c = cmd.get<ECT_NEXT_SUBPASS>();
                // TODO some barriers based on subpass dependencies?
                // not needed now tho, we dont support multiple subpasses yet
            }
            break;
            case ECT_END_RENDERPASS:
            {
                auto& c = cmd.get<ECT_END_RENDERPASS>();
                // no-op
            }
            break;
            case ECT_SET_DEVICE_MASK:
            {
                auto& c = cmd.get<ECT_SET_DEVICE_MASK>();
                // no-op
            }
            break;
            case ECT_BIND_GRAPHICS_PIPELINE:
            {
                auto& c = cmd.get<ECT_BIND_GRAPHICS_PIPELINE>();

                ctxlocal->updateNextState_pipelineAndRaster(c.pipeline->getRenderpassIndependentPipeline(), ctxid);
            }
            break;
            case ECT_BIND_COMPUTE_PIPELINE:
            {
                auto& c = cmd.get<ECT_BIND_COMPUTE_PIPELINE>();

                const COpenGLComputePipeline* glppln = static_cast<const COpenGLComputePipeline*>(c.pipeline.get());
                ctxlocal->nextState.pipeline.compute.usedShader = glppln ? glppln->getShaderGLnameForCtx(0u, ctxid) : 0u;
                ctxlocal->nextState.pipeline.compute.pipeline = core::smart_refctd_ptr<const COpenGLComputePipeline>(glppln);
            }
            break;
            case ECT_RESET_QUERY_POOL:
            {
                auto& c = cmd.get<ECT_RESET_QUERY_POOL>();
            }
            break;
            case ECT_BEGIN_QUERY:
            {
                auto& c = cmd.get<ECT_BEGIN_QUERY>();
            }
            break;
            case ECT_END_QUERY:
            {
                auto& c = cmd.get<ECT_END_QUERY>();
            }
            break;
            case ECT_COPY_QUERY_POOL_RESULTS:
            {
                auto& c = cmd.get<ECT_COPY_QUERY_POOL_RESULTS>();
            }
            break;
            case ECT_WRITE_TIMESTAMP:
            {
                auto& c = cmd.get<ECT_WRITE_TIMESTAMP>();
            }
            break;
            case ECT_BIND_DESCRIPTOR_SETS:
            {
                auto& c = cmd.get<ECT_BIND_DESCRIPTOR_SETS>();

                asset::E_PIPELINE_BIND_POINT pbp = c.pipelineBindPoint;

                const IGPUPipelineLayout* layouts[IGPUPipelineLayout::DESCRIPTOR_SET_COUNT]{};
                for (uint32_t i = 0u; i < IGPUPipelineLayout::DESCRIPTOR_SET_COUNT; ++i)
                    layouts[i] = ctxlocal->nextState.descriptorsParams[pbp].descSets[i].pplnLayout.get();
                const IGPUDescriptorSet* descriptorSets[IGPUPipelineLayout::DESCRIPTOR_SET_COUNT]{};
                for (uint32_t i = 0u; i < c.dsCount; ++i)
                    descriptorSets[i] = c.descriptorSets[i].get();
                bindDescriptorSets_generic(c.layout.get(), c.firstSet, c.dsCount, descriptorSets, layouts);

                for (uint32_t i = 0u; i < IGPUPipelineLayout::DESCRIPTOR_SET_COUNT; ++i)
                    if (!layouts[i])
                        ctxlocal->nextState.descriptorsParams[pbp].descSets[i] = { nullptr, nullptr, nullptr };

                for (uint32_t i = 0u; i < c.dsCount; i++)
                {
                    ctxlocal->nextState.descriptorsParams[pbp].descSets[c.firstSet + i] =
                    {
                        core::smart_refctd_ptr<const COpenGLPipelineLayout>(static_cast<const COpenGLPipelineLayout*>(c.layout.get())),
                        core::smart_refctd_ptr<const COpenGLDescriptorSet>(static_cast<const COpenGLDescriptorSet*>(descriptorSets[i])),
                        c.dynamicOffsets
                    };
                }
            }
            break;
            case ECT_PUSH_CONSTANTS:
            {
                auto& c = cmd.get<ECT_PUSH_CONSTANTS>();

                if (pushConstants_validate(c.layout.get(), c.stageFlags, c.offset, c.size, c.values))
                {
                    asset::SPushConstantRange updtRng;
                    updtRng.offset = c.offset;
                    updtRng.size = c.size;

                    if (c.stageFlags & asset::ISpecializedShader::ESS_ALL_GRAPHICS)
                        ctxlocal->pushConstants<EPBP_GRAPHICS>(static_cast<const COpenGLPipelineLayout*>(c.layout.get()), c.stageFlags, c.offset, c.size, c.values);
                    if (c.stageFlags & asset::ISpecializedShader::ESS_COMPUTE)
                        ctxlocal->pushConstants<EPBP_COMPUTE>(static_cast<const COpenGLPipelineLayout*>(c.layout.get()), c.stageFlags, c.offset, c.size, c.values);
                }
            }
            break;
            case ECT_CLEAR_COLOR_IMAGE:
            {
                auto& c = cmd.get<ECT_CLEAR_COLOR_IMAGE>();
            }
            break;
            case ECT_CLEAR_DEPTH_STENCIL_IMAGE:
            {
                auto& c = cmd.get<ECT_CLEAR_DEPTH_STENCIL_IMAGE>();
            }
            break;
            case ECT_CLEAR_ATTACHMENTS:
            {
                auto& c = cmd.get<ECT_CLEAR_ATTACHMENTS>();
            }
            break;
            case ECT_FILL_BUFFER:
            {
                auto& c = cmd.get<ECT_FILL_BUFFER>();

                GLuint buf = static_cast<const COpenGLBuffer*>(c.dstBuffer.get())->getOpenGLName();
                gl->extGlClearNamedBufferSubData(buf, GL_R32UI, c.dstOffset, c.size, GL_RED, GL_UNSIGNED_INT, &c.data);
            }
            break;
            case ECT_UPDATE_BUFFER:
            {
                auto& c = cmd.get<ECT_UPDATE_BUFFER>();

                GLuint buf = static_cast<const COpenGLBuffer*>(c.dstBuffer.get())->getOpenGLName();
                gl->extGlNamedBufferSubData(buf, c.dstOffset, c.dataSize, c.data);
            }
            break;
            case ECT_EXECUTE_COMMANDS:
            {
                auto& c = cmd.get<ECT_EXECUTE_COMMANDS>();

                // sadly have to use dynamic_cast here because of virtual base
                dynamic_cast<COpenGLCommandBuffer*>(c.cmdbuf.get())->executeAll(gl, ctxlocal, ctxid);
            }
            break;
            }
        }
    }
}
}