if &cp | set nocp | endif
let s:cpo_save=&cpo
set cpo&vim
imap <S-Tab> <Plug>SuperTabBackward
inoremap <C-Tab> 	
noremap! <Right> 
noremap! <Left> 
noremap! <Down> 
noremap! <Up> 
map <silent> h :let @" = expand("%:t:r"):!qb64_parse "
map <silent> k :let @" = expand("%:t:r"):!qb64_p "
map <silent> l :let @" = expand("%:t:r"):!qb64_n "
nnoremap <silent>  <Left>
nnoremap <silent> 	 d?\.
nnoremap <silent> <NL> <Down>
vnoremap <silent>  
nnoremap <silent>  <Up>
nnoremap <silent>  <Right>
vnoremap  >gv
nnoremap <silent>  :set foldmethod=syntax:set foldmethod=manual
nmap <silent>  :NERDTreeToggle
nnoremap <silent>  :setlocal spelllang=en_us
map <silent> l >
map <silent> k +
map <silent> j -
map <silent> h <
nnoremap <silent>  k :make qemu-test-multiboot
nnoremap <silent>  q :QFix
nnoremap <silent>  j :make
map ,' :s/^/'/:nohlsearch
nnoremap <silent> :WQ :wq
nnoremap <silent> :Q :q
nnoremap <silent> :W :w
nnoremap I i
nnoremap <silent> J :bp
nnoremap <silent> K :bn
nnoremap P Ph
noremap W @q
nnoremap <silent> _ f_x~
nmap gx <Plug>NetrwBrowseX
nmap j gj
nmap k gk
nnoremap <silent> <Plug>NetrwBrowseX :call netrw#NetrwBrowseX(expand("<cfile>"),0)
nnoremap <M-F8> :call NextColor(0)
nnoremap <S-F8> :call NextColor(-1)
nnoremap <F8> :call NextColor(1)
noremap <Right> ""
noremap <Left> ""
noremap <Down> ""
noremap <Up> ""
imap 	 <Plug>SuperTabForward
inoremap <silent>  
imap <silent>  pa
inoremap <silent> { {}O
let &cpo=s:cpo_save
unlet s:cpo_save
set autoindent
set backspace=indent,eol,start
set directory=/var/tmp/swp/
set expandtab
set fileencodings=ucs-bom,utf-8,default,latin1
set guifont=Terminus\ 8
set helplang=en
set hidden
set laststatus=2
set nomore
set mouse=a
set ruler
set runtimepath=~/.vim/bundle/vundle,~/.vim/bundle/vim-fugitive,~/.vim/bundle/vim-colorschemes,~/.vim/bundle/vim-airline,~/.vim/bundle/supertab,~/.vim/bundle/vim-distinguished,~/.vim/bundle/tagbar,~/.vim,/usr/share/vim/vimfiles,/usr/share/vim/vim74,/usr/share/vim/vimfiles/after,~/.vim/after,~/.vim/bundle/vundle/,~/.vim/bundle/vundle/after,~/.vim/bundle/vim-fugitive/after,~/.vim/bundle/vim-colorschemes/after,~/.vim/bundle/vim-airline/after,~/.vim/bundle/supertab/after,~/.vim/bundle/vim-distinguished/after,~/.vim/bundle/tagbar/after
set shiftwidth=4
set showtabline=2
set suffixes=.bak,~,.swp,.o,.info,.aux,.log,.dvi,.bbl,.blg,.brf,.cb,.ind,.idx,.ilg,.inx,.out,.toc
set tabline=%!airline#extensions#tabline#get()
set tabstop=4
set wildmenu
set wildmode=longest,list,full
" vim: set ft=vim :
