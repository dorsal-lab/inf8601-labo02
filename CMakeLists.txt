cmake_minimum_required(VERSION 3.13.5)
project(tp2 LANGUAGES C)

set(CMAKE_C_FLAGS "${CMAKE_CFLAGS} -g -Wall")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_CFLAGS_RELEASE} -O2")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer")
set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fsanitize=address")

include_directories(include)
add_executable(sinoscope)
target_link_libraries(sinoscope -lm -lpthread)
target_sources(sinoscope PUBLIC
    source/color.c
    source/headless.c
    source/image.c
    source/main.c
    source/opencl.c
    source/sinoscope.c
    source/sinoscope-serial.c
    source/sinoscope-opencl.c
    source/sinoscope-openmp.c
    source/viewer.c
)

include(FindOpenCL)
if(OpenCL_FOUND)
    target_link_libraries(sinoscope ${OpenCL_LIBRARY})
    target_include_directories(sinoscope PUBLIC ${OpenCL_INCLUDE_DIR})
else()
    # Un gros hack de cave parce que les ordinateurs du L4712 n'ont
    # pas une installation de OpenCL utilisant des dossiers standards.
    # Probablement aussi de la faute de CMake. TODO: utiliser un bon build system.
    if(EXISTS /usr/lib64/libOpenCL.so.1)
        target_link_libraries(sinoscope /usr/lib64/libOpenCL.so.1)
    else()
        message(FATAL_ERROR "opencl is required for building the application")
    endif()
endif()

include(FindOpenMP)
if(OpenMP_C_FOUND)
    target_link_libraries(sinoscope ${OpenMP_C_LIBRARIES})
else()
    message(FATAL_ERROR "openmp is required for building the application")
endif()

include(FindOpenGL)
if(OPENGL_FOUND)
    if(NOT OPENGL_GLU_FOUND)
        message(FATAL_ERROR "glu is required for building the application")
    endif()

    target_link_libraries(sinoscope ${OPENGL_LIBRARIES})
    target_include_directories(sinoscope PUBLIC ${OPENGL_INCLUDE_DIR})
else()
    message(FATAL_ERROR "opengl is required for building the application")
endif()

include(FindGLUT)
if(GLUT_FOUND)
    target_link_libraries(sinoscope ${GLUT_LIBRARIES})
    target_include_directories(sinoscope PUBLIC ${GLUT_INCLUDE_DIR})
else()
    message(FATAL_ERROR "glut is required for building the application")
endif()

include(FindGLEW)
if(GLEW_FOUND)
    target_link_libraries(sinoscope ${GLEW_LIBRARIES})
    target_include_directories(sinoscope PUBLIC ${GLEW_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "glew is required for building the application")
endif()

include(FindPNG)
if(PNG_FOUND)
    target_link_libraries(sinoscope ${PNG_LIBRARIES})
    target_include_directories(sinoscope PUBLIC ${PNG_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "libpng is required for building the application")
endif()

set_source_files_properties(source/sinoscope-openmp.c PROPERTIES COMPILE_FLAGS -fopenmp)
add_definitions(-D__KERNEL_FILE__="${PROJECT_SOURCE_DIR}/source/kernel/sinoscope.cl")
add_definitions(-D__OPENCL_INCLUDE__="${PROJECT_SOURCE_DIR}/source/kernel")
add_definitions(-DCL_TARGET_OPENCL_VERSION=220)

add_custom_target(format
    COMMAND clang-format -i --verbose `find source -type f -iname '*.cl'`
    COMMAND clang-format -i --verbose `find source -type f -iname '*.c'` `find include -type f -iname '*.h'`
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

add_custom_target(remise
    COMMAND zip --must-match -r ${CMAKE_CURRENT_BINARY_DIR}/remise.zip
            source/*.c source/kernel/*.cl include/*.h CMakeLists.txt .clang-format README.pdf
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
add_dependencies(remise sinoscope)

add_custom_target(handout
    COMMAND asciidoctor-pdf README.asc
    COMMAND zip --must-match -r ${CMAKE_CURRENT_BINARY_DIR}/handout.zip
            source/*.c source/kernel/*.cl include/*.h CMakeLists.txt .clang-format .gitignore README.pdf
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)
add_dependencies(handout sinoscope)

add_custom_target(check
    COMMAND ./sinoscope --check
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(check sinoscope)
