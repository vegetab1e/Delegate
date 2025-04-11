cmake_minimum_required(VERSION 3.16)

project(delegate LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

add_executable(delegate main.cpp
    type_name.h
    ifaces.h
    containers.h
    delegate.h
    tutils.h
)

include(GNUInstallDirs)
install(TARGETS delegate
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
