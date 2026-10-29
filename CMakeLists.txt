cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake --preset debug or release.")
endif()

project(Project3 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Project3 requires Windows and the Windows SDK.")
endif()
if(NOT MSVC)
    message(FATAL_ERROR
        "The bundled krabsetw requires an MSVC-compatible compiler. "
        "Run CMake from an x64 Native Tools Command Prompt for Visual Studio.")
endif()

add_executable(main
    main.cpp
    HcsSandbox/HcsSandbox.cpp
    StartProcess/StartProcess.cpp
    NormalizePath/NormalizePath.cpp
    FilterFiles/FilterFiles.cpp
    ProcessContext/ProcessContext.cpp
    KernelFileProvider/KernelFileProvider.cpp
    KernelFileProvider/ProviderEventsHandlers/Common/HandlerCommon.cpp
    KernelFileProvider/ProviderEventsHandlers/Common/FileEventRecorder.cpp
    KernelFileProvider/ProviderEventsHandlers/CloseHandler/CloseHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/CreateNewFileHandler/CreateNewFileHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/CreateOpenHandler/CreateOpenHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/DeletePathHandler/DeletePathHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/NameCreateHandler/NameCreateHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/NameDeleteHandler/NameDeleteHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/OperationEndHandler/OperationEndHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/ReadHandler/ReadHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/RenameHandler/RenameHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/RenamePathHandler/RenamePathHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/SetDeleteHandler/SetDeleteHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/SetInformationHandler/SetInformationHandler.cpp
    KernelFileProvider/ProviderEventsHandlers/WriteHandler/WriteHandler.cpp
    KernelProcessProvider/KernelProcessProvider.cpp
    KernelProcessProvider/ProviderEventsHandlers/Common/ProcessHandlerCommon.cpp
    KernelProcessProvider/ProviderEventsHandlers/ImageLoadHandler/ImageLoadHandler.cpp
    KernelProcessProvider/ProviderEventsHandlers/ImageUnloadHandler/ImageUnloadHandler.cpp
    KernelProcessProvider/ProviderEventsHandlers/ProcessRundownHandler/ProcessRundownHandler.cpp
    KernelProcessProvider/ProviderEventsHandlers/ProcessStartHandler/ProcessStartHandler.cpp
    KernelProcessProvider/ProviderEventsHandlers/ProcessStopHandler/ProcessStopHandler.cpp
    KernelProcessProvider/ProviderEventsHandlers/ThreadStartHandler/ThreadStartHandler.cpp
    KernelProcessProvider/ProviderEventsHandlers/ThreadStopHandler/ThreadStopHandler.cpp
)

# std::optional in ProcessContext and std::string_view in krabsetw require C++17.
target_compile_features(main PRIVATE cxx_std_17)
target_compile_definitions(main PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
target_compile_options(main PRIVATE /utf-8)
target_include_directories(main PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/krabs"
)
# ETW/TDH, process security, COM allocation/GUIDs, and known-folder lookup.
target_link_libraries(main PRIVATE tdh advapi32 ole32 shell32)
set_target_properties(main PROPERTIES
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/main.dir"
)
