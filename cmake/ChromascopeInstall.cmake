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
if(CHROMASCOPE_V101_P5_PORTABLE)
    install(FILES docs/PACKAGED_README_V1_0_1_WINDOWS.md
        DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}" RENAME "README.md")
    install(FILES docs/PORTABLE_RELEASE_V1_0_1_WINDOWS.md
        DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}" RENAME "PORTABLE_RELEASE_V1_0_1.md")
elseif(CHROMASCOPE_V101_P3_PORTABLE)
    install(FILES docs/PACKAGED_README_V1_0_1.md
        DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}" RENAME "README.md")
    install(FILES docs/PORTABLE_RELEASE_V1_0_1.md
        DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}")
else()
    install(FILES docs/PACKAGED_README.md
        DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}" RENAME "README.md")
endif()

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
if(CHROMASCOPE_V101_P3_PORTABLE OR CHROMASCOPE_V101_P5_PORTABLE)
    list(REMOVE_ITEM CHROMASCOPE_SHIPPED_USER_GUIDES
        docs/INSTALLATION_1_0_0.md
        docs/RELEASE_EXAMPLES_1_0_0.md
        docs/RELEASE_NOTES_1_0_0.md)
endif()
install(FILES ${CHROMASCOPE_SHIPPED_USER_GUIDES}
    DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}/docs")
install(DIRECTORY licenses/
    DESTINATION "${CHROMASCOPE_INSTALL_DOCDIR}/licenses"
    FILES_MATCHING PATTERN "*.txt" PATTERN "*.md")
install(FILES models/README.md models/NANODET_NOTICES.md
    models/selected_model.json models/model_provenance.json
    models/person_evaluation_policy.json
    DESTINATION "${CHROMASCOPE_INSTALL_DATADIR}/models")

# Exactly one reviewed model is distributable in the v1.0.1 portable formats.
# P5 will enable CHROMASCOPE_V101_P5_PORTABLE with its own Windows release gate.
# Never change the v1.0.0 install tree or install models/ wholesale.
if(CHROMASCOPE_V101_P3_PORTABLE OR CHROMASCOPE_V101_P5_PORTABLE)
    set(_approved_nanodet "${CMAKE_CURRENT_SOURCE_DIR}/models/object_detection_nanodet_2022nov.onnx")
    if(NOT EXISTS "${_approved_nanodet}")
        message(FATAL_ERROR "v1.0.1 portable model is missing")
    endif()
    file(SHA256 "${_approved_nanodet}" _approved_nanodet_sha)
    if(NOT _approved_nanodet_sha STREQUAL
            "4b82da9944b88577175ee23a459dce2e26e6e4be573def65b1055dc2d9720186")
        message(FATAL_ERROR "v1.0.1 portable model SHA-256 mismatch")
    endif()
    install(FILES "${_approved_nanodet}" models/LICENSE
        DESTINATION "${CHROMASCOPE_INSTALL_DATADIR}/models")
endif()

# The four Skittles camera photographs are owned by the project author and
# explicitly approved for v1.0.1 redistributions only. Do not install arbitrary
# images or use directory-wide install rules: this is an exact byte allowlist.
if(CHROMASCOPE_V101_P3_PORTABLE OR CHROMASCOPE_V101_P5_PORTABLE)
    set(_skittles_names s1.jpg s2.jpg s3.jpg s4.jpg)
    set(_skittles_hashes
        "515454a4a2fd8625f831baea9c2ca39d614877fbfb489be3c55333c8ce56b2aa"
        "9589016e0b67b5bfb40bf1a8bd8738651d0337a7777f20796c6ff3df53985a6e"
        "1b928939929d46cd6dc94489c4c16c94361c77c5d72d15eec500884bdec69491"
        "9d44d9c1b66dd748804620a610188bc1dff6ca1465cafaff1a3b06316e4f633c")
    set(_skittles_dir "${CMAKE_CURRENT_SOURCE_DIR}/samples/skittles")
    foreach(_index RANGE 0 3)
        list(GET _skittles_names ${_index} _filename)
        list(GET _skittles_hashes ${_index} _expected_sha)
        set(_photo "${_skittles_dir}/${_filename}")
        if(NOT EXISTS "${_photo}" OR IS_SYMLINK "${_photo}")
            message(FATAL_ERROR "Missing or symlinked approved Skittles sample: ${_filename}")
        endif()
        file(SHA256 "${_photo}" _photo_sha)
        if(NOT _photo_sha STREQUAL _expected_sha)
            message(FATAL_ERROR "Modified Skittles sample: ${_filename}")
        endif()
    endforeach()
    file(GLOB _all_skittles RELATIVE "${_skittles_dir}" "${_skittles_dir}/*")
    list(SORT _all_skittles)
    if(NOT "${_all_skittles}" STREQUAL "s1.jpg;s2.jpg;s3.jpg;s4.jpg")
        message(FATAL_ERROR "Unapproved file in Skittles samples")
    endif()
    install(FILES
        "${_skittles_dir}/s1.jpg" "${_skittles_dir}/s2.jpg"
        "${_skittles_dir}/s3.jpg" "${_skittles_dir}/s4.jpg"
        DESTINATION "${CHROMASCOPE_INSTALL_DATADIR}/samples/skittles")
endif()

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
# P5/P3 portable builds have a reviewed, checksum-pinned model and a v1.0.1
# documentation allowlist; historical v1.0.0 remains model-free.
set(_chromascope_v101_portable_stage OFF)
if(CHROMASCOPE_V101_P3_PORTABLE OR CHROMASCOPE_V101_P5_PORTABLE)
    set(_chromascope_v101_portable_stage ON)
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
            "-DCHROMASCOPE_V101_PORTABLE_STAGE=${_chromascope_v101_portable_stage}"
            "-DCHROMASCOPE_SMOKE_LIBDIR=${_chromascope_smoke_libdir}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyInstallStage.cmake")
    set_tests_properties(chromascope_install_stage PROPERTIES
        LABELS "packaging;integration;smoke;security" TIMEOUT 90)
endif()
