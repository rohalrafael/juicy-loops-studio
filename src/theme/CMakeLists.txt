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
