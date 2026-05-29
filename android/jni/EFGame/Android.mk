# ============================================================================
# SP game + UI modules, built from the SIBLING Elite-Force-VR checkout
# (EFVR_ROOT, set in Application.mk -> ../../../Elite-Force-VR).  Never a
# submodule; these makefiles live in ioef-sp and reach the sibling by path.
#
#   libefgameaarch64.so : server-game + cgame + icarus (Q2-style GetGameAPI +
#                         dllEntry/vmMain).  Loaded by SV_SP_InitGameProgs.
#   libefuiaarch64.so   : SP UI (GetUIAPI).  Loaded by CL_SP_InitUI.
#
# These are the MSVC-built efgame/efui DLLs recompiled with clang/NDK for arm64,
# so expect MSVC-ism fixups on first build (R2): __declspec, inline asm, stricmp,
# anonymous structs, and 'long' being 8 bytes (LP64) vs 4 on Win64 (LLP64) --
# audit serialized/savegame structs.
# ============================================================================

EFGAME_CPPFLAGS := \
    -DNDEBUG -D_CRT_SECURE_NO_WARNINGS \
    -DARCH_STRING=\"aarch64\" \
    -include $(EFGAME_PATH)/ef_android_compat.h \
    -fexceptions -fpermissive -fno-strict-aliasing \
    -Wno-write-strings -Wno-invalid-offsetof -Wno-narrowing \
    -Wno-format-security \
    -fcommon -fvisibility=hidden
# -fvisibility=hidden is REQUIRED: without it every C++ vague-linkage symbol
# (libc++ template instantiations, static-init guards) is exported and interposes
# with libc++_shared.so, corrupting static init (SEGV in __cxa_guard_acquire at
# dlopen).  The dlsym entry points (GetGameAPI/GetUIAPI/dllEntry/vmMain) stay
# exported via EF_DLL_EXPORT's explicit visibility("default") (see q_shared.h).
# -fcommon: EF declares some globals (gi, etc.) as tentative definitions in more
# than one TU (e.g. g_main.cpp + ui/gameinfo.cpp).  MSVC merged these; clang's
# default -fno-common treats them as duplicate symbols.  -fcommon restores the
# merge.

# ---- libefgameaarch64.so (game + cgame + icarus) ---------------------------
include $(CLEAR_VARS)
LOCAL_PATH   := $(EFVR_ROOT)
LOCAL_MODULE := efgameaarch64

LOCAL_CPPFLAGS := $(EFGAME_CPPFLAGS) -std=gnu++20
LOCAL_CFLAGS   := $(EFGAME_CPPFLAGS)
LOCAL_C_INCLUDES := \
    $(EFVR_ROOT) \
    $(EFVR_ROOT)/game \
    $(EFVR_ROOT)/cgame \
    $(EFVR_ROOT)/icarus \
    $(EFVR_ROOT)/qcommon \
    $(EFVR_ROOT)/ui \
    $(EFVR_ROOT)/speedrun \
    $(EFVR_ROOT)/speedrun/overbounce_prediction \
    $(EFVR_ROOT)/speedrun/strafe_helper
LOCAL_LDLIBS := -llog -lz

EFGAME_SRC := \
    $(wildcard $(LOCAL_PATH)/game/*.cpp) \
    $(wildcard $(LOCAL_PATH)/game/*.c) \
    $(wildcard $(LOCAL_PATH)/cgame/*.cpp) \
    $(wildcard $(LOCAL_PATH)/cgame/*.c) \
    $(LOCAL_PATH)/icarus/BlockStream.cpp \
    $(LOCAL_PATH)/icarus/Instance.cpp \
    $(LOCAL_PATH)/icarus/Sequence.cpp \
    $(LOCAL_PATH)/icarus/Sequencer.cpp \
    $(LOCAL_PATH)/icarus/TaskManager.cpp \
    $(LOCAL_PATH)/ui/gameinfo.cpp \
    $(LOCAL_PATH)/speedrun/PlayerOverbouncePrediction.cpp \
    $(LOCAL_PATH)/speedrun/overbounce_prediction/OverbouncePrediction.cpp \
    $(LOCAL_PATH)/speedrun/sound_skipping.cpp \
    $(LOCAL_PATH)/speedrun/strafe_helper/strafe_helper.c \
    $(LOCAL_PATH)/speedrun/strafe_helper_customization.cpp
# bg_lib.cpp is Q3's freestanding libc (memcpy/tolower/vsprintf/sscanf/qsort/...).
# Redundant on Android (Bionic provides all of it) and would both break (va_list
# cast) and clash at link, so drop it -- libc fills those symbols.
EFGAME_SRC := $(filter-out $(LOCAL_PATH)/game/bg_lib.cpp,$(EFGAME_SRC))
LOCAL_SRC_FILES := $(EFGAME_SRC:$(LOCAL_PATH)/%=%)

include $(BUILD_SHARED_LIBRARY)

# ---- libefuiaarch64.so (UI) ------------------------------------------------
include $(CLEAR_VARS)
LOCAL_PATH   := $(EFVR_ROOT)
LOCAL_MODULE := efuiaarch64

# _USRDLL: this is the UI module, so shared files (g_weaponLoad.cpp) take their
# UI-dll branch -- using gameinfo's `gi` (gameinfo_import_t), not the game's.
LOCAL_CPPFLAGS := $(EFGAME_CPPFLAGS) -std=gnu++20 -D_USRDLL
LOCAL_CFLAGS   := $(EFGAME_CPPFLAGS) -D_USRDLL
LOCAL_C_INCLUDES := \
    $(EFVR_ROOT) \
    $(EFVR_ROOT)/ui \
    $(EFVR_ROOT)/game \
    $(EFVR_ROOT)/qcommon
LOCAL_LDLIBS := -llog -lz

EFUI_SRC := \
    $(wildcard $(LOCAL_PATH)/ui/*.cpp) \
    $(wildcard $(LOCAL_PATH)/ui/*.c) \
    $(LOCAL_PATH)/game/q_math.cpp \
    $(LOCAL_PATH)/game/q_shared.cpp \
    $(LOCAL_PATH)/game/g_weaponLoad.cpp
LOCAL_SRC_FILES := $(EFUI_SRC:$(LOCAL_PATH)/%=%)

include $(BUILD_SHARED_LIBRARY)
