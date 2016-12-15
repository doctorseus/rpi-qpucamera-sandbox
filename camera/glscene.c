#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

#define check() assert(glGetError() == 0)

typedef struct app_state_s {
    EGLDisplay egl_display;
    EGLSurface egl_surface;
    EGLContext egl_context;

    int screen_width;
    int screen_height;
    
    GLuint buf_vertex;
    GLuint attr_vertex;
    GLuint tex_obj;
    GLuint tex_fb;
    
    GLuint shader_vshader;
    GLuint shader_fshader;
    GLuint shader_program;
} app_state_t;

static void ogl_init(app_state_t *app);
static void ogl_destroy(app_state_t *app);
static void ogl_draw(app_state_t *app);

static void app_init(app_state_t *app);
static void app_draw(app_state_t *app);

static void ogl_init(app_state_t *app) {
    int32_t success = 0;
    EGLBoolean result;
    EGLint num_config;

    static EGL_DISPMANX_WINDOW_T nativewindow;

    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    static const EGLint attribute_list[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE
    };

    static const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLConfig config;

    // get an EGL display connection
    app->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(app->egl_display != EGL_NO_DISPLAY);
    check();

    // initialize the EGL display connection
    result = eglInitialize(app->egl_display, NULL, NULL);
    assert(EGL_FALSE != result);
    check();

    // get an appropriate EGL frame buffer configuration
    result = eglChooseConfig(app->egl_display, attribute_list, &config, 1, &num_config);
    assert(EGL_FALSE != result);
    check();

    // get an appropriate EGL frame buffer configuration
    result = eglBindAPI(EGL_OPENGL_ES_API);
    assert(EGL_FALSE != result);
    check();

    // create an EGL rendering context
    app->egl_context = eglCreateContext(app->egl_display, config, EGL_NO_CONTEXT, context_attributes);
    assert(app->egl_context != EGL_NO_CONTEXT);
    check();

    // create an EGL window surface
    success = graphics_get_display_size(0 /* LCD */, &app->screen_width, &app->screen_height);
    assert( success >= 0 );

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = app->screen_width;
    dst_rect.height = app->screen_height;
    
    // NO FULLSCREEN - Top right corner
    int rect_width = app->screen_width / 4;
    int rect_height = app->screen_height / 4;
    dst_rect.width = rect_width;
    dst_rect.height = rect_height;
    dst_rect.x = app->screen_width - rect_width;
    dst_rect.y = 0;
    
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = app->screen_width << 16;
    src_rect.height = app->screen_height << 16;        

    dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
    dispman_update = vc_dispmanx_update_start( 0 );

    dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
    0/*layer*/, &dst_rect, 0/*src*/,
    &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, 0/*transform*/);

    nativewindow.element = dispman_element;
    nativewindow.width = app->screen_width;
    nativewindow.height = app->screen_height;
    vc_dispmanx_update_submit_sync( dispman_update );

    check();

    app->egl_surface = eglCreateWindowSurface(app->egl_display, config, &nativewindow, NULL );
    assert(app->egl_surface != EGL_NO_SURFACE);
    check();

    // connect the context to the surface
    result = eglMakeCurrent(app->egl_display, app->egl_surface, app->egl_surface, app->egl_context);
    assert(EGL_FALSE != result);
    check();

    // Set background color and clear buffers
    glClearColor(0.15f, 0.25f, 0.35f, 1.0f);
    glClear( GL_COLOR_BUFFER_BIT );

    check();
}

static void ogl_destroy(app_state_t *app) {
    eglDestroyContext(app->egl_display, app->egl_context);
    eglDestroySurface(app->egl_display, app->egl_surface);
    eglTerminate(app->egl_display);
}

static void ogl_draw(app_state_t *app){
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    check();

    // draw app frame
    app_draw(app);

    glFlush();
    glFinish();
    check();

    eglSwapBuffers(app->egl_display, app->egl_surface);
    check();
}

static void app_init(app_state_t *app) {
    const GLchar *vshader_source =
    "attribute vec4 vertex;"
    "varying vec2 tcoord;"
    "void main(void) {"
    "   gl_Position = vertex;"
    "   tcoord = vertex.xy*0.5+0.5;"
    "}";

    const GLchar *fshader_source =
    "varying vec2 tcoord;"
    "void main(void) {"
    "   gl_FragColor = vec4(tcoord.x, tcoord.y, 1.0, 1.0);"
    "}";
    
    // Compile vertex shader
    app->shader_vshader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(app->shader_vshader, 1, &vshader_source, 0);
    glCompileShader(app->shader_vshader);
    check();

    // Compile fragment shader
    app->shader_fshader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(app->shader_fshader, 1, &fshader_source, 0);
    glCompileShader(app->shader_fshader);
    check();

    // Link shader
    app->shader_program = glCreateProgram();
    glAttachShader(app->shader_program, app->shader_vshader);
    glAttachShader(app->shader_program, app->shader_fshader);
    glLinkProgram(app->shader_program);
    check();
    
    app->attr_vertex = glGetAttribLocation(app->shader_program, "vertex");
    
    // Vertex data
    static const GLfloat vertex_data[] = {
        -1.0,-1.0,1.0,1.0,
        1.0,-1.0,1.0,1.0,
        1.0,1.0,1.0,1.0,
        -1.0,1.0,1.0,1.0
    };
    
    // Upload vertex data to a buffer
    glGenBuffers(1, &app->buf_vertex);
    glBindBuffer(GL_ARRAY_BUFFER, app->buf_vertex);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);
    glVertexAttribPointer(app->attr_vertex, 4, GL_FLOAT, 0, 16, 0);
    glEnableVertexAttribArray(app->attr_vertex);
    check();
    
    // Prepare viewport
    glViewport(0, 0, app->screen_width, app->screen_height);
    check();
    
    /*
    // Prepare a texture image
    glGenTextures(1, &app->tex_obj);
    check();
    glBindTexture(GL_TEXTURE_2D, app->tex_obj);
    check();
    
    // glActiveTexture(0)
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, app->screen_width, app->screen_height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 0);
    check();
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    check();
    
    
    // Prepare a framebuffer for rendering
    glGenFramebuffers(1, &app->tex_fb);
    check();
    glBindFramebuffer(GL_FRAMEBUFFER, app->tex_fb);
    check();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D, app->tex_obj, 0);
    check();
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    check();
    */
}

static void app_draw(app_state_t *app) {
    glBindBuffer(GL_ARRAY_BUFFER, app->buf_vertex);
    check();
    
    glUseProgram(app->shader_program);
    check();

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    check();

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int main(int argc, char **argv)
{
    app_state_t app;
    
    bcm_host_init();
    
    ogl_init(&app);
    
    printf(" - OpenGL initialized successfully.\n");
    
    const char *s;
    s = eglQueryString(app.egl_display, EGL_VERSION);
    printf("EGL_VERSION = %s\n", s);

    s = eglQueryString(app.egl_display, EGL_VENDOR);
    printf("EGL_VENDOR = %s\n", s);

    s = eglQueryString(app.egl_display, EGL_EXTENSIONS);
    printf("EGL_EXTENSIONS = %s\n", s);

    s = eglQueryString(app.egl_display, EGL_CLIENT_APIS);
    printf("EGL_CLIENT_APIS = %s\n", s);

    // Initialize Application (Shaders,...)
    app_init(&app);
    
    int terminate = 0;
    struct timespec start, stop;
    double accum;
    while (!terminate) {
        clock_gettime(CLOCK_MONOTONIC_RAW, &start);
        
        ogl_draw(&app);
        
        clock_gettime(CLOCK_MONOTONIC_RAW, &stop);
        //accum = ( stop.tv_sec - start.tv_sec ) + ( stop.tv_nsec - start.tv_nsec ) / 1000000.0;
        accum = 1000 / (( stop.tv_sec - start.tv_sec ) + ( stop.tv_nsec - start.tv_nsec ) / 1000000.0);
        printf( "fps = %lf\n", accum);
    }
    
    ogl_destroy(&app);

    return EXIT_SUCCESS;
}
