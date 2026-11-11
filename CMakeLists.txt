cmake_minimum_required(VERSION 3.16)
project(MiryuHardwareManager VERSION 45.0.0 LANGUAGES CXX)

set(TRANSLATION_DOMAIN "miryu-hardware-manager")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH "${ECM_MODULE_PATH}" ${CMAKE_MODULE_PATH})
include(GNUInstallDirs)
include(KDEInstallDirs)
include(KDECMakeSettings)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(KF6 REQUIRED COMPONENTS CoreAddons I18n Crash)
find_package(Gettext REQUIRED)

add_executable(miryu-hardware-manager
    src/main.cpp
    src/mainwindow.cpp
    src/basicinfopage.cpp
    src/systeminfo.cpp
    src/driverpage.cpp
    src/driverutils.cpp
    src/driverscripts.cpp
    src/installlogdialog.cpp
    src/mainwindow.h
    src/basicinfopage.h
    src/systeminfo.h
    src/driverpage.h
    src/driverutils.h
    src/driverscripts.h
    src/installlogdialog.h
)

target_link_libraries(miryu-hardware-manager
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    KF6::CoreAddons
    KF6::I18n
    KF6::Crash
)

install(TARGETS miryu-hardware-manager RUNTIME DESTINATION ${KDE_INSTALL_BINDIR})
install(FILES data/miryu-hardware-manager.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(PROGRAMS data/miryu-hardware-manager-helper
                 data/miryu-hardware-manager-mok-helper
                 data/miryu-hardware-manager-mok-cert-helper
                 data/miryu-hardware-manager-dmi-helper
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
install(FILES data/org.miryugaming.hardwaremanager.policy
              data/org.miryugaming.hardwaremanager.dmi.policy
        DESTINATION ${KDE_INSTALL_DATADIR}/polkit-1/actions)

# Compile and install the gettext catalogues explicitly.  This guarantees that
# the installed files are named after TRANSLATION_DOMAIN, which is what
# KLocalizedString looks up at runtime and what %find_lang expects.
file(GLOB _translation_po_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/po/*.po")
set(_translation_mo_files)
foreach(_po_file IN LISTS _translation_po_files)
    get_filename_component(_lang "${_po_file}" NAME_WE)
    set(_mo_dir "${CMAKE_CURRENT_BINARY_DIR}/locale/${_lang}/LC_MESSAGES")
    set(_mo_file "${_mo_dir}/${TRANSLATION_DOMAIN}.mo")
    add_custom_command(
        OUTPUT "${_mo_file}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_mo_dir}"
        COMMAND "${GETTEXT_MSGFMT_EXECUTABLE}" --check --check-format -o "${_mo_file}" "${_po_file}"
        DEPENDS "${_po_file}"
        COMMENT "Compiling ${_lang} translation"
        VERBATIM
    )
    list(APPEND _translation_mo_files "${_mo_file}")
    install(FILES "${_mo_file}" DESTINATION "${KDE_INSTALL_LOCALEDIR}/${_lang}/LC_MESSAGES")
endforeach()

add_custom_target(miryu-hardware-manager-translations ALL
    DEPENDS ${_translation_mo_files})
