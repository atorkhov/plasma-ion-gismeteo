project(plasma-ion-gismeteo)

find_package(KDE4 REQUIRED)
find_package(KDE4Workspace REQUIRED)
include(KDE4Defaults)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

set(PLASMA_ION_GISMETEO_SHARE_PATH ${KDE4_DATA_INSTALL_DIR}/plasma-ion-gismeteo)
configure_file(config.h.cmake config.h)

SET (ion_gismeteo_SRCS ion_gismeteo.cpp)
kde4_add_plugin(ion_gismeteo ${ion_gismeteo_SRCS})
target_link_libraries (ion_gismeteo
    ${QT_QTXML_LIBRARY}
    ${QT_QTXMLPATTERNS_LIBRARY}
    ${KDE4_KDEUI_LIBS}
    ${KDE4_KIO_LIBS}
    ${PLASMA_LIBS}
    ${KDE4WORKSPACE_WEATHERION_LIBRARY}
    ${KDE4_KUNITCONVERSION_LIBS}
    qlibxmlnodemodel
    )

INSTALL (FILES ion-gismeteo.desktop DESTINATION ${SERVICES_INSTALL_DIR})
INSTALL (FILES gismeteo.xq DESTINATION ${PLASMA_ION_GISMETEO_SHARE_PATH})

INSTALL (TARGETS ion_gismeteo DESTINATION ${PLUGIN_INSTALL_DIR})

