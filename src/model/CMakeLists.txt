add_library(jls_model STATIC
    document.cpp
    project.cpp)

target_include_directories(jls_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(jls_model PUBLIC jls_engine)

if(MSVC)
    target_compile_options(jls_model PRIVATE /W4 /WX)
else()
    target_compile_options(jls_model PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
