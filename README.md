ruby bindings for lm-sensors.

Currently needs patched version from https://github.com/hramrach/lm-sensors/tree/testing

The json.rb test replicates the sensors -J option.

It would be nice to get the constants and enums out of the header with sometning like ruby-ffi. So far only interfaces returning strings are used.
