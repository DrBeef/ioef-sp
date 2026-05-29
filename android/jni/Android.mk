# Top-level aggregator.  Variables (TOP_DIR, GL4ES_PATH, IOEF_ROOT, ...) come
# from Application.mk, which ndk-build parses first.

include $(SUPPORT_LIBS)/libmad/Android.mk # libmad.a (MP3 decode, static)
include $(EFXR_PATH)/Android.mk          # libefxr.so engine module
include $(GL4ES_PATH)/Android.mk         # libgl4es.so
include $(EFGAME_PATH)/Android.mk        # libefgameaarch64.so + libefuiaarch64.so
