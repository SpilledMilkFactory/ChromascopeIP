# M11.1: installation *layout* only. Runtime library bundling and per-OS
# redistributable/license qualification are separate M11.2-M11.9 gates.
include(GNUInstallDirs)

set(CHROMASCOPE_INSTALL_DATADIR "${CMAKE_INSTALL_DATAROOTDIR}/chromascopeip")
set(CHROMASCOPE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/chromascopeip")
set(CHROMASCOPE_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}/chromascopeip")
foreach(_destination IN ITEMS CMAKE_INSTALL_BINDIR CHROMASCOPE_INSTALL_DATADIR
                              CHROMASCOPE_INSTALL_DOCDIR CHROMASCOPE_INSTALL_LIBDIR)
    if(IS_ABSOLUTE "${${_destination}}" OR "${${_destination}}" MATCHES "(^|/)\\.\\.(/|$)")
        message(FATAL_ERROR "${_destination} must be relative and must not traverse parents")
    endif()
endforeach()

# Compute layout-relative paths (no source, build, install-prefix or home paths
# compiled into the application). Allow customary GNUInstallDirs overrides.
file(RELATIVE_PATH _chromascope_data_from_bin
    "/${CMAKE_INSTALL_BINDIR}" "/${CHROMASCOPE_INSTALL_DATADIR}")
file(RELATIVE_PATH _chromascope_lib_from_bin
    "/${CMAKE_INSTALL_BINDIR}" "/${CHROMASCOPE_INSTALL_LIBDIR}")
target_compile_definitions(chromascope_core PRIVATE
    "CHROMASCOPE_DATA_FROM_BINDIR=\"${_chromascope_data_from_bin}\"")

# Do not preserve the OpenCV build prefix in installed ELF RUNPATHs. The future
# distribution-specific packages stage approved libraries under private lib/;
# on Windows the runtime loader searches adjacent DLLs / managed system libs.
if(UNIX AND NOT APPLE)
    foreach(_frontend IN ITEMS chromascope-cli chromascope-gui)
        if(TARGET ${_frontend})
            set_target_properties(${_frontend} PROPERTIES
                BUILD_WITH_INSTALL_RPATH FALSE
                INSTALL_RPATH "\$ORIGIN/${_chromascope_lib_from_bin}"
                INSTALL_RPATH_USE_LINK_PATH FALSE)
        endif()
    endforeach()
endif()

install(TARGETS chromascope-cli
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
if(TARGET chromascope-gui)
    install(TARGETS chromascope-gui
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

# Explicit allowlist for *binary-distribution* user documentation. Do not
# package the repository-wide docs/ tree: that includes source-only engineering
# history, qualification contracts, audit drafts and internal provenance data.
# License and third-party notices are installed independently below.
install(FILES LICENSE
    DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}")
install(FILES docs/PACKAGED_SECURITY.md
    DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}" RENAME "SECURITY.md")
install(FILES docs/PACKAGED_README.md
    DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}" RENAME "README.md")

# This list is intentionally reviewed as a release packaging surface. Do not
# replace it with a glob or install(DIRECTORY docs/ ...).
set(CHROMASCOPE_SHIPPED_USER_GUIDES
    docs/INSTALLATION_1_0_0.md
    docs/USER_GUIDE.md
    docs/CLI_REFERENCE.md
    docs/PRIVACY_GUIDE.md
    docs/TROUBLESHOOTING.md
    docs/RELEASE_EXAMPLES_1_0_0.md
    docs/RELEASE_NOTES_1_0_0.md
    docs/CHANGELOG.md
    docs/IMAGE_FILTER_PROFILES.md
    docs/SEGMENTATION_PROFILES.md
    docs/MEDIA_RIGHTS.md
    docs/RESOURCE_BUDGETS.md
)
install(FILES ${CHROMASCOPE_SHIPPED_USER_GUIDES}
    DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}/docs")
install(DIRECTORY licenses/
    DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}/licenses"
    FILES_MATCHING PATTERN "*.txt" PATTERN "*.md")
install(FILES models/README.md models/NANODET_NOTICES.md
    models/selected_model.json models/model_provenance.json
    models/person_evaluation_policy.json
    DESTINATION "${CHROMASCOPE_INSTALL_DATADIR}/models")

# Stage in the *build* tree, so source checkouts and user media are unaffected.
# For the relocated CLI *smoke only*, provide the build's OpenCV libdir via
# the loader environment. This is NOT bundled into the installation tree and
# does not purport to prove standalone redistribution compatibility.
set(_chromascope_smoke_libdir "")
if(UNIX AND NOT APPLE AND OpenCV_DIR)
    get_filename_component(_chromascope_opencv_prefix "${OpenCV_DIR}/../../.." ABSOLUTE)
    if(IS_DIRECTORY "${_chromascope_opencv_prefix}/lib")
        set(_chromascope_smoke_libdir "${_chromascope_opencv_prefix}/lib")
    endif()
endif()
# A CTest installs, moves the staged prefix, and audits the resulting tree.
if(BUILD_TESTING AND CHROMASCOPE_BUILD_TESTS)
    add_test(NAME chromascope_install_stage
        COMMAND "${CMAKE_COMMAND}"
            "-DCHROMASCOPE_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            "-DCHROMASCOPE_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DCHROMASCOPE_BINDIR=${CMAKE_INSTALL_BINDIR}"
            "-DCHROMASCOPE_DATADIR=${CHROMASCOPE_INSTALL_DATADIR}"
            "-DCHROMASCOPE_DOCDIR=${CHROMASCOPE_INSTALL_DOCDIR}"
            "-DCHROMASCOPE_CONFIG=$<CONFIG>"
            "-DCHROMASCOPE_SMOKE_LIBDIR=${_chromascope_smoke_libdir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyInstallStage.cmake")
    set_tests_properties(chromascope_install_stage PROPERTIES
        LABELS "packaging;integration;smoke;security" TIMEOUT 90)
endif()
