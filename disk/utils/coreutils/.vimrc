
let g:syntastic_c_check_header = 1
" let g:syntastic_c_no_default_include_dirs = 1
let g:syntastic_c_auto_refresh_includes = 1
let g:syntastic_always_populate_loc_list = 1
"let g:syntastic_auto_loc_list = 1

let g:syntastic_c_include_dirs = [ '../../root/usr/include', './include' ]
let g:syntastic_c_compiler_options = "-std=gnu99 -Wall -Wsign-compare -O2 -fno-strict-aliasing -fno-omit-frame-pointer"
let g:syntastic_c_gcc_exec = 'i686-protura-gcc'

