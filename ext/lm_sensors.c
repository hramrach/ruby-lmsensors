#include <sensors/error.h>
#include <sensors/sensors.h>
#include "ruby.h"
#include "extconf.h"

static VALUE lm_sensors_version()
{
	return rb_str_new2(libsensors_version);
}

static void sensors_free(void * ptr)
{
	sensors_cleanup_r(ptr);
}

static const rb_data_type_t sensors_data = {
	.wrap_struct_name = "LMSensors",
	{
		/*
		 * There is nothing to mark here - no ruby data is referenced from C
		 * Calculating the memory used by a sensors config is futile, skip
		 */
		.dfree = sensors_free,
	},
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static VALUE sensors_alloc(VALUE klass)
{
	/*
	 * All examples show how to allocate data here.
	 * There is nothing to allocate, though.
	 * The argument is needed to do the allocation, and it cannot be passed to here.
	 */
	return TypedData_Wrap_Struct(klass, &sensors_data, 0);
}

static VALUE sensors_initialize(VALUE self, VALUE config_file)
{
	const char *filename = StringValueCStr(config_file);
	FILE *fd;
	sensors_config *config;
	int err;
	TypedData_Get_Struct(self, sensors_config, &sensors_data, config);
	if (config) /* Should be 0 from sensors_alloc */
		rb_raise(rb_eRuntimeError, "Config already initialized!");
	fd = fopen(filename, "rb");
	if (!fd)
		rb_sys_fail(filename);
	config = sensors_init_r(fd, &err); /* here the allocation really happens */
	if (!config)
		rb_raise(rb_eRuntimeError, sensors_strerror(err));
	RTYPEDDATA_DATA(self) = (config);
	return self;
}

void Init_lm_sensors()
{
	VALUE klass = rb_define_class("LMSensors", rb_cObject);
	rb_define_singleton_method(klass, "version", lm_sensors_version, 0);
	rb_define_alloc_func(klass, sensors_alloc);
	rb_define_method(klass, "initialize", sensors_initialize, 1);
}
