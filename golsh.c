#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <GL/glew.h>
#include <GL/glfw.h>

int width = 480;
int height = 270;
int window_width = 1920;
int window_height = 1080;
int paused = 0;
int advance_frame = 0;
float fps = 15;
GLuint tex1, tex2;
GLuint fb1, fb2;
int use2 = 0;
int exiting = 0;

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

void randomize () {
    unsigned char data[width*height];
    uint i;
    for (i = 0; i < width*height; i++) {
        data[i] = rand() & 1 ? 0xff : 0x00;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA2, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
}
void clear () {
    unsigned char data[width*height];
    uint i;
    for (i = 0; i < width*height; i++) {
        data[i] = 0;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA2, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
}

int GLFWCALL close_cb () {
    exiting = 1;
    return GL_TRUE;
}
void GLFWCALL key_cb (int code, int action) {
    if (action == GLFW_PRESS) {
        switch (code) {
            case GLFW_KEY_ESC:
                glfwTerminate();
                exit(0);
            case ' ':
                paused = !paused;
                break;
            case '-':
                fps /= 2;
                break;
            case '=':
                fps *= 2;
                break;
            case 'R':
                randomize();
                break;
            case 'C':
                clear();
                break;
            case '.':
                if (paused)
                    advance_frame = 1;
                else
                    paused = 1;
                break;
            default:
                break;
        }
    }
}
void GLFWCALL resize_cb (int w, int h) {
    window_width = w;
    window_height = h;
}

void draw (int x, int y, int val) {
    printf("Drawing at %d, %d\n", x, y);
    unsigned char byte = val ? 0xff : 0x00;
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, &byte);
}

int left_clicking = 0;
int right_clicking = 0;

void GLFWCALL button_cb (int code, int action) {
    if (action == GLFW_PRESS) {
        int x, y;
        glfwGetMousePos(&x, &y);
        x = x * width / window_width;
        y = (window_height - y - 1) * height / window_height;
        if (code == GLFW_MOUSE_BUTTON_LEFT) {
            left_clicking = 1;
            draw(x, y, 1);
        }
        else if (code == GLFW_MOUSE_BUTTON_RIGHT) {
            right_clicking = 1;
            draw(x, y, 0);
        }
    }
    else {
        if (code == GLFW_MOUSE_BUTTON_LEFT) {
            left_clicking = 0;
        }
        else if (code == GLFW_MOUSE_BUTTON_RIGHT) {
            right_clicking = 0;
        }
    }
}
void GLFWCALL motion_cb (int x, int y) {
    if (left_clicking) {
        draw(x * width / window_width, (window_height - y - 1) * height / window_height, 1);
    }
    else if (right_clicking) {
        draw(x * width / window_width, (window_height - y - 1) * height / window_height, 0);
    }
}

enum ReadState {
    WANT_X,
    WANT_X_EQ,
    WANT_X_VAL,
    WANT_X_COMMA,
    WANT_Y,
    WANT_Y_EQ,
    WANT_Y_VAL,
    WANT_Y_COMMA,
    WANT_DATA
};

void read_rle (const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Can't open %s for reading: %s\n", filename, strerror(errno));
        exit(1);
    }
    int state = WANT_X;
    int start_x = 0;
    int start_y = height-1;
    int x;
    int y;
    int comment = 0;
    int c = fgetc(f);
    for (;;) {
        if (c == EOF) {
            fprintf(stderr, "RLE parse error; premature EOF.\n");
            exit(1);
        }
        else if (comment) {
            if (c == '\n') {
                comment = 0;
            }
            else {
                c = fgetc(f);
            }
        }
        else if (isspace(c)) {
            c = fgetc(f);
        }
        else if (c == '#') {
            comment = 1;
            c = fgetc(f);
        }
        else switch (state) {
            case WANT_X: {
                if (c == 'x') {
                    state = WANT_X_EQ;
                    c = fgetc(f);
                }
                else {
                    fprintf(stderr, "RLE parse error; expected x but got %c.\n", c);
                    exit(1);
                }
                break;
            }
            case WANT_X_EQ: {
                if (c == '=') {
                    state = WANT_X_VAL;
                    c = fgetc(f);
                }
                else {
                    fprintf(stderr, "RLE parse error; expected = but got %c.\n", c);
                    exit(1);
                }
                break;
            }
            case WANT_X_VAL: {
                if (isdigit(c)) {
                    x = c - '0';
                    c = fgetc(f);
                    while (isdigit(c)) {
                        x *= 10;
                        x += c - '0';
                        c = fgetc(f);
                    }
                    if (x > width) {
                        fprintf(stderr, "Sorry, this RLE file is too wide for me (%d > %d).\n", x, width);
                        exit(1);
                    }
                    start_x = (width - x) / 2;
                    state = WANT_X_COMMA;
                }
                else {
                    fprintf(stderr, "RLE parse error; expected number but got %c.\n", c);
                    exit(1);
                }
                break;
            }
            case WANT_X_COMMA: {
                if (c == ',') {
                    state = WANT_Y;
                    c = fgetc(f);
                }
                else {
                    fprintf(stderr, "RLE parse error; expected , but got %c.\n", c);
                    exit(1);
                }
                break;
            }
            case WANT_Y: {
                if (c == 'y') {
                    state = WANT_Y_EQ;
                    c = fgetc(f);
                }
                else {
                    fprintf(stderr, "RLE parse error; expected x but got %c.\n", c);
                    exit(1);
                }
                break;
            }
            case WANT_Y_EQ: {
                if (c == '=') {
                    state = WANT_Y_VAL;
                    c = fgetc(f);
                }
                else {
                    fprintf(stderr, "RLE parse error; expected = but got %c.\n", c);
                    exit(1);
                }
                break;
            }
            case WANT_Y_VAL: {
                if (isdigit(c)) {
                    y = c - '0';
                    c = fgetc(f);
                    while (isdigit(c)) {
                        y *= 10;
                        y += c - '0';
                        c = fgetc(f);
                    }
                    if (y > width) {
                        fprintf(stderr, "Sorry, this RLE file is too tall for me (%d > %d).\n", y, height);
                        exit(1);
                    }
                    start_y = (height + y) / 2 - 1;
                    state = WANT_Y_COMMA;
                }
                else {
                    fprintf(stderr, "RLE parse error; expected number but got %c.\n", c);
                    exit(1);
                }
                break;
            }
            case WANT_Y_COMMA: {
                if (c == ',') {
                    comment = 1;
                    state = WANT_DATA;
                    x = start_x;
                    y = start_y;
                    c = fgetc(f);
                }
                else {
                    fprintf(stderr, "RLE parse error; expected , but got %c.\n", c);
                    exit(1);
                }
                break;
            }
            case WANT_DATA: {
                int run_count = 1;
                if (isdigit(c)) {
                    run_count = c - '0';
                    c = fgetc(f);
                    while (isdigit(c)) {
                        run_count *= 10;
                        run_count += c - '0';
                        c = fgetc(f);
                    }
                }
                if (c == 'b' || c == 'o') {
                    char dat [run_count];
                    int i;
                    for (i = 0; i < run_count; i++)
                        dat[i] = c == 'o' ? 0xff : 0x00;
                    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, run_count, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, dat);
                    x += run_count;
                    c = fgetc(f);
                }
                else if (c == '$') {
                    x = start_x;
                    y -= run_count;
                    c = fgetc(f);
                }
                else if (c == '!') {
                    goto done_reading;
                }
                else {
                    fprintf(stderr, "RLE parse error; unrecognized character %c.\n", c);
                    exit(1);
                }
                break;
            }
        }
    }
    done_reading:
    fclose(f);
}

int main (int argc, char** argv) {
    srand(time(0));
    glfwInit();
    glfwOpenWindow(window_width, window_height, 8, 8, 8, 0, 0, 0, GLFW_FULLSCREEN);
    glfwEnable(GLFW_MOUSE_CURSOR);
    glfwDisable(GLFW_AUTO_POLL_EVENTS);
    glfwSetWindowCloseCallback(close_cb);
    glfwSetWindowSizeCallback(resize_cb);
    glfwSetKeyCallback(key_cb);
    glfwSetMouseButtonCallback(button_cb);
    glfwSetMousePosCallback(motion_cb);
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

    glGenTextures(1, &tex1);
    glBindTexture(GL_TEXTURE_2D, tex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA2, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    glerr("after creating tex 1");
    glGenFramebuffers(1, &fb1);
    glBindFramebuffer(GL_FRAMEBUFFER, fb1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);
    glerr("after creating fb 1");
    GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer 1 creation failed: 0x%04X\n", fb_status);
        exit(1);
    }

    glGenTextures(1, &tex2);
    glBindTexture(GL_TEXTURE_2D, tex2);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA2, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    glerr("after creating tex 2");
    glGenFramebuffers(1, &fb2);
    glBindFramebuffer(GL_FRAMEBUFFER, fb2);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glerr("after creating fb 2");
    fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Framebuffer 2 creation failed: 0x%04X\n", fb_status);
        exit(1);
    }

    glBindTexture(GL_TEXTURE_2D, tex1);
    if (argc == 2) {
        read_rle(argv[1]);
    }
    else {
        randomize();
    }

    float verts [8] = { 0, 0,  1, 0,  1, 1,  0, 1 };
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, verts);

    int first_frame = 1;
    while (!exiting) {
        if (!first_frame && (!paused || advance_frame)) {
             // Run a step
            glUniform1i(uni_do_calc, 1);
            glBindTexture(GL_TEXTURE_2D, use2 ? tex2 : tex1);
            glBindFramebuffer(GL_FRAMEBUFFER, use2 ? fb1 : fb2);
            glViewport(0, 0, width, height);
            glDrawArrays(GL_QUADS, 0, 4);
             // Switch buffer
            use2 = !use2;
        }
        first_frame = 0;
        advance_frame = 0;
         // Copy to window
        glUniform1i(uni_do_calc, 0);
        glBindTexture(GL_TEXTURE_2D, use2 ? tex2 : tex1);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, window_width, window_height);
        glDrawArrays(GL_QUADS, 0, 4);
        glerr("after doing a render");
        glfwSwapBuffers();
        if (!paused) {
            glfwSleep(1/fps);
            glfwPollEvents();
        }
        else {
            glfwWaitEvents();
        }
    }
    return 0;
}
