#include <sensors/error.h>
#include <sensors/sensors.h>
#include "ruby.h"
#include "extconf.h"

#if defined(DEBUG)
#define D 1
#else
#define D 0
#endif

static VALUE rb_str_new_static_cstr(const char *str)
{
	return rb_str_new_static(str, strlen(str));
}

static VALUE lm_sensors_version()
{
	return rb_str_new_static_cstr(libsensors_version);
}

static void sensors_free(void * config)
{
	if (D) fprintf(stderr, "%s config %p\n", __FUNCTION__, config);
	sensors_cleanup_r(config);
}

static const rb_data_type_t sensors_data = {
	.wrap_struct_name = "LMSensors",
	{
		/*
		 * There is nothing to mark here - no ruby data is referenced from C
		 * Additional bookkeeping is stored in a ruby instance variable, no need to mark
		 * Calculating the memory used by a sensors config is futile, skip
		 */
		.dfree = sensors_free,
	},
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static const rb_data_type_t sensor_chip_data = {
	.wrap_struct_name = "LMSensorsChip",
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static void dbg_inspect(const char *msg1, const char *msg2, VALUE object)
{
	VALUE str = rb_str_new_static_cstr(msg1);
	if (msg2) {
		rb_str_cat_cstr(str, " ");
		rb_str_cat_cstr(str, msg2);
	}
	rb_str_cat_cstr(str, " ");
	rb_funcall(str, rb_intern("concat"), 1, rb_funcall(object, rb_intern("inspect"), 0));
	rb_funcall(rb_const_get(rb_cObject, rb_intern("STDERR")), rb_intern("puts"), 1, str);
}

static VALUE chip_class = Qnil;
static VALUE exc_class = Qnil;

static VALUE sensors_alloc(VALUE klass)
{
	/*
	 * All examples show how to allocate data here.
	 * There is nothing to allocate, though.
	 * The argument is needed to do the allocation, and it cannot be passed to here.
	 */
	VALUE self = TypedData_Wrap_Struct(klass, &sensors_data, 0);
	if (D) fprintf(stderr, "%s allocated value 0x%016lx\n", __FUNCTION__, self);
	if (D) dbg_inspect(__FUNCTION__, "self", self);
	return self;
}

static VALUE sensors_initialize(VALUE self, VALUE config_file)
{
	const char *filename = StringValueCStr(config_file);
	FILE *fd;
	sensors_config *config;
	int err;

	if (D) fprintf(stderr, "%s config file %s\n", __FUNCTION__, filename);
	if (D) dbg_inspect(__FUNCTION__, "self", self);

	TypedData_Get_Struct(self, sensors_config, &sensors_data, config);
	if (config) /* Should be 0 from sensors_alloc */
		rb_raise(exc_class, "Config already initialized!");
	fd = fopen(filename, "r");
	if (!fd)
		rb_sys_fail(filename);
	config = sensors_init_r(fd, &err); /* here the allocation really happens */
	if (!config)
		rb_raise(exc_class, sensors_strerror(err));
	RTYPEDDATA_DATA(self) = config;
	if (D) fprintf(stderr, "%s config %p\n", __FUNCTION__, config);
	if (D) dbg_inspect(__FUNCTION__, "self", self);

	return self;
}

static VALUE sensor_chip_alloc(VALUE klass) /* somehow this is required although never used */
{
	VALUE self = TypedData_Wrap_Struct(klass, &sensor_chip_data, 0);
	if (D) fprintf(stderr, "%s allocated value 0x%016lx\n", __FUNCTION__, self);
	if (D) dbg_inspect(__FUNCTION__, "self", self);
	return self;
}

static VALUE sensors_each_chip(VALUE self)
{
	/* FIXME this returns new objects wrapping the same chips each iteration, the chips should be cached/singleton */
	if (D) dbg_inspect(__FUNCTION__, "self", self);

	if (rb_block_given_p()) {
		const sensors_chip_name *chip;
		sensors_config *config;
		int cnum = 0;
		TypedData_Get_Struct(self, sensors_config, &sensors_data, config);
		if (D) fprintf(stderr, "%s config %p\n", __FUNCTION__, config);
		while (!!(chip = sensors_get_detected_chips_r(config, NULL, &cnum))) {
			VALUE chip_obj;
			if (D) fprintf(stderr, "%s chip %p\n", __FUNCTION__, chip);
			/* The chip objects do not have a ruby constructor, this is the only way to create them */
			chip_obj = TypedData_Wrap_Struct(chip_class, &sensor_chip_data, (void *)chip);
			if (D) dbg_inspect(__FUNCTION__, "chip", chip_obj);
			rb_iv_set(chip_obj, "parent", self); /* invisible instance variable to keep config reference */
			rb_yield(chip_obj);
		}
	} else {
		/* do the enumerator magic, except in C */
		return rb_funcall(self, rb_intern("to_enum"), 1, ID2SYM(rb_intern("each")));
	}

	return self;
}

static VALUE sensor_chip_path(VALUE self)
{
	const sensors_chip_name *chip;

	if (D) dbg_inspect(__FUNCTION__, "self", self);

	TypedData_Get_Struct(self, sensors_chip_name, &sensor_chip_data, chip);
	if (D) fprintf(stderr, "%s chip %p\n", __FUNCTION__, chip);

	return rb_str_new_static_cstr(chip->path); /* data is stored in config, reference held above */
}

static VALUE sensor_chip_name(VALUE self)
{
	const sensors_chip_name *chip;
	VALUE result;
	int length;
	char * buffer;

	if (D) dbg_inspect(__FUNCTION__, "self", self);

	TypedData_Get_Struct(self, sensors_chip_name, &sensor_chip_data, chip);
	if (D) fprintf(stderr, "%s chip %p\n", __FUNCTION__, chip);

	length = sensors_snprintf_chip_name(NULL, 0, chip);
	if (D) fprintf(stderr, "%s chip name length %d\n", __FUNCTION__, length);

	if (length < 0)
		rb_raise(exc_class, "%s: %s", __FUNCTION__, sensors_strerror(length));

	length += 1; /* The sensors snprintf zero-terminates, need extra space */
	/* would be cool to use the ruby string buffer but did not figure out how to set the length after it's written */
	buffer = malloc(length);
	sensors_snprintf_chip_name(buffer, length, chip);
	if (D) fprintf(stderr, "%s chip name %.*s\n", __FUNCTION__, length, buffer);
	result = rb_str_new(buffer, length - 1);
	free(buffer);

	return result;
}

void Init_lm_sensors()
{
	VALUE klass = rb_define_class("LMSensors", rb_cObject);
	rb_define_singleton_method(klass, "version", lm_sensors_version, 0);
	exc_class = rb_define_class_under(klass, "Error", rb_eRuntimeError);
	rb_define_alloc_func(klass, sensors_alloc);
	chip_class = rb_define_class_under(klass, "Chip", rb_cObject);
	rb_define_alloc_func(chip_class, sensor_chip_alloc);
	rb_define_method(klass, "initialize", sensors_initialize, 1);
	rb_include_module(klass, rb_mEnumerable);
	rb_define_method(klass, "each", sensors_each_chip, 0);
	rb_define_method(klass, "each_chip", sensors_each_chip, 0);
	rb_define_method(chip_class, "path", sensor_chip_path, 0);
	rb_define_method(chip_class, "name", sensor_chip_name, 0);
}
