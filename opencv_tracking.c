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

#include <stdio.h>
#include <string.h>
#include "libfreenect.h"

#include <pthread.h>
#include <math.h>

#include <cv.h>
#include <highgui.h>

#include "hist_segment.h"
#include "feature_extract.h"
#include "optflow.h"

volatile int die = 0;

int g_argc;
char **g_argv;

int window;

freenect_device *f_dev;
int freenect_angle = 0;
int freenect_led;


pthread_cond_t gl_frame_cond = PTHREAD_COND_INITIALIZER;
int got_frames = 0;

/* OpenCV stuff */
CvMat *cv_depth_mat;//(Size(640,480), CV_16UC1);
CvMat *cv_rgb_mat;//(Size(640,480), CV_8UC3, Scalar(0));

pthread_t freenect_thread;
freenect_context *f_ctx;

pthread_mutex_t buf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t frame_cond = PTHREAD_COND_INITIALIZER;

uint16_t t_gamma[2048];

void depth_cb(freenect_device *dev, void *depth, uint32_t timestamp)
{
    pthread_mutex_lock(&buf_mutex);

    // Get the depth data to cv_depth_mat
    memcpy(cv_depth_mat->data.s, (short*)depth, sizeof(short)*FREENECT_IR_FRAME_PIX);

	got_frames++;
    pthread_cond_signal(&frame_cond);
    pthread_mutex_unlock(&buf_mutex);
}

void rgb_cb(freenect_device *dev, void *rgb, uint32_t timestamp)
{

    pthread_mutex_lock(&buf_mutex);

    memcpy(cv_rgb_mat->data.ptr, rgb, FREENECT_VIDEO_RGB_SIZE);

	got_frames++;
    pthread_cond_signal(&frame_cond);
    pthread_mutex_unlock(&buf_mutex);
}

void *freenect_threadfunc(void *arg)
{
    printf("Freenect threadfunc\n");
    while( !die && freenect_process_events(f_ctx) >= 0 ) {}

    printf("Freenect thread exit");
    return NULL;
}


int main(int argc, char **argv)
{

    cv_depth_mat = cvCreateMat(480, 640, CV_16UC1);
    cv_rgb_mat = cvCreateMat(480, 640, CV_8UC3);

    int res;
	g_argc = argc;
	g_argv = argv;

	if (freenect_init(&f_ctx, NULL) < 0) {
		printf("freenect_init() failed\n");
		return 1;
	}

	freenect_set_log_level(f_ctx, FREENECT_LOG_INFO);

	int nr_devices = freenect_num_devices (f_ctx);
	printf ("Number of devices found: %d\n", nr_devices);

	int user_device_number = 0;
	if (argc > 1)
		user_device_number = atoi(argv[1]);

	if (nr_devices < 1)
		return 1;

	if (freenect_open_device(f_ctx, &f_dev, user_device_number) < 0) {
		printf("Could not open device\n");
		return 1;
	}

	//freenect_set_tilt_degs(f_dev,15);
	freenect_set_tilt_degs(f_dev,0);
	freenect_set_led(f_dev,LED_RED);
	freenect_set_depth_callback(f_dev, depth_cb);
	freenect_set_video_callback(f_dev, rgb_cb);
	freenect_set_video_format(f_dev, FREENECT_VIDEO_RGB);
	freenect_set_depth_format(f_dev, FREENECT_DEPTH_11BIT);

	freenect_start_depth(f_dev);
	freenect_start_video(f_dev);

    cvNamedWindow("rgb", CV_WINDOW_NORMAL);
    cvNamedWindow("depth", CV_WINDOW_NORMAL);
    cvNamedWindow("depth_th", CV_WINDOW_NORMAL);
    cvNamedWindow("contourWin", CV_WINDOW_NORMAL);

    CvMat* cv_depth_threshold_mat = cvCreateMat(480,640, CV_8UC1);

	res = pthread_create(&freenect_thread, NULL, freenect_threadfunc, NULL);
	if (res) {
		printf("pthread_create failed\n");
		return 1;
	}

    // Variables for contour finding
    CvSeq* contours = NULL;
    CvMemStorage* memStorage = cvCreateMemStorage(0);
    IplImage* contour_image = cvCreateImage(cvSize(640,480), 8, 1);


    //hist_segment_init();
    //feature_extract_init();

    optflow_init(cvSize(640,480));

	while (!die)
	{
        cvConvertScale(cv_depth_mat, cv_depth_threshold_mat, 255.0/2048.0, 0);

        // 120 ~ 2.7 m
        cvThreshold( cv_depth_threshold_mat, cv_depth_threshold_mat, 120.0, 120.0, CV_THRESH_TOZERO_INV);

        //cvThreshold( cv_depth_threshold_mat, cv_depth_threshold_mat, 120.0, 255.0, CV_THRESH_BINARY_INV);

        // Find contours
        /*cvClearMemStorage(memStorage);
        cvFindContours( cv_depth_threshold_mat, memStorage, &contours, sizeof(CvContour), CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cvPoint(0,0));
        cvZero(contour_image);

        // Draw the contours
        if (contours)
        {
            cvDrawContours(contour_image, contours, cvScalarAll(255.0), cvScalarAll(255.0), 1, 1, 8, cvPoint(0,0));
        }
        cvShowImage("contourWin",contour_image);*/

        //extractFeatures(cv_depth_threshold_mat);
        //hist_segment(cv_depth_threshold_mat, NULL);
        optflow_calculate(cv_depth_threshold_mat, NULL);


        cvShowImage("rgb", cv_rgb_mat);

		cvShowImage("depth_th", cv_depth_threshold_mat);


        char k = cvWaitKey(5);
        if( k == 27 ) break;

    }

    optflow_deinit();
    //hist_segment_deinit();
    //feature_extract_deinit();


	printf("-- done!\n");

	cvDestroyWindow("rgb");
	cvDestroyWindow("depth");
	cvDestroyWindow("depth_th");

    cvReleaseMat(&cv_depth_mat);
    cvReleaseMat(&cv_rgb_mat);

    cvReleaseMat(&cv_depth_threshold_mat);

    // Release Contour variables
    cvDestroyWindow("contourWin");

	pthread_join(freenect_thread, NULL);
	pthread_exit(NULL);
}
