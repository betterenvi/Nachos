#!/bin/bash
ctags -R
cd bin; ctags -R
cd ../filesys/; ctags -R
cd ../machine/; ctags -R
cd ../network/; ctags -R
cd ../test/; ctags -R
cd ../threads/; ctags -R
cd ../userprog/; ctags -R
cd ../vm/; ctags -R
