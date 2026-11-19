# Fuzz build for far2l: upstream's WinPort + utils static libraries (their own CMakeLists)
# plus the multiarc format readers, all compiled with the flags build.sh passes in
# (sanitizers + SanCov), and one fuzzer + one standalone binary per harness.
cmake_minimum_required(VERSION 3.10)
project(far2l_mayhem C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(F2L "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# Same global flags as far2l's top-level CMakeLists.txt (LEGACY=yes default -> WINPORT_REGISTRY)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -Wno-unused-function -D_FILE_OFFSET_BITS=64 -DWINPORT_REGISTRY")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -std=c99 -fPIC -Wno-unused-function -D_FILE_OFFSET_BITS=64 -DWINPORT_REGISTRY")
set(TESTING NO)
include_directories(${F2L}/utils/include)

# MAYHEM_TESTS=ON (build.sh's separate, unsanitized test build): only upstream's own unit test
# that builds headless -- the transformer plugin's ConfigTest, wired exactly like the
# `if(TESTING)` block of plugins/transformer/CMakeLists.txt (src/Config.cpp + utils).
option(MAYHEM_TESTS "build upstream's headless unit tests instead of the fuzzers" OFF)
if(MAYHEM_TESTS)
    add_subdirectory(${F2L}/utils utils)
    set(TR ${F2L}/plugins/transformer)
    enable_testing()
    # ConfigTest.cpp's main is renamed and called from mayhem/tests/config_test_driver.cpp,
    # which prints a pass line only after all of upstream's asserts held (no -DNDEBUG here).
    add_executable(transformer_config_test ${TR}/tests/ConfigTest.cpp ${TR}/src/Config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/config_test_driver.cpp)
    set_source_files_properties(${TR}/tests/ConfigTest.cpp PROPERTIES
        COMPILE_DEFINITIONS "main=transformer_config_test_main")
    target_link_libraries(transformer_config_test PRIVATE utils)
    target_include_directories(transformer_config_test PRIVATE ${TR}/src ${F2L}/utils/include)
    add_test(NAME transformer_config_test
        COMMAND transformer_config_test "${TR}/configs/plug/formats")
    return()
endif()

add_subdirectory(${F2L}/WinPort WinPort)
add_subdirectory(${F2L}/utils utils)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LibArchive REQUIRED libarchive)

# multiarc format readers (source list from plugins/multiarc/CMakeLists.txt, minus the
# plugin UI layer and the custom/external-archiver format)
set(MA ${F2L}/plugins/multiarc/src)
file(GLOB SEVENZ_C ${MA}/formats/7z/C/*.c)
file(GLOB HA_C ${MA}/formats/ha/ha/*.c)
set(UNRAR_FILES scantree sha1 threadpool crc hash strlist rs16 resource consio secpassword qopen
    recvol rijndael timefn encname rawread file strfn sha256 filefn filcreat headers blake2s options
    volume system crypt cmddata extinfo filestr ui list find smallfn unicode pathfn global rarvm
    getbits rs errhnd archive dll extract match unpack arcread rdwrfn rarpch largepage)
set(UNRAR_SRC ${MA}/formats/rar/unrar/rar.cpp)
foreach(f ${UNRAR_FILES})
    list(APPEND UNRAR_SRC ${MA}/formats/rar/unrar/${f}.cpp)
endforeach()
set_source_files_properties(${UNRAR_SRC} PROPERTIES COMPILE_FLAGS "-Wno-parentheses -Wno-switch -Wno-unused-variable -Wno-sign-compare -Wno-missing-braces -Wno-unknown-warning-option")

add_library(ma_formats STATIC
    ${MA}/formats/ha/ha.cpp ${MA}/formats/arj/arj.cpp ${MA}/formats/ace/ace.cpp
    ${MA}/formats/arc/arc.cpp ${MA}/formats/zip/zip.cpp ${MA}/formats/lzh/lzh.cpp
    ${MA}/formats/targz/targz.cpp ${MA}/formats/cab/cab.cpp
    ${MA}/formats/7z/7z.cpp ${SEVENZ_C} ${HA_C}
    ${MA}/formats/rar/rar.cpp ${UNRAR_SRC}
    ${MA}/formats/libarch/libarch_utils.cpp ${MA}/formats/libarch/libarch_crutches.cpp
    ${MA}/formats/libarch/libarch.cpp
    # helpers the format readers call (CanBeExecutableFileHeader, UnixTimeToFileTime) and
    # what ArcMix.cpp itself references: plugin globals and the builtin extractor mains
    ${MA}/ArcMix.cpp ${MA}/global.cpp ${MA}/formats/7z/7zMain.cpp
    ${MA}/formats/libarch/libarch_cmd_read.cpp ${MA}/formats/libarch/libarch_cmd_write.cpp
    )
# Misaligned loads through packed on-disk header structs are pervasive in these readers
# (benign on x86, and UBSan halts on the first one on every valid archive), so only the
# alignment check is relaxed for the format code; ASan and the rest of UBSan stay on.
target_compile_options(ma_formats PRIVATE -fno-sanitize=alignment)
target_compile_definitions(ma_formats PUBLIC -D_7ZIP_ST -D_7ZIP_PPMD_SUPPPORT -DRARDLL -DHAVE_UNRAR -DHAVE_LIBARCHIVE)
target_include_directories(ma_formats PUBLIC ${MA} ${F2L}/WinPort ${F2L}/far2l/far2sdk ${LibArchive_INCLUDE_DIRS})
target_include_directories(ma_formats PRIVATE ${MA}/formats/rar/unrar)
target_link_libraries(ma_formats PUBLIC WinPort utils ${LibArchive_LDFLAGS})

# LSAN_OFF_SRC (mayhem/lsan_off.cc, passed by build.sh): __lsan_is_turned_off() -> leak checks off
add_library(lsan_off OBJECT ${LSAN_OFF_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/harnesses/far2l_globals.cpp)
add_library(standalone_main OBJECT ${STANDALONE_MAIN})
set_source_files_properties(${STANDALONE_MAIN} PROPERTIES LANGUAGE C)

function(add_fuzzer name src)
    add_executable(${name} ${src} $<TARGET_OBJECTS:lsan_off>)
    target_link_options(${name} PRIVATE ${FUZZ_ENGINE})
    add_executable(${name}-standalone ${src} $<TARGET_OBJECTS:lsan_off> $<TARGET_OBJECTS:standalone_main>)
    foreach(t ${name} ${name}-standalone)
        target_link_libraries(${t} PRIVATE ${ARGN} dl)
        set_target_properties(${t} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${OUT_DIR})
    endforeach()
endfunction()

set(H ${CMAKE_CURRENT_SOURCE_DIR}/harnesses)
foreach(spec
        "zip;ZIP;.zip;0" "sevenz;SEVENZ;.7z;0" "rar;RAR;.rar;1" "targz;TARGZ;.tar;1"
        "lzh;LZH;.lzh;1" "arj;ARJ;.arj;0" "ace;ACE;.ace;0" "arc;ARC;.arc;0" "cab;CAB;.cab;0"
        "ha;HA;.ha;1" "libarch;LIBARCH;.bin;0")
    list(GET spec 0 n)
    list(GET spec 1 p)
    list(GET spec 2 e)
    list(GET spec 3 s)
    add_fuzzer(fuzz_multiarc_${n} ${H}/fuzz_multiarc.cpp ma_formats)
    foreach(t fuzz_multiarc_${n} fuzz_multiarc_${n}-standalone)
        target_compile_definitions(${t} PRIVATE FMT=${p} FMT_EXT="${e}")
        if(s)
            target_compile_definitions(${t} PRIVATE FMT_HAS_SETFARINFO=1)
        endif()
    endforeach()
endforeach()

add_fuzzer(fuzz_tty_input ${H}/fuzz_tty_input.cpp WinPort utils)
foreach(t fuzz_tty_input fuzz_tty_input-standalone)
    target_include_directories(${t} PRIVATE ${F2L}/WinPort ${F2L}/WinPort/src ${F2L}/WinPort/src/Backend)
endforeach()

add_fuzzer(fuzz_keyfile ${H}/fuzz_keyfile.cpp utils)

# inside plugin ELF reader (header-only template, opens the file through WinPort's sudo client)
add_fuzzer(fuzz_elf ${H}/fuzz_elf.cpp WinPort utils)
foreach(t fuzz_elf fuzz_elf-standalone)
    target_include_directories(${t} PRIVATE ${F2L}/plugins/inside/src ${F2L}/WinPort)
endforeach()

# NetRocks FTP listing parsers
set(NR ${F2L}/plugins/NetRocks/src)
add_library(nr_ftpparse STATIC ${NR}/Protocol/FTP/FTPParseLIST.cpp ${NR}/Protocol/FTP/FTPParseMLST.cpp ${NR}/Erroring.cpp)
target_include_directories(nr_ftpparse PUBLIC ${NR} ${F2L}/WinPort ${F2L}/far2l/far2sdk)
target_link_libraries(nr_ftpparse PUBLIC WinPort utils)
add_fuzzer(fuzz_ftp_listing ${H}/fuzz_ftp_listing.cpp nr_ftpparse)

# far2l's regular expression engine (far2l core sources RegExp.cpp + FARString.cpp)
# far2l core headers include bootstrap/lang.inc, generated by far2l/bootstrap's perl scripts;
# run the same two commands (far2l/bootstrap/CMakeLists.txt) into this build dir.
file(READ "${F2L}/packaging/version" F2L_VERSION)
string(STRIP "${F2L_VERSION}" F2L_VERSION)
string(REPLACE "." ";" F2L_VLIST "${F2L_VERSION}")
list(GET F2L_VLIST 0 F2L_VMAJ)
list(GET F2L_VLIST 1 F2L_VMIN)
list(GET F2L_VLIST 2 F2L_VPAT)
set(F2L_SCRIPTS ${F2L}/far2l/bootstrap/scripts)
set(F2L_BOOTSTRAP ${CMAKE_BINARY_DIR}/bootstrap)
file(MAKE_DIRECTORY ${F2L_BOOTSTRAP})
add_custom_command(OUTPUT ${F2L_BOOTSTRAP}/farlang.templ
    COMMAND cat "${F2L_SCRIPTS}/farlang.templ.m4" | perl -I. ${F2L_SCRIPTS}/far2l_m4.pl "${F2L_VMAJ}" "${F2L_VMIN}" "${F2L_VPAT}" "${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}" > ${F2L_BOOTSTRAP}/farlang.templ
    WORKING_DIRECTORY ${F2L_SCRIPTS}
    DEPENDS ${F2L_SCRIPTS}/farlang.templ.m4 ${F2L_SCRIPTS}/far2l_m4.pl)
add_custom_command(OUTPUT ${F2L_BOOTSTRAP}/lang.inc
    COMMAND perl -I. ${F2L_SCRIPTS}/farlng.pl ${F2L_BOOTSTRAP}/farlang.templ ${F2L_BOOTSTRAP}
    WORKING_DIRECTORY ${F2L_SCRIPTS}
    DEPENDS ${F2L_BOOTSTRAP}/farlang.templ)
add_custom_target(f2l_bootstrap DEPENDS ${F2L_BOOTSTRAP}/lang.inc)
set(FC ${F2L}/far2l/src)
add_library(f2l_regexp STATIC ${FC}/mix/RegExp.cpp ${FC}/base/FARString.cpp)
target_compile_definitions(f2l_regexp PUBLIC -DUNICODE)
target_include_directories(f2l_regexp PUBLIC ${F2L}/far2l ${F2L}/far2l/far2sdk ${FC} ${FC}/base ${FC}/mix
    ${FC}/cfg ${FC}/console ${FC}/locale ${FC}/plug ${F2L}/WinPort)
target_link_libraries(f2l_regexp PUBLIC WinPort utils)
target_include_directories(f2l_regexp PUBLIC ${CMAKE_BINARY_DIR})
add_dependencies(f2l_regexp f2l_bootstrap)
add_fuzzer(fuzz_regexp ${H}/fuzz_regexp.cpp f2l_regexp)
