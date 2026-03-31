include_guard(GLOBAL)

function(cuda_test_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} INTERFACE /W4 /permissive-)
  else()
    target_compile_options(${target} INTERFACE -Wall -Wextra -Wpedantic)
  endif()
endfunction()
