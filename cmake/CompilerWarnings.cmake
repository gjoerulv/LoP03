# High warning levels applied to PROJECT targets only (never to dependencies).

function(set_project_warnings target)
    set(MSVC_WARNINGS
        /W4
        /permissive-
    )
    set(GCC_CLANG_WARNINGS
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
    )

    if(MSVC)
        set(WARNINGS ${MSVC_WARNINGS})
    else()
        set(WARNINGS ${GCC_CLANG_WARNINGS})
    endif()

    if(CRYSTAL_WARNINGS_AS_ERRORS)
        if(MSVC)
            list(APPEND WARNINGS /WX)
        else()
            list(APPEND WARNINGS -Werror)
        endif()
    endif()

    target_compile_options(${target} PRIVATE ${WARNINGS})
endfunction()
