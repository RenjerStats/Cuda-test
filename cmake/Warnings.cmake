include_guard(GLOBAL)

function(cuda_test_apply_warnings target)
  if(MSVC)
    # Keep warning policy on host C++ compilation only. Passing raw MSVC warning flags through
    # nvcc breaks CUDA compilation under the VS generator because they leak into the CUDA front-end.
    target_compile_options(
      ${target}
      INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:/W4>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
    )
  else()
    # The same restriction applies here: CUDA compilation is handled separately from host C++.
    target_compile_options(
      ${target}
      INTERFACE
        $<$<COMPILE_LANGUAGE:CXX>:-Wall>
        $<$<COMPILE_LANGUAGE:CXX>:-Wextra>
        $<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>
    )
  endif()
endfunction()
