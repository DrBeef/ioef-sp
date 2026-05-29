LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# Generic Khronos OpenXR loader, used to satisfy the engine link.  At runtime
# the vendor-specific loader (libopenxr_loader_meta.so / _pico.so, shipped as a
# jniLib in android/libs/arm64-v8a/) is System.loadLibrary'd first by the Java
# activity, selected from Build.MANUFACTURER.
LOCAL_MODULE := openxr_loader
LOCAL_SRC_FILES := lib$(LOCAL_MODULE).so

# Guard so `make clean` (which may delete the prebuilt) doesn't error out.
ifneq (,$(wildcard $(LOCAL_PATH)/$(LOCAL_SRC_FILES)))
  include $(PREBUILT_SHARED_LIBRARY)
endif
