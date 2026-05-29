APP_ABI      := arm64-v8a
APP_PLATFORM := android-29
APP_STL      := c++_shared

APP_CFLAGS += -Wl,--no-undefined
APP_ALLOW_MISSING_DEPS := true

# ---- shared paths (visible to every Android.mk parsed below) ----------------
APPLICATIONMK_PATH := $(call my-dir)
# import-module,X resolves X/Android.mk under NDK_MODULE_PATH; point it at jni/.
NDK_MODULE_PATH := $(APPLICATIONMK_PATH)

TOP_DIR      := $(APPLICATIONMK_PATH)
SUPPORT_LIBS := $(TOP_DIR)/SupportLibs
GL4ES_PATH   := $(SUPPORT_LIBS)/gl4es
OPENXR_SDK   := $(TOP_DIR)/OpenXR-SDK
EFXR_PATH    := $(TOP_DIR)/EFXR
EFGAME_PATH  := $(TOP_DIR)/EFGame

# ioef-sp engine sources (android/jni -> repo root /code).
IOEF_ROOT    := $(TOP_DIR)/../../code
# Elite-Force-VR SP game/UI sources -- SIBLING checkout, never a submodule.
EFVR_ROOT    := $(TOP_DIR)/../../../Elite-Force-VR

# efxr           = engine .so (libefxr.so)
# gl4es          = GL1.x -> GLES3 translation layer
# openxr_loader  = generic prebuilt loader (link stub; vendor loader preloaded at runtime)
# efgameaarch64  = SP server-game + cgame + icarus (libefgameaarch64.so)
# efuiaarch64    = SP UI (libefuiaarch64.so)
# mad = libmad MP3 decoder (static lib linked into efxr for snd_codec_mp3.c)
APP_MODULES := efxr gl4es openxr_loader efgameaarch64 efuiaarch64 mad

