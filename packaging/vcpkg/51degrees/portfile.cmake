if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO 51Degrees/51degrees-cxx
    REF 34585ffb7db554a41e4668f7c495ecc68c29ca0e
    SHA512 e1cd7ed44390ad707a5de679dd1e480f7fa6328a4f3ee044d243eac00745d0d1e97b200ad0270783f890036fa9752b9de42976b2fbfb6ebcab46a0edc6379c4e
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
