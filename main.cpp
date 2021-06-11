#include <assert.h>

#include <SDL.h>
#include <SDL_opengl.h>

#define ArrayCount(a) (sizeof(a)/sizeof(*(a)))

typedef void (APIENTRY *gl_gen_buffers)(GLsizei, GLuint *);
typedef void (APIENTRY *gl_bind_buffer)(GLenum, GLuint);
typedef void (APIENTRY *gl_buffer_data)(GLenum, GLsizeiptr, const GLvoid *, GLenum);
typedef void (APIENTRY *gl_enable_client_state)(GLenum);
typedef void (APIENTRY *gl_vertex_pointer)(GLint, GLenum, GLsizei, const GLvoid *);
typedef void (APIENTRY *gl_color_pointer)(GLint, GLenum, GLsizei, const GLvoid *);
typedef void (APIENTRY *gl_shader_source)(GLuint, GLsizei, const GLchar **, const GLint *);
typedef void (APIENTRY *gl_compile_shader)(GLuint);
typedef void (APIENTRY *gl_attach_shader)(GLuint, GLuint);
typedef void (APIENTRY *gl_link_program)(GLuint);
typedef void (APIENTRY *gl_get_programiv)(GLuint, GLenum, GLint *);
typedef void (APIENTRY *gl_get_program_info_log)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void (APIENTRY *gl_get_shader_info_log)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void (APIENTRY *gl_use_program)(GLuint);
typedef void (APIENTRY *gl_gen_vertex_arrays)(GLsizei, GLuint *);
typedef void (APIENTRY *gl_bind_vertex_array)(GLuint);
typedef void (APIENTRY *gl_enable_vertex_attrib_array)(GLuint);
typedef void (APIENTRY *gl_vertex_attrib_pointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const GLvoid *);
typedef GLint (APIENTRY *gl_get_attrib_location)(GLuint, const GLchar *);
typedef GLint (APIENTRY *gl_get_uniform_location)(GLuint, const GLchar *);
typedef void (APIENTRY *gl_uniform_matrix_4fv)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void (APIENTRY *gl_uniform_1f)(GLint, GLfloat);
typedef void (APIENTRY *gl_uniform_2f)(GLint, GLfloat, GLfloat);
typedef void (APIENTRY *gl_uniform_1i)(GLint, GLint);
typedef GLuint (APIENTRY *gl_create_shader)(GLenum);
typedef GLuint (APIENTRY *gl_create_program)(void);
typedef void (APIENTRY *gl_get_shaderiv)(GLuint, GLenum, GLint *);
typedef void (APIENTRY *gl_dispatch_compute)(GLuint, GLuint, GLuint);
typedef void (APIENTRY *gl_gen_framebuffers)(GLsizei, GLuint *);
typedef void (APIENTRY *gl_bind_framebuffer)(GLenum, GLuint);
typedef void (APIENTRY *gl_framebuffer_texture_2d)(GLenum, GLenum, GLuint, GLuint, GLint);
typedef void (APIENTRY *gl_bind_image_texture)(GLuint, GLuint, GLint, GLboolean, GLint, GLenum, GLenum);
typedef void (APIENTRY *gl_tex_storage_2d)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
typedef void (APIENTRY *gl_tex_storage_3d)(GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLsizei);
typedef void (APIENTRY *gl_memory_barrier)(GLbitfield);

static gl_gen_buffers glGenBuffers = 0;
static gl_bind_buffer glBindBuffer = 0;
static gl_buffer_data glBufferData = 0;
static gl_create_shader glCreateShader = 0;
static gl_shader_source glShaderSource = 0;
static gl_compile_shader glCompileShader = 0;
static gl_create_program glCreateProgram = 0;
static gl_attach_shader glAttachShader = 0;
static gl_link_program glLinkProgram = 0;
static gl_get_programiv glGetProgramiv = 0;
static gl_get_program_info_log glGetProgramInfoLog = 0;
static gl_get_shader_info_log glGetShaderInfoLog = 0;
static gl_use_program glUseProgram = 0;
static gl_gen_vertex_arrays glGenVertexArrays = 0;
static gl_bind_vertex_array glBindVertexArray = 0;
static gl_enable_vertex_attrib_array glEnableVertexAttribArray = 0;
static gl_vertex_attrib_pointer glVertexAttribPointer = 0;
static gl_get_attrib_location glGetAttribLocation = 0;
static gl_get_uniform_location glGetUniformLocation = 0;
static gl_uniform_matrix_4fv glUniformMatrix4fv = 0;
static gl_uniform_1f glUniform1f = 0;
static gl_uniform_2f glUniform2f = 0;
static gl_uniform_1i glUniform1i = 0;
static gl_get_shaderiv glGetShaderiv = 0;
static gl_dispatch_compute glDispatchCompute = 0;
static gl_gen_framebuffers glGenFramebuffers = 0;
static gl_bind_framebuffer glBindFramebuffer = 0;
static gl_framebuffer_texture_2d glFramebufferTexture2D = 0;
static gl_bind_image_texture glBindImageTexture = 0;
static gl_tex_storage_2d glTexStorage2D = 0;
static gl_tex_storage_3d glTexStorage3D = 0;
static gl_memory_barrier glMemoryBarrier = 0;

static void
LoadOpenGLFunctions()
{
    glGenBuffers = (gl_gen_buffers) SDL_GL_GetProcAddress("glGenBuffers");
    glBindBuffer = (gl_bind_buffer) SDL_GL_GetProcAddress("glBindBuffer");
    glBufferData = (gl_buffer_data) SDL_GL_GetProcAddress("glBufferData");
    glCreateShader = (gl_create_shader) SDL_GL_GetProcAddress("glCreateShader");
    glShaderSource = (gl_shader_source) SDL_GL_GetProcAddress("glShaderSource");
    glCompileShader = (gl_compile_shader) SDL_GL_GetProcAddress("glCompileShader");
    glCreateProgram = (gl_create_program) SDL_GL_GetProcAddress("glCreateProgram");
    glAttachShader = (gl_attach_shader) SDL_GL_GetProcAddress("glAttachShader");
    glLinkProgram = (gl_link_program) SDL_GL_GetProcAddress("glLinkProgram");
    glGetProgramiv = (gl_get_programiv) SDL_GL_GetProcAddress("glGetProgramiv");
    glGetProgramInfoLog = (gl_get_program_info_log) SDL_GL_GetProcAddress("glGetProgramInfoLog");
    glGetShaderInfoLog = (gl_get_shader_info_log) SDL_GL_GetProcAddress("glGetShaderInfoLog");
    glUseProgram = (gl_use_program) SDL_GL_GetProcAddress("glUseProgram");
    glGenVertexArrays = (gl_gen_vertex_arrays) SDL_GL_GetProcAddress("glGenVertexArrays");
    glBindVertexArray = (gl_bind_vertex_array) SDL_GL_GetProcAddress("glBindVertexArray");
    glEnableVertexAttribArray = (gl_enable_vertex_attrib_array) SDL_GL_GetProcAddress("glEnableVertexAttribArray");
    glVertexAttribPointer = (gl_vertex_attrib_pointer) SDL_GL_GetProcAddress("glVertexAttribPointer");
    glGetAttribLocation = (gl_get_attrib_location) SDL_GL_GetProcAddress("glGetAttribLocation");
    glGetUniformLocation = (gl_get_uniform_location) SDL_GL_GetProcAddress("glGetUniformLocation");
    glUniform1f = (gl_uniform_1f) SDL_GL_GetProcAddress("glUniform1f");
    glUniform2f = (gl_uniform_2f) SDL_GL_GetProcAddress("glUniform2f");
    glUniform1i = (gl_uniform_1i) SDL_GL_GetProcAddress("glUniform1i");
    glUniformMatrix4fv = (gl_uniform_matrix_4fv) SDL_GL_GetProcAddress("glUniformMatrix4fv");
    glGetShaderiv = (gl_get_shaderiv)SDL_GL_GetProcAddress("glGetShaderiv");
    glDispatchCompute = (gl_dispatch_compute)SDL_GL_GetProcAddress("glDispatchCompute");
    glGenFramebuffers = (gl_gen_framebuffers)SDL_GL_GetProcAddress("glGenFramebuffers");
    glBindFramebuffer = (gl_bind_framebuffer)SDL_GL_GetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D = (gl_framebuffer_texture_2d)SDL_GL_GetProcAddress("glFramebufferTexture2D");
    glBindImageTexture = (gl_bind_image_texture)SDL_GL_GetProcAddress("glBindImageTexture");
    glTexStorage2D = (gl_tex_storage_2d)SDL_GL_GetProcAddress("glTexStorage2D");
    glTexStorage3D = (gl_tex_storage_3d)SDL_GL_GetProcAddress("glTexStorage3D");
    glMemoryBarrier = (gl_memory_barrier)SDL_GL_GetProcAddress("glMemoryBarrier");
}

enum timer_names {
    Timer_Sim,
    Timer_RenderFieldEval,
    Timer_Count,
};
uint64_t GlobalTimers[Timer_Count][2];
uint64_t GlobalPerformaceFreq;

#define TIMER_START(name) GlobalTimers[name][0] = SDL_GetPerformanceCounter()
#define TIMER_END(name) GlobalTimers[name][1] = (SDL_GetPerformanceCounter() - GlobalTimers[name][0]) * 0.05 + 0.95 * GlobalTimers[name][1]
#define TIMER_GET_MS(name) (float)GlobalTimers[name][1] / (float)GlobalPerformaceFreq * 1000.0f

#include "work_queue.h"
#include "linalg.h"
#include "sim.h"
#include "render.h"

#include "posix_work_queue.cpp"
#include "sim.cpp"
#include "render.cpp"

int
main(void)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *Window = SDL_CreateWindow("fluid", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 512, 512, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GL_SetSwapInterval(1);

    SDL_GLContext context = SDL_GL_CreateContext(Window);
    assert(context);

    LoadOpenGLFunctions();

    GlobalPerformaceFreq = SDL_GetPerformanceFrequency();

    InitQueue(&GlobalWorkQueue);

    sim Sim = {};
    InitSim(&Sim);

    opengl OpenGL = {};
    InitializeOpenGL(&OpenGL, Sim.HashGrid, Sim.ParticleCount, Sim.Particles);

    bool Running = true;
    bool RenderContour = false;

    while (Running) {
        bool ToggleRender = false;

        SDL_Event Event;
        while (SDL_PollEvent(&Event)) {
            if (Event.type == SDL_QUIT) {
                Running = false;
                break;
            } else if (Event.type == SDL_KEYDOWN) {
                if (Event.key.keysym.sym == SDLK_f && Event.key.repeat == 0) {
                    ToggleRender = true; 
                }
            }
        }

        if (ToggleRender) {
            RenderContour = !RenderContour;
        }

        int ScreenWidth = 0, ScreenHeight = 0;
        SDL_GetWindowSize(Window, &ScreenWidth, &ScreenHeight);

        int MouseX = 0;
        int MouseY = 0;
        int ButtonState = SDL_GetMouseState(&MouseX, &MouseY);
        Sim.PullPoint = (V2(MouseX, MouseY) * V2(1.0f / ScreenWidth, 1.0f / ScreenHeight) - V2(0.5f)) * V2(WORLD_WIDTH, -WORLD_HEIGHT);
        Sim.Pulling = ButtonState & SDL_BUTTON(SDL_BUTTON_LEFT);

        TIMER_START(Timer_Sim);
        Simulate(&Sim);
        TIMER_END(Timer_Sim);

        Render(&Sim, &OpenGL, ScreenWidth, ScreenHeight, RenderContour);

        SDL_GL_SwapWindow(Window);
    }

    SDL_Quit();
}
