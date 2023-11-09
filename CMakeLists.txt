cmake_minimum_required(VERSION 3.26)

project(fxpo C)

set(CMAKE_C_STANDARD 17)

set(TARGET_SOURCES
    main.c
    fxpo_common.h
    fxpo_alloc.h
    fxpo_log.h
    fxpo_log.c
    fxpo_http.h
    fxpo_http.c
    fxpo_jpeg.h
    fxpo_jpeg.c
    fxpo_ortho.h
    fxpo_ortho.c
    fxpo_nvtt3.h
    fxpo_nvtt3.c
    stb/stb_image_resize2.h
)
list(TRANSFORM TARGET_SOURCES PREPEND src/)
add_executable(fxpo ${TARGET_SOURCES})

find_package(OpenMP REQUIRED)
target_link_libraries(fxpo PRIVATE OpenMP::OpenMP_C)

find_package(CURL REQUIRED)
target_link_libraries(fxpo PRIVATE CURL::libcurl)

find_package(libjpeg-turbo CONFIG REQUIRED)
target_link_libraries(fxpo PRIVATE $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>)

set(NVTT3_ROOT "$ENV{ProgramFiles}/NVIDIA Corporation/NVIDIA Texture Tools")
set(NVTT3_INCLUDE_DIR ${NVTT3_ROOT}/include)
set(NVTT3_LIB_PATH ${NVTT3_ROOT}/lib/x64-v142/nvtt30204.lib)
set(NVTT3_DLL_PATH ${NVTT3_ROOT}/nvtt30204.dll)
set(CUDA_DLL_PATH "$ENV{ProgramFiles}/NVIDIA GPU Computing Toolkit/CUDA/v11.8/bin/cudart64_110.dll")
target_include_directories(fxpo PRIVATE ${NVTT3_INCLUDE_DIR})
target_link_libraries(fxpo PRIVATE "${NVTT3_LIB_PATH}")
add_custom_command(TARGET fxpo POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${NVTT3_DLL_PATH}" $(TargetDir))
add_custom_command(TARGET fxpo POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CUDA_DLL_PATH}" $(TargetDir))

if(MSVC)
    # These CRT features cause all sorts of compatibility issues (e.g. localtime_s C11 definition differs from CRT).
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    target_compile_options(fxpo PUBLIC /arch:AVX2 /ZI /W4 /WX)
    target_link_options(fxpo PUBLIC /INCREMENTAL)
endif()

MESSAGE(STATUS "Using vcpkg toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
MESSAGE(STATUS "Using release flags: ${CMAKE_C_FLAGS_RELEASE}")