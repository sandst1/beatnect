/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */


#include <QDebug>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <pthread.h>

#if defined(__APPLE__)
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <math.h>

#include <libfreenect/libfreenect.h>

#include <pthread.h>
#include <math.h>

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <QtGui>
#include <QtOpenGL>

#include <math.h>

#include "glwidget.h"


#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE  0x809D
#endif



pthread_t freenect_thread;
volatile int die = 0;

int g_argc;
char **g_argv;

int window;

pthread_mutex_t gl_backbuf_mutex = PTHREAD_MUTEX_INITIALIZER;

// back: owned by libfreenect (implicit for depth)
// mid: owned by callbacks, "latest frame ready"
// front: owned by GL, "currently being drawn"
uint8_t *depth_mid, *depth_front;
uint8_t *rgb_back, *rgb_mid, *rgb_front;

GLuint gl_depth_tex;
GLuint gl_rgb_tex;

freenect_context *f_ctx;
freenect_device *f_dev;
int freenect_angle = 0;
int freenect_led;

freenect_video_format requested_format = FREENECT_VIDEO_RGB;
freenect_video_format current_format = FREENECT_VIDEO_RGB;

pthread_cond_t gl_frame_cond = PTHREAD_COND_INITIALIZER;
int got_rgb = 0;
int got_depth = 0;



GLWidget::GLWidget(QWidget *parent)
    : QGLWidget(QGLFormat(QGL::SampleBuffers), parent),
      timer(NULL)
{
    this->setFixedSize(1280, 480);

    timer = new QTimer(this);
    timer->setInterval(33);
    connect(timer, SIGNAL(timeout()), this, SLOT(repaint()));
}

GLWidget::~GLWidget()
{
}

QSize GLWidget::minimumSizeHint() const
{
    return QSize(1280, 480);
}

QSize GLWidget::sizeHint() const
{
    return QSize(1280, 400);
}

void GLWidget::initializeGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glShadeModel(GL_FLAT);

    glGenTextures(1, &gl_depth_tex);
    glBindTexture(GL_TEXTURE_2D, gl_depth_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &gl_rgb_tex);
    glBindTexture(GL_TEXTURE_2D, gl_rgb_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void GLWidget::paintGL()
{
    qDebug() << __PRETTY_FUNCTION__;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    pthread_mutex_lock(&gl_backbuf_mutex);

    // When using YUV_RGB mode, RGB frames only arrive at 15Hz, so we shouldn't force them to draw in lock-step.
    // However, this is CPU/GPU intensive when we are receiving frames in lockstep.
    if (current_format == FREENECT_VIDEO_YUV_RGB) {
            while (!got_depth && !got_rgb) {
                    pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
            }
    } else {
            while ((!got_depth || !got_rgb) && requested_format != current_format) {
                    pthread_cond_wait(&gl_frame_cond, &gl_backbuf_mutex);
            }
    }

    if (requested_format != current_format) {
            pthread_mutex_unlock(&gl_backbuf_mutex);
            return;
    }

    uint8_t *tmp;

    if (got_depth) {
            tmp = depth_front;
            depth_front = depth_mid;
            depth_mid = tmp;
            got_depth = 0;
    }
    if (got_rgb) {
            tmp = rgb_front;
            rgb_front = rgb_mid;
            rgb_mid = tmp;
            got_rgb = 0;
    }

    pthread_mutex_unlock(&gl_backbuf_mutex);

    glBindTexture(GL_TEXTURE_2D, gl_depth_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, 640, 480, 0, GL_RGB, GL_UNSIGNED_BYTE, depth_front);

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0, 0); glVertex3f(0,0,0);
    glTexCoord2f(1, 0); glVertex3f(640,0,0);
    glTexCoord2f(1, 1); glVertex3f(640,480,0);
    glTexCoord2f(0, 1); glVertex3f(0,480,0);
    glEnd();

    glBindTexture(GL_TEXTURE_2D, gl_rgb_tex);
    if (current_format == FREENECT_VIDEO_RGB || current_format == FREENECT_VIDEO_YUV_RGB)
            glTexImage2D(GL_TEXTURE_2D, 0, 3, 640, 480, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb_front);
    else
            glTexImage2D(GL_TEXTURE_2D, 0, 1, 640, 480, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, rgb_front+640*4);

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glTexCoord2f(0, 0); glVertex3f(640,0,0);
    glTexCoord2f(1, 0); glVertex3f(1280,0,0);
    glTexCoord2f(1, 1); glVertex3f(1280,480,0);
    glTexCoord2f(0, 1); glVertex3f(640,480,0);
    glEnd();

    qDebug() << __PRETTY_FUNCTION__ << "out";

}

void GLWidget::resizeGL(int width, int height)
{
    glViewport(0,0,width,height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho (0, 1280, 480, 0, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

uint16_t t_gamma[2048];

void depth_cb(freenect_device *dev, void *v_depth, uint32_t timestamp)
{
    qDebug() << __PRETTY_FUNCTION__;
        int i;
        uint16_t *depth = (uint16_t*)v_depth;

        pthread_mutex_lock(&gl_backbuf_mutex);
        for (i=0; i<640*480; i++) {
                int pval = t_gamma[depth[i]];
                int lb = pval & 0xff;
                switch (pval>>8) {
                        case 0:
                                depth_mid[3*i+0] = 255;
                                depth_mid[3*i+1] = 255-lb;
                                depth_mid[3*i+2] = 255-lb;
                                break;
                        case 1:
                                depth_mid[3*i+0] = 255;
                                depth_mid[3*i+1] = lb;
                                depth_mid[3*i+2] = 0;
                                break;
                        case 2:
                                depth_mid[3*i+0] = 255-lb;
                                depth_mid[3*i+1] = 255;
                                depth_mid[3*i+2] = 0;
                                break;
                        case 3:
                                depth_mid[3*i+0] = 0;
                                depth_mid[3*i+1] = 255;
                                depth_mid[3*i+2] = lb;
                                break;
                        case 4:
                                depth_mid[3*i+0] = 0;
                                depth_mid[3*i+1] = 255-lb;
                                depth_mid[3*i+2] = 255;
                                break;
                        case 5:
                                depth_mid[3*i+0] = 0;
                                depth_mid[3*i+1] = 0;
                                depth_mid[3*i+2] = 255-lb;
                                break;
                        default:
                                depth_mid[3*i+0] = 0;
                                depth_mid[3*i+1] = 0;
                                depth_mid[3*i+2] = 0;
                                break;
                }
        }
        got_depth++;
        pthread_cond_signal(&gl_frame_cond);
        pthread_mutex_unlock(&gl_backbuf_mutex);
}

void rgb_cb(freenect_device *dev, void *rgb, uint32_t timestamp)
{
    qDebug() << __PRETTY_FUNCTION__;
        pthread_mutex_lock(&gl_backbuf_mutex);

        // swap buffers
        assert (rgb_back == rgb);
        rgb_back = rgb_mid;
        freenect_set_video_buffer(dev, rgb_back);
        rgb_mid = (uint8_t*)rgb;

        got_rgb++;
        pthread_cond_signal(&gl_frame_cond);
        pthread_mutex_unlock(&gl_backbuf_mutex);
}

void *freenect_threadfunc(void *arg)
{
        int accelCount = 0;

        freenect_set_tilt_degs(f_dev,freenect_angle);
        freenect_set_led(f_dev,LED_RED);
        freenect_set_depth_callback(f_dev, depth_cb);
        freenect_set_video_callback(f_dev, rgb_cb);
        freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, current_format));
        freenect_set_depth_mode(f_dev, freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT));
        freenect_set_video_buffer(f_dev, rgb_back);

        freenect_start_depth(f_dev);
        freenect_start_video(f_dev);

        qDebug("'w'-tilt up, 's'-level, 'x'-tilt down, '0'-'6'-select LED mode, 'f'-video format\n");

        while (!die && freenect_process_events(f_ctx) >= 0) {
                //Throttle the text output
                if (accelCount++ >= 2000)
                {
                        accelCount = 0;
                        freenect_raw_tilt_state* state;
                        freenect_update_tilt_state(f_dev);
                        state = freenect_get_tilt_state(f_dev);
                        double dx,dy,dz;
                        freenect_get_mks_accel(state, &dx, &dy, &dz);
                        qDebug("\r raw acceleration: %4d %4d %4d  mks acceleration: %4f %4f %4f", state->accelerometer_x, state->accelerometer_y, state->accelerometer_z, dx, dy, dz);
                        fflush(stdout);
                }

                if (requested_format != current_format) {
                        freenect_stop_video(f_dev);
                        freenect_set_video_mode(f_dev, freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, requested_format));
                        freenect_start_video(f_dev);
                        current_format = requested_format;
                }
        }

        qDebug("\nshutting down streams...\n");

        freenect_stop_depth(f_dev);
        freenect_stop_video(f_dev);

        freenect_close_device(f_dev);
        freenect_shutdown(f_ctx);

        qDebug("-- done!\n");
        return NULL;
}


void GLWidget::start()
{
    int res;

    depth_mid = (uint8_t*)malloc(640*480*3);
    depth_front = (uint8_t*)malloc(640*480*3);
    rgb_back = (uint8_t*)malloc(640*480*3);
    rgb_mid = (uint8_t*)malloc(640*480*3);
    rgb_front = (uint8_t*)malloc(640*480*3);

    qDebug("Kinect camera test\n");

    int i;
    for (i=0; i<2048; i++) {
            float v = i/2048.0;
            v = powf(v, 3)* 6;
            t_gamma[i] = v*6*256;
    }

    if (freenect_init(&f_ctx, NULL) < 0) {
            qDebug("freenect_init() failed\n");
            return;
    }

    qDebug() << "Freenect init called";

    freenect_set_log_level(f_ctx, FREENECT_LOG_DEBUG);
    freenect_select_subdevices(f_ctx, (freenect_device_flags)(FREENECT_DEVICE_MOTOR | FREENECT_DEVICE_CAMERA));

    int nr_devices = freenect_num_devices (f_ctx);
    qDebug ("Number of devices found: %d\n", nr_devices);

    int user_device_number = 0;
    //if (argc > 1)
    //        user_device_number = atoi(argv[1]);

    if (nr_devices < 1)
            return;

    if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0) {
            qDebug("Could not open device\n");
            return;
    }

    res = pthread_create(&freenect_thread, NULL, freenect_threadfunc, NULL);
    if (res) {
            qDebug("pthread_create failed\n");
            return;
    }

    // OS X requires GLUT to run on the main thread
    //gl_threadfunc(NULL);

    //return 0;

    timer->start();

}


