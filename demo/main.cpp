/*
* Copyright (C) 2026 uncognic
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <GL/gl.h>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>

#pragma comment(lib, "opengl32.lib")

typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)
(HDC, HGLRC, const int*);
typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int);

typedef GLuint(APIENTRY* PFNGLCREATESHADERPROC)(GLenum);
typedef void  (APIENTRY* PFNGLSHADERSOURCEPROC)(GLuint, GLsizei, const char**, const GLint*);
typedef void  (APIENTRY* PFNGLCOMPILESHADERPROC)(GLuint);
typedef GLuint(APIENTRY* PFNGLCREATEPROGRAMPROC)();
typedef void  (APIENTRY* PFNGLATTACHSHADERPROC)(GLuint, GLuint);
typedef void  (APIENTRY* PFNGLLINKPROGRAMPROC)(GLuint);
typedef void  (APIENTRY* PFNGLUSEPROGRAMPROC)(GLuint);
typedef GLint(APIENTRY* PFNGLGETUNIFORMLOCATIONPROC)(GLuint, const char*);
typedef void  (APIENTRY* PFNGLUNIFORM1FPROC)(GLint, GLfloat);
typedef void  (APIENTRY* PFNGLUNIFORM2FPROC)(GLint, GLfloat, GLfloat);
typedef void  (APIENTRY* PFNGLGETSHADERIVPROC)(GLuint, GLenum, GLint*);
typedef void  (APIENTRY* PFNGLGETSHADERINFOLOGPROC)(GLuint, GLsizei, GLsizei*, char*);
typedef void(APIENTRY* PFNGLGENVERTEXARRAYSPROC)(GLsizei, GLuint*);
typedef void(APIENTRY* PFNGLBINDVERTEXARRAYPROC)(GLuint);

// load all pointers from the driver
#define GLPROC(type, name) type name = (type)wglGetProcAddress(#name);

// pass UV
static const char* VERT_SRC = R"(
#version 330 core
out vec2 fragCoord;
void main() {
    vec2 pos[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
    fragCoord = pos[gl_VertexID];
}
)";

// read shader from file
static std::string readFile(const std::string& path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// compile shader and check for errors
static GLuint compileShader(GLenum type, const char* src) {
    GLPROC(PFNGLCREATESHADERPROC, glCreateShader);
    GLPROC(PFNGLSHADERSOURCEPROC, glShaderSource);
    GLPROC(PFNGLCOMPILESHADERPROC, glCompileShader);
    GLPROC(PFNGLGETSHADERIVPROC, glGetShaderiv);
    GLPROC(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog);

    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, 0x8B81 /* GL COMPILE STATUS */, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        MessageBoxA(nullptr, log, "Shader Error", MB_OK);
    }
    return shader;
}

// build program from fragment shader source
static GLuint buildProgram(const std::string& fragSrc) {
    GLPROC(PFNGLCREATEPROGRAMPROC, glCreateProgram);
    GLPROC(PFNGLATTACHSHADERPROC, glAttachShader);
    GLPROC(PFNGLLINKPROGRAMPROC, glLinkProgram);

    GLuint vert = compileShader(0x8B31 /*GL_VERTEX_SHADER*/, VERT_SRC);
    GLuint frag = compileShader(0x8B30 /*GL_FRAGMENT_SHADER*/, fragSrc.c_str());

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);
    return prog;
}

// win32 stuff
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_DESTROY || (msg == WM_KEYDOWN && wp == VK_ESCAPE)) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

// entry point
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    //create window
    WNDCLASSEXA wc = {sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "demo";
    wc.style = CS_OWNDC;
    RegisterClassExA(&wc);

    HWND hwnd = CreateWindowExA(0, "demo", "demo", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                                CW_USEDEFAULT, 1280, 720, nullptr, nullptr, hInst, nullptr);

    HDC dc = GetDC(hwnd);

    // set pixel format so ogl can draw
    PIXELFORMATDESCRIPTOR pfd = {sizeof(pfd), 1};
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    SetPixelFormat(dc, ChoosePixelFormat(dc, &pfd), &pfd);

    // get wglCreateContextAattribsARB
    HGLRC tempCtx = wglCreateContext(dc);
    wglMakeCurrent(dc, tempCtx);

    auto wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC) wglGetProcAddress("wglCreateContextAttribsARB");

    // opengl 3.3
    int attribs[] = {0x2091, 3,          // WGL_CONTEXT_MAJOR_VERSION_ARB
                     0x2092, 3,          // WGL_CONTEXT_MINOR_VERSION_ARB
                     0x9126, 0x00000001, // WGL_CONTEXT_PROFILE_MASK_ARB = core
                     0};
    HGLRC ctx = wglCreateContextAttribsARB(dc, nullptr, attribs);
    wglMakeCurrent(dc, ctx);
    wglDeleteContext(tempCtx);

    // vsync
    auto wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC) wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(1);
    }

    // get shader
    std::string fragSrc = readFile("../shader.glsl");
    GLuint program = buildProgram(fragSrc);

    GLPROC(PFNGLUSEPROGRAMPROC, glUseProgram);
    GLPROC(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);
    GLPROC(PFNGLUNIFORM1FPROC, glUniform1f);
    GLPROC(PFNGLUNIFORM2FPROC, glUniform2f);

    glUseProgram(program);

    // set up vao
    GLPROC(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays);
    GLPROC(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray);
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // main loop
    auto start = std::chrono::steady_clock::now();
    MSG msg = {};

    for (;;) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        auto now = std::chrono::steady_clock::now();
        float iTime = std::chrono::duration<float>(now - start).count();

        RECT rect;
        GetClientRect(hwnd, &rect);
        float w = static_cast<float>(rect.right - rect.left);
        float h = static_cast<float>(rect.bottom - rect.top);
        glViewport(0, 0, static_cast<GLsizei>(w), static_cast<GLsizei>(h));

        glUniform1f(glGetUniformLocation(program, "iTime"), iTime);
        glUniform2f(glGetUniformLocation(program, "iResolution"), w, h);

        glDrawArrays(0x0004 /*GL_TRIANGLES*/, 0, 3);

        SwapBuffers(dc);
    }

    __assume(0);
}