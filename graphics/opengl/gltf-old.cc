// SPDX-License-Identifier: MIT

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <vector>

#ifdef _WIN32
#   include <Windows.h>
#endif

#include <cgltf.h>
#include <glad/glad.h>
#include <HandmadeMath.h>
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_sdl2.h>

#define LOG_ERR(...) std::fprintf(stderr, __VA_ARGS__); std::fprintf(stderr, "\n"); std::abort();
#define LOG_MSG(...) std::fprintf(stdout, __VA_ARGS__); std::fprintf(stdout, "\n");

#define ASSERT(cond) if (!(cond)) { LOG_ERR("ASSERT FAILED: " #cond); }

enum {
    COMPONENT_POSITION  = 1 << 0,
    COMPONENT_TEXTURE   = 1 << 1,
};

struct Vertex {
    HMM_Vec3 p;
    HMM_Vec2 t;
};

struct Primitive {
    GLuint m_vao;
    GLuint m_vbo;
    GLuint m_ibo;
    size_t m_indices;
};

struct Mesh {
    std::vector<Primitive> m_primitives;
};

struct Node {
    Mesh*       m_mesh;
    HMM_Mat4    m_trans;
};

struct Uniforms {
    HMM_Mat4 m_proj;
    HMM_Mat4 m_model;
    HMM_Mat4 m_cam;
    HMM_Mat4 m_rot;
};

static struct {
    char                m_path[ 256 ];
    cgltf_data*         m_gltf;
    std::vector<Mesh>   m_meshes;
    std::vector<Node>   m_nodes;
    Uniforms            m_uniforms;
    int                 m_components;
    HMM_Vec3            m_cam;
    float               m_cam_z_plane;
    bool                m_model_rotate;
} g = { };

const char* VS = R""""(
#version 330 core

layout (location = 0) in vec3 p;
layout (location = 1) in vec2 t;

uniform mat4 u_proj;
uniform mat4 u_model;
uniform mat4 u_cam;
uniform mat4 u_rot;

out vec2 v_t;

void main() {
    v_t = t;
    gl_Position = u_proj * u_cam * u_rot * u_model * vec4(p, 1.0f);
}
)"""";

const char* FS = R""""(
#version 330 core

in vec2 v_t;

out vec4 color;

void main() {
    int checker = 16;
    if ((int(v_t.x * checker) % 2 == 0) ^^ (int(v_t.y * checker) % 2 == 0)) {
        color = vec4(0.47f, 0.55f, 0.81f, 1.0f);
    } else {
        color = vec4(0.29f, 0.38f, 0.68f, 1.0f);
    }
}
)"""";

static GLuint check_shader_compile( GLuint shader ) {
    // check compile status
    GLint status;
    glGetShaderiv( shader, GL_COMPILE_STATUS, &status );
    if ( !status ) {
        static char log[ 512 ];
        glGetShaderInfoLog( shader, sizeof( log ), NULL, log );
        LOG_ERR( "Failed to compile OpenGL shader: %s", log );
    }

    return shader;
}

static bool open_file_dialog() {
#ifdef _WIN32
    OPENFILENAME ofn = { }; ofn.lStructSize = sizeof( ofn );
    ofn.lpstrFile       = g.m_path;
    ofn.nMaxFile        = sizeof( g.m_path );
    ofn.lpstrFilter     = "glTF models (*.glb, *.gltf)\0*.glb;*.gltf\0";
    ofn.nFilterIndex    = 1;
    ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    return GetOpenFileName( &ofn );
#endif
    LOG_ERR( "Not imlemented!" );
    return false;
}

static void load_model() {
    cgltf_options options = { };
    if ( cgltf_parse_file( &options, g.m_path, &g.m_gltf ) != cgltf_result_success ) {
        LOG_ERR( "Failed to parse glTF file" );
    }
    if ( cgltf_load_buffers( &options, g.m_gltf, g.m_path ) != cgltf_result_success ) {
        LOG_ERR( "Failed to load glTF buffers" );
    }

    g.m_meshes.resize( g.m_gltf->meshes_count );
    for ( size_t i = 0; i < g.m_gltf->meshes_count; ++i ) {
        Mesh* mesh              = &g.m_meshes[ i ];
        const cgltf_mesh* gmesh = &g.m_gltf->meshes[ i ];

        mesh->m_primitives.resize( gmesh->primitives_count );
        for ( size_t j = 0; j < gmesh->primitives_count; ++j ) {
            Primitive* prim                 = &mesh->m_primitives[ j ];
            const cgltf_primitive* gprim    = &gmesh->primitives[ j ];

            // Vertex layout can vary widely from model to model
            // This viewer only requires position data (POSITION) and texture data (TEXTURE_0)

            const cgltf_attribute* gattr_p = nullptr;
            const cgltf_attribute* gattr_t = nullptr;

            for ( size_t k = 0; k < gprim->attributes_count; ++k ) {
                const cgltf_attribute* gattr = &gprim->attributes[ k ];
                switch ( gattr->type ) {
                case cgltf_attribute_type_position: {
                    gattr_p = gattr;
                } break;
                case cgltf_attribute_type_texcoord: {
                    gattr_t = gattr;
                } break;
                }
            }

            ASSERT( gattr_p );
            if ( gattr_p && gattr_t ) {
                ASSERT( gattr_p->data->count == gattr_t->data->count );
            }

            // Now upload it to the GPU and set the layout
            glGenVertexArrays( 1, &prim->m_vao );
            glBindVertexArray( prim->m_vao );

            // Allocate memory on GPU
            glGenBuffers( 1, &prim->m_vbo );
            glBindBuffer( GL_ARRAY_BUFFER, prim->m_vbo );
            glBufferData( GL_ARRAY_BUFFER, sizeof( Vertex ) * gattr_p->data->count, NULL, GL_STATIC_DRAW );

            // Write data
            Vertex* buffer = (Vertex*)glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY );
            for ( size_t k = 0; k < gattr_p->data->count; ++k ) {
                // position
                ASSERT( cgltf_accessor_read_float( gattr_p->data, k, buffer[ k ].p.Elements, 3 ) );
                // texture
                if ( gattr_t ) {
                    ASSERT( cgltf_accessor_read_float( gattr_t->data, k, buffer[ k ].t.Elements, 2 ) );
                }
                else {
                    buffer[ k ].t = HMM_V2( 0, 0 );
                }
                
            }
            glUnmapBuffer( GL_ARRAY_BUFFER );

            // Set layout
            glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, sizeof( Vertex ), (void*)offsetof( Vertex, p ) );
            glEnableVertexAttribArray( 0 );
            glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, sizeof( Vertex ), ( void* )offsetof( Vertex, t ) );
            glEnableVertexAttribArray( 1 );

            // Index data
            glGenBuffers( 1, &prim->m_ibo );
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, prim->m_ibo );
            glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( unsigned int ) * gprim->indices->count, NULL, GL_STATIC_DRAW );
            unsigned int* indices = ( unsigned int* )glMapBuffer( GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY );
            for ( size_t k = 0; k < gprim->indices->count; ++k ) {
                ASSERT( cgltf_accessor_read_uint( gprim->indices, k, &indices[ k ], 1 ) );
            }
            glUnmapBuffer( GL_ELEMENT_ARRAY_BUFFER );

            prim->m_indices = gprim->indices->count;
        }
    }

    g.m_nodes.resize( g.m_gltf->nodes_count );
    for ( size_t i = 0; i < g.m_gltf->nodes_count; ++i ) {
        Node* node              = &g.m_nodes[ i ];
        const cgltf_node* gnode = &g.m_gltf->nodes[ i ];

        if ( !gnode->mesh ) {
            continue;
        }

        //printf( "mesh index: %d\n", gnode->mesh - g.m_gltf->meshes );
        node->m_mesh = &g.m_meshes[ gnode->mesh - g.m_gltf->meshes ];

        cgltf_node_transform_world( gnode, node->m_trans.Columns[ 0 ].Elements );
    }

}

#if 0
static void GLAPIENTRY on_gl_error( GLenum, GLenum, GLuint, GLenum severity, GLsizei, const GLchar* message, const void* ) {
    LOG_MSG( "GL: %s", message );
    if ( severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM ) {
        // abort();
    }
}
#endif

int main( int argc, char* argv[] ) {
    g.m_cam.Z = 2.0f;
    g.m_cam_z_plane = 100.0f;
#ifdef _WIN32
    SetProcessDPIAware();
#endif
    // Initialize SDL
    if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 ) {
        LOG_ERR( "Failed to initialize SDL: %s", SDL_GetError() );
    }
    SDL_version ver; SDL_GetVersion( &ver );
    LOG_MSG( "SDL version %d.%d.%d", ver.major, ver.minor, ver.patch );

    // Create window
    const Uint32 WNDFLAGS = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
    SDL_Window* wnd = SDL_CreateWindow( "glTF viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, WNDFLAGS );
    assert( wnd && "Failed to create SDL window" );

    // OpenGL 3.3 core context
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 3 );
    SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
    SDL_GLContext gl = SDL_GL_CreateContext( wnd );
    assert( gl && "Failed to create OpenGL context" );
    SDL_GL_MakeCurrent( wnd, gl );

    // Load OpenGL functions
    if ( !gladLoadGLLoader( ( GLADloadproc )SDL_GL_GetProcAddress ) ) {
        LOG_ERR( "Failed to load OpenGL functions" );
    }

#if 0
    // Enable debug output
    glDebugMessageCallback( on_gl_error, NULL );
    glEnable( GL_DEBUG_OUTPUT );
#endif

    // Compile the OpenGL shader
    GLuint program = glCreateProgram();
    GLuint vs = glCreateShader( GL_VERTEX_SHADER );
    GLuint fs = glCreateShader( GL_FRAGMENT_SHADER );
    // Traditional OpenGL style
    glShaderSource( vs, 1, &VS, NULL );
    glShaderSource( fs, 1, &FS, NULL );
    glCompileShader( vs );
    glCompileShader( fs );
    check_shader_compile( vs );
    check_shader_compile( fs );
    glAttachShader( program, vs );
    glAttachShader( program, fs );
    glLinkProgram( program );
    GLint link_status;
    glGetProgramiv( program, GL_LINK_STATUS, &link_status );
    if ( !link_status ) {
        static char log[ 512 ];
        glGetProgramInfoLog( program, sizeof( log ), NULL, log );
        LOG_ERR( "Failed to link OpenGL program: %s", log );
    }

    // Initialize Dear ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL( wnd, gl );
    ImGui_ImplOpenGL3_Init();

    ImGui::GetIO().IniFilename = NULL;

    // Auto-load
    if (argc > 1) {
        strncpy(g.m_path, argv[1], sizeof(g.m_path));
        load_model();
    }

    // Window loop
    bool running = true;
    do {
        // Poll events
        SDL_Event evt;
        while ( SDL_PollEvent( &evt ) ) {
            ImGui_ImplSDL2_ProcessEvent( &evt );
            switch ( evt.type ) {
            case SDL_QUIT: { running = false; } break;
            };
        }

        // Update the viewport
        int vp_w, vp_h;
        SDL_GL_GetDrawableSize( wnd, &vp_w, &vp_h );
        glViewport( 0, 0, vp_w, vp_h );

        g.m_uniforms.m_proj = HMM_Perspective_RH_NO( HMM_ToRad( 90.0f / 2.0f ), ( float )vp_w / ( float )vp_h, 0.1f, g.m_cam_z_plane );
        g.m_uniforms.m_cam = HMM_M4D( 1.0f );
        g.m_uniforms.m_cam[ 3 ][ 0 ] = -g.m_cam.X;
        g.m_uniforms.m_cam[ 3 ][ 1 ] = -g.m_cam.Y;
        g.m_uniforms.m_cam[ 3 ][ 2 ] = -g.m_cam.Z;

        if ( g.m_model_rotate ) {
            g.m_uniforms.m_rot = HMM_Rotate_RH( ( float )SDL_GetTicks() / 1e3f, HMM_V3( 0.0f, 1.0f, 0.0f ) );
        }
        else {
            g.m_uniforms.m_rot = HMM_M4D( 1.0f );
        }

#if 0
        static auto dump_mat4 = []( const char* name, const HMM_Mat4& mat ) {
            LOG_MSG( "%s:", name );
            LOG_MSG( "    %f, %f, %f, %f", mat.Elements[ 0 ][ 0 ], mat.Elements[ 0 ][ 1 ], mat.Elements[ 0 ][ 2 ], mat.Elements[ 0 ][ 3 ] );
            LOG_MSG( "    %f, %f, %f, %f", mat.Elements[ 1 ][ 0 ], mat.Elements[ 1 ][ 1 ], mat.Elements[ 1 ][ 2 ], mat.Elements[ 1 ][ 3 ] );
            LOG_MSG( "    %f, %f, %f, %f", mat.Elements[ 2 ][ 0 ], mat.Elements[ 2 ][ 1 ], mat.Elements[ 2 ][ 2 ], mat.Elements[ 2 ][ 3 ] );
            LOG_MSG( "    %f, %f, %f, %f", mat.Elements[ 3 ][ 0 ], mat.Elements[ 3 ][ 1 ], mat.Elements[ 3 ][ 2 ], mat.Elements[ 3 ][ 3 ] );
        };
#endif

        // Set OpenGL state
        glUseProgram( program );      // Use our shader for rendering
        glEnable( GL_DEPTH_TEST );    // Enable z-buffer testing

        // Clear
        glClearColor( 0.1f, 0.1f, 0.1f, 1.0f );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

        // Render model
        for ( auto& node : g.m_nodes ) {
            if ( !node.m_mesh ) {
                continue;
            }

            //cgltf_node_transform_local(node.)
            g.m_uniforms.m_model = node.m_trans;

            // Update uniforms
            glUniformMatrix4fv(glGetUniformLocation(program, "m_cam"), 1, GL_FALSE, g.m_uniforms.m_cam.Elements[0]);
            glUniformMatrix4fv(glGetUniformLocation(program, "m_model"), 1, GL_FALSE, g.m_uniforms.m_model.Elements[0]);
            glUniformMatrix4fv(glGetUniformLocation(program, "m_proj"), 1, GL_FALSE, g.m_uniforms.m_proj.Elements[0]);
            glUniformMatrix4fv(glGetUniformLocation(program, "m_rot"), 1, GL_FALSE, g.m_uniforms.m_rot.Elements[0]);

            // Render primitives
            for ( auto& prim : node.m_mesh->m_primitives ) {
                // Bind buffers
                glBindVertexArray( prim.m_vao );

                glDrawElements( GL_TRIANGLES, prim.m_indices, GL_UNSIGNED_INT, NULL );
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        if ( ImGui::BeginMainMenuBar() ) {
            if ( ImGui::Button( "Open..." ) ) {
                if ( open_file_dialog() ) {
                    load_model();
                }
            }
            ImGui::Separator();
            static bool wireframe = false;
            if ( ImGui::Checkbox( "Wireframe", &wireframe ) ) {
                glPolygonMode( GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL );
            }
            ImGui::Separator();
            ImGui::Text( "Current model: %s", g.m_path[ 0 ] ? g.m_path : "None" );
            ImGui::EndMainMenuBar();
        }

        if ( ImGui::Begin( "Controls" ) ) {
            ImGui::SeparatorText( "Camera" );

            static float range_exponent = 1;
            ImGui::SliderFloat( "Range scale", &range_exponent, 1, 100 );
            float range_min = -10.0f * range_exponent;
            float range_max =  10.0f * range_exponent;
            ImGui::SliderFloat( "X", &g.m_cam.X, range_min, range_max );
            ImGui::SliderFloat( "Y", &g.m_cam.Y, range_min, range_max );
            ImGui::SliderFloat( "Z", &g.m_cam.Z, range_min, range_max );
            ImGui::SliderFloat( "Far clipping plane", &g.m_cam_z_plane, 1.0f, 10000.0f );

            ImGui::SeparatorText( "Model" );
            ImGui::Checkbox( "Spin", &g.m_model_rotate );

            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData() );

        // Update the screen
        SDL_GL_SwapWindow( wnd );
    } while ( running );

    SDL_DestroyWindow( wnd );
    SDL_Quit();
}
