from conan import ConanFile


class RaitoServer(ConanFile):
    requires = [
        "crowcpp-crow/1.0+5",
        "nlohmann_json/3.11.3",
        "re2/20231101",
        "fmt/10.2.1",
        "openssl/3.2.1",
        "cpr/1.10.5",
        "freeimage/3.18.0",
        "sqlitecpp/3.3.1",
    ]
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def configure(self):
        self.options["libtiff"].lzma = False
