# Try to find libdrm
#
# This will define:
#
#   DRM_LIBRARIES   - Link this to use libdrm
#   DRM_INCLUDE_DIR - Include directory for libdrm

find_library(DRM_LIBRARY drm)
find_path(DRM_INCLUDE_DIR NAMES drm.h HINTS "/usr/include/drm")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libdrm DEFAULT_MSG DRM_LIBRARY DRM_INCLUDE_DIR)

mark_as_advanced(DRM_LIBRARY DRM_INCLUDE_DIR)

