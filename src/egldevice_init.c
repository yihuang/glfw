//========================================================================
// GLFW 3.3 EGLDevice - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "internal.h"

#include <linux/limits.h>

static GLFWbool initExtensions(void)
{
    _glfw.egldev.QueryDevicesEXT = (PFNEGLQUERYDEVICESEXTPROC)
        eglGetProcAddress("eglQueryDevicesEXT");
    _glfw.egldev.QueryDeviceStringEXT = (PFNEGLQUERYDEVICESTRINGEXTPROC)
        eglGetProcAddress("eglQueryDeviceStringEXT");
    _glfw.egldev.GetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC)
        eglGetProcAddress("eglGetPlatformDisplayEXT");
    _glfw.egldev.GetOutputLayersEXT = (PFNEGLGETOUTPUTLAYERSEXTPROC)
        eglGetProcAddress("eglGetOutputLayersEXT");
    _glfw.egldev.CreateStreamKHR = (PFNEGLCREATESTREAMKHRPROC)
        eglGetProcAddress("eglCreateStreamKHR");
    _glfw.egldev.DestroyStreamKHR = (PFNEGLDESTROYSTREAMKHRPROC)
        eglGetProcAddress("eglDestroyStreamKHR");
    _glfw.egldev.StreamConsumerOutputEXT = (PFNEGLSTREAMCONSUMEROUTPUTEXTPROC)
        eglGetProcAddress("eglStreamConsumerOutputEXT");
    _glfw.egldev.CreateStreamProducerSurfaceKHR = (PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC)
        eglGetProcAddress("eglCreateStreamProducerSurfaceKHR");

    if (!_glfw.egldev.QueryDevicesEXT ||
        !_glfw.egldev.QueryDeviceStringEXT ||
        !_glfw.egldev.GetPlatformDisplayEXT ||
        !_glfw.egldev.GetOutputLayersEXT ||
        !_glfw.egldev.CreateStreamKHR ||
        !_glfw.egldev.DestroyStreamKHR ||
        !_glfw.egldev.StreamConsumerOutputEXT ||
        !_glfw.egldev.CreateStreamProducerSurfaceKHR)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Missing required function(s)");
        return GLFW_FALSE;
    }

    return GLFW_TRUE;
}

static EGLDeviceEXT getEGLDevice(void)
{
    int i, deviceCount;
    EGLDeviceEXT* devices, device;
    const char* clientExtensionString;

    device = EGL_NO_DEVICE_EXT;
    clientExtensionString = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

    if (!_glfwStringInExtensionString("EGL_EXT_device_base",
                                      clientExtensionString) &&
        (!_glfwStringInExtensionString("EGL_EXT_device_enumeration",
                                       clientExtensionString) ||
         !_glfwStringInExtensionString("EGL_EXT_device_query",
                                       clientExtensionString)))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: EGL_EXT_device base extensions not found");
    }

    if (!eglQueryDevicesEXT(0, NULL, &deviceCount))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Falied to query EGLDevice");
    }

    if (deviceCount < 1)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: No devices found");
    }

    devices = calloc(deviceCount, sizeof(EGLDeviceEXT));
    if (devices == NULL)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Unable to allocate memory for device storage");
    }

    // Select suitable device
    if (!eglQueryDevicesEXT(deviceCount, devices, &deviceCount))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Failed to query EGL devices");
    }

    for (i = 0;  i <= deviceCount;  i++)
    {
        const char* deviceExtensionString;

        deviceExtensionString =
            eglQueryDeviceStringEXT(devices[i], EGL_EXTENSIONS);
        if (_glfwStringInExtensionString("EGL_EXT_device_drm",
                                         deviceExtensionString))
        {
            device = devices[i];
            break;
        }
    }

    free(devices);

    if (device == EGL_NO_DEVICE_EXT)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Required extension EGL_EXT_device_drm missing");
    }

    return device;
}

static int getDRMFd(EGLDeviceEXT device)
{
    int drm_fd;
    const char* drmName;

    drmName = eglQueryDeviceStringEXT(device, EGL_DRM_DEVICE_FILE_EXT);
    if (!drmName || (strnlen(drmName, PATH_MAX) == 0))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Failed to obtain device file from 0x%p",
                        (void*)(uintptr_t) device);
    }

    drm_fd = drmOpen(drmName, NULL);
    if (drm_fd < 0)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Failed to open device file '%s'", drmName);
    }

    return drm_fd;
}

static GLFWbool initEGLDisplay(EGLDeviceEXT device, int drm_fd)
{
    const char* displayExtensionString;
    EGLint displayAttribs[] = {
        EGL_DRM_MASTER_FD_EXT,
        drm_fd,
        EGL_NONE
    };

    _glfw.egl.display = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT,
                                                 (void*) device,
                                                 displayAttribs);
    if (_glfw.egl.display == EGL_NO_DISPLAY)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Failed to obtain EGLDisplay for device");
        return GLFW_FALSE;
    }

    if (!eglInitialize(_glfw.egl.display, &_glfw.egl.major, &_glfw.egl.minor))
    {
        _glfwInputError(GLFW_API_UNAVAILABLE, "EGL: Failed to initialize EGL");
        return GLFW_FALSE;
    }

    // Check for stream_consumer_egloutput + output_drm support
    displayExtensionString = eglQueryString(_glfw.egl.display, EGL_EXTENSIONS);
    if (!_glfwStringInExtensionString("EGL_EXT_output_base",
                                      displayExtensionString))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Required extension EGL_EXT_output_base missing");
        return GLFW_FALSE;
    }

    if (!_glfwStringInExtensionString("EGL_EXT_output_drm",
                                      displayExtensionString))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Required extension EGL_EXT_output_drm missing");
        return GLFW_FALSE;
    }

    if (!_glfwStringInExtensionString("EGL_KHR_stream",
                                      displayExtensionString))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Required extension EGL_KHR_stream missing");
        return GLFW_FALSE;
    }

    if (!_glfwStringInExtensionString("EGL_KHR_stream_producer_eglsurface",
                                      displayExtensionString))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Required extension EGL_KHR_stream_producer_eglsurface missing");
        return GLFW_FALSE;
    }

    if (!_glfwStringInExtensionString("EGL_EXT_stream_consumer_egloutput",
                                      displayExtensionString))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Required extension EGL_EXT_stream_consumer_egloutput missing");
        return GLFW_FALSE;
    }

    return GLFW_TRUE;
}


//////////////////////////////////////////////////////////////////////////
//////                       GLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

int _glfwPlatformInit(void)
{
    EGLDeviceEXT device;
    int drm_fd;

    if (!_glfwInitThreadLocalStoragePOSIX())
        return GLFW_FALSE;

    // Initialize EGL
    if (!_glfwInitEGL())
        return GLFW_FALSE;

    // Initialize global data and extension function pointers
    if (!initExtensions())
        return GLFW_FALSE;

    // Query and Obtain EGLDevice
    device = getEGLDevice();

    // Obtain and open DRM device file
    drm_fd = getDRMFd(device);

    // Store for use later
    _glfw.egldev.drmFd = drm_fd;

    // Obtain EGLDisplay
    if (!initEGLDisplay(device, drm_fd))
        return GLFW_FALSE;

   _glfwInitTimerPOSIX();

    return GLFW_TRUE;
}

void _glfwPlatformTerminate(void)
{
    _glfwTerminateEGL();
    _glfwTerminateJoysticksLinux();
    _glfwTerminateThreadLocalStoragePOSIX();
}

const char* _glfwPlatformGetVersionString(void)
{
    return _GLFW_VERSION_NUMBER "EGLDevice EGL"
#if defined(_GLFW_BUILD_DLL)
        " shared"
#endif
        ;
}
