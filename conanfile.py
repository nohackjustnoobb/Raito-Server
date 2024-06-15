from conan import ConanFile


class RaitoServer(ConanFile):
    requires = [
        "crowcpp-crow/1.2.0",
        "drogon/1.9.3",
        "nlohmann_json/3.11.3",
        "re2/20231101",
        "fmt/10.2.1",
        "hash-library/8.0",
        "cpr/1.10.5",
        "freeimage/3.18.0",
        "lexbor/2.3.0",
        "soci/4.0.3",
    ]
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def configure(self):
        self.options["libtiff"].lzma = False
        self.options["freeimage"].with_jxr = False
        self.options["freeimage"].with_eigen = False
        self.options["drogon"].with_orm = False
        self.options["drogon"].with_boost = False
        self.options["soci"].with_sqlite3 = True
        self.options["soci"].with_mysql = True
        self.options["soci"].with_postgresql = True
        self.options["libcurl"].with_ssl = "openssl"
