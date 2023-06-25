local version = "0.1.0"
set_version(version)

target("misp")
set_kind("binary")
add_files("src/*.c")
add_includedirs("include/")
set_license("GPL-3.0-or-later")

add_defines("MISP_VERSION=\""..version.."\"")
add_rules("mode.debug")
