#define _USE_MATH_DEFINES

#include <math.h>
#include <ctype.h>
#include <string.h>
#include "vibrant_logs.h"
#include "dynamic_array_spellbook.h"
#include "dynamic_map_spellbook.h"
#include "snaky_strings.h"

// number of bytes a snaky_string will increase by as an attrib string is parsed
#define STR_CAPACITY_INCREASE 50

static bool init = false;

// map of constant names to constant values:
typedef struct constant_registry
{
	char **keys;
	float *values;
	size_t size, capacity;
	bool alloc_failure;
} constant_registry;
static constant_registry constants = {0};

// maps of function names to generic pointers:

typedef struct eval_func_registry
{
	char **keys;
	snaky_eval_func *values;
	size_t size, capacity;
	bool alloc_failure;
} eval_func_registry;
static eval_func_registry eval_functions = {0};

typedef struct function
{
	snaky_data_type return_type;
	void *fn_ptr;
} function;
typedef struct func_registry
{
	char **keys;
	function *values;
	size_t size, capacity;
	bool alloc_failure;
} func_registry;
static func_registry functions = {0};

static float factorial(float f)
{
	if(f == 0)
		return 1.0f;

	float ff = f;
	while(f > 1)
		ff *= (f -= 1);

	return ff;
}
static float rads(float f)
{
	return f * (M_PI / 180.0f);
}

static int encrypt_key = INT_MAX % 26;

int snaky_init()
{
	if(init)
	{
		vl_log(VL_ERROR, "Cannot re-initialize snaky_strings!\n");
		return 0;
	}

	dynmaps_init(&constants);
	if(constants.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for constants registry!\n");
		return 0;
	}

	dynmaps_init(&eval_functions);
	if(eval_functions.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for evaluation functions registry!\n");
		return 0;
	}

	dynmaps_init(&functions);
	if(functions.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for functions registry!\n");
		return 0;
	}

	// make 'init' true here since define_constant(...) will fail if it's false
	init = true;

	// define some constants
	if(!snaky_define_constant("E", M_E))
		return 0;
	if(!snaky_define_constant("PI", M_PI))
		return 0;
	if(!snaky_define_constant("INF", INFINITY))
		return 0;
	if(!snaky_define_constant("NAN", NAN))
		return 0;

	// define some functions
	if(!snaky_define_eval_function("factorial", factorial))
		return 0;
	if(!snaky_define_eval_function("sin", sinf))
		return 0;
	if(!snaky_define_eval_function("cos", cosf))
		return 0;
	if(!snaky_define_eval_function("tan", tanf))
		return 0;
	if(!snaky_define_eval_function("sqrt", sqrtf))
		return 0;
	if(!snaky_define_eval_function("rads", rads))
		return 0;
	if(!snaky_define_eval_function("round", roundf))
		return 0;
	if(!snaky_define_eval_function("floor", floorf))
		return 0;
	if(!snaky_define_eval_function("ceil", ceilf))
		return 0;

	vl_log(VL_SUCCESS, "snaky_strings successfully initialized!\n");

	return 1;
}
bool snaky_is_init()
{
	return init;
}
int snaky_shutdown()
{
	if(!init)
	{
		vl_log(VL_ERROR, "snaky_strings was never initialized, cannot shutdown!\n");
		return 0;
	}

	dynmaps_free_strkey(&constants);
	dynmaps_free_strkey(&eval_functions);
	dynmaps_free_strkey(&functions);

	vl_log(VL_SUCCESS, "snaky_strings successfully shut down!\n");

	return 1;
}

int snaky_create_str(snaky_string *str, char *data)
{
	if(!str || !data)
		return 0;

	// clear out old memory if necessary
	if(str->data)
		snaky_free_str(str);

	str->data = strdup(data);
	if(!str->data)
	{
		vl_log(VL_ERROR, "Failed to create snaky_string! Memory allocation failed!\n");
		return 0;
	}
	str->len = strlen(data);
	// always one more byte to account for '\0'
	str->size = str->len + 1;

	return 1;
}
int snaky_prepare_str(snaky_string *str, size_t bytes)
{
	if(!str)
		return 0;

	// clear out old memory if necessary
	if(str->data)
		snaky_free_str(str);

	str->data = calloc(bytes, sizeof(char));
	if(!str->data)
	{
		vl_log(VL_ERROR, "Failed to prepare snaky_string! Memory allocation failed!\n");
		return 0;
	}
	str->len = 0;
	str->size = bytes;

	return 1;
}
void snaky_free_str(snaky_string *str)
{
	if(!str || !str->data)
		return;

	free(str->data);
	str->data = NULL;
	str->len = 0;
	str->size = 0;
}
int snaky_strcpy(snaky_string *dest, const char *src)
{
	if(!dest || !src)
		return 0;

	// compare lengths of each string
	size_t curr_size = dest->size;
	size_t src_len = strlen(src);

	// src_len + 1 represents the total amount of bytes 'src' takes up
	if(src_len + 1 > curr_size)
	{
		// realloc necessary

		size_t bytes_needed = (src_len + 1) - curr_size;
		if(dest->size >= SIZE_MAX - bytes_needed)
		{
			vl_log(VL_ERROR, "snaky_string size is too large! SIZE_MAX reached.\n");
			return 0;
		}
		size_t new_size = dest->size + bytes_needed;

		if(!snaky_realloc_str(dest, new_size))
			return 0;
	}

	// perform strcpy here:
	strcpy(dest->data, src);
	dest->len = strlen(dest->data);

	return 1;
}
int snaky_realloc_str(snaky_string *str, size_t bytes)
{
	if(!str)
		return 0;

	char *new_data = realloc(str->data, bytes);
	if(!new_data)
	{
		vl_log(VL_ERROR, "Failed to reallocate snaky_string's memory!\n");
		return 0;
	}
	str->data = new_data;
	str->size = bytes;
	str->len = strlen(str->data);

	return 1;
}

static const char *find_end_of_group(const char *start, char open, char close)
{
	size_t depth = 0;

	for(const char *p = start; *p; ++p)
	{
		if(*p == open)
			depth++;
		else if(*p == close)
		{
			depth--;

			// when the depth matches the starting depth, the end of the group is found
			if(depth == 0)
				// p will point to char directly after the matching closing character
				return p + 1;
		}
	}

	return NULL;
}
static const char *find_end_of_object(const char *start)
{
	return find_end_of_group(start, '{', '}');
}
static const char *find_end_of_array(const char *start)
{
	return find_end_of_group(start, '[', ']');
}
static const char *find_end_of_attribs(const char *start)
{
	return find_end_of_group(start, '<', '>');
}

// skip all whitespace in a string until a valid char is reached
static void skip_whitespace(const char **str, size_t *out_num_spaces)
{
	if(out_num_spaces)
		*out_num_spaces = 0;
	while(**str && isspace(**str))
	{
		if(out_num_spaces)
			*out_num_spaces += 1;
		(*str)++;

		// after moving to next char, check for a comment
		if(strncmp(*str, "/*", 2) == 0)
		{
			const char *comment_origin = *str;

			while(**str && strncmp(*str, "*/", 2) != 0)
				(*str)++;

			if(!**str)
			{
				vl_log(VL_ERROR, "Expected '*/' to close off a comment in string: '%s'!\n", comment_origin);
				return;
			}

			// when the '*/' is found, skip those last 2 chars
			(*str) += 2;
		}
	}
}

/*
   - 'start' should point to the character directly after the ':' character
   - 'out_jump' is how many characters are in between the last char in the attrib name and the '=' sign
   - return -1 on complete failure, 0 on successful call but no data type resolved, 1 on complete success
*/
static int resolve_data_type(const char *start, snaky_data_type *out_data_type, size_t *out_jump)
{
	if(!out_data_type)
		return 0;
	// default to invalid value
	*out_data_type = SNAKY_INVALID_VALUE;

	const char *origin = start;

	size_t num_spaces = 0;

	// get number of spaces skipped:
	skip_whitespace(&start, &num_spaces);
	// add that to jump as well
	if(out_jump)
		*out_jump += num_spaces;

	// if the first char is a letter, skip to the ':'
	if(isalpha(*start))
	{
		while(*start && *start != ':')
			start++;
	}
	// if now on the ':', skip it
	if(*start == ':')
		start++;

	// now find which data type the user provided
	if(strncmp(start, "char", 4) == 0)
	{
		*out_data_type = SNAKY_CHAR;
		if(out_jump)
			*out_jump += 4;
	}
	else if(strncmp(start, "bool", 4) == 0)
	{
		*out_data_type = SNAKY_BOOL;
		if(out_jump)
			*out_jump += 4;
	}
	else if(strncmp(start, "int", 3) == 0)
	{
		*out_data_type = SNAKY_INT;
		if(out_jump)
			*out_jump += 3;
	}
	else if(strncmp(start, "float", 5) == 0)
	{
		*out_data_type = SNAKY_FLOAT;
		if(out_jump)
			*out_jump += 5;
	}
	else if(strncmp(start, "double", 6) == 0)
	{
		*out_data_type = SNAKY_DOUBLE;
		if(out_jump)
			*out_jump += 6;
	}
	else if(strncmp(start, "array", 5) == 0)
	{
		*out_data_type = SNAKY_ARRAY;
		if(out_jump)
			*out_jump += 5;
	}
	else if(strncmp(start, "object", 6) == 0)
	{
		*out_data_type = SNAKY_OBJECT;
		if(out_jump)
			*out_jump += 6;
	}
	else if(strncmp(start, "string", 6) == 0)
	{
		*out_data_type = SNAKY_STRING;
		if(out_jump)
			*out_jump += 6;
	}
	else
	{
		vl_log(VL_ERROR, "Unknown or missing data type in string: '%s'! Defaulting to SNAKY_STRING!\n", origin);

		// default to the string type when no type is given
		*out_data_type = SNAKY_STRING;
		return 1;
	}

	return -1;
}

static void return_start_pos(const char *attribs_start, const char **out_start_pos)
{
	// find start pos of next attribute
	if(out_start_pos)
	{
		while(*attribs_start && *attribs_start != ',' && *attribs_start != '<')
			attribs_start++;

		if(!*attribs_start)
			*out_start_pos = NULL;
		else
		{
			skip_whitespace(&attribs_start, NULL);
			*out_start_pos = attribs_start;
		}
	}
}

static int parse_value(const char *value_start, snaky_string *sbuffer, char *buffer, size_t buffer_size, const char **out_start_pos, bool break_on_comma, const char *(*end_pos_fn)(const char*), const char *origin)
{
	// determine end of the value in the string
	const char *end_pos = end_pos_fn(origin);

	size_t i = 0;
	while(*value_start && value_start != end_pos)
	{
		if(break_on_comma && *value_start == ',')
			break;

		if(sbuffer)
		{
			if(i + 1 >= sbuffer->size)
			{
				if(!snaky_realloc_str(sbuffer, sbuffer->size + STR_CAPACITY_INCREASE))
					return 0;
			}
		}
		else
			if(i + 1 >= buffer_size)
				break;

		char next = *value_start++;
		/*if(isspace(next))
			continue;*/

		if(sbuffer)
			sbuffer->data[i++] = next;
		else if(buffer)
			buffer[i++] = next;
	}

	if(sbuffer)
	{
		if(sbuffer->data[i - 1] == '\n')
			sbuffer->data[i - 1] = '\0';
		sbuffer->data[i] = '\0';
	}
	else if(buffer && buffer_size > 0)
	{
		if(buffer[i - 1] == '\n')
			buffer[i - 1] = '\0';
		buffer[i] = '\0';
	}

	return_start_pos(value_start, out_start_pos);

	return 1;
}

// if using a snaky_string, 'sbuffer' won't be NULL, but if using a normal stack-allocated buffer, it will be NULL
static int parse_target_attrib(const char *attribs_start, snaky_string *sbuffer, char *buffer, size_t buffer_size, const char *attrib_name, size_t attrib_len, const char **out_start_pos, snaky_data_type *out_data_type)
{
	const char *origin = attribs_start;

	skip_whitespace(&attribs_start, NULL);

	while(*attribs_start && *attribs_start != '=')
		attribs_start++;

	if(!*attribs_start)
	{
		vl_log(VL_ERROR, "Expected '=' after attribute: '%s'!\n", attrib_name);
		return 0;
	}

	// go to the character after the '='
	attribs_start++;
	skip_whitespace(&attribs_start, NULL);

	if(!*attribs_start)
	{
		vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", origin);
		return 0;
	}

	// if the start of the attribute is an opening of another attribute string, error out
	if(*attribs_start == '<')
	{
		vl_log(VL_ERROR, "Nested attribute strings must be wrapped around '{}'!\n");
		return 0;
	}

	// determine if a nested attribute or string is given
	if(*attribs_start == '{')
	{
		if(out_data_type)
			*out_data_type = SNAKY_OBJECT;
		return parse_value(attribs_start, sbuffer, buffer, buffer_size, out_start_pos, false, find_end_of_object, origin);
	}
	else if(*attribs_start == '[')
	{
		if(out_data_type)
			*out_data_type = SNAKY_ARRAY;
		return parse_value(attribs_start, sbuffer, buffer, buffer_size, out_start_pos, false, find_end_of_array, origin);
	}

	// if not a nested string, copy the attrib value normally
	return parse_value(attribs_start, sbuffer, buffer, buffer_size, out_start_pos, true, find_end_of_attribs, origin);
}
static int prepare_parse_target_attrib(const char *str, snaky_string *sbuffer, char *buffer, size_t buffer_size, const char *attrib_name, const char **out_start_pos, snaky_data_type *out_data_type)
{
	size_t attrib_len = attrib_name ? strlen(attrib_name) : 0;

	if(!str || strlen(str) == 0 || !attrib_name || attrib_len == 0)
		return 0;

	// see if user is trying to find a nested attribute :

	// the resolved attrib name is the final attribute name after the last '.' character in the original attribute name string
	const char *resolved_attrib_name = attrib_name;
	size_t resolved_attrib_len = 0;
	size_t last_nested_attrib_pos = 0;
	int i = 0;
	for(const char *p = attrib_name; *p; ++p)
	{
		if(*p == '.')
			last_nested_attrib_pos = i;
		i++;
	}

	resolved_attrib_name = last_nested_attrib_pos > 0 ? attrib_name + last_nested_attrib_pos + 1 : attrib_name;
	if(!*resolved_attrib_name)
	{
		vl_log(VL_ERROR, "Unexpected termination of attribute name: '%s'!\n", attrib_name);
		return 0;
	}
	resolved_attrib_len = strlen(resolved_attrib_name);

	// see if user searching for a value within an array (requires '[N]' in 'attrib_name')
	char index_value[SNAKY_BUF_SIZE + 1] = {0};
	for(const char *p = attrib_name; *p; ++p)
	{
		if(*p == '[')
		{
			// skip the '['
			p++;

			size_t i = 0;
			while(*p && *p != ']')
			{
				if(i + 1 >= sizeof(index_value))
					break;
				index_value[i++] = *p++;
			}
			index_value[i] = '\0';
			break;
		}
	}

	// if index_value length exceeds 0, user provided an index
	size_t index_value_len = strlen(index_value);
	if(index_value_len > 0)
	{
		// now parse the index value
		int s = 0;
		int index = snaky_parse_int(str, index_value, &s);
		if(s == 0)
		{
			vl_log(VL_ERROR, "Failed to parse target index: '%s'!\n", attrib_name);
			return 0;
		}

		// validate index
		if(index < 0)
		{
			vl_log(VL_ERROR, "Cannot have a negative index: '%s'!\n", attrib_name);
			return 0;
		}

		// because the attrib name contains a '[N]' part, the actual length of the array's name in the string does not include these characters
		size_t actual_array_name_len = 0;
		for(const char *a = attrib_name; *a; ++a)
		{
			if(*a == '[')
				break;
			actual_array_name_len++;
		}

		// now find the array in the attribute string, go to the specific index, and parse the value there
		for(const char *p = str; *p; ++p)
		{
			if(strncmp(p, resolved_attrib_name, actual_array_name_len) == 0)
			{
				const char *array_start = p;

				// jump to the character right after the '['
				while(*p && *p != '[')
					p++;
				if(!*p)
				{
					vl_log(VL_ERROR, "Unexpected termination of string: '%s'!\n", str);
					return 0;
				}

				const char *start_of_array = p;

				// when the array is found, go to the specific index in the array
				size_t curr_index = 0;
				while(*p && *p != ']')
				{
					// see if a ',' is found
					if(*p == ',')
						curr_index++;

					// go to next valid character
					p++;
					skip_whitespace(&p, NULL);

					// when the current index matches the target index:
					if(curr_index == index)
						return parse_value(p, sbuffer, buffer, buffer_size, out_start_pos, false, find_end_of_array, start_of_array);
				}

				if(curr_index < index)
				{
					vl_log(VL_ERROR, "Index %zu is out of bounds for array '%s'!\n", index, array_start);
					return 0;
				}
			}
		}

		return 0;
	}

	// see if resolved attrib name is invalid (starts with number of symbol)
	if(!isalpha(*resolved_attrib_name))
	{
		vl_log(VL_ERROR, "Argument name cannot start with a symbol or number: '%s'!\n", resolved_attrib_name);
		return 0;
	}

	bool in_attrib_string = false;

	for(const char *p = str; *p; ++p)
	{
		// get char
		char c = *p;

		// if the first char is a group indicator, skip the string entirely (unless user is searching for nested attribute )
		if(*p == '{')
		{
			// if user included a '.' to find a nested attribute, search for that attribute now:
			if(last_nested_attrib_pos > 0)
			{
				// search the rest of the string:
				while(*p)
				{
					const char *attribs_start = p + 1;

					skip_whitespace(&attribs_start, NULL);

					if(!*attribs_start)
					{
						vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", str);
						return 0;
					}

					if(strncmp(attribs_start, resolved_attrib_name, resolved_attrib_len) == 0)
					{
						resolve_data_type(attribs_start, out_data_type, NULL);
						return parse_target_attrib(attribs_start, sbuffer, buffer, buffer_size, resolved_attrib_name, resolved_attrib_len, out_start_pos, out_data_type);
					}

					++p;
				}
			}

			if(!*p)
			{
				vl_log(VL_ERROR, "Unexpected termination of string: '%s'!\n", str);
				return 0;
			}

			continue;
		}

		// when the first '<' or ',' is encountered, compare attrib to the target arg
		if(c == '<' && !in_attrib_string)
		{
			// now the parser is in an attribute string
			in_attrib_string = true;

			// compare the attribute with the target attribute
			const char *attribs_start = p + 1;

			skip_whitespace(&attribs_start, NULL);

			if(!*attribs_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", str);
				return 0;
			}

			if(strncmp(attribs_start, attrib_name, attrib_len) == 0)
			{
				resolve_data_type(attribs_start, out_data_type, NULL);
				return parse_target_attrib(attribs_start, sbuffer, buffer, buffer_size, attrib_name, attrib_len, out_start_pos, out_data_type);
			}

			continue;
		}

		// parse all subsequent attributes in an attrib string
		if(c == ',' && in_attrib_string)
		{
			// same logic as above
			const char *attribs_start = p + 1;

			// skip whitespace
			skip_whitespace(&attribs_start, NULL);

			if(!*attribs_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", str);
				return 0;
			}

			if(strncmp(attribs_start, attrib_name, attrib_len) == 0)
			{
				resolve_data_type(attribs_start, out_data_type, NULL);
				return parse_target_attrib(attribs_start, sbuffer, buffer, buffer_size, attrib_name, attrib_len, out_start_pos, out_data_type);
			}

			continue;
		}

		// exit an attrib string when '>' is found
		if(c == '>' && in_attrib_string)
			in_attrib_string = false;
	}

	return 0;
}
int snaky_parse_target_attrib_buf(const char *str, char *buffer, size_t buffer_size, const char *attrib_name, const char **out_start_pos, snaky_data_type *out_data_type)
{
	return prepare_parse_target_attrib(str, NULL, buffer, buffer_size, attrib_name, out_start_pos, out_data_type);
}
int snaky_parse_target_attrib_str(const char *str, snaky_string *buffer, const char *attrib_name, const char **out_start_pos, snaky_data_type *out_data_type)
{
	return prepare_parse_target_attrib(str, buffer, NULL, 0, attrib_name, out_start_pos, out_data_type);
}

static int parse_attrib(const char *attribs_start, snaky_string *name_str, snaky_string *value_str, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, const char **out_start_pos, snaky_data_type *out_data_type)
{
	const char *origin = attribs_start;

	skip_whitespace(&attribs_start, NULL);

	// 'attribs_start' points to the first char in the attrib name
	size_t i = 0;
	bool data_type_resolved = false;
	while(*attribs_start && *attribs_start != '=')
	{
		if(name_str)
		{
			if(i + 1 >= name_str->size)
			{
				if(!snaky_realloc_str(name_str, name_str->size + STR_CAPACITY_INCREASE))
					return 0;
			}
		}
		else if(name_buffer)
		{
			if(i + 1 >= name_buffer_size)
			{
				vl_log(VL_WARNING, "Name buffer isn't large enough: size = %zu!\n", name_buffer_size);
				break;
			}
		}

		char next = *attribs_start++;
		// when parsing names, spaces don't matter
		if(isspace(next))
			continue;

		// see if a ':' is found
		if(next == ':')
		{
			size_t jump = 0;
			resolve_data_type(attribs_start, out_data_type, &jump);
			attribs_start += jump;
			data_type_resolved = true;
			continue;
		}

		if(name_buffer)
			name_buffer[i++] = next;
		else if(name_str)
			name_str->data[i++] = next;
	}

	if(name_buffer)
		name_buffer[i] = '\0';
	else if(name_str)
		name_str->data[i] = '\0';

	if(!*attribs_start)
	{
		vl_log(VL_ERROR, "Expected '=' after attribute: '%s'!\n", name_buffer ? name_buffer : name_str->data);
		return 0;
	}

	// find the '=' for this attrib name
	while(*attribs_start && *attribs_start != '=')
		attribs_start++;

	// if '=' was found, skip it
	attribs_start++;

	// skip all whitespace:
	skip_whitespace(&attribs_start, NULL);

	// if the start of the attribute is an opening of another attribute string, error out
	if(*attribs_start == '<')
	{
		vl_log(VL_ERROR, "Nested attribute strings must be wrapped around '{}'!\n");
		return 0;
	}

	if(!*attribs_start)
	{
		vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", origin);
		return 0;
	}

	if(*attribs_start == '[')
	{
		if(out_data_type)
			*out_data_type = SNAKY_ARRAY;
		return parse_value(attribs_start, value_str, value_buffer, value_buffer_size, out_start_pos, false, find_end_of_array, origin);
	}
	else if(*attribs_start == '{')
	{
		if(out_data_type)
			*out_data_type = SNAKY_OBJECT;
		return parse_value(attribs_start, value_str, value_buffer, value_buffer_size, out_start_pos, false, find_end_of_object, origin);
	}

	// if not a nested string, copy the attrib value normally
	i = 0;
	while(*attribs_start && *attribs_start != '>' && *attribs_start != ',')
	{
		if(value_str)
		{
			if(i + 1 >= value_str->size)
			{
				if(!snaky_realloc_str(value_str, value_str->size + STR_CAPACITY_INCREASE))
					return 0;
			}
		}
		else if(value_buffer)
		{
			if(i + 1 >= value_buffer_size)
			{
				vl_log(VL_WARNING, "Value buffer isn't large enough: size = %zu!\n", value_buffer_size);
			}
		}

		char next = *attribs_start++;
		/*if(isspace(next))
			continue;*/

		if(value_buffer)
			value_buffer[i++] = next;
		else if(value_str)
			value_str->data[i++] = next;
	}

	if(value_buffer)
		value_buffer[i] = '\0';
	else if(value_str)
		value_str->data[i] = '\0';

	return_start_pos(attribs_start, out_start_pos);

	// default to string here
	if(!data_type_resolved && out_data_type)
		*out_data_type = SNAKY_STRING;

	return 1;
}
static int prepare_parse_attrib(const char *str, snaky_string *name_str, snaky_string *value_str, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, const char **out_start_pos, snaky_data_type *out_data_type)
{
	if(!str || strlen(str) == 0)
		return 0;

	bool in_attrib_string = false;

	for(const char *p = str; *p; ++p)
	{
		// get char
		char c = *p;

		// see if the first char in the string is just a letter
		if(isalpha(c))
			return parse_attrib(p, name_str, value_str, name_buffer, name_buffer_size, value_buffer, value_buffer_size, out_start_pos, out_data_type);

		// if the first char is a group indicator, skip the string entirely
		if(c == '{')
		{
			const char *start_of_object = p;

			const char *end_of_object = find_end_of_object(p);
			if(!end_of_object)
			{
				vl_log(VL_ERROR, "Object was never closed in string: '%s'!\n", start_of_object);
				return 0;
			}

			while(*p && p != end_of_object)
				++p;

			continue;
		}

		// when the first '<' or ',' is encountered, see what the very next attrib is
		if((c == '<' || c == ',') && !in_attrib_string)
		{
			in_attrib_string = true;

			const char *attribs_start = p + 1;

			skip_whitespace(&attribs_start, NULL);

			if(!*attribs_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", str);
				return 0;
			}

			return parse_attrib(attribs_start, name_str, value_str, name_buffer, name_buffer_size, value_buffer, value_buffer_size, out_start_pos, out_data_type);
		}

		// parse the very next attrib found
		if(c == ',' && in_attrib_string)
		{
			const char *attribs_start = p + 1;

			skip_whitespace(&attribs_start, NULL);

			if(*attribs_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", str);
				return 0;
			}

			return parse_attrib(attribs_start, name_str, value_str, name_buffer, name_buffer_size, value_buffer, value_buffer_size, out_start_pos, out_data_type);
		}

		// nested attributes are skipped
		if(c == '<' && in_attrib_string)
		{
			while(*p && *p != '>')
				++p;

			if(!*p)
			{
				vl_log(VL_ERROR, "Expected '>' in nested attribute string: '%s'!\n", str);
				return 0;
			}

			continue;
		}

		// exit an attrib string when '>' is found
		if(c == '>' && in_attrib_string)
			in_attrib_string = false;
	}

	return 0;
}
int snaky_parse_attrib_buf(const char *str, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, const char **out_start_pos, snaky_data_type *out_data_type)
{
	return prepare_parse_attrib(str, NULL, NULL, name_buffer, name_buffer_size, value_buffer, value_buffer_size, out_start_pos, out_data_type);
}
int snaky_parse_attrib_str(const char *str, snaky_string *name_str, snaky_string *value_str, const char **out_start_pos, snaky_data_type *out_data_type)
{
	return prepare_parse_attrib(str, name_str, value_str, NULL, 0, NULL, 0, out_start_pos, out_data_type);
}

int snaky_remove_attrib_buf(char *str, const char *attrib_name)
{
	size_t attrib_name_len = attrib_name ? strlen(attrib_name) : 0;

	if(!str || strlen(str) == 0 || !attrib_name || attrib_name_len == 0)
		return 0;

	// see if the attrib is found
	const char *start_pos = NULL;
	char attrib_value[SNAKY_BUF_SIZE + 1];
	if(!snaky_parse_target_attrib_buf(str, attrib_value, sizeof(attrib_value), attrib_name, &start_pos, NULL))
	{
		vl_log(VL_ERROR, "The '%s' attribute was not found in this string: '%s'!\n", attrib_name, str);
		return 0;
	}

	// the attrib is present in the string so it can be removed:

	/*
	   the start_pos pointer points to the first character of the attrib value,
	   so it can be moved backwards to obtain the total length of the sub-string

	   + 2 for the '=' and attribute separator
	*/
	size_t sub_str_len = strlen(attrib_value) + attrib_name_len + 2;

	// now sub_str_len represents the total amount of characters that need to be removed

	// now get a pointer to the first character separating this attribute
	char *sub_str_start_pos = (char*) (start_pos - attrib_name_len - 1);

	memmove(sub_str_start_pos, sub_str_start_pos + sub_str_len, strlen(sub_str_start_pos + sub_str_len) + 1);

	// see if that was the last attribute in the string, and if so, remove trailing ',' and replace with '>'
	size_t len = strlen(str);

	if(len > 0 && str[len - 1] == ',')
		str[len - 1] = '>';

	return 1;
}
int snaky_remove_attrib_str(snaky_string *str, const char *attrib_name)
{
	size_t attrib_name_len = attrib_name ? strlen(attrib_name) : 0;

	if(!str || str->len == 0 || !attrib_name || attrib_name_len == 0)
		return 0;

	// see if the attrib is found
	const char *start_pos = NULL;
	char attrib_value[SNAKY_BUF_SIZE + 1];
	if(!snaky_parse_target_attrib_buf(str->data, attrib_value, sizeof(attrib_value), attrib_name, &start_pos, NULL))
	{
		vl_log(VL_ERROR, "The '%s' attribute was not found in this string: '%s'!\n", attrib_name, str);
		return 0;
	}

	// the attrib is present in the string so it can be removed:

	/*
	   the start_pos pointer points to the first character of the attrib value,
	   so it can be moved backwards to obtain the total length of the sub-string

	   + 2 for the '=' and attribute separator
	*/
	size_t sub_str_len = strlen(attrib_value) + attrib_name_len + 2;

	// now sub_str_len represents the total amount of characters that need to be removed

	// now get a pointer to the first character separating this attribute
	char *sub_str_start_pos = (char*) (start_pos - attrib_name_len - 1);

	memmove(sub_str_start_pos, sub_str_start_pos + sub_str_len, strlen(sub_str_start_pos + sub_str_len) + 1);

	// see if that was the last attribute in the string, and if so, remove trailing ',' and replace with '>'
	size_t len = strlen(str->data);
	str->len = len;

	if(len > 0 && str->data[len - 1] == ',')
		str->data[len - 1] = '>';

	return 1;
}

static void insert_char_buf(char *buffer, char c)
{
	// move all chars at pos one slot over to the right
	memmove(buffer + 1, buffer, strlen(buffer) + 1);
	*(buffer++) = c;
}
int snaky_set_attrib_buf(char *buf, size_t buffer_size, const char *attrib_name, const char *new_attrib_value)
{
	size_t str_len = strlen(buf);

	if(!buf || str_len >= buffer_size || buffer_size == 0 || !attrib_name || strlen(attrib_name) == 0 || !new_attrib_value || strlen(new_attrib_value) == 0)
		return 0;

	char *edit_pos = NULL;
	size_t curr_len = 0;
	bool attrib_found = false;
	const char *start_pos = NULL;

	char attrib_value[SNAKY_BUF_SIZE + 1];
	if(snaky_parse_target_attrib_buf(buf, attrib_value, sizeof(attrib_value), attrib_name, &start_pos, NULL))
	{
		edit_pos = (char*) start_pos;
		// curr_len is equal to length of the found attribute value
		curr_len = strlen(attrib_value);
		attrib_found = true;
	}

	// see if current value is a bool and user passed "OPPOSITE"
	if(strcmp(new_attrib_value, "OPPOSITE") == 0)
	{
		if(strcmp(attrib_value, "TRUE") == 0)
			new_attrib_value = "FALSE";
		else if(strcmp(attrib_value, "ON") == 0)
			new_attrib_value = "OFF";
		else if(strcmp(attrib_value, "FALSE") == 0)
			new_attrib_value = "TRUE";
		else if(strcmp(attrib_value, "OFF") == 0)
			new_attrib_value = "ON";
		else
		{
			vl_log(VL_ERROR, "The 'OPPOSITE' attrib value can only be used on boolean attributes!\n");
			return 0;
		}
	}

	// if the attrib wasn't found, it must be added
	size_t new_len = 0;
	if(attrib_found)
	{
		new_len = strlen(new_attrib_value);

		if(new_len > curr_len)
		{
			size_t new_total_len = str_len + (new_len - curr_len);
			if(new_total_len + 1 > buffer_size)
				return 0;
		}
	}
	else
	{
		new_len = strlen(attrib_name) + strlen(new_attrib_value) + 3;

		size_t new_total_len = str_len + new_len;

		if(new_total_len + 1 > buffer_size)
			return 0;
	}

	if(!attrib_found)
	{
		/*
		   add ',attrib_name=new_attrib_value' to string,
		   unless the attribute string is empty, then just add
		   'attrib_name=new_attrib_value' to it
		*/

		// walk forward in string until the next '>' is hit, but preserve the string's origin ptr
		const char *origin = buf;
		while(*buf && *buf != '>')
		{
			if(*buf == '{')
			{
				const char *end_of_object = find_end_of_object(buf);
				if(!end_of_object)
				{
					vl_log(VL_ERROR, "Object was never closed in string: '%s'!\n", buf);
					return 0;
				}

				// jump to end of group
				buf = (char*) end_of_object;
				buf++;
				break;
			}

			buf++;
		}

		// use origin here instead of the current pointer
		bool empty = strlen(origin) == 0;
		size_t num_attribs = empty ? 0 : snaky_count_attribs(origin);

		if(!empty && num_attribs > 0)
		{
			insert_char_buf(buf++, ',');
			insert_char_buf(buf++, ' ');
		}
		else if(empty)
			insert_char_buf(buf++, '<');

		// now insert attrib name
		for(size_t i = 0; i < strlen(attrib_name); ++i)
			insert_char_buf(buf++, *(attrib_name + i));

		// now insert the '='
		insert_char_buf(buf++, '=');

		// now insert the new attrib value
		for(size_t i = 0; i < strlen(new_attrib_value); ++i)
			insert_char_buf(buf++, *(new_attrib_value + i));

		// close attrib string if necessary
		if(empty || num_attribs == 0)
			insert_char_buf(buf++, '>');
	}
	else
	{
		// modify attrib value directly within string:

		size_t new_len = strlen(new_attrib_value);

		// first shift everything occupying that space to the right
		memmove(edit_pos + new_len, edit_pos + curr_len, strlen(edit_pos + curr_len) + 1);

		// then copy the new attrib value into the new space
		memcpy(edit_pos, new_attrib_value, new_len);
	}

	return 1;
}
int snaky_set_attrib_str(snaky_string *str, const char *attrib_name, const char *new_attrib_value)
{
	if(!str || !attrib_name || !new_attrib_value || strlen(new_attrib_value) == 0)
		return 0;

	// copy of str->data pointer
	char *buf = str->data;

	char *edit_pos = NULL;
	size_t curr_len = 0;
	bool attrib_found = false;
	const char *start_pos = NULL;

	char attrib_value[SNAKY_BUF_SIZE + 1];
	if(snaky_parse_target_attrib_buf(str->data, attrib_value, sizeof(attrib_value), attrib_name, &start_pos, NULL))
	{
		edit_pos = (char*) start_pos;
		// curr_len is equal to length of the found attribute value
		curr_len = strlen(attrib_value);
		attrib_found = true;
	}

	// see if current value is a bool and user passed "OPPOSITE"
	if(strcmp(new_attrib_value, "OPPOSITE") == 0)
	{
		if(strcmp(attrib_value, "TRUE") == 0)
			new_attrib_value = "FALSE";
		else if(strcmp(attrib_value, "ON") == 0)
			new_attrib_value = "OFF";
		else if(strcmp(attrib_value, "FALSE") == 0)
			new_attrib_value = "TRUE";
		else if(strcmp(attrib_value, "OFF") == 0)
			new_attrib_value = "ON";
		else
		{
			vl_log(VL_ERROR, "The 'OPPOSITE' attrib value can only be used on boolean attributes!\n");
			return 0;
		}
	}

	// if the attrib wasn't found, it must be added
	size_t new_len = 0;
	if(attrib_found)
	{
		new_len = strlen(new_attrib_value);

		if(new_len > curr_len)
		{
			size_t new_total_len = str->len + (new_len - curr_len);
			if(new_total_len + 1 > str->size && !snaky_realloc_str(str, new_total_len + 1))
				return 0;
		}
	}
	else
	{
		new_len = strlen(attrib_name) + strlen(new_attrib_value) + 3;

		size_t new_total_len = str->len + new_len;

		if(new_total_len + 1 > str->size && !snaky_realloc_str(str, new_total_len + 1))
			return 0;
	}

	buf = str->data;

	if(!attrib_found)
	{
		// add ',attrib_name=new_attrib_value' to string:

		// walk forward in string until the next '>' is hit
		while(*buf && *buf != '>')
		{
			// see if an object should be skipped
			if(*buf == '{')
			{
				const char *end_of_object = find_end_of_object(buf);
				if(!end_of_object)
				{
					vl_log(VL_ERROR, "Object was never closed in string: '%s'!\n", buf);
					return 0;
				}
				
				// jump to end of group
				buf = (char*) end_of_object;
				buf++;
				break;
			}

			buf++;
		}

		// get number of present attribs:
		bool empty = str->len == 0;
		size_t num_attribs = empty ? 0 : snaky_count_attribs(str->data);

		// insert ', ' there if at least one attrib is already present
		if(!empty && num_attribs > 0)
		{
			insert_char_buf(buf++, ',');
			insert_char_buf(buf++, ' ');
		}
		// if no attribs are present, insert '<' and keep track of this so later the '>' can be inserted too
		else if(empty)
			insert_char_buf(buf++, '<');

		// now insert attrib name
		for(size_t i = 0; i < strlen(attrib_name); ++i)
			insert_char_buf(buf++, *(attrib_name + i));

		// now insert the '='
		insert_char_buf(buf++, '=');

		// now insert the new attrib value
		for(size_t i = 0; i < strlen(new_attrib_value); ++i)
			insert_char_buf(buf++, *(new_attrib_value + i));

		// close attrib string if necessary
		if(empty || num_attribs == 0)
			insert_char_buf(buf++, '>');
	}
	else
	{
		// modify attrib value directly within string:

		size_t new_len = strlen(new_attrib_value);

		// first shift everything occupying that space to the right
		memmove(edit_pos + new_len, edit_pos + curr_len, strlen(edit_pos + curr_len) + 1);

		// then copy the new attrib value into the new space
		memcpy(edit_pos, new_attrib_value, new_len);
	}

	// re-calculate string length
	str->len = strlen(str->data);

	return 1;
}
int snaky_set_attribs_buf(char *str, size_t buffer_size, const char *attribs)
{
	if(!str || strlen(str) >= buffer_size || buffer_size == 0 || !attribs || strlen(attribs) == 0)
		return 0;

	// because 'attribs' should be its own attribute string, get the attribute data from it
	snaky_attrib_map map = {0};
	if(snaky_generate_attrib_map(attribs, &map))
	{
		// go through the map
		for(size_t i = 0; i < map.size; ++i)
		{
			// get the attrib name and new value
			const char *name = map.keys[i];
			const char *val = map.values[i];

			// set it
			if(!snaky_set_attrib_buf(str, buffer_size, name, val))
			{
				dynmaps_free_strkeyval(&map);
				return 0;
			}
		}

		dynmaps_free_strkeyval(&map);
		return 1;
	}

	vl_log(VL_ERROR, "Failed to set attributes: string: '%s', attributes string: '%s'!\n", str, attribs);
	return 0;
}
int snaky_set_attribs_str(snaky_string *str, const char *attribs)
{
	if(!str || !attribs || strlen(attribs) == 0)
		return 0;

	// get attrib data from 'attribs'
	snaky_attrib_map map = {0};
	if(snaky_generate_attrib_map(attribs, &map))
	{
		// go through the map
		for(size_t i = 0; i < map.size; ++i)
		{
			// get attrib entry
			const char *name = map.keys[i];
			const char *value = map.values[i];

			// set it
			if(!snaky_set_attrib_str(str, name, value))
			{
				dynmaps_free_strkeyval(&map);
				return 0;
			}
		}

		dynmaps_free_strkeyval(&map);
		return 1;
	}

	vl_log(VL_ERROR, "Failed to set attributes: string: '%s', attributes string: '%s'!\n", str->data, attribs);
	return 0;
}

size_t snaky_count_attribs(const char *str)
{
	if(!str || strlen(str) == 0)
		return 0;

	size_t count = 0;

	for(const char *p = str; *p; ++p)
	{
		char c = *p;

		// an attrib is counted when the 'name=value' format is found exactly
		char name[SNAKY_BUF_SIZE + 1];
		char value[SNAKY_BUF_SIZE + 1];
		if(snaky_parse_attrib_buf(p, name, sizeof(name), value, sizeof(value), NULL, NULL))
			count++;
	}

	return count;
}

bool snaky_is_object(const char *str)
{
	return *str && *str == '{';
}

const char *snaky_get_attrib_pos(const char *str, const char *attrib_name)
{
	size_t attrib_name_len = attrib_name ? strlen(attrib_name) : 0;

	if(!str || !attrib_name || strlen(str) == 0 || attrib_name_len == 0)
		return NULL;

	while(*str)
	{
		if(strncmp(str, attrib_name, attrib_name_len) == 0)
			return str;
		str++;
	}

	return NULL;
}

int snaky_generate_attrib_map(const char *str, snaky_attrib_map *map)
{
	if(!str || strlen(str) == 0 || !map || !map->keys || !map->values)
		return 0;

	snaky_string name = {0};
	if(!snaky_prepare_str(&name, SNAKY_BUF_SIZE + 1))
		return 0;

	snaky_string value = {0};
	if(!snaky_prepare_str(&value, SNAKY_BUF_SIZE + 1))
	{
		snaky_free_str(&name);
		return 0;
	}

	const char *start_pos = str;
	while(start_pos && snaky_parse_attrib_str(start_pos, &name, &value, &start_pos, NULL))
	{
		dynmaps_set_strkeyval(map, name.data, value.data);
		if(map->alloc_failure)
		{
			snaky_free_str(&name);
			snaky_free_str(&value);
			return 0;
		}
	}
	snaky_free_str(&name);
	snaky_free_str(&value);

	if(map->size == 0)
	{
		vl_log(VL_WARNING, "No attribute entries found in string: '%s'!\n", str);
		return 0;
	}

	return 1;
}
int snaky_generate_attrib_list(const char *str, snaky_attrib_list *list)
{
	if(!str || strlen(str) == 0 || !list || !list->data)
		return 0;

	snaky_string name = {0};
	if(!snaky_prepare_str(&name, SNAKY_BUF_SIZE + 1))
		return 0;

	snaky_string value = {0};
	if(!snaky_prepare_str(&value, SNAKY_BUF_SIZE + 1))
	{
		snaky_free_str(&name);
		return 0;
	}

	const char *start_pos = str;
	snaky_data_type data_type = SNAKY_INVALID_VALUE;
	while(start_pos && snaky_parse_attrib_str(start_pos, &name, &value, &start_pos, &data_type))
	{
		snaky_attrib_entry entry = {0};
		if(!snaky_prepare_str(&entry.name, SNAKY_BUF_SIZE + 1))
		{
			snaky_free_str(&name);
			snaky_free_str(&value);
			return 0;
		}
		if(!snaky_prepare_str(&entry.value, SNAKY_BUF_SIZE + 1))
		{
			snaky_free_str(&name);
			snaky_free_str(&value);
			snaky_free_str(&entry.name);
			return 0;
		}

		if(!snaky_strcpy(&entry.name, name.data))
		{
			snaky_free_str(&name);
			snaky_free_str(&value);
			return 0;
		}
		if(!snaky_strcpy(&entry.value, value.data))
		{
			snaky_free_str(&name);
			snaky_free_str(&value);
			return 0;
		}

		entry.data_type = data_type;
		snaky_parse_data_type(data_type, entry.data_type_str, sizeof(entry.data_type_str));

		dynas_add(list, entry);
		if(list->alloc_failure)
		{
			snaky_free_str(&name);
			snaky_free_str(&value);
			return 0;
		}
	}
	snaky_free_str(&name);
	snaky_free_str(&value);

	if(list->size == 0)
	{
		vl_log(VL_WARNING, "No attribute entries found in string: '%s'!\n", str);
		return 0;
	}

	return 1;
}
int snaky_destroy_attrib_list(snaky_attrib_list *list)
{
	if(!list || !list->data)
		return 0;

	for(size_t i = 0; i < list->size; ++i)
	{
		// get entry
		snaky_attrib_entry *entry = &list->data[i];
		snaky_free_str(&entry->name);
		snaky_free_str(&entry->value);
		entry->data_type = SNAKY_INVALID_VALUE;
	}

	dynas_free(list);

	return 1;
}

char snaky_parse_char(const char *str, int *out_success)
{
	// default to 0
	if(out_success)
		*out_success = 0;

	if(!str || strlen(str) != 1)
		return '\0';

	char c = *str;

	if(out_success)
		*out_success = 1;

	return c;
}
bool snaky_parse_bool(const char *str, int *out_success)
{
	// default to 0
	if(out_success)
		*out_success = 0;

	if(!str || strlen(str) == 0)
		return false;

	if(strncmp(str, "TRUE", 4) == 0 || strncmp(str, "ON", 2) == 0)
	{
		if(out_success)
			*out_success = 1;
		return true;
	}
	else if(strncmp(str, "FALSE", 5) == 0 || strncmp(str, "OFF", 3) == 0)
	{
		if(out_success)
			*out_success = 1;
		return false;
	}
	else
		return false;
}

static char peek_next_char(const char **str)
{
	skip_whitespace(str, NULL);
	return **str;
}
static char get_next_char(const char **str)
{
	skip_whitespace(str, NULL);
	return *(*str)++;
}
static float parse_term(const char*, const char**, int*);
static float parse_expression(const char *origin, const char **str, int *out_success)
{
	int s = 0;
	float f = parse_term(origin, str, &s);
	*out_success = s;
	if(s == 0)
		return 0.0f;
	while(peek_next_char(str) == '+' || peek_next_char(str) == '-')
	{
		char op = get_next_char(str);
		float next_val = parse_term(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;
		if(op == '+')
			f += next_val;
		else
			f -= next_val;
	}
	return f;
}
static float parse_factor(const char *origin, const char **str, int *out_success)
{
	// find expression wrapped around '()'
	char c = peek_next_char(str);
	if(c == '(')
	{
		char next = get_next_char(str);
		int s = 0;
		float f = parse_expression(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;
		if(peek_next_char(str) == ')')
		{
			get_next_char(str);
			*out_success = 1;
			return f;
		}
		else
			return 0.0f;
	}

	// see if it's just a normal number
	if(isdigit(c) || c == '-' || c == '.')
	{
		char *end = NULL;
		float f = strtof(*str, &end);
		if(end == *str)
		{
			*out_success = 0;
			return 0.0f;
		}
		*str = end;
		*out_success = 1;
		return f;
	}

	// see if user provided a function or attrib name
	if(isalpha(c))
	{
		char attrib_name[SNAKY_BUF_SIZE + 1];
		size_t i = 0;

		// keep track of where to start the recursive search
		const char *start_of_str = *str;
		// whether or not only letters are accepted
		bool checking_only_alpha = true;
		while(**str && i + 1 < sizeof(attrib_name))
		{
			// the first character cannot be a digit
			if(i == 0 && isdigit(**str))
			{
				vl_log(VL_ERROR, "The first character in a function or attribute cannot be a digit!\n");
				*out_success = 0;
				return 0.0f;
			}

			// stop searching for just letters when an array index starts
			if(**str == '[')
				checking_only_alpha = false;

			// re-enable alpha-only rule
			if(**str == ']')
				checking_only_alpha = true;

			if(checking_only_alpha && !isalpha(**str))
				break;

			attrib_name[i++] = *(*str)++;
		}

		attrib_name[i] = '\0';

		// see what the value is:

		// try to parse as a target attribute:
		char attrib[SNAKY_BUF_SIZE + 1];
		snaky_data_type data_type = SNAKY_INVALID_VALUE;
		if(snaky_parse_target_attrib_buf(origin, attrib, sizeof(attrib), attrib_name, NULL, &data_type))
		{
			// if the attrib is an invalid type, no value can be parsed
			if(data_type == SNAKY_ARRAY)
			{
				vl_log(VL_ERROR, "The parsed attribute '%s' is an array, cannot parse a value!\n", attrib_name);
				*out_success = 0;
				return 0.0f;
			}
			else if(data_type == SNAKY_OBJECT)
			{
				vl_log(VL_ERROR, "The parsed attribute '%s' is an object, cannot parse a value!\n", attrib_name);
				*out_success = 0;
				return 0.0f;
			}
			else if(data_type == SNAKY_STRING)
			{
				vl_log(VL_ERROR, "The parsed attribute '%s' is a string, cannot parse a value!\n", attrib_name);
			}

			float f = 0.0f;
			int s = 0;
			snaky_parse_value(origin, attrib, SNAKY_FLOAT, &f, &s);
			*out_success = s;
			if(s == 1)
				return f;
		}

		// parse all matching constants:
		for(size_t i = 0; i < constants.size; ++i)
		{
			if(strcmp(attrib_name, constants.keys[i]) == 0)
			{
				*out_success = 1;
				return constants.values[i];
			}
		}

		// parse all matching eval functions:
		for(size_t i = 0; i < eval_functions.size; ++i)
		{
			if(strcmp(attrib_name, eval_functions.keys[i]) == 0)
			{
				if(peek_next_char(str) == '(')
				{
					char next = get_next_char(str);
					int s = 0;
					float f = parse_expression(origin, str, &s);
					*out_success = s;
					if(s == 0)
						return 0.0f;
					if(peek_next_char(str) == ')')
					{
						get_next_char(str);
						*out_success = 1;
						return eval_functions.values[i](f);
					}
					else
					{
						vl_log(VL_ERROR, "Expected ')' after evaluation function: '%s'!\n", eval_functions.keys[i]);
						return 0.0f;
					}
				}
			}
		}

		// parse all matching functions:
		for(size_t i = 0; i < functions.size; ++i)
		{
			if(strcmp(attrib_name, functions.keys[i]) == 0)
			{
				if(peek_next_char(str) == '(')
				{
					char next = get_next_char(str);

					// get the string being parsed here:
					char fn_arg[SNAKY_BUF_SIZE + 1];
					size_t j = 0;

					// move str to the next ')', while adding to fn_arg buffer
					while(**str && **str != ')')
						fn_arg[j++] = *(*str)++;

					fn_arg[j] = '\0';

					if(peek_next_char(str) == ')')
					{
						get_next_char(str);
						*out_success = 1;

						// parse func data
						function func_data = functions.values[i];

						switch(func_data.return_type)
						{
							case SNAKY_CHAR:
							{
								char (*fn_ptr) (const void*) = func_data.fn_ptr;
								return fn_ptr(fn_arg);
							}
							case SNAKY_BOOL:
							{
								bool (*fn_ptr) (const void*) = func_data.fn_ptr;
								return fn_ptr(fn_arg);
							}
							case SNAKY_INT:
							{
								int (*fn_ptr) (const void*) = func_data.fn_ptr;
								return fn_ptr(fn_arg);
							}
							case SNAKY_FLOAT:
							{
								float (*fn_ptr) (const void*) = func_data.fn_ptr;
								return fn_ptr(fn_arg);
							}
							case SNAKY_DOUBLE:
							{
								double (*fn_ptr) (const void*) = func_data.fn_ptr;
								return fn_ptr(fn_arg);
							}
							default:
								vl_log(VL_ERROR, "Invalid return type of function: %d!\n", func_data.return_type);
								*out_success = 0;
								return 0;
						}
					}
					else
					{
						vl_log(VL_ERROR, "Expected ')' after function: '%s'!\n", functions.keys[i]);
						return 0.0f;
					}
				}
			}
		}
	}

	*out_success = 0;
	return 0.0f;
}
static float parse_exponent(const char*, const char**, int*);
static float parse_term(const char *origin, const char **str, int *out_success)
{
	int s = 0;
	float f = parse_exponent(origin, str, &s);
	*out_success = s;
	if(s == 0)
		return 0.0f;

	while(peek_next_char(str) == '*' || peek_next_char(str) == '/')
	{
		char op = get_next_char(str);
		float next_val = parse_exponent(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;
		if(op == '*')
			f *= next_val;
		else
		{
			if(next_val == 0)
			{
				vl_log(VL_WARNING, "Dividing by 0 is undefined. Returning 0.0f.\n");
				return 0.0f;
			}
			f /= next_val;
		}

		*out_success = 1;
	}

	return f;
}
static float parse_exponent(const char *origin, const char **str, int *out_success)
{
	int s = 0;
	float f = parse_factor(origin, str, &s);
	*out_success = s;
	if(s == 0)
		return 0.0f;
	
	if(peek_next_char(str) == '^')
	{
		get_next_char(str);

		float exp = parse_exponent(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;
		f = powf(f, exp);
	}

	*out_success = 1;
	return f;
}
int snaky_parse_int(const char *origin, const char *str, int *out_success)
{
	// default to 0
	if(out_success)
		*out_success = 0;

	if(!str || strlen(str) == 0)
		return 0;

	int s = 0;
	float ff = parse_expression(origin, &str, &s);
	if(s == 1)
	{
		if(out_success)
			*out_success = 1;
		return ff;
	}

	char *endptr = NULL;
	int i = strtol(str, &endptr, 10);
	if(endptr != str)
	{
		if(out_success)
			*out_success = 1;
		return i;
	}

	return 0;
}
float snaky_parse_float(const char *origin, const char *str, int *out_success)
{
	// default to 0
	if(out_success)
		*out_success = 0;

	if(!str || strlen(str) == 0)
		return 0.0f;

	int s = 0;
	float ff = parse_expression(origin, &str, &s);
	if(s == 1)
	{
		if(out_success)
			*out_success = 1;
		return ff;
	}

	char *endptr = NULL;
	float f = strtof(str, &endptr);
	if(endptr != str)
	{
		if(out_success)
			*out_success = 1;
		return f;
	}

	return 0.0f;
}
double snaky_parse_double(const char *origin, const char *str, int *out_success)
{
	// default to 0
	if(out_success)
		*out_success = 0;

	if(!str || strlen(str) == 0)
		return 0.0;

	int s = 0;
	float ff = parse_expression(origin, &str, &s);
	if(s == 1)
	{
		if(out_success)
			*out_success = 1;
		return ff;
	}

	char *endptr = NULL;
	double d = strtod(str, &endptr);
	if(endptr != str)
	{
		if(out_success)
			*out_success = 1;
		return d;
	}

	return 0.0;
}
void snaky_parse_value(const char *origin, const char *str, snaky_data_type target_type, void *out_value, int *out_success)
{
	if(!out_success)
	{
		vl_log(VL_ERROR, "snaky_parse_value(...) requires 'out_success' to be a valid pointer!\n");
		return;
	}

	// make sure it's 0 by default
	*out_success = 0;

	switch(target_type)
	{
		case SNAKY_CHAR:
			char c = snaky_parse_char(str, out_success);
			if(*out_success == 1 && out_value)
				*((char*) out_value) = c;
			break;
		case SNAKY_BOOL:
			bool b = snaky_parse_bool(str, out_success);
			if(*out_success == 1 && out_value)
				*((bool*) out_value) = b;
			break;
		case SNAKY_INT:
			int i = snaky_parse_int(origin, str, out_success);
			if(*out_success == 1 && out_value)
				*((int*) out_value) = i;
			break;
		case SNAKY_FLOAT:
			float f = snaky_parse_float(origin, str, out_success);
			if(*out_success == 1 && out_value)
				*((float*) out_value) = f;
			break;
		case SNAKY_DOUBLE:
			double d = snaky_parse_double(origin, str, out_success);
			if(*out_success == 1 && out_value)
				*((double*) out_value) = d;
			break;
		default:
			vl_log(VL_ERROR, "Invalid data type in snaky_parse_value(...): %d\n", target_type);
			break;
	}

	if(*out_success == 0)
		vl_log(VL_ERROR, "Failed to parse string value: '%s'!\n", str);
}
void snaky_parse_target_attrib_value(const char *str, const char *attrib_name, snaky_data_type target_type, void *out_value, const char **out_start_pos, int *out_success)
{
	if(!out_success)
	{
		vl_log(VL_ERROR, "snaky_parse_value(...) requires 'out_success' to be a valid pointer!\n");
		return;
	}

	// make sure it is 0 by default
	*out_success = 0;

	snaky_string attrib = {0};
	if(!snaky_prepare_str(&attrib, SNAKY_BUF_SIZE + 1))
		return;

	snaky_data_type resolved_type = SNAKY_INVALID_VALUE;
	if(snaky_parse_target_attrib_str(str, &attrib, attrib_name, out_start_pos, &resolved_type))
	{
		if(resolved_type == SNAKY_INVALID_VALUE)
			resolved_type = target_type;
		snaky_parse_value(str, attrib.data, resolved_type, out_value, out_success);
	}

	snaky_free_str(&attrib);

	if(*out_success == 0)
		vl_log(VL_ERROR, "Failed to parse target attrib value: string: '%s', attribute name: '%s'!\n", str, attrib_name);
}
int snaky_parse_data_type(snaky_data_type data_type, char *buffer, size_t buffer_size)
{
	if(!buffer || buffer_size == 0)
		return 0;

	switch(data_type)
	{
		case SNAKY_CHAR:
			snprintf(buffer, buffer_size, "char");
			break;
		case SNAKY_BOOL:
			snprintf(buffer, buffer_size, "bool");
			break;
		case SNAKY_INT:
			snprintf(buffer, buffer_size, "int");
			break;
		case SNAKY_FLOAT:
			snprintf(buffer, buffer_size, "float");
			break;
		case SNAKY_DOUBLE:
			snprintf(buffer, buffer_size, "double");
			break;
		case SNAKY_ARRAY:
			snprintf(buffer, buffer_size, "array");
			break;
		case SNAKY_OBJECT:
			snprintf(buffer, buffer_size, "object");
			break;
		case SNAKY_STRING:
			snprintf(buffer, buffer_size, "string");
			break;
		default:
			vl_log(VL_ERROR, "Unknown data type: %d!\n", data_type);
			snprintf(buffer, buffer_size, "NULL");
			return 0;
	}

	return 1;
}

bool snaky_file_exists(const char *file_path)
{
	FILE *f = fopen(file_path, "rb");

	bool e = f != NULL;

	if(f)
		fclose(f);

	return e;
}
long snaky_get_file_size(const char *file_path)
{
	if(!file_path || strlen(file_path) == 0)
		return 0;

	FILE *f = fopen(file_path, "rb");
	if(!f)
	{
		vl_log(VL_ERROR, "Failed to open file at '%s'!\n", file_path);
		return 0;
	}

	fseek(f, 0, SEEK_END);

	long bytes = ftell(f);

	fclose(f);

	return bytes;
}
int snaky_read_file_buf(const char *file_path, char *buffer, size_t buffer_size, snaky_fwrite_mode mode)
{
	if(!file_path || strlen(file_path) == 0 || !buffer || buffer_size == 0)
		return 0;

	FILE *f = fopen(file_path, "rb");
	if(!f)
	{
		vl_log(VL_ERROR, "Failed to open file at path: '%s'!\n", file_path);
		return 0;
	}

	if(mode == SNAKY_FWRITE_BINARY)
	{
		snaky_attrib_map map = {0};
		dynmaps_init(&map);
		if(map.alloc_failure)
		{
			vl_log(VL_ERROR, "Allocation error!\n");
			return 0;
		}

		if(snaky_read_file_map(file_path, &map, mode))
		{
			for(size_t i = 0; i < map.size; ++i)
			{
				// set attrib in original buffer
				if(!snaky_set_attrib_buf(buffer, buffer_size, map.keys[i], map.values[i]))
				{
					dynmaps_free_strkeyval(&map);
					goto snaky_read_file_err;
				}
			}

			dynmaps_free_strkeyval(&map);
		}
		else
		{
			dynmaps_free_strkeyval(&map);
			goto snaky_read_file_err;
		}
	}
	else if(mode == SNAKY_FWRITE_TEXT)
	{
		// TODO use snaky strings while reading, no more MAX_LINE_LEN
		char read[SNAKY_MAX_LINE_LEN + 1];
		size_t total_size = 0;
		while(fgets(read, sizeof(read), f))
		{
			total_size += strlen(read);

			if(total_size + 1 > buffer_size)
			{
				vl_log(VL_ERROR, "Not enough memory allocated for reading file's contents: '%s'!\n", file_path);
				return 0;
			}

			// append contents to user's buffer
			strcat(buffer, read);
		}
	}
	else if(mode == SNAKY_FWRITE_ENCRYPT)
	{
		snaky_attrib_map map = {0};
		dynmaps_init(&map);
		if(map.alloc_failure)
		{
			vl_log(VL_ERROR, "Allocation error!\n");
			return 0;
		}

		if(snaky_read_file_map(file_path, &map, mode))
		{
			for(size_t i = 0; i < map.size; ++i)
			{
				// set attrib in original buffer
				if(!snaky_set_attrib_buf(buffer, buffer_size, map.keys[i], map.values[i]))
				{
					dynmaps_free_strkeyval(&map);
					goto snaky_read_file_err;
				}
			}

			dynmaps_free_strkeyval(&map);
		}
		else
		{
			dynmaps_free_strkeyval(&map);
			goto snaky_read_file_err;
		}
	}else
		goto snaky_read_file_err;

	goto snaky_read_file_success;

	snaky_read_file_err:
	vl_log(VL_ERROR, "Failed to read attribute data from file: '%s'!\n", file_path);
	fclose(f);
	return 0;

	snaky_read_file_success:
	vl_log(VL_SUCCESS, "Attribute data read from file: '%s'!\n", file_path);
	fclose(f);
	return 1;
}
int snaky_read_file_str(const char *file_path, snaky_string *str, snaky_fwrite_mode mode)
{
	if(!file_path || !str || !str->data)
		return 0;

	FILE *f = fopen(file_path, "rb");
	if(!f)
	{
		vl_log(VL_ERROR, "Failed to open file at path: '%s'!\n", file_path);
		return 0;
	}

	if(mode == SNAKY_FWRITE_BINARY)
	{
		snaky_attrib_map map = {0};
		dynmaps_init(&map);
		if(map.alloc_failure)
		{
			vl_log(VL_ERROR, "Allocation error!\n");
			return 0;
		}

		if(snaky_read_file_map(file_path, &map, mode))
		{
			for(size_t i = 0; i < map.size; ++i)
			{
				// set attrib in original buffer
				if(!snaky_set_attrib_buf(str->data, str->size, map.keys[i], map.values[i]))
				{
					dynmaps_free_strkeyval(&map);
					goto snaky_read_file_err;
				}
			}

			dynmaps_free_strkeyval(&map);
		}
		else
			goto snaky_read_file_err;
	}
	else if(mode == SNAKY_FWRITE_TEXT)
	{
		char read[SNAKY_MAX_LINE_LEN + 1];
		size_t total_size = 0;
		while(fgets(read, sizeof(read), f))
		{
			total_size += strlen(read);

			if(total_size + 1 >= str->size && !snaky_realloc_str(str, str->size + strlen(read)))
				return 0;

			// append contents to string
			strcat(str->data, read);
		}
	}
	else if(mode == SNAKY_FWRITE_ENCRYPT)
	{
		snaky_attrib_map map = {0};
		dynmaps_init(&map);
		if(map.alloc_failure)
		{
			vl_log(VL_ERROR, "Allocation error!\n");
			return 0;
		}

		if(snaky_read_file_map(file_path, &map, mode))
		{
			for(size_t i = 0; i < map.size; ++i)
			{
				// set attrib in original buffer
				if(!snaky_set_attrib_buf(str->data, str->size, map.keys[i], map.values[i]))
				{
					dynmaps_free_strkeyval(&map);
					goto snaky_read_file_err;
				}
			}

			dynmaps_free_strkeyval(&map);
		}
		else
			goto snaky_read_file_err;
	}
	else
		goto snaky_read_file_err;

	goto snaky_read_file_success;

	snaky_read_file_err:
	vl_log(VL_ERROR, "Failed to read attribute data from file: '%s'!\n", file_path);
	fclose(f);
	return 0;

	snaky_read_file_success:
	vl_log(VL_SUCCESS, "Attribute data read from file: '%s'!\n", file_path);
	fclose(f);
	return 1;	
}

// encrypt and decrypt a string:
static void encrypt_str(char *data, size_t len, int key)
{
	for(size_t i = 0; i < len; ++i)
	{
		char c = data[i];

		if(isupper(c))
			data[i] = (c - 'A' + key) % 26 + 'A';
		else if(islower(c))
			data[i] = (c - 'a' + key) % 26 + 'a';
		else if(isdigit(c))
			data[i] = (c - '0' + key) % 10 + '0';
	}
}
static void decrypt_str(char *data, size_t len, int key)
{
	for(size_t i = 0; i < len; ++i)
	{
		char c = data[i];

		if(isupper(c))
			data[i] = (c - 'A' - key + 26) % 26 + 'A';
		else if(islower(c))
			data[i] = (c - 'a' - key + 26) % 26 + 'a';
		else if(isdigit(c))
			data[i] = (c - '0' - key + 10) % 10 + '0';
	}
}
int snaky_read_file_map(const char *file_path, snaky_attrib_map *map, snaky_fwrite_mode mode)
{
	if(!map || !map->keys || !map->values)
		return 0;

	FILE *f = fopen(file_path, "rb");
	if(!f)
	{
		vl_log(VL_ERROR, "Failed to open file at path: '%s'!\n", file_path);
		return 0;
	}

	// determine how the map should be read:
	if(mode == SNAKY_FWRITE_BINARY)
	{
		// first read the number of entries in the file:
		size_t size = 0;
		if(fread(&size, sizeof(size_t), 1, f) != 1)
			goto snaky_read_file_err;

		// now read each entry in the file:
		for(size_t i = 0; i < size; ++i)
		{
			size_t key_len = 0;
			if(fread(&key_len, sizeof(size_t), 1, f) != 1)
				goto snaky_read_file_err;

			char key[key_len + 1];
			if(fread(key, sizeof(char), key_len, f) != key_len)
				goto snaky_read_file_err;

			// insert '\0' into k
			key[key_len] = '\0';

			size_t val_len = 0;
			if(fread(&val_len, sizeof(size_t), 1, f) != 1)
				goto snaky_read_file_err;

			char val[val_len + 1];
			if(fread(val, sizeof(char), val_len, f) != val_len)
				goto snaky_read_file_err;

			// insert '\0' into v
			val[val_len] = '\0';

			// copy string into map
			dynmaps_set_strkeyval(map, key, val);
			if(map->alloc_failure)
			{
				vl_log(VL_ERROR, "Allocation error!\n");
				return 0;
			}
		}
	}
	else if(mode == SNAKY_FWRITE_TEXT)
	{
		/*
		   because text mode writes plain text, binary map like
		   the map's size or the lengths of keys and values
		   are not present in the file. Therefore, the entirety
		   of the file needs to be read into a string buffer
		   and the arg map needs to be collected from that instead
		*/
		snaky_string file_contents = {0};
		if(!snaky_prepare_str(&file_contents, 1024))
			goto snaky_read_file_err;
		// read file contents into a single snaky_string
		if(!snaky_read_file_str(file_path, &file_contents, mode))
		{
			snaky_free_str(&file_contents);
			goto snaky_read_file_err;
		}

		// collect attrib map from that string
		int success = snaky_generate_attrib_map(file_contents.data, map);

		// free allocated string buffer
		snaky_free_str(&file_contents);

		goto snaky_read_file_success;
	}
	else if(mode == SNAKY_FWRITE_ENCRYPT)
	{
		// same as BINARY mode but decrypt strings:

		size_t size = 0;
		if(fread(&size, sizeof(size_t), 1, f) != 1)
			goto snaky_read_file_err;

		for(size_t i = 0; i < size; ++i)
		{
			size_t key_len = 0;
			if(fread(&key_len, sizeof(size_t), 1, f) != 1)
				goto snaky_read_file_err;

			char key[key_len + 1];
			if(fread(key, sizeof(char), key_len, f) != key_len)
				goto snaky_read_file_err;

			key[key_len] = '\0';

			decrypt_str(key, key_len, encrypt_key);

			size_t val_len = 0;
			if(fread(&val_len, sizeof(size_t), 1, f) != 1)
				goto snaky_read_file_err;

			char val[val_len + 1];
			if(fread(val, sizeof(char), val_len, f) != val_len)
				goto snaky_read_file_err;

			val[val_len] = '\0';

			decrypt_str(val, val_len, encrypt_key);

			dynmaps_set_strkeyval(map, key, val);
			if(map->alloc_failure)
			{
				vl_log(VL_ERROR, "Allocation error!\n");
				return 0;
			}
		}
	}
	else
		goto snaky_read_file_err;

	// if no errors, skip to success:
	goto snaky_read_file_success;

	snaky_read_file_err:
	vl_log(VL_ERROR, "Failed to read attribute map from file: '%s'!\n", file_path);
	fclose(f);
	return 0;

	snaky_read_file_success:
	vl_log(VL_SUCCESS, "Attribute map read from file: '%s'!\n", file_path);
	fclose(f);
	return 1;
}
int snaky_write_file_map(const char *file_path, snaky_attrib_map *map, snaky_fwrite_mode mode)
{
	if(!file_path || !map || map->size == 0 || map->alloc_failure)
		return 0;

	FILE *f = fopen(file_path, "wb");
	if(!f)
	{
		vl_log(VL_ERROR, "Failed to open file at path: '%s'!\n", file_path);
		return 0;
	}

	// write each part of the map to the file:

	// determine how the map should be written:
	if(mode == SNAKY_FWRITE_BINARY)
	{
		// first, the number of entries in the map:
		if(fwrite(&map->size, sizeof(map->size), 1, f) != 1)
			goto snaky_write_file_err;

		// now write each entry in the map:
		for(size_t i = 0; i < map->size; ++i)
		{
			size_t key_len = strlen(map->keys[i]);
			size_t val_len = strlen(map->values[i]);

			// write length of key
			if(fwrite(&key_len, sizeof(size_t), 1, f) != 1)
				goto snaky_write_file_err;

			// write key contents
			if(fwrite(map->keys[i], sizeof(char), key_len, f) != key_len)
				goto snaky_write_file_err;

			// write length of value
			if(fwrite(&val_len, sizeof(size_t), 1, f) != 1)
				goto snaky_write_file_err;

			// write value contents
			if(fwrite(map->values[i], sizeof(char), val_len, f) != val_len)
				goto snaky_write_file_err;
		}
	}
	else if(mode == SNAKY_FWRITE_TEXT)
	{
		// write each entry in the map:
		for(size_t i = 0; i < map->size; ++i)
		{
			size_t key_len = strlen(map->keys[i]);
			size_t val_len = strlen(map->values[i]);

			if(fwrite("<", sizeof(char), 1, f) != 1)
				goto snaky_write_file_err;
			if(fwrite(map->keys[i], sizeof(char), key_len, f) != key_len)
				goto snaky_write_file_err;
			if(fwrite("=", sizeof(char), 1, f) != 1)
				goto snaky_write_file_err;
			if(fwrite(map->values[i], sizeof(char), val_len, f) != val_len)
				goto snaky_write_file_err;
			if(fwrite(">\n", sizeof(char), 2, f) != 2)
				goto snaky_write_file_err;
		}
	}
	else if(mode == SNAKY_FWRITE_ENCRYPT)
	{
		// same as BINARY mode but encrypt strings:

		if(fwrite(&map->size, sizeof(map->size), 1, f) != 1)
			goto snaky_write_file_err;

		for(size_t i = 0; i < map->size; ++i)
		{
			size_t key_len = strlen(map->keys[i]);
			size_t val_len = strlen(map->values[i]);

			if(fwrite(&key_len, sizeof(size_t), 1, f) != 1)
				goto snaky_write_file_err;

			char *encrypted_key = calloc(key_len + 1, sizeof(char));
			if(!encrypted_key)
				goto snaky_write_file_err;
			// copy key into encrypted key buffer
			strcpy(encrypted_key, map->keys[i]);

			encrypt_str(encrypted_key, key_len, encrypt_key);
			if(fwrite(encrypted_key, sizeof(char), key_len, f) != key_len)
			{
				free(encrypted_key);
				goto snaky_write_file_err;
			}

			free(encrypted_key);

			if(fwrite(&val_len, sizeof(size_t), 1, f) != 1)
				goto snaky_write_file_err;

			char *encrypted_value = calloc(val_len + 1, sizeof(char));
			if(!encrypted_value)
				goto snaky_write_file_err;
			// copy value into encrypted value buffer
			strcpy(encrypted_value, map->values[i]);

			encrypt_str(encrypted_value, val_len, encrypt_key);
			if(fwrite(encrypted_value, sizeof(char), val_len, f) != val_len)
			{
				free(encrypted_value);
				goto snaky_write_file_err;
			}

			free(encrypted_value);
		}
	}
	else
		goto snaky_write_file_err;

	// if no errors, skip to success:
	goto snaky_write_file_success;

	snaky_write_file_err:
	vl_log(VL_ERROR, "Failed to write attribute map to file: '%s'!\n", file_path);
	fclose(f);
	return 0;

	snaky_write_file_success:
	vl_log(VL_SUCCESS, "Attribute map written to file: '%s'!\n", file_path);
	fclose(f);
	return 1;
}
int snaky_write_file_buf(const char *file_path, const char *buffer, snaky_fwrite_mode mode)
{
	size_t buffer_len = buffer ? strlen(buffer) : 0;

	if(!file_path || !buffer || buffer_len == 0)
		return 0;

	if(mode == SNAKY_FWRITE_BINARY)
	{
		// convert buffer to attrib_data and write that instead
		snaky_attrib_map map = {0};
		dynmaps_init(&map);
		if(map.alloc_failure)
		{
			vl_log(VL_ERROR, "Allocation error!\n");
			return 0;
		}

		if(snaky_generate_attrib_map(buffer, &map))
		{
			snaky_write_file_map(file_path, &map, mode);
			dynmaps_free_strkeyval(&map);
		}
		else
			goto snaky_write_file_err;
	}
	else if(mode == SNAKY_FWRITE_TEXT)
	{
		FILE *f = fopen(file_path, "wb");
		if(!f)
		{
			vl_log(VL_ERROR, "Failed to open file at path: '%s'!\n", file_path);
			return 0;
		}

		// just write the buffer normally
		if(fwrite(buffer, sizeof(char), buffer_len, f) != buffer_len)
			goto snaky_write_file_err;
		else
			fclose(f);
	}
	else if(mode == SNAKY_FWRITE_ENCRYPT)
	{
		snaky_attrib_map map = {0};
		dynmaps_init(&map);
		if(map.alloc_failure)
		{
			vl_log(VL_ERROR, "Allocation error!\n");
			return 0;
		}

		if(snaky_generate_attrib_map(buffer, &map))
		{
			snaky_write_file_map(file_path, &map, mode);
			dynmaps_free_strkeyval(&map);
		}
		else
			goto snaky_write_file_err;
	}
	else
		goto snaky_write_file_err;

	goto snaky_write_file_success;

	snaky_write_file_err:
	vl_log(VL_ERROR, "Failed to write attribute data to file: '%s'!\n", file_path);
	return 0;

	snaky_write_file_success:
	vl_log(VL_SUCCESS, "Attribute data written to file: '%s'!\n", file_path);
	return 1;
}
int snaky_get_next_line(char **cursor, char *buffer, size_t buffer_size)
{
	if(!cursor || !*cursor || strlen(*cursor) == 0 || !buffer || buffer_size == 0)
		return 0;

	// walk until a '\n' is found
	size_t i = 0;
	while(**cursor && **cursor != '\n' && i + 1 < buffer_size)
		buffer[i++] = *(*cursor)++;

	buffer[i] = '\0';

	if(**cursor == '\n')
		(*cursor)++;

	return 1;
}

int snaky_define_constant(const char *str, float value)
{
	if(!init)
	{
		vl_log(VL_ERROR, "Cannot define a constant without initializing snaky_strings!\n");
		return 0;
	}

	dynmaps_set_strkey(&constants, str, value);
	if(constants.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to define constant: '%s'. An allocation error occurred!\n", str);
		return 0;
	}

	return 1;
}
int snaky_define_eval_function(const char *str, snaky_eval_func func)
{
	if(!func)
	{
		vl_log(VL_ERROR, "Cannot define a NULL function!\n");
		return 0;
	}

	dynmaps_set_strkey(&eval_functions, str, func);
	if(eval_functions.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to define function: '%s'. An allocation error occurred!\n", str);
		return 0;
	}

	return 1;
}
int snaky_define_function(const char *str, snaky_data_type return_type, void *fn_ptr)
{
	if(!fn_ptr)
	{
		vl_log(VL_ERROR, "Cannot define a NULL function!\n");
		return 0;
	}

	function func_data = {0};
	func_data.return_type = return_type;
	func_data.fn_ptr = fn_ptr;

	dynmaps_set_strkey(&functions, str, func_data);
	if(functions.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to define function: '%s'. An allocation error occurred!\n", str);
		return 0;
	}

	return 1;
}

void snaky_set_encryption_key(int k)
{
	encrypt_key = k % 26;
}
