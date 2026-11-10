# M1.7: Apply diagnostics exclusively to ChromascopeIP-owned C++ targets.
# Imported OpenCV headers and the vendored Dear ImGui implementation are not
# converted into first-party warning-policy targets.
option(CHROMASCOPE_WARNINGS_AS_ERRORS "Fail on first-party compiler warnings" OFF)
option(CHROMASCOPE_ENABLE_SANITIZERS "Instrument first-party C++ with ASan and UBSan" OFF)

if(CHROMASCOPE_ENABLE_SANITIZERS)
    if(MSVC OR WIN32 OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        message(FATAL_ERROR "M1.7 sanitizer instrumentation requires Linux GCC/Clang")
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(FATAL_ERROR "Sanitizers must use a Debug configuration in M1.7")
    endif()
endif()

function(chromascope_apply_quality target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(CHROMASCOPE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wformat=2)
        if(CHROMASCOPE_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
    if(CHROMASCOPE_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        # Required on executables too: instrumentation in static libraries
        # needs the sanitizer runtimes at the final link step.
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-sanitize-recover=all)
    endif()
endfunction()
