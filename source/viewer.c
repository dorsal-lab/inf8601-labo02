/* DO NOT EDIT THIS FILE */

#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include <GL/glew.h>
#include <GL/glut.h>
#include <GL/glxew.h>

#include <GL/freeglut_ext.h>

#include "log.h"
#include "sinoscope.h"
#include "viewer.h"

typedef struct timespec timespec_t;

typedef struct viewer {
    unsigned int width;
    unsigned int height;
    unsigned int window_id;

    sinoscope_t* sinoscope;
    GLuint texture;
    bool enabled;

    long fps_count;
    timespec_t fps_start;
} viewer_t;

static viewer_t* viewer                = NULL;
static const unsigned int fps_delay_ms = 1000;

int viewer_init(sinoscope_t* sinoscope) {
    if (viewer != NULL) {
        LOG_ERROR("viewer has already been initialised");
        goto fail_exit;
    }

    viewer = malloc(sizeof(*viewer));
    if (viewer == NULL) {
        LOG_ERROR_ERRNO("malloc");
        goto fail_exit;
    }

    viewer->width     = sinoscope->width;
    viewer->height    = sinoscope->height;
    viewer->window_id = 0;

    viewer->sinoscope = sinoscope;
    viewer->texture   = 0;
    viewer->enabled   = true;

    viewer->fps_count = 0;

    if (clock_gettime(CLOCK_MONOTONIC, &viewer->fps_start) < 0) {
        LOG_ERROR_ERRNO("clock_gettime");
        goto fail_free_viewer;
    }

    return 0;

fail_free_viewer:
    free(viewer);
    viewer = NULL;
fail_exit:
    return -1;
}

void viewer_destroy() {
    if (viewer != NULL) {
        free(viewer);
        viewer = NULL;
    }
}

static int pre_display() {
    if (viewer == NULL) {
        LOG_ERROR("viewer has not been initialised");
        goto fail_exit;
    }

    glViewport(0, 0, viewer->width, viewer->height);
    if (LOG_ERROR_OPENGL("glViewport") < 0) {
        goto fail_exit;
    }

    glMatrixMode(GL_PROJECTION);
    if (LOG_ERROR_OPENGL("glMatrixMode") < 0) {
        goto fail_exit;
    }

    glLoadIdentity();
    if (LOG_ERROR_OPENGL("glLoadIdentity") < 0) {
        goto fail_exit;
    }

    gluOrtho2D(0.0, 1.0, 0.0, 1.0);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    if (LOG_ERROR_OPENGL("glClearColor") < 0) {
        goto fail_exit;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    if (LOG_ERROR_OPENGL("glClear") < 0) {
        goto fail_exit;
    }

    return 0;

fail_exit:
    return -1;
}

static int display() {
    if (viewer == NULL) {
        LOG_ERROR("viewer has not been initialised");
        goto fail_exit;
    }

    if (viewer->sinoscope == NULL) {
        LOG_ERROR_NULL_PTR();
        goto fail_exit;
    }

    if (sinoscope_corners(viewer->sinoscope) < 0) {
        LOG_ERROR("failed to forward sinoscope");
        goto fail_exit;
    }

    if (viewer->sinoscope->handler(viewer->sinoscope) < 0) {
        LOG_ERROR("failed to call sinoscope handler `%s`\n", viewer->sinoscope->name);
        goto fail_exit;
    }

    if (viewer->texture == 0) {
        glGenTextures(1, &viewer->texture);
        if (LOG_ERROR_OPENGL("glGenTextures") < 0) {
            goto fail_exit;
        }
    }

    if (viewer->enabled) {
        glBindTexture(GL_TEXTURE_2D, viewer->texture);
        if (LOG_ERROR_OPENGL("glBindTexture") < 0) {
            goto fail_exit;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, 3, viewer->sinoscope->width, viewer->sinoscope->height, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, viewer->sinoscope->buffer);
        if (LOG_ERROR_OPENGL("glTexImage2D") < 0) {
            goto fail_exit;
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        if (LOG_ERROR_OPENGL("glTexParameteri") < 0) {
            goto fail_exit;
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        if (LOG_ERROR_OPENGL("glTexParameteri") < 0) {
            goto fail_exit;
        }

        glEnable(GL_TEXTURE_2D);
        if (LOG_ERROR_OPENGL("glEnable") < 0) {
            goto fail_exit;
        }

        glBegin(GL_QUADS);
        glTexCoord2d(0.0, 0.0);
        glVertex2d(0.0, 0.0);
        glTexCoord2d(1.0, 0.0);
        glVertex2d(1.0, 0.0);
        glTexCoord2d(1.0, 1.0);
        glVertex2d(1.0, 1.0);
        glTexCoord2d(0.0, 1.0);
        glVertex2d(0.0, 1.0);
        glEnd();

        if (LOG_ERROR_OPENGL("glBegin / glEnd") < 0) {
            goto fail_exit;
        }
    }

    viewer->fps_count++;

    return 0;

fail_exit:
    return -1;
}

static inline void post_display() {
    glutSwapBuffers();
}

static void callback_display() {
    if (pre_display() < 0) {
        LOG_ERROR("pre-display failed");
    }

    if (display() < 0) {
        LOG_ERROR("display failed");
    }

    post_display();
}

static void callback_idle() {
    glutSetWindow(viewer->window_id);
    glutPostRedisplay();
}

void callback_timer_fps(int value) {
    (void)value;

    if (viewer == NULL) {
        LOG_ERROR("viewer has not been initialised");
        return;
    }

    timespec_t fps_end;

    if (clock_gettime(CLOCK_MONOTONIC, &fps_end) < 0) {
        LOG_ERROR_ERRNO("clock_gettime");
        return;
    }

    float start_sec = viewer->fps_start.tv_sec + viewer->fps_start.tv_nsec / 1e9;
    float end_sec   = fps_end.tv_sec + fps_end.tv_nsec / 1e9;
    float diff_sec  = end_sec - start_sec;

    printf("FPS: %2.2f\n", (float)viewer->fps_count / diff_sec);
    viewer->fps_count = 0;
    viewer->fps_start = fps_end;

    glutTimerFunc(fps_delay_ms, callback_timer_fps, 0);
}

void callback_keyboard(unsigned char key, int x, int y) {
    if (viewer == NULL) {
        LOG_ERROR("viewer has not been initialised");
        return;
    }

    switch (key) {
    case 'q':
        printf("Closing application\n");
        glutLeaveMainLoop();
        break;
    case '1':
        printf("Selected serial implementation\n");
        viewer->sinoscope->name    = "serial";
        viewer->sinoscope->handler = sinoscope_image_serial;
        break;
    case '2':
        printf("Selected openmp implementation\n");
        viewer->sinoscope->name    = "openmp";
        viewer->sinoscope->handler = sinoscope_image_openmp;
        break;
    case '3':
        printf("Selected opencl implementation\n");
        viewer->sinoscope->name    = "opencl";
        viewer->sinoscope->handler = sinoscope_image_opencl;
        break;
    case ' ':
        printf("Rendering %s\n", viewer->enabled ? "disabled" : "enabled");
        viewer->enabled = !viewer->enabled;
        break;
    case '+':
        viewer->sinoscope->taylor++;
        printf("Increased taylor polynomial to %d\n", viewer->sinoscope->taylor);
        break;
    case '-':
        if (viewer->sinoscope->taylor - 1 > 0) {
            viewer->sinoscope->taylor--;
            printf("Decreased taylor polynomial to %d\n", viewer->sinoscope->taylor);
        } else {
            printf("Cannot set taylor polynomial to zero\n");
        }

        break;
    default:
        break;
    }
}

static void callback_reshape(int width, int height) {
    glutSetWindow(viewer->window_id);
    glutReshapeWindow(width, height);

    viewer->width  = width;
    viewer->height = height;
}

int viewer_open() {
    if (viewer == NULL) {
        LOG_ERROR("viewer has not been initialised");
        goto fail_exit;
    }

    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);

    int x_pos = (glutGet(GLUT_SCREEN_WIDTH) - viewer->width) / 2;
    int y_pos = (glutGet(GLUT_SCREEN_HEIGHT) - viewer->height) / 2;

    glutInitWindowPosition(x_pos, y_pos);
    glutInitWindowSize(viewer->width, viewer->height);
    viewer->window_id = glutCreateWindow("sinoscope");

    glutDisplayFunc(callback_display);
    glutIdleFunc(callback_idle);
    glutTimerFunc(fps_delay_ms, callback_timer_fps, 0);
    glutKeyboardFunc(callback_keyboard);
    glutReshapeFunc(callback_reshape);

    glewInit();
    glXSwapIntervalMESA(0);
    glutMainLoop();

    return 0;

fail_exit:
    return -1;
}