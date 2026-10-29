find_program(GO_EXECUTABLE NAMES go REQUIRED)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env GOTOOLCHAIN=local "${GO_EXECUTABLE}" version
    OUTPUT_VARIABLE go_version_output
    RESULT_VARIABLE go_version_result
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT go_version_result EQUAL 0 OR
   NOT go_version_output MATCHES "go([0-9]+\\.[0-9]+(\\.[0-9]+)?)")
    message(FATAL_ERROR "Could not determine the installed Go version.")
endif()
set(go_version "${CMAKE_MATCH_1}")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/go.mod" go_version_line REGEX "^go ")
string(REGEX REPLACE "^go +" "" go_minimum_version "${go_version_line}")
if(go_version VERSION_LESS go_minimum_version)
    message(FATAL_ERROR "HCS requires Go ${go_minimum_version} or newer; found ${go_version}.")
endif()
message(STATUS "HCS tools: Go ${go_version}")

if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    set(go_arch amd64)
elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "X86")
    set(go_arch 386)
elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64")
    set(go_arch arm64)
else()
    message(FATAL_ERROR "Unsupported HCS target architecture: ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
endif()

# Go compiles whole packages; track added/removed package files for Ninja.
file(GLOB_RECURSE hcs_go_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cmd/*.go"
    "${CMAKE_CURRENT_SOURCE_DIR}/internal/*.go"
)
list(FILTER hcs_go_sources EXCLUDE REGEX "_test\\.go$")

# Keep dependency metadata, caches and temporary objects inside this build.
configure_file(go.mod go.mod COPYONLY)
configure_file(go.sum go.sum COPYONLY)
set(go_build_flags -mod=readonly -buildvcs=false)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND go_build_flags "-gcflags=all=-N -l")
endif()

add_custom_command(
    OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/sandbox-runner.exe"
        "${CMAKE_CURRENT_BINARY_DIR}/workspace-delete.exe"
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/go/cache"
        "${CMAKE_CURRENT_BINARY_DIR}/go/mod"
        "${CMAKE_CURRENT_BINARY_DIR}/go/path"
        "${CMAKE_CURRENT_BINARY_DIR}/go/tmp"
    COMMAND "${CMAKE_COMMAND}" -E env
        "GOCACHE=${CMAKE_CURRENT_BINARY_DIR}/go/cache"
        "GOMODCACHE=${CMAKE_CURRENT_BINARY_DIR}/go/mod"
        "GOPATH=${CMAKE_CURRENT_BINARY_DIR}/go/path"
        "GOTMPDIR=${CMAKE_CURRENT_BINARY_DIR}/go/tmp"
        "TMP=${CMAKE_CURRENT_BINARY_DIR}/go/tmp"
        "TEMP=${CMAKE_CURRENT_BINARY_DIR}/go/tmp"
        GOOS=windows "GOARCH=${go_arch}" CGO_ENABLED=0
        GOTOOLCHAIN=local GOWORK=off GOENV=off GOFLAGS=
        "${GO_EXECUTABLE}" build ${go_build_flags}
        "-modfile=${CMAKE_CURRENT_BINARY_DIR}/go.mod"
        -o "${CMAKE_CURRENT_BINARY_DIR}/"
        ./cmd/sandbox-runner ./cmd/workspace-delete
    DEPENDS ${hcs_go_sources} go.mod go.sum "${GO_EXECUTABLE}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Building HCS tools (${CMAKE_BUILD_TYPE}, windows/${go_arch})"
    VERBATIM
)

# Preserve the HCS layout expected by InstallHcsCodexSupportFiles.
set(hcs_support_outputs)
foreach(support_file IN ITEMS hcs-workspace.config.toml rules/hcs-workspace.rules)
    get_filename_component(support_directory "${support_file}" DIRECTORY)
    set(support_output "${CMAKE_CURRENT_BINARY_DIR}/${support_file}")
    add_custom_command(
        OUTPUT "${support_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_BINARY_DIR}/${support_directory}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/${support_file}" "${support_output}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${support_file}"
        VERBATIM
    )
    list(APPEND hcs_support_outputs "${support_output}")
endforeach()

add_custom_target(hcs_tools ALL DEPENDS
    "${CMAKE_CURRENT_BINARY_DIR}/sandbox-runner.exe"
    "${CMAKE_CURRENT_BINARY_DIR}/workspace-delete.exe"
    ${hcs_support_outputs}
)
