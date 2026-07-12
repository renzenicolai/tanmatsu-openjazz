file(GLOB_RECURSE OJ_CPP "src/*.cpp")
list(FILTER OJ_CPP EXCLUDE REGEX "/jj2/")
list(FILTER OJ_CPP EXCLUDE REGEX "/platforms/(android|apple|gp2x|haiku|riscos|xdg)\\.cpp$")
file(GLOB XMP_C "ext/xmp/src/*.c" "ext/xmp/src/loaders/*.c")
idf_component_register(SRCS ${OJ_CPP} ${XMP_C}
  "ext/argparse/argparse.c" "ext/miniz/miniz.c" "ext/stb/stb_rect_pack.c"
  INCLUDE_DIRS "src" "ext/argparse" "ext/miniz" "ext/stb" "ext/xmp"
  REQUIRES sdl fatfs)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_14)
target_compile_definitions(${COMPONENT_LIB} PUBLIC OJ_SDL3=1 PORTABLE
  DEFAULT_SCREEN_WIDTH=320 DEFAULT_SCREEN_HEIGHT=200
  NO_RESIZE
  DATAPATH="/sdcard/openjazz" OJ_URL="https://alister.eu/jazz/oj/"
  OJ_BUGREPORT="https://github.com/AlisterT/openjazz/issues"
  LIBXMP_CORE_PLAYER LIBXMP_CORE_DISABLE_IT LIBXMP_STATIC)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error -fno-math-errno -fexceptions)
