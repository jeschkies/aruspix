cmake_minimum_required(VERSION 3.16)
project(aruspix CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- IM library ---------------------------------------------------------
# IM is built locally under third_party/im (see scripts/build-im.sh).
# Tecmake places libs in lib/<TEC_UNAME>/, so locate the subdir at config time.
set(IM_DIR "${CMAKE_SOURCE_DIR}/third_party/im" CACHE PATH "Path to vendored IM toolkit")
file(GLOB IM_LIB_DIRS LIST_DIRECTORIES true "${IM_DIR}/lib/*")
set(IM_LIB_DIR "")
foreach(d ${IM_LIB_DIRS})
    if(IS_DIRECTORY "${d}")
        set(IM_LIB_DIR "${d}")
        break()
    endif()
endforeach()
if(NOT IM_LIB_DIR)
    message(FATAL_ERROR "Could not find IM library output under ${IM_DIR}/lib. Run scripts/build-im.sh first.")
endif()
message(STATUS "IM_LIB_DIR: ${IM_LIB_DIR}")

find_library(IM_LIBRARY        NAMES im         PATHS "${IM_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
find_library(IM_PROCESS_LIBRARY NAMES im_process PATHS "${IM_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)
find_library(IM_FFTW_LIBRARY    NAMES im_fftw3 im_fftw PATHS "${IM_LIB_DIR}" NO_DEFAULT_PATH REQUIRED)

# ---- wxWidgets ----------------------------------------------------------
find_package(wxWidgets REQUIRED COMPONENTS base core net html aui)
include(${wxWidgets_USE_FILE})

# ---- libxml2 ------------------------------------------------------------
find_package(LibXml2 REQUIRED)

# ---- Sources ------------------------------------------------------------
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

    src/im/imbrink3classes.cpp
    src/im/imbrink.cpp
    src/im/imext.cpp
    src/im/imkmeans.cpp
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

add_executable(aruspix ${aruspixSources})

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
    "${IM_DIR}/include"
    ${LIBXML2_INCLUDE_DIR}
)

target_compile_definitions(aruspix PRIVATE
    AX_SUPERIMPOSITION
    AX_COMPARISON
    AX_EDT
    AX_RECOGNITION
    __AXDEBUG__
    _FILE_OFFSET_BITS=64
    _LARGE_FILES
    ${LIBXML2_DEFINITIONS}
)

if(MSVC)
    target_compile_options(aruspix PRIVATE /W3)
else()
    target_compile_options(aruspix PRIVATE -g -Wno-write-strings -Wno-format-security)
endif()

target_link_libraries(aruspix PRIVATE
    ${IM_LIBRARY}
    ${IM_PROCESS_LIBRARY}
    ${IM_FFTW_LIBRARY}
    ${wxWidgets_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    z
)

if(UNIX AND NOT APPLE)
    target_link_libraries(aruspix PRIVATE pthread uuid)
elseif(APPLE)
    # Homebrew FFTW (linked transitively through libim_fftw3.a)
    target_link_directories(aruspix PRIVATE /opt/homebrew/lib /usr/local/lib)
    target_link_libraries(aruspix PRIVATE fftw3f fftw3 png jpeg pthread)
endif()
