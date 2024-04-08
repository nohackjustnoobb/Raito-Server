from conan import ConanFile


class RaitoServer(ConanFile):
    requires = [
        "crowcpp-crow/1.0+5",
        "nlohmann_json/3.11.3",
        "re2/20231101",
        "fmt/10.2.1",
        "hash-library/8.0",
        "cpr/1.10.5",
        "freeimage/3.18.0",
        "sqlitecpp/3.3.1",
        "lexbor/2.3.0",
    ]
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def configure(self):
        self.options["libtiff"].lzma = False
        self.options["freeimage"].with_jxr = False
        self.options["freeimage"].with_eigen = False
        self.options["freeimage"].with_openexr = False
