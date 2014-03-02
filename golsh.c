#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GL/glfw.h>

int width = 512;
int height = 512;
int zoom = 1;

void glerr (const char* when) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        fprintf(stderr, "OpenGL error 0x%04X %s\n", err, when);
        glerr(when);
    }
}

const char* vssrc =
    "#version 110\n"
    "attribute vec2 pos;\n"
    "varying vec2 tp;\n"
    "void main () {\n"
    "    tp = pos;\n"
    "    gl_Position = vec4(pos.x*2.0-1.0, pos.y*2.0-1.0, 0, 1);\n"
    "}\n"
;
const char* fssrc =
    "#version 110\n"
    "uniform bool do_calc;\n"
    "uniform sampler2D tex;\n"
    "uniform vec2 tex_size;\n"
    "varying vec2 tp;\n"
    "void main () {\n"
    "    if (do_calc) {\n"
    "        float x = tp.x;"
    "        float y = tp.y;"
    "        float l = x - 1.0 / tex_size.x;"
    "        float b = y - 1.0 / tex_size.y;"
    "        float r = x + 1.0 / tex_size.x;"
    "        float t = y + 1.0 / tex_size.y;"
    "        float on = texture2D(tex, vec2(x, y)).r;\n"
    "        float count = \n"
    "            + texture2D(tex, vec2(l, b)).r\n"
    "            + texture2D(tex, vec2(x, b)).r\n"
    "            + texture2D(tex, vec2(r, b)).r\n"
    "            + texture2D(tex, vec2(l, y)).r\n"
    "            + texture2D(tex, vec2(r, y)).r\n"
    "            + texture2D(tex, vec2(l, t)).r\n"
    "            + texture2D(tex, vec2(x, t)).r\n"
    "            + texture2D(tex, vec2(r, t)).r\n"
    "        ;"
    "        if (on == 1.0 ? count == 2.0 || count == 3.0 : count == 3.0) {\n"
    "            gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
    "        }\n"
    "        else {\n"
    "            gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);\n"
    "        }\n"
    "    }\n"
    "    else {\n"
    "        gl_FragColor = texture2D(tex, tp);\n"
    "    }\n"
    "}\n"
;

int GLFWCALL close_cb () {
    return GL_TRUE;
}
void GLFWCALL key_cb (int code, int action) {
    if (code == GLFW_KEY_ESC && action == GLFW_PRESS) {
        glfwTerminate();
        exit(0);
    }
}


int main () {
    glfwInit();
    glfwOpenWindow(width*zoom, height*zoom, 8, 8, 8, 0, 0, 0, GLFW_WINDOW);
    glfwSetWindowCloseCallback(close_cb);
    glfwSetKeyCallback(key_cb);
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "GLEW error: %s\n", glewGetErrorString(err));
        exit(1);
    }
    GLint status;
    GLint loglen;
    GLuint vsid = glCreateShader(GL_VERTEX_SHADER);
    int vslen = strlen(vssrc);
    glShaderSource(vsid, 1, &vssrc, &vslen);
    glCompileShader(vsid);
    glGetShaderiv(vsid, GL_COMPILE_STATUS, &status);
    glGetShaderiv(vsid, GL_INFO_LOG_LENGTH, &loglen);
    if (!status || loglen > 1) {
        char log [loglen];
        glGetShaderInfoLog(vsid, loglen, NULL, log);
        fprintf(stderr, "Shader info log for vertex shader:\n");
        fputs(log, stderr);
        if (!status) {
            fprintf(stderr, "Failed to compile GL shader.\n");
            exit(1);
        }
    }
    glerr("after compiling vertex shader");
    GLuint fsid = glCreateShader(GL_FRAGMENT_SHADER);
    int fslen = strlen(fssrc);
    glShaderSource(fsid, 1, &fssrc, &fslen);
    glCompileShader(fsid);
    glGetShaderiv(fsid, GL_COMPILE_STATUS, &status);
    glGetShaderiv(fsid, GL_INFO_LOG_LENGTH, &loglen);
    if (!status || loglen > 1) {
        char log [loglen];
        glGetShaderInfoLog(fsid, loglen, NULL, log);
        fprintf(stderr, "Shader info log for fragment shader:\n");
        fputs(log, stderr);
        if (!status) {
            fprintf(stderr, "Failed to compile GL shader.\n");
            exit(1);
        }
    }
    glerr("after compiling fragment shader");

    GLuint prid = glCreateProgram();
    glAttachShader(prid, vsid);
    glAttachShader(prid, fsid);
    glLinkProgram(prid);
    glGetProgramiv(prid, GL_LINK_STATUS, &status);
    glGetProgramiv(prid, GL_INFO_LOG_LENGTH, &loglen);
    if (!status || loglen > 1) {
        char log [loglen];
        glGetProgramInfoLog(prid, loglen, NULL, log);
        fprintf(stderr, "Program info log:\n");
        fputs(log, stderr);
        if (!status) {
            fprintf(stderr, "Failed to link GL program.\n");
            exit(1);
        }
    }
    glerr("after linking program");
    glUseProgram(prid);

    GLint uni_tex = glGetUniformLocation(prid, "tex");
    GLint uni_tex_size = glGetUniformLocation(prid, "tex_size");
    GLint uni_do_calc = glGetUniformLocation(prid, "do_calc");
    glUniform1i(uni_tex, 0);
    glUniform2f(uni_tex_size, width, height);
    glerr("after getting uniforms");

    unsigned char data[width*height];
    uint i;
    for (i = 0; i < width*height; i++) {
        data[i] = rand() & 1 ? 0xff : 0x00;
    }

    GLuint tex1; glGenTextures(1, &tex1);
    glBindTexture(GL_TEXTURE_2D, tex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA2, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    glerr("after creating tex 1");
    GLuint fb1; glGenFramebuffers(1, &fb1);
    glBindFramebuffer(GL_FRAMEBUFFER, fb1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);
    glerr("after creating fb 1");
    GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer 1 creation failed: 0x%04X\n", fb_status);
        exit(1);
    }

    GLuint tex2; glGenTextures(1, &tex2);
    glBindTexture(GL_TEXTURE_2D, tex2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA2, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    glerr("after creating tex 2");
    GLuint fb2; glGenFramebuffers(1, &fb2);
    glBindFramebuffer(GL_FRAMEBUFFER, fb2);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glerr("after creating fb 2");
    fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer 2 creation failed: 0x%04X\n", fb_status);
        exit(1);
    }


    int use2 = 0;
    float verts [8] = { 0, 0,  1, 0,  1, 1,  0, 1 };
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);
    for (;;) {
        if (!use2) {
            glUniform1i(uni_do_calc, 1);
            glBindTexture(GL_TEXTURE_2D, tex1);
            glBindFramebuffer(GL_FRAMEBUFFER, fb2);
            glDrawArrays(GL_QUADS, 0, 4);

            glUniform1i(uni_do_calc, 0);
            glBindTexture(GL_TEXTURE_2D, tex2);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDrawArrays(GL_QUADS, 0, 4);
        }
        else {
            glUniform1i(uni_do_calc, 1);
            glBindTexture(GL_TEXTURE_2D, tex2);
            glBindFramebuffer(GL_FRAMEBUFFER, fb1);
            glDrawArrays(GL_QUADS, 0, 4);

            glUniform1i(uni_do_calc, 0);
            glBindTexture(GL_TEXTURE_2D, tex1);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDrawArrays(GL_QUADS, 0, 4);
        }
        use2 = !use2;
        glerr("after doing a render");
        glfwSwapBuffers();
        glfwSleep(1/60.0);
    }

}
