require 'mkmf'
append_ldflags(['-lsensors'])
create_header
create_makefile 'lmsensors'
