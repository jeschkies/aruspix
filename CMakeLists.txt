cmake_minimum_required(VERSION 3.21)
project(aruspix CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies are provided by Conan; see conanfile.py.
# Configure with `cmake --preset conan-release` after `conan install`.
find_package(OpenCV REQUIRED)
find_package(wxWidgets REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(doctest REQUIRED)

set(aruspixSources
    src/app/axabout_wdr.cpp
    src/app/axapp.cpp
    src/app/axapp_wdr.cpp
    src/app/axctrl.cpp
    src/app/axenv.cpp
    src/app/axfile.cpp
    src/app/axframe.cpp
    src/app/axgotodlg.cpp
    src/app/aximagecontroller.cpp
    src/app/aximage.cpp
    src/app/aximcontrol_wdr.cpp
    src/app/axoptionsdlg.cpp
    src/app/axoptions_wdr.cpp
    src/app/axprocess.cpp
    src/app/axprogressdlg.cpp
    src/app/axscrolledwindow.cpp
    src/app/axundo.cpp
    src/app/wxprec.cpp

    src/comparison/cmp.cpp
    src/comparison/cmpctrl.cpp
    src/comparison/cmpfile.cpp
    src/comparison/cmpmus.cpp
    src/comparison/cmp_wdr.cpp

    src/edition/edt.cpp
    src/edition/edtfile.cpp
    src/edition/edt_wdr_inc.cpp

    src/im/binarize.cpp
    src/im/imbrink3classes.cpp
    src/im/imbrink.cpp
    src/im/imext.cpp
    src/im/imoperator.cpp
    src/im/impage.cpp
    src/im/imregister.cpp
    src/im/imstaff.cpp
    src/im/imstaffsegment.cpp

    src/mus/musapp.cpp
    src/mus/musbarline.cpp
    src/mus/musbboxdc.cpp
    src/mus/musbeam.cpp
    src/mus/musclef.cpp
    src/mus/musdc.cpp
    src/mus/musdoc.cpp
    src/mus/musdurationinterface.cpp
    src/mus/musiobin.cpp
    src/mus/musio.cpp
    src/mus/musiodarms.cpp
    src/mus/musiomei.cpp
    src/mus/musiomlf.cpp
    src/mus/musiomusxml.cpp
    src/mus/musiopae.cpp
    src/mus/musiowwg.cpp
    src/mus/muskeysig.cpp
    src/mus/muslayer.cpp
    src/mus/muslayerelement.cpp
    src/mus/musleipzigbbox.cpp
    src/mus/musmensur.cpp
    src/mus/musneume.cpp
    src/mus/musneumesymbol.cpp
    src/mus/musnote.cpp
    src/mus/musmlfdic.cpp
    src/mus/musobject.cpp
    src/mus/muspage.cpp
    src/mus/muspitchinterface.cpp
    src/mus/muspositioninterface.cpp
    src/mus/musrc.cpp
    src/mus/musrc_beam.cpp
    src/mus/musrc_bezier.cpp
    src/mus/musrc_element.cpp
    src/mus/musrc_graph.cpp
    src/mus/musrc_page.cpp
    src/mus/musrc_neume.cpp
    src/mus/musrc_tuplet.cpp
    src/mus/musrest.cpp
    src/mus/musslur.cpp
    src/mus/musstaff.cpp
    src/mus/mussvgdc.cpp
    src/mus/mussymbol.cpp
    src/mus/mussystem.cpp
    src/mus/mustie.cpp
    src/mus/mustoolpanel.cpp
    src/mus/mustuplet.cpp
    src/mus/muswindow.cpp
    src/mus/muswwg.cpp
    src/mus/muswxdc.cpp

    src/recognition/recbookctrl.cpp
    src/recognition/recbookfile.cpp
    src/recognition/rec.cpp
    src/recognition/recfile.cpp
    src/recognition/recim.cpp
    src/recognition/recmlfbmp.cpp
    src/recognition/recmodels.cpp
    src/recognition/recmus.cpp
    src/recognition/rec_wdr.cpp

    src/superimposition/supbookctrl.cpp
    src/superimposition/supbookfile.cpp
    src/superimposition/sup.cpp
    src/superimposition/supfile.cpp
    src/superimposition/supim.cpp
    src/superimposition/sup_wdr.cpp

    src/tinyxml/tinystr.cpp
    src/tinyxml/tinyxml.cpp
    src/tinyxml/tinyxmlerror.cpp
    src/tinyxml/tinyxmlparser.cpp
)

if(WIN32)
    list(APPEND aruspixSources src/aruspix_uuid_win.cpp)
endif()

add_executable(aruspix ${aruspixSources})

# wxWidgets' wxIMPLEMENT_APP macro emits WinMain, not main, so the
# executable needs the GUI subsystem on Windows. WIN32_EXECUTABLE also
# wires up the right entry-point shim in the C runtime.
set_target_properties(aruspix PROPERTIES WIN32_EXECUTABLE TRUE)

target_include_directories(aruspix PRIVATE
    src
    src/app
    src/comparison
    src/recognition
    src/rtmidi
    src/edition
    src/superimposition
    src/tinyxml
    src/im
    src/ml
    src/mus
)

target_compile_definitions(aruspix PRIVATE
    AX_SUPERIMPOSITION
    AX_COMPARISON
    AX_EDT
    AX_RECOGNITION
    __AXDEBUG__
    _FILE_OFFSET_BITS=64
    _LARGE_FILES
)

if(MSVC)
    # /Z7 embeds debug info per-object instead of a shared vc*.pdb, which
    # is what actually works under Ninja's parallel compilation (/Zi's
    # shared pdb needs /FS or serializes access). /DEBUG makes the linker
    # still emit a final aruspix.pdb from that so a Release crash offset
    # (e.g. from Windows Event Viewer / WER) can be resolved to a
    # function/line instead of staying an opaque address.
    target_compile_options(aruspix PRIVATE /W3 /Z7)
    target_link_options(aruspix PRIVATE /DEBUG)
else()
    target_compile_options(aruspix PRIVATE -g -Wno-write-strings -Wno-format-security)
endif()

target_link_libraries(aruspix PRIVATE
    opencv::opencv
    wxWidgets::wxWidgets
    LibXml2::LibXml2
)

if(UNIX AND NOT APPLE)
    target_link_libraries(aruspix PRIVATE pthread uuid)
elseif(APPLE)
    target_link_libraries(aruspix PRIVATE pthread)
elseif(WIN32)
    # rpcrt4 provides UuidCreate.
    target_link_libraries(aruspix PRIVATE rpcrt4)

    # Copy any runtime DLLs into the output directory so the exe can
    # find them at launch without needing PATH tweaks. When the DLL
    # list is empty (Conan's all-static build), cmake -E
    # copy_if_different fails on a destination-only invocation — dispatch
    # via a small script so we can no-op cleanly in that case.
    add_custom_command(TARGET aruspix POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                "-DDLLS=$<TARGET_RUNTIME_DLLS:aruspix>"
                "-DDEST=$<TARGET_FILE_DIR:aruspix>"
                -P "${CMAKE_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake"
    )
endif()

# ---------------------------------------------------------------------------
# Tests
#
# A separate target that exercises the wx-free leaf functions in src/im/
# (imext.cpp, imbrink.cpp, imbrink3classes.cpp) via their ax:: interfaces.
# The wx-coupled wrapper classes (ImPage, ImStaff, ImRegister, ImOperator)
# are intentionally not covered here — they need real-image fixtures and
# belong in an integration-test target.
# ---------------------------------------------------------------------------
enable_testing()

add_executable(aruspix_tests
    src/im/binarize.cpp
    src/im/imext.cpp
    src/im/imbrink.cpp
    src/im/imbrink3classes.cpp

    tests/test_main.cpp
    tests/test_imext_helpers.cpp
    tests/test_binarize.cpp
    tests/test_binarize_goldens.cpp
    tests/test_image_ops.cpp
)

target_include_directories(aruspix_tests PRIVATE src/im)

# Point the goldens test at the fixtures directory regardless of the
# working directory ctest ends up in.
target_compile_definitions(aruspix_tests PRIVATE
    AX_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
)

target_link_libraries(aruspix_tests PRIVATE
    opencv::opencv
    doctest::doctest
)

add_test(NAME aruspix_tests COMMAND aruspix_tests)

# ---------------------------------------------------------------------------
# Integration tests (wx-coupled)
#
# aximage_loadfile_tests exercises AxImage::LoadFile (src/app/aximage.cpp)
# on real, oversized scans to reproduce a reported Windows Superimposition
# crash locally. Unlike aruspix_tests it links wxWidgets, so it gets its
# own main (tests/test_aximage_loadfile_main.cpp) that sets up just enough
# of wx (wxInitializer + image handlers) to decode PNGs without a full
# wxApp/event loop. Fixtures are not committed; the test no-ops if
# tests/fixtures/sup_pages/ is empty.
# ---------------------------------------------------------------------------
add_executable(aximage_loadfile_tests
    src/app/aximage.cpp

    tests/test_aximage_loadfile_main.cpp
    tests/test_aximage_loadfile.cpp
)

target_include_directories(aximage_loadfile_tests PRIVATE src/app)

target_compile_definitions(aximage_loadfile_tests PRIVATE
    AX_SUP_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/sup_pages"
)

target_link_libraries(aximage_loadfile_tests PRIVATE
    opencv::opencv
    wxWidgets::wxWidgets
    doctest::doctest
)

add_test(NAME aximage_loadfile_tests COMMAND aximage_loadfile_tests)

# ---------------------------------------------------------------------------
# imregister_integration_tests exercises the real ImRegister/ImPage
# pipeline (Init -> DetectPoints -> Register) end to end against real
# scan pairs -- the "Batch superimposition" code path, including the
# cv::dft-ported cross-correlation. Built with -DAX_CMDLINE so
# AxProgressDlg is the non-GUI stub (src/app/axprogressdlg.h) instead of
# the real wxDialog; SupEnv's static config members are provided by
# tests/support_sup_env_stub.cpp instead of linking the full GUI
# src/superimposition/sup.cpp. Fixtures are not committed; every case
# no-ops if tests/fixtures/sup_pages/ doesn't have the named files.
# ---------------------------------------------------------------------------
add_executable(imregister_integration_tests
    src/im/imregister.cpp
    src/im/impage.cpp
    src/im/imstaff.cpp
    src/im/imstaffsegment.cpp
    src/im/imoperator.cpp
    src/im/imext.cpp
    src/im/imbrink.cpp
    src/im/imbrink3classes.cpp
    src/im/binarize.cpp
    src/app/aximage.cpp
    src/tinyxml/tinystr.cpp
    src/tinyxml/tinyxml.cpp
    src/tinyxml/tinyxmlerror.cpp
    src/tinyxml/tinyxmlparser.cpp

    tests/support_sup_env_stub.cpp
    tests/test_imregister_integration_main.cpp
    tests/test_imregister_integration.cpp
)

target_include_directories(imregister_integration_tests PRIVATE
    src src/im src/app src/superimposition src/tinyxml
)

target_compile_definitions(imregister_integration_tests PRIVATE
    AX_CMDLINE
    AX_SUPERIMPOSITION
    AX_SUP_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures/sup_pages"
)

target_link_libraries(imregister_integration_tests PRIVATE
    opencv::opencv
    wxWidgets::wxWidgets
    doctest::doctest
)

add_test(NAME imregister_integration_tests COMMAND imregister_integration_tests)

# ---------------------------------------------------------------------------
# Tools
#
# dump_binarize: one-shot fixture generator. Runs ax::binarize_and_clean
# on a JPG and writes a PNG + FNV-1a-64 hash. A near-twin on master
# runs the IM-based algorithm and produces the same-format output so
# we can compare byte-for-byte between the two branches.
# ---------------------------------------------------------------------------
add_executable(dump_binarize
    tools/dump_binarize.cpp
    src/im/binarize.cpp
    src/im/imext.cpp
    src/im/imbrink.cpp
    src/im/imbrink3classes.cpp
)
target_include_directories(dump_binarize PRIVATE src)
target_link_libraries(dump_binarize PRIVATE
    opencv::opencv
)
