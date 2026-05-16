# Find poppler

find_package(Poppler QUIET COMPONENTS Core Cpp)

if(Poppler_FOUND)
    target_link_libraries(${PROJECT_NAME}
        Poppler::Core
        Poppler::Cpp
    )
elseif(WIN32)
    # On Windows, look for pre-built poppler in OpenBoard-ThirdParty next to the source tree.
    # Expected structure: ../OpenBoard-ThirdParty/poppler/{include,lib,bin}
    set(_POPPLER_THIRDPARTY_HINT "$ENV{OPENBOARD_THIRDPARTY}/poppler")
    if(NOT EXISTS "${_POPPLER_THIRDPARTY_HINT}")
        get_filename_component(_SRC_PARENT "${CMAKE_SOURCE_DIR}" DIRECTORY)
        set(_POPPLER_THIRDPARTY_HINT "${_SRC_PARENT}/OpenBoard-ThirdParty/poppler")
    endif()

    find_path(POPPLER_INCLUDE_DIR
        NAMES poppler/cpp/poppler-version.h
        HINTS "${_POPPLER_THIRDPARTY_HINT}/include"
        NO_DEFAULT_PATH
    )
    find_library(POPPLER_LIBRARY
        NAMES poppler
        HINTS "${_POPPLER_THIRDPARTY_HINT}/lib"
        NO_DEFAULT_PATH
    )
    find_library(POPPLER_CPP_LIBRARY
        NAMES poppler-cpp
        HINTS "${_POPPLER_THIRDPARTY_HINT}/lib"
        NO_DEFAULT_PATH
    )

    if(POPPLER_INCLUDE_DIR AND POPPLER_LIBRARY AND POPPLER_CPP_LIBRARY)
        message(STATUS "Found Poppler (Windows ThirdParty): ${_POPPLER_THIRDPARTY_HINT}")
        target_include_directories(${PROJECT_NAME} PRIVATE "${POPPLER_INCLUDE_DIR}")
        target_link_libraries(${PROJECT_NAME} "${POPPLER_LIBRARY}" "${POPPLER_CPP_LIBRARY}")
    else()
        message(WARNING "Poppler not found in ${_POPPLER_THIRDPARTY_HINT}.")
        message(WARNING "Place pre-built poppler headers in ../OpenBoard-ThirdParty/poppler/include/")
        message(WARNING "and import libs in ../OpenBoard-ThirdParty/poppler/lib/")
        message(FATAL_ERROR "Poppler is required. Set OPENBOARD_THIRDPARTY or place poppler in the expected location.")
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(Poppler REQUIRED IMPORTED_TARGET poppler poppler-cpp)

    if(Poppler_FOUND)
        target_link_libraries(${PROJECT_NAME}
            PkgConfig::Poppler
        )
    endif()
endif()
