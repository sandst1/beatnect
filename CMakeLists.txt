if (BUILD_OPENCV_TRACKING)
	include_directories (${CMAKE_SOURCE_DIR}/include)
	link_directories (${CMAKE_BINARY_DIR}/lib)

	find_package(Threads REQUIRED)
	find_package(OpenGL REQUIRED)
	include_directories(${OpenGL_INCLUDE_DIRS})
	find_package(GLUT REQUIRED)
	include_directories(${GLUT_INCLUDE_DIRS})

	#find_package(LIBUSB_1 REQUIRED)
	#include_directories(${libusb-1.0_INCLUDE_DIRS})

	find_library (PTHREAD pthread)

	find_package( OpenCV REQUIRED )
	include_directories(${OpenCV_INCLUDE_DIRS})

	add_executable( opencv_tracking opencv_tracking.c hist_segment.c feature_extract.c optflow.c)
        target_link_libraries(opencv_tracking freenect ${OpenGL_LIBRARIES} ${GLUT_LIBRARY} ${OpenCV_LIBS} m)
        install (TARGETS opencv_tracking
                 DESTINATION bin)

        add_executable( opencv_template opencv_template.c )
        target_link_libraries(opencv_template freenect ${OpenGL_LIBRARIES} ${GLUT_LIBRARY} ${OpenCV_LIBS} m)
        install (TARGETS opencv_template
                 DESTINATION bin)

endif (BUILD_OPENCV_TRACKING)
