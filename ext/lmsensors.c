#include <sensors/error.h>
#include <sensors/sensors.h>
#include "ruby.h"
#include "extconf.h"

#define VERSION "0.2"

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

/* StringValueCStr requires a lvalue, cannot be called on return value directly */
static const char *class_name(VALUE obj)
{
	obj = rb_funcall(rb_funcall(obj, rb_intern("class"), 0), rb_intern("to_s"), 0);
	return StringValueCStr(obj);
}

static const char *inspect_str(VALUE obj)
{
	obj = rb_funcall(obj, rb_intern("inspect"), 0);
	return StringValueCStr(obj);
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

static const rb_data_type_t sensor_feature_data = {
	.wrap_struct_name = "LMSensorsFeature",
	.flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

static const rb_data_type_t sensor_subfeature_data = {
	.wrap_struct_name = "LMSensorsSubFeature",
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
static VALUE feature_class = Qnil;
static VALUE subfeature_class = Qnil;
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

static VALUE sensor_feature_alloc(VALUE klass) /* somehow this is required although never used */
{
	VALUE self = TypedData_Wrap_Struct(klass, &sensor_feature_data, 0);
	if (D) fprintf(stderr, "%s allocated value 0x%016lx\n", __FUNCTION__, self);
	if (D) dbg_inspect(__FUNCTION__, "self", self);
	return self;
}

static VALUE sensor_subfeature_alloc(VALUE klass) /* somehow this is required although never used */
{
	VALUE self = TypedData_Wrap_Struct(klass, &sensor_subfeature_data, 0);
	if (D) fprintf(stderr, "%s allocated value 0x%016lx\n", __FUNCTION__, self);
	if (D) dbg_inspect(__FUNCTION__, "self", self);
	return self;
}

/*
 * Generic caching of wrapped singleton C data
 * It is probably overkill for sensors - the data wrapper objects are about the size of the proxy cache objects.
 * Nonethless, it's useful for debugging to get consistent objects, and in general to figure out how to cache.
 * Very verbose debug prints are included.
 */
static VALUE cache_get(VALUE self, const char *var_name, const void *ptr)
{
	VALUE cache = rb_iv_get(self, var_name);
	VALUE idx = RB_UINT2NUM((uintptr_t)ptr);
	VALUE live = Qnil;
	VALUE object;
	const char *self_name = class_name(self);

	if (D) fprintf(stderr, "%s %s %s %s 0x%016lx\n", __FUNCTION__,
			self_name,
			var_name,
			class_name(cache),
			cache); /* may contain stale weak references, cannot inspect */

	if (cache == Qnil) { /* cache not initialized, create */
		VALUE newcache = rb_funcall(rb_const_get(rb_cObject, rb_intern("Hash")), rb_intern("new"), 0);
		/* use hidden instance variable not staritn with @ - it's not
		 * visible to ruby code but the GC marks it automagically, no
		 * need to do anything special on the C side */
		rb_iv_set(self, var_name, newcache);
		cache = newcache;
		if (D) fprintf(stderr, "%s %s %s %s 0x%016lx\n",
				__FUNCTION__,
				self_name,
				var_name,
				class_name(cache),
				cache);
		return Qnil;
	}

	/* fetch weakref from cache */
	object = rb_funcall(cache, rb_intern("[]"), 1, idx);
	if (object != Qnil) /* a weakref was found but may be stale */
		live = rb_funcall(object, rb_intern("weakref_alive?"), 0);

	if (D) fprintf(stderr, "%s %s %s %p live %s\n",
			__FUNCTION__,
			self_name,
			var_name,
			ptr,
			inspect_str(live));
	if (live != Qfalse && live != Qnil) { /* stale weakref returns nil here */
		/* get the original object from weakref, undocumented */
		object = rb_funcall(object, rb_intern("__getobj__"), 0);
		return object;
	}

	return Qnil;
}

static void cache_set(VALUE self, const char *var_name, const void *ptr, VALUE object)
{
	VALUE cache = rb_iv_get(self, var_name);
	VALUE idx = RB_UINT2NUM((uintptr_t)ptr);
	const char *self_name = class_name(self);
	VALUE weakref;
	if (D) fprintf(stderr, "%s %s %s %p %s\n",
			__FUNCTION__,
			self_name,
			var_name,
			ptr,
			inspect_str(object));
	weakref = rb_funcall(rb_const_get(rb_cObject, rb_intern("WeakRef")), rb_intern("new"), 1, object);
	/* The cache was created when trying to get the object, assume it's available */
	rb_funcall(cache, rb_intern("[]="), 2, idx, weakref);
}

static VALUE sensors_each_chip(VALUE self)
{
	if (D) dbg_inspect(__FUNCTION__, "self", self);

	if (rb_block_given_p()) {
		const sensors_chip_name *chip;
		sensors_config *config;
		int cnum = 0;
		const char * chips_var = "chips";
		TypedData_Get_Struct(self, sensors_config, &sensors_data, config);
		if (D) fprintf(stderr, "%s config %p\n", __FUNCTION__, config);
		while (!!(chip = sensors_get_detected_chips_r(config, NULL, &cnum))) {
			VALUE chip_obj;
			if (D) fprintf(stderr, "%s chip %p\n", __FUNCTION__, chip);
			/* Already wrapped chip data is retrieved from object cache */
			chip_obj = cache_get(self, chips_var, chip);
			if (chip_obj != Qnil) {
				if (D) dbg_inspect(__FUNCTION__, "cached chip", chip_obj);
			} else {
				/* The chip objects do not have a ruby constructor, this is the only way to create them */
				chip_obj = TypedData_Wrap_Struct(chip_class, &sensor_chip_data, (void *)chip);
				if (D) dbg_inspect(__FUNCTION__, "chip", chip_obj);
				rb_iv_set(chip_obj, "parent", self); /* invisible instance variable to keep config reference */
				rb_iv_set(chip_obj, "config", self);
				cache_set(self, chips_var, chip, chip_obj);
			}
			rb_yield(chip_obj);
		}
	} else {
		/* do the enumerator magic, except in C */
		return rb_funcall(self, rb_intern("to_enum"), 1, ID2SYM(rb_intern("each")));
	}

	return self;
}

static VALUE sensors_each_feature(VALUE self)
{
	if (D) dbg_inspect(__FUNCTION__, "self", self);

	if (rb_block_given_p()) {
		const sensors_chip_name *chip;
		VALUE config_obj;
		sensors_config *config;
		int fnum = 0;
		const sensors_feature *feature;
		const char * features_var = "features";
		TypedData_Get_Struct(self, sensors_chip_name, &sensor_chip_data, chip);
		config_obj = rb_iv_get(self, "config");
		TypedData_Get_Struct(config_obj, sensors_config, &sensors_data, config);
		if (D) fprintf(stderr, "%s config %p\n", __FUNCTION__, config);
		while (!!(feature = sensors_get_features_r(config, chip, &fnum))) {
			VALUE feature_obj;
			if (D) fprintf(stderr, "%s feature %p\n", __FUNCTION__, feature);
			feature_obj = cache_get(self, features_var, feature);
			if (feature_obj != Qnil) {
				if (D) dbg_inspect(__FUNCTION__, "cached feature", feature_obj);
			} else {
				feature_obj = TypedData_Wrap_Struct(feature_class, &sensor_feature_data, (void *)feature);
				if (D) dbg_inspect(__FUNCTION__, "feature", feature_obj);
				rb_iv_set(feature_obj, "config", config_obj);
				rb_iv_set(feature_obj, "parent", self);
				cache_set(self, features_var, feature, feature_obj);
			}
			rb_yield(feature_obj);
		}
	} else {
		/* do the enumerator magic, except in C */
		return rb_funcall(self, rb_intern("to_enum"), 1, ID2SYM(rb_intern("each")));
	}

	return self;
}

static VALUE sensors_each_subfeature(VALUE self)
{
	if (D) dbg_inspect(__FUNCTION__, "self", self);

	if (rb_block_given_p()) {
		VALUE chip_obj;
		const sensors_chip_name *chip;
		VALUE config_obj;
		sensors_config *config;
		int fnum = 0;
		const sensors_feature *feature;
		const sensors_subfeature *subfeature;
		const char * subfeatures_var = "subfeatures";
		TypedData_Get_Struct(self, sensors_feature, &sensor_feature_data, feature);
		config_obj = rb_iv_get(self, "config");
		chip_obj = rb_iv_get(self, "parent");
		TypedData_Get_Struct(config_obj, sensors_config, &sensors_data, config);
		if (D) fprintf(stderr, "%s config %p\n", __FUNCTION__, config);
		TypedData_Get_Struct(chip_obj, sensors_chip_name, &sensor_chip_data, chip);
		if (D) fprintf(stderr, "%s chip %p\n", __FUNCTION__, chip);
		while (!!(subfeature = sensors_get_all_subfeatures_r(config, chip, feature, &fnum))) {
			VALUE subfeature_obj;
			if (D) fprintf(stderr, "%s subfeature %p\n", __FUNCTION__, subfeature);
			subfeature_obj = cache_get(self, subfeatures_var, subfeature);
			if (subfeature_obj != Qnil) {
				if (D) dbg_inspect(__FUNCTION__, "cached subfeature", subfeature_obj);
			} else {
				subfeature_obj = TypedData_Wrap_Struct(subfeature_class, &sensor_subfeature_data, (void *)subfeature);
				if (D) dbg_inspect(__FUNCTION__, "subfeature", subfeature_obj);
				rb_iv_set(subfeature_obj, "config", config_obj);
				rb_iv_set(subfeature_obj, "parent", self);
				cache_set(self, subfeatures_var, subfeature, subfeature_obj);
			}
			rb_yield(subfeature_obj);
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

static VALUE sensor_chip_adapter(VALUE self)
{
	const sensors_chip_name *chip;
	sensors_config *config;
	VALUE config_obj;
	const char *adapter;

	if (D) dbg_inspect(__FUNCTION__, "self", self);

	TypedData_Get_Struct(self, sensors_chip_name, &sensor_chip_data, chip);
	if (D) fprintf(stderr, "%s chip %p\n", __FUNCTION__, chip);

	config_obj = rb_iv_get(self, "config");
	TypedData_Get_Struct(config_obj, sensors_config, &sensors_data, config);
	if (D) fprintf(stderr, "%s config %p\n", __FUNCTION__, config);

	adapter = sensors_get_adapter_name_r(config, &chip->bus);

	if (adapter)
		return rb_str_new_static_cstr(adapter);
	return Qnil;
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

static VALUE sensor_feature_name(VALUE self)
{
	const sensors_feature *feature;

	if (D) dbg_inspect(__FUNCTION__, "self", self);

	TypedData_Get_Struct(self, sensors_feature, &sensor_feature_data, feature);
	if (D) fprintf(stderr, "%s feature %p\n", __FUNCTION__, feature);

	return rb_str_new_static_cstr(feature->name); /* data is stored in config, reference held above */
}

static VALUE sensor_feature_label(VALUE self)
{
	const sensors_feature *feature;
	const sensors_chip_name *chip;
	sensors_config *config;
	VALUE chip_obj, config_obj;
	VALUE result;
	char *label;

	if (D) dbg_inspect(__FUNCTION__, "self", self);
	TypedData_Get_Struct(self, sensors_feature, &sensor_feature_data, feature);

	config_obj = rb_iv_get(self, "config");
	TypedData_Get_Struct(config_obj, sensors_config, &sensors_data, config);
	if (D) fprintf(stderr, "%s config %p\n", __FUNCTION__, config);
	chip_obj = rb_iv_get(self, "parent");
	TypedData_Get_Struct(chip_obj, sensors_chip_name, &sensor_chip_data, chip);
	if (D) fprintf(stderr, "%s chip %p\n", __FUNCTION__, chip);

	label = sensors_get_label_r(config, chip, feature);
	if (D) fprintf(stderr, "%s chip label %s\n", __FUNCTION__, label);
	result = rb_str_new_cstr(label);
	free(label);

	return result;
}

static VALUE sensor_subfeature_name(VALUE self)
{
	const sensors_subfeature *subfeature;

	if (D) dbg_inspect(__FUNCTION__, "self", self);

	TypedData_Get_Struct(self, sensors_subfeature, &sensor_subfeature_data, subfeature);
	if (D) fprintf(stderr, "%s subfeature %p\n", __FUNCTION__, subfeature);

	return rb_str_new_static_cstr(subfeature->name); /* data is stored in config, reference held above */
}

static VALUE sensor_subfeature_quant(VALUE self)
{
	const sensors_subfeature *subfeature;

	if (D) dbg_inspect(__FUNCTION__, "self", self);

	TypedData_Get_Struct(self, sensors_subfeature, &sensor_subfeature_data, subfeature);
	if (D) fprintf(stderr, "%s subfeature %p\n", __FUNCTION__, subfeature);

	return rb_str_new_static_cstr(sensors_get_quantity_name(sensors_get_subfeature_quantity(subfeature->type)));
}

static VALUE sensor_subfeature_unit(VALUE self)
{
	const sensors_subfeature *subfeature;

	if (D) dbg_inspect(__FUNCTION__, "self", self);

	TypedData_Get_Struct(self, sensors_subfeature, &sensor_subfeature_data, subfeature);
	if (D) fprintf(stderr, "%s subfeature %p\n", __FUNCTION__, subfeature);

	return rb_str_new_static_cstr(sensors_get_quantity_unit(sensors_get_subfeature_quantity(subfeature->type)));
}

static VALUE sensor_subfeature_value(VALUE self)
{
	const sensors_subfeature *subfeature;
	const sensors_chip_name *chip;
	sensors_config *config;
	VALUE chip_obj, config_obj;
	double value;

	if (D) dbg_inspect(__FUNCTION__, "self", self);

	TypedData_Get_Struct(self, sensors_subfeature, &sensor_subfeature_data, subfeature);
	if (D) fprintf(stderr, "%s subfeature %p\n", __FUNCTION__, subfeature);
	config_obj = rb_iv_get(self, "config");
	TypedData_Get_Struct(config_obj, sensors_config, &sensors_data, config);
	if (D) fprintf(stderr, "%s config %p\n", __FUNCTION__, config);
	chip_obj = rb_iv_get(self, "parent");
	chip_obj = rb_iv_get(chip_obj, "parent");
	TypedData_Get_Struct(chip_obj, sensors_chip_name, &sensor_chip_data, chip);

	sensors_get_value_r(config, chip, subfeature->number, &value);

	return DBL2NUM(value);
}

void Init_lmsensors()
{
	VALUE klass;
	rb_funcall(rb_cObject, rb_intern("require"), 1, rb_str_new_static_cstr("weakref"));
	klass = rb_define_class("LMSensors", rb_cObject);
	rb_define_const(klass, "VERSION", rb_str_new_static_cstr(VERSION));
	exc_class = rb_define_class_under(klass, "Error", rb_eRuntimeError);
	rb_define_singleton_method(klass, "version", lm_sensors_version, 0);
	rb_define_alloc_func(klass, sensors_alloc);
	chip_class = rb_define_class_under(klass, "Chip", rb_cObject);
	rb_define_alloc_func(chip_class, sensor_chip_alloc);
	rb_define_method(klass, "initialize", sensors_initialize, 1);
	rb_include_module(klass, rb_mEnumerable);
	rb_define_method(klass, "each", sensors_each_chip, 0);
	rb_define_method(klass, "each_chip", sensors_each_chip, 0);
	rb_define_method(chip_class, "path", sensor_chip_path, 0);
	rb_define_method(chip_class, "adapter", sensor_chip_adapter, 0);
	rb_define_method(chip_class, "name", sensor_chip_name, 0);
	rb_include_module(chip_class, rb_mEnumerable);
	feature_class = rb_define_class_under(chip_class, "Feature", rb_cObject);
	rb_define_method(chip_class, "each", sensors_each_feature, 0);
	rb_define_method(chip_class, "each_feature", sensors_each_feature, 0);
	rb_define_alloc_func(feature_class, sensor_feature_alloc);
	rb_include_module(feature_class, rb_mEnumerable);
	rb_define_method(feature_class, "name", sensor_feature_name, 0);
	rb_define_method(feature_class, "label", sensor_feature_label, 0);
	subfeature_class = rb_define_class_under(feature_class, "SubFeature", rb_cObject);
	rb_define_alloc_func(subfeature_class, sensor_subfeature_alloc);
	rb_define_method(feature_class, "each", sensors_each_subfeature, 0);
	rb_define_method(feature_class, "each_subfeature", sensors_each_subfeature, 0);
	rb_define_method(subfeature_class, "name", sensor_subfeature_name, 0);
	rb_define_method(subfeature_class, "quantity", sensor_subfeature_quant, 0);
	rb_define_method(subfeature_class, "unit", sensor_subfeature_unit, 0);
	rb_define_method(subfeature_class, "value", sensor_subfeature_value, 0);
}
