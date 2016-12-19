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

//////////////////////////////////////////////////////////////////////////
//////                       GLFW platform API                      //////
//////////////////////////////////////////////////////////////////////////

int _glfwPlatformCreateWindow(_GLFWwindow* window,
                              const _GLFWwndconfig* wndconfig,
                              const _GLFWctxconfig* ctxconfig,
                              const _GLFWfbconfig* fbconfig)
{
    int count;
    _GLFWmonitor* monitor;
    EGLAttrib layerAttribs[] = { EGL_NONE, EGL_NONE, EGL_NONE };
    EGLint streamAttribs[] = { EGL_STREAM_FIFO_LENGTH_KHR,
                             window->egldev.fifo, EGL_NONE };
    EGLint surfaceAttribs[] = { EGL_WIDTH, 0, EGL_HEIGHT, 0, EGL_NONE };

    if (window->monitor)
        monitor = window->monitor;
    else
        monitor = _glfw.monitors[0];

    window->egldev.width  = wndconfig->width;
    window->egldev.height = wndconfig->height;

    if (!_glfwCreateContextEGL(window, ctxconfig, fbconfig))
        return GLFW_FALSE;

    // Get the layer for this crtc/plane
    layerAttribs[0] = EGL_DRM_CRTC_EXT;
    layerAttribs[1] = (EGLAttrib) monitor->egldev.crtcId;

    if (!eglGetOutputLayersEXT(_glfw.egl.display, layerAttribs,
                               &window->egldev.layer,
                               1, &count) || !count)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Unable to obtain EGLOutputLayer");
        return GLFW_FALSE;
    }

    // Create a stream and connect to the output
    window->egldev.stream = eglCreateStreamKHR(_glfw.egl.display, streamAttribs);
    if (window->egldev.stream == EGL_NO_STREAM_KHR)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Unable to create stream (error 0x%x)",
                        eglGetError());
        return GLFW_FALSE;
    }

    if (!eglStreamConsumerOutputEXT(_glfw.egl.display,
                                    window->egldev.stream,
                                    window->egldev.layer))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Unable to connect stream (error 0x%x)",
                        eglGetError());
        return GLFW_FALSE;
    }

    // Create a surface to feed the stream
    surfaceAttribs[1] = window->egldev.width;
    surfaceAttribs[3] = window->egldev.height;
    window->context.egl.surface =
        eglCreateStreamProducerSurfaceKHR(_glfw.egl.display,
                                          window->context.egl.config,
                                          window->egldev.stream,
                                          surfaceAttribs);
    if (window->context.egl.surface == EGL_NO_SURFACE)
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Unable to create rendering surface (error 0x%x)", eglGetError());
        return GLFW_FALSE;
    }

    return GLFW_TRUE;
}

void _glfwPlatformDestroyWindow(_GLFWwindow* window)
{
    if (window->context.client != GLFW_NO_API)
        window->context.destroy(window);

    if (window->egldev.stream != EGL_NO_STREAM_KHR)
    {
        eglDestroyStreamKHR(_glfw.egl.display,
                            window->egldev.stream);
    }
}

void _glfwPlatformSetWindowTitle(_GLFWwindow* window, const char* title)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformSetWindowTitle is not supported");
}

void _glfwPlatformSetWindowIcon(_GLFWwindow* window,
                                int count, const GLFWimage* images)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformSetWindowIcon is not supported");
}

void _glfwPlatformGetWindowPos(_GLFWwindow* window, int* xpos, int* ypos)
{
    if (xpos)
        *xpos = window->egldev.xoffset;
    if (ypos)
        *ypos = window->egldev.yoffset;
}

void _glfwPlatformSetWindowPos(_GLFWwindow* window, int xpos, int ypos)
{
    window->egldev.xoffset = xpos;
    window->egldev.yoffset = ypos;

    drmModeRes* res_info;
    _GLFWmonitor* monitor;

    if (window->monitor)
        monitor = window->monitor;
    else
        monitor = _glfw.monitors[0];

    res_info = drmModeGetResources(_glfw.egldev.drmFd);
    if (drmModeSetCrtc(_glfw.egldev.drmFd,
                       res_info->crtcs[monitor->egldev.crtcIndex],
                       -1, window->egldev.xoffset, window->egldev.yoffset,
                       &res_info->connectors[monitor->egldev.crtcIndex], 1, NULL))
    {
        _glfwInputError(GLFW_PLATFORM_ERROR,
                        "EGLDevice: Setting window pos failed");
    }

    drmModeFreeResources(res_info);
}

void _glfwPlatformGetWindowSize(_GLFWwindow* window, int* width, int* height)
{
    if (width)
        *width = window->egldev.width;
    if (height)
        *height = window->egldev.height;
}

void _glfwPlatformSetWindowSize(_GLFWwindow* window, int width, int height)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformSetWindowSize not implemented");
}

void _glfwPlatformSetWindowSizeLimits(_GLFWwindow* window,
                                      int minwidth, int minheight,
                                      int maxwidth, int maxheight)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformSetWindowSizeLimits not implemented");
}

void _glfwPlatformSetWindowAspectRatio(_GLFWwindow* window, int numer, int denom)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformSetWindowAspectRatio not implemented");
}

void _glfwPlatformGetFramebufferSize(_GLFWwindow* window, int* width, int* height)
{
    _glfwPlatformGetWindowSize(window, width, height);
}

void _glfwPlatformGetWindowFrameSize(_GLFWwindow* window,
                                     int* left, int* top,
                                     int* right, int* bottom)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformGetWindowFrameSize not implemented");
}

void _glfwPlatformIconifyWindow(_GLFWwindow* window)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformIconifyWindow not implemented");
}

void _glfwPlatformRestoreWindow(_GLFWwindow* window)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformRestoreWindow not implemented");
}

void _glfwPlatformMaximizeWindow(_GLFWwindow* window)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformMaximizeWindow not implemented");
}

void _glfwPlatformShowWindow(_GLFWwindow* window)
{
}

void _glfwPlatformUnhideWindow(_GLFWwindow* window)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformUnhideWindow not implemented");
}

void _glfwPlatformHideWindow(_GLFWwindow* window)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformHideWindow not implemented");
}

void _glfwPlatformFocusWindow(_GLFWwindow* window)
{
}

void _glfwPlatformSetWindowMonitor(_GLFWwindow* window,
                                   _GLFWmonitor* monitor,
                                   int xpos, int ypos,
                                   int width, int height,
                                   int refreshRate)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformSetWindowMonitor not implemented");
}
int _glfwPlatformWindowFocused(_GLFWwindow* window)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformWindowFocused not implemented");
    return GLFW_FALSE;
}

void _glfwPlatformSetWindowResizable(_GLFWwindow* window, GLFWbool enabled)
{
}

void _glfwPlatformSetWindowDecorated(_GLFWwindow* window, GLFWbool enabled)
{
}

void _glfwPlatformSetWindowFloating(_GLFWwindow* window, GLFWbool enabled)
{
}

int _glfwPlatformWindowIconified(_GLFWwindow* window)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformWindowIconified not implemented");
    return GLFW_FALSE;
}

int _glfwPlatformWindowVisible(_GLFWwindow* window)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformWindowVisible not implemented");
    return GLFW_FALSE;
}

int _glfwPlatformWindowMaximized(_GLFWwindow* window)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformWindowMaximized not implemented");
    return 0;
}

void _glfwPlatformPollEvents(void)
{
}

void _glfwPlatformWaitEvents(void)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformWaitEvents not supported");
}

void _glfwPlatformWaitEventsTimeout(double timeout)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformWaitEventsTimeout not supported");
}

void _glfwPlatformPostEmptyEvent(void)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformPostEmptyEvent not supported");
}

void _glfwPlatformGetCursorPos(_GLFWwindow* window, double* xpos, double* ypos)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformGetCursorPos not supported");
}

void _glfwPlatformSetCursorPos(_GLFWwindow* window, double x, double y)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformSetCursorPos not supported");
}

void _glfwPlatformSetCursorMode(_GLFWwindow* window, int mode)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformSetCursorMode not supported");
}

const char* _glfwPlatformGetKeyName(int key, int scancode)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformGetKeyName not supported");
    return NULL;
}

int _glfwPlatformGetKeyScancode(int key)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformGetKeyScancode not supported");
    return 0;
}

int _glfwPlatformCreateCursor(_GLFWcursor* cursor,
                              const GLFWimage* image,
                              int xhot, int yhot)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformCreateCursor not supported");
    return GLFW_FALSE;
}

int _glfwPlatformCreateStandardCursor(_GLFWcursor* cursor, int shape)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformCreateStandardCursor not supported");
    return GLFW_FALSE;
}

void _glfwPlatformDestroyCursor(_GLFWcursor* cursor)
{
}

void _glfwPlatformSetCursor(_GLFWwindow* window, _GLFWcursor* cursor)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformSetCursor not supported");
}

void _glfwPlatformSetClipboardString(_GLFWwindow* window, const char* string)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformSetClipboardString not supported");
}

const char* _glfwPlatformGetClipboardString(_GLFWwindow* window)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformGetClipboardString not supported");
    return NULL;
}

void _glfwPlatformGetRequiredInstanceExtensions(char** extensions)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformGetRequiredInstanceExtensions not supported");
}

int _glfwPlatformGetPhysicalDevicePresentationSupport(VkInstance instance,
                                                      VkPhysicalDevice device,
                                                      uint32_t queuefamily)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformGetPhysicalDevicePresentationSupport not supported");
    return 0;
}

VkResult _glfwPlatformCreateWindowSurface(VkInstance instance,
                                          _GLFWwindow* window,
                                          const VkAllocationCallbacks* allocator,
                                          VkSurfaceKHR* surface)
{
    _glfwInputError(GLFW_PLATFORM_ERROR,
                    "EGLDevice: _glfwPlatformCreateWindowSurface not supported");
    return VK_ERROR_INITIALIZATION_FAILED;
}

