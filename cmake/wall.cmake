function(set_wall target)
  if(NOT MSVC AND CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "Using -Wall/-Werror")
    target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic -Werror)

    # These are pissing me off so much. I cannot debug with these warnings.
    target_compile_options(${target} PRIVATE -Wno-unused-variable)
    target_compile_options(${target} PRIVATE -Wno-unused-lambda-capture)
  endif()
endfunction()
