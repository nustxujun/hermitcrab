cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_SYSTEM_VERSION "10.0")

project(hermitcrab)

# add_definitions(-D_CONSOLE)
add_definitions(-DNO_UE4)
add_definitions(-DWIN32_LEAN_AND_MEAN)

#std::wstring_convert is deprecated in c++17, but there is no easy way to replace it. 
#display warnings and remain using it
add_definitions(-D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++17")
set(IsOnWin7 FALSE CACHE BOOL "if operation system is win7" )


file(GLOB ENGINE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
source_group(src FILES ${ENGINE_HEADERS})

AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR} ENGINE_SOURCES)
source_group(src FILES ${ENGINE_SOURCES})

AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/imgui IMGUI_SOURCES)
source_group(imgui FILES ${IMGUI_SOURCES})

if (${IsOnWin7} STREQUAL ON)
    add_definitions(-DD3D12ON7)
endif()

add_library(hermitcrab STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS} ${IMGUI_SOURCES})

