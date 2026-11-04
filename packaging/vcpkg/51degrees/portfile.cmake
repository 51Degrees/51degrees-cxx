if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO 51Degrees/51degrees-cxx
    REF e6024b51fff295314e72310007da17bfb0dbb737
    SHA512 2abc37b11bcf3e5d4d936f71622af9f12811dd9a1121f9dd2d1472c71689fbfa2fd6b544eef8c914fe1b64e1ac88aae957ff7adc6a130f185ab0a0bea6f4813b
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTING=OFF
        -DFIFTYONE_BUILD_EXAMPLES=OFF
        -DFIFTYONE_CXX_BOTH_PRODUCTS=ON
        "-DFIFTYONE_VERSION=${VERSION}"
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME 51degrees CONFIG_PATH lib/cmake/51degrees)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE" "${SOURCE_PATH}/NOTICE")
