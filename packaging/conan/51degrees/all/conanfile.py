import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get, rmdir

required_conan_version = ">=2.0.9"


class FiftyOneDegreesConan(ConanFile):
    name = "51degrees"
    description = ("Device Detection, User-Agent and Client Hints parsing, "
                   "crawler detection, and IP geolocation for C and C++, "
                   "on-premise.")
    license = "EUPL-1.2"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://51degrees.com/device-detection"
    topics = ("device-detection", "user-agent", "user-agent-parser",
              "client-hints", "crawler-detection", "ip-geolocation",
              "ip-intelligence", "geoip")
    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }
    implements = ["auto_shared_fpic"]

    def layout(self):
        cmake_layout(self, src_folder="src")

    def validate(self):
        check_min_cppstd(self, 17)
        # Nothing in the libraries is marked for export, which a Windows DLL
        # needs.
        if self.settings.os == "Windows" and self.options.shared:
            raise ConanInvalidConfiguration(
                f"{self.ref} cannot be built as a shared library on Windows")

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["BUILD_TESTING"] = False
        tc.cache_variables["FIFTYONE_BUILD_EXAMPLES"] = False
        # Gives each product's copy of the common C++ library its own linked
        # names, so one program can use both products.
        tc.cache_variables["FIFTYONE_CXX_BOTH_PRODUCTS"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        licenses = os.path.join(self.package_folder, "licenses")
        copy(self, "LICENSE", self.source_folder, licenses)
        copy(self, "NOTICE", self.source_folder, licenses)
        cmake = CMake(self)
        cmake.install()
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "51degrees")

        system_libs = []
        if self.settings.os in ["Linux", "FreeBSD"]:
            system_libs = ["m", "pthread", "atomic"]

        # IP Intelligence's copy of common is compiled with these, and the
        # headers depend on them.
        ipi_defines = [
            "FIFTYONE_DEGREES_LARGE_DATA_FILE_SUPPORT",
            "_FILE_OFFSET_BITS=64",
            "FIFTYONE_DEGREES_REDUCED_FILE",
            "FIFTYONE_DEGREES_IPI_SYMBOLS",
            "FIFTYONE_DEGREES_IPI_CXX_SYMBOLS",
        ]

        def component(name, requires, defines=None):
            info = self.cpp_info.components[name]
            info.set_property("cmake_target_name", f"51Degrees::{name}")
            info.libs = [f"fiftyone-{name}"]
            info.requires = requires
            info.defines = defines or []
            info.system_libs = system_libs

        component("common-c", [])
        component("common-cxx", ["common-c"])
        component("device-detection-c", ["common-c"])
        component("device-detection-cxx", ["device-detection-c", "common-cxx"])
        component("hash-c", ["device-detection-c"])
        component("hash-cxx", ["hash-c", "device-detection-cxx"])
        component("common-c-ipi", [], ipi_defines)
        component("common-cxx-ipi", ["common-c-ipi"])
        component("ip-intelligence-c", ["common-c-ipi"])
        component("ip-intelligence-cxx", ["ip-intelligence-c", "common-cxx-ipi"])
