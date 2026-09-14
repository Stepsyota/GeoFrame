function(geoframe_set_warnings target)
    target_compile_features(${target} PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()
