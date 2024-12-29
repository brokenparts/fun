// SPDX-License-Identifier: MIT

// https://metalbyexample.com/modern-metal-1/

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <cstdint>
#include <string>
#include <vector>

#include <cgltf.h>
#include <HandmadeMath.h>
#include <SDL3/SDL.h>
#include <spng.h>

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#define ARRLEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#define DIE(...) std::fprintf(stderr, "ERR: " __VA_ARGS__); std::fprintf(stderr, "\n"); std::fflush(stderr); *(volatile int*)0x0 = 1234;
#define LOG(...) std::fprintf(stdout, "MSG: " __VA_ARGS__); std::fprintf(stdout, "\n"); std::fflush(stdout);

#define ASSERT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            DIE("ASSERT FAILED %s:%d: %s", __FILE__, __LINE__, #cond);  \
        }                                                               \
    } while (0);                                                        \

#define MODEL_FOLDER    "common/glTF/"
#define MODEL_PATH      MODEL_FOLDER "/FlightHelmet.gltf"

#define SHADER_PATH     "graphics/metal/gltf.metallib"

struct Vertex_1P1N1T {
    HMM_Vec3 p;
    HMM_Vec3 n;
    HMM_Vec2 t;
};
// static_assert(sizeof(Vertex_1P1N1T) == sizeof(float) * 12);

struct Uniforms {
    HMM_Mat4 model;
    HMM_Mat4 view;
};

struct Primitive {
    MTL::Buffer*    vb;
    MTL::Buffer*    ib;
    size_t          indices;
    size_t          texture_idx;
};

struct Model {
    std::vector<Primitive>  primitives;
    HMM_Mat4                transform;
};

struct Texture {
    MTL::Texture* tex;
};

int main(int argc, char* argv[]) {
    // Initialize SDL
    // Create application window
    constexpr Uint32 WNDFLAGS = SDL_WINDOW_RESIZABLE;
    SDL_Window* wnd = SDL_CreateWindow(__FILE__, 1280, 720, WNDFLAGS);
    if (!wnd) {
        DIE("Failed to create SDL window: %s", SDL_GetError());
    }

    // Create metal view and layer - used to render to the application window
    SDL_MetalView mview = SDL_Metal_CreateView(wnd);
    CA::MetalLayer* mlayer = (CA::MetalLayer*)SDL_Metal_GetLayer(mview);

    // Create metal device and assign it to the layer
    MTL::Device* mdev = MTL::CreateSystemDefaultDevice(); ASSERT(mdev);
    mlayer->setDevice(mdev);

    // Load glTF scene
    cgltf_data* gltf = { };
    cgltf_options options = { };
    if (cgltf_parse_file(&options, MODEL_PATH, &gltf) != cgltf_result_success) {
        DIE("Failed to parse glTF file " MODEL_PATH);
    }
    if (cgltf_load_buffers(&options, gltf, MODEL_PATH) != cgltf_result_success) {
        DIE("Failed to load glTF buffers");
    }

    // Upload scene to GPU
    std::vector<Model> scene = { }; scene.resize(gltf->meshes_count);
    for (size_t i = 0; i < gltf->meshes_count; ++i) {
        Model* model = &scene[i];
        model->primitives.resize(gltf->meshes[i].primitives_count);
        for (size_t j = 0; j < gltf->meshes[i].primitives_count; ++j) {
            Primitive* prim                 = &model->primitives[j];
            const cgltf_primitive* gprim    = &gltf->meshes[i].primitives[j];

            // Need position, normal, and texcoord vertex attributes
            const cgltf_attribute* gattr_p = NULL;
            const cgltf_attribute* gattr_n = NULL;
            const cgltf_attribute* gattr_t = NULL;
            for (size_t k = 0; k < gprim->attributes_count; ++k) {
                const cgltf_attribute* gattr = &gprim->attributes[k];
                switch (gattr->type) {
                    case cgltf_attribute_type_position: { gattr_p = gattr; } break;
                    case cgltf_attribute_type_normal:   { gattr_n = gattr; } break;
                    case cgltf_attribute_type_texcoord: { gattr_t = gattr; } break;
                    default: break;
                };
            }
            if (!gattr_p || !gattr_n || !gattr_t) {
                DIE("glTF model is missing one or more required vertex attributes");
            }
            if (gattr_p->data->count != gattr_n->data->count || gattr_n->data->count != gattr_t->data->count) {
                DIE("glTF model vertex attribute data size mismatch");
            }
            const size_t vertex_count = gattr_p->data->count;

            // Create vertex buffer on GPU
            prim->vb = mdev->newBuffer(sizeof(Vertex_1P1N1T) * vertex_count, MTL::ResourceStorageModeShared);

            // Write vertex data and synchronize the buffer
            Vertex_1P1N1T* vertices = (Vertex_1P1N1T*)prim->vb->contents();
            for (size_t k = 0; k < vertex_count; ++k) {
                // position
                cgltf_accessor_read_float(gattr_p->data, k, vertices[k].p.Elements, 3);
                // normal
                cgltf_accessor_read_float(gattr_n->data, k, vertices[k].n.Elements, 3);
                // texcoord
                cgltf_accessor_read_float(gattr_t->data, k, vertices[k].t.Elements, 2);
            }

            // Create index buffer on GPU
            prim->ib = mdev->newBuffer(sizeof(uint16_t) * gprim->indices->count, MTL::ResourceStorageModeShared);

            // Write index data and synchronize the buffer
            uint16_t* indices = (uint16_t*)prim->ib->contents();
            for (size_t k = 0; k < gprim->indices->count; ++k) {
                unsigned int temp = 0;
                cgltf_accessor_read_uint(gprim->indices, k, &temp, 1);
                indices[k] = temp;
            }

            // Future draw calls need to know the number of indices
            prim->indices = gprim->indices->count;

            // Store texture index
            cgltf_texture* texture = gprim->material->pbr_metallic_roughness.base_color_texture.texture; ASSERT(texture);
            prim->texture_idx = texture - gltf->textures;
            ASSERT(prim->texture_idx < gltf->textures_count);
        }
    }

    // Store model transformation
    for (size_t i = 0; i < gltf->nodes_count; ++i) {
        cgltf_node* node = &gltf->nodes[i];
        if (!node->mesh) {
            continue;
        }
        const size_t model_index = node->mesh - gltf->meshes; ASSERT(model_index < scene.size());
        cgltf_node_transform_world(node, scene[model_index].transform.Elements[0]);
    }

    // Upload images
    std::vector<Texture> textures = { }; textures.resize(gltf->textures_count);
    for (size_t i = 0; i < gltf->textures_count; ++i) {
        // Fix relative image path
        if (!gltf->textures[i].image->uri) {
            DIE("glTF texture has NULL URI");
        }
        char texture_path[1024] = { };
        std::snprintf(texture_path, sizeof(texture_path), MODEL_FOLDER "/%s", gltf->textures[i].image->uri);

        // Load image file
        std::FILE* f = std::fopen(texture_path, "rb");
        if (!f) {
            DIE("Failed to open image file %s", texture_path);
        }
        std::vector<uint8_t> bytes = { };
        std::fseek(f, 0, SEEK_END);
        bytes.resize(std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        std::fread(&bytes[0], sizeof(uint8_t), bytes.size(), f);
        std::fclose(f);

        // Parse PNG (RGBA8)
        spng_ctx* spng = spng_ctx_new(0); ASSERT(spng);
        spng_set_png_buffer(spng, &bytes[0], bytes.size());
        spng_ihdr hdr = { };
        if (spng_get_ihdr(spng, &hdr)) {
            DIE("Image %s is not a PNG file", texture_path);
        }
        std::vector<uint32_t> pixels = { }; pixels.resize(hdr.width * hdr.height);
        int decode_response = spng_decode_image(spng, &pixels[0], pixels.size() * sizeof(uint32_t), SPNG_FMT_RGBA8, 0);
        if (decode_response != 0) {
            DIE("Error while decoding PNG image %s: %s", texture_path, spng_strerror(decode_response));
        }
        spng_ctx_free(spng);

        // Upload to GPU
        MTL::Region region = MTL::Region(0, 0, hdr.width, hdr.height);
        MTL::TextureDescriptor* mtexd = MTL::TextureDescriptor::alloc()->init();
        mtexd->setPixelFormat(MTL::PixelFormatRGBA8Unorm);
        mtexd->setWidth(hdr.width);
        mtexd->setHeight(hdr.height);

        textures[i].tex = mdev->newTexture(mtexd);
        textures[i].tex->replaceRegion(region, 0, &pixels[0], sizeof(uint32_t) * hdr.width);

        mtexd->release();

        LOG("[%02lu/%02lu] Loaded texture %s", (unsigned long)i+1, (unsigned long)gltf->textures_count, texture_path);
    }

    // Set vertex layout descriptor
    MTL::VertexDescriptor* mvd = MTL::VertexDescriptor::alloc()->init();
    mvd->layouts()->object(0)->setStride(sizeof(Vertex_1P1N1T));
    {
        // V3 position
        MTL::VertexAttributeDescriptor* attr = mvd->attributes()->object(0);
        attr->setFormat(MTL::VertexFormatFloat3);
        attr->setBufferIndex(0);
        attr->setOffset(offsetof(Vertex_1P1N1T, p));
        // V3 normal
        attr = mvd->attributes()->object(1);
        attr->setFormat(MTL::VertexFormatFloat3);
        attr->setBufferIndex(0);
        attr->setOffset(offsetof(Vertex_1P1N1T, n));
        // V2 texcoord
        attr = mvd->attributes()->object(2);
        attr->setFormat(MTL::VertexFormatFloat2);
        attr->setBufferIndex(0);
        attr->setOffset(offsetof(Vertex_1P1N1T, t));
    }

    // Load shader library
    NS::Error* lib_err = { };
    MTL::Library* mlib = mdev->newLibrary(NS::String::string(SHADER_PATH, NS::UTF8StringEncoding), &lib_err);
    if (lib_err) {
        DIE("Failed to create shader %s:\n%s", SHADER_PATH, lib_err->localizedDescription()->utf8String());
    }
    MTL::Function* func_vs = mlib->newFunction(NS::String::string("main_vs", NS::UTF8StringEncoding));
    MTL::Function* func_fs = mlib->newFunction(NS::String::string("main_fs", NS::UTF8StringEncoding));
    if (!func_vs || !func_fs) {
        DIE("Failed to find shader entry points");
    }
    LOG("Loaded shader library %s", SHADER_PATH);

    // Create command queue - ordered list of command buffers
    MTL::CommandQueue* mcq = mdev->newCommandQueue();

    // Create render pass descriptor - stores attachments
    MTL::RenderPassDescriptor* mrpd = MTL::RenderPassDescriptor::alloc()->init();
    // Set color attachment
    MTL::RenderPassColorAttachmentDescriptor* mrpd_color0 = mrpd->colorAttachments()->object(0);
    mrpd_color0->setLoadAction(MTL::LoadActionClear);
    mrpd_color0->setStoreAction(MTL::StoreActionStore);
    mrpd_color0->setClearColor(MTL::ClearColor(0.1f, 0.1f, 0.1f, 1.0f));

    // Create render pipeline
    MTL::RenderPipelineDescriptor* mpiped = MTL::RenderPipelineDescriptor::alloc()->init();
    mpiped->setVertexFunction(func_vs);
    mpiped->setFragmentFunction(func_fs);
    mpiped->colorAttachments()->object(0)->setPixelFormat(mlayer->pixelFormat());
    mpiped->setVertexDescriptor(mvd);
    mpiped->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    NS::Error* pipe_err = { };
    MTL::RenderPipelineState* mpipe = mdev->newRenderPipelineState(mpiped, &pipe_err);
    if (pipe_err) {
        DIE("Failed to create render pipeline: %s", pipe_err->localizedDescription()->utf8String());
    }
    mpiped->release();

    // Create depth stencil
    MTL::DepthStencilDescriptor* mdsd = MTL::DepthStencilDescriptor::alloc()->init();
    mdsd->setDepthCompareFunction(MTL::CompareFunctionLess);
    mdsd->setDepthWriteEnabled(true);
    MTL::DepthStencilState* mdss = mdev->newDepthStencilState(mdsd);

    // Create uniform buffer
    MTL::Buffer* ub = mdev->newBuffer(sizeof(Uniforms), MTL::ResourceStorageModeShared);
    Uniforms* uniforms = (Uniforms*)ub->contents();

    // Window loop
    bool running = true;
    do {
        // Poll events
        SDL_Event evt = { };
        while (SDL_PollEvent(&evt)) {
            switch (evt.type) {
                case SDL_EVENT_QUIT: {
                    running = false;
                } break;
                case SDL_EVENT_KEY_DOWN: {
                    if (evt.key.key == SDLK_ESCAPE) {
                        running = false;
                    }
                } break;
            }
        }

        // Application time in seconds
        const float now = (float)SDL_GetTicks() / 1e3f;

        // Update viewport dimensions
        int vp_w = 0; int vp_h = 0;
        SDL_GetWindowSizeInPixels(wnd, &vp_w, &vp_h);

        // Update camera
        HMM_Vec3 cam = HMM_V3(HMM_SinF(now), 0.3f, HMM_CosF(now));

        // Update view matrix
        // Camera matrix
        HMM_Mat4 vp_cam = HMM_LookAt_RH(cam, HMM_V3(0.0f, 0.3f, 0.0f), HMM_V3(0.0f, 1.0f, 0.0f));
        // Perspective projection matrix
        HMM_Mat4 vp_proj = HMM_Perspective_RH_NO(HMM_ToRad(90.0f / 2.0f), (float)vp_w / (float)vp_h, 0.1f, 10.0f);
        // Combine camera + projection
        uniforms->view = HMM_Mul(vp_proj, vp_cam);

        // Begin rendering
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

        // Use the window as the render color attachment
        CA::MetalDrawable* mdrawable = mlayer->nextDrawable();
        mrpd_color0->setTexture(mdrawable->texture());

        // Set up command buffer
        MTL::CommandBuffer* mcb = mcq->commandBuffer();
        // Set up command encoder
        MTL::RenderCommandEncoder* mrce = mcb->renderCommandEncoder(mrpd);
        mrce->setRenderPipelineState(mpipe);
        mrce->setDepthStencilState(mdss);

        // Render scene
        for (Model& model : scene) {
            // Update transformation
            uniforms->model = model.transform;

            // Render primitives
            for (Primitive& prim : model.primitives) {
                mrce->setVertexBuffer(prim.vb, 0, 0);
                mrce->setVertexBuffer(ub, 0, 1);
                mrce->setFragmentTexture(textures[prim.texture_idx].tex, 0);
                mrce->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, prim.indices, MTL::IndexTypeUInt16, prim.ib, 0);
            }
        }
        
        // End command encoding
        mrce->endEncoding();
        // Commit draw and present
        mcb->presentDrawable(mdrawable);
        mcb->commit();
        mcb->waitUntilCompleted();

        pool->release();
    } while (running);

    SDL_DestroyWindow(wnd);
    SDL_Quit();

}
