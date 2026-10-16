FetchContent_Declare(nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa)
FetchContent_MakeAvailable(nlohmann_json)

add_library(jls_theme STATIC
    builtin_themes.cpp
    theme.cpp)

target_include_directories(jls_theme PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(jls_theme PRIVATE nlohmann_json::nlohmann_json)

if(MSVC)
    target_compile_options(jls_theme PRIVATE /W4 /WX)
else()
    target_compile_options(jls_theme PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
