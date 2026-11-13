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

// syntax and key marker characters:
#define ATTRIB_STRING_OPENING "{<"
#define ATTRIB_STRING_CLOSING ">}"
#define LABEL_OPENING "/["
#define LABEL_CLOSING "]/"
#define COMMENT_OPENING "/*"
#define COMMENT_CLOSING "*/"
#define ARR_OPENING '['
#define ARR_CLOSING ']'
#define ARR_ELEM_DELIM ','
#define ATTRIB_DELIM ';'
#define DATA_TYPE_ASSIGNMENT ':'
#define OBJ_FIELD_DELIM '.'
#define EXPR_OPENING '('
#define EXPR_CLOSING ')'
// strings can be created using "string" or 'string'
#define STR_MARKER_1 '"'
#define STR_MARKER_2 '\''

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
	void *fn_ptr;
	size_t num_args;
	snaky_data_type return_type;
} function;
typedef struct func_registry
{
	char **keys;
	function *values;
	size_t size, capacity;
	bool alloc_failure;
} func_registry;
static func_registry functions = {0};

static int encrypt_key = INT_MAX % 26;

// custom functions used in attribute expressions:
static float cstm_factorial(float f)
{
	if(f == 0)
		return 1.0f;

	float ff = f;
	while(f > 1)
		ff *= (f -= 1);

	return ff;
}
static float cstm_rads(float f)
{
	return f * (M_PI / 180.0f);
}
static float cstm_abs(float f)
{
	return abs((int) f);
}
static float cstm_pow(float *args, size_t n)
{
	return powf(args[0], args[1]);
}
static float cstm_max(float *args, size_t n)
{
	return fmaxf(args[0], args[1]);
}
static float cstm_min(float *args, size_t n)
{
	return fminf(args[0], args[1]);
}
static float cstm_dist(float *args, size_t n)
{
	float x1 = args[0], y1 = args[1], x2 = args[3], y2 = args[4];
	return sqrtf(powf(x2 - x1, 2) + powf(y2 - y1, 2));
}
static float cstm_logf(float *args, size_t n)
{
	float base = args[0];
	float x = args[1];

	if(x <= 0 || base <= 0 || base == 1.0)
		return 0.0f;

	return logf(x) / logf(base);
}
static double cstm_logd(float *args, size_t n)
{
	double base = args[0];
	double x = args[1];

	if(x <= 0 || base <= 0 || base == 1.0)
		return 0.0;

	return log(x) / log(base);
}

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
	if(!snaky_define_eval_function("factorial", cstm_factorial))
		return 0;
	if(!snaky_define_eval_function("sin", sinf))
		return 0;
	if(!snaky_define_eval_function("cos", cosf))
		return 0;
	if(!snaky_define_eval_function("tan", tanf))
		return 0;
	if(!snaky_define_eval_function("sqrt", sqrtf))
		return 0;
	if(!snaky_define_eval_function("rads", cstm_rads))
		return 0;
	if(!snaky_define_eval_function("round", roundf))
		return 0;
	if(!snaky_define_eval_function("floor", floorf))
		return 0;
	if(!snaky_define_eval_function("ceil", ceilf))
		return 0;
	if(!snaky_define_eval_function("abs", cstm_abs))
		return 0;

	if(!snaky_define_function("pow", SNAKY_FLOAT, cstm_pow, 2))
		return 0;
	if(!snaky_define_function("max", SNAKY_FLOAT, cstm_max, 2))
		return 0;
	if(!snaky_define_function("min", SNAKY_FLOAT, cstm_min, 2))
		return 0;
	if(!snaky_define_function("dist", SNAKY_FLOAT, cstm_dist, 4))
		return 0;
	if(!snaky_define_function("logf", SNAKY_FLOAT, cstm_logf, 2))
		return 0;
	if(!snaky_define_function("logd", SNAKY_DOUBLE, cstm_logd, 2))
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
int snaky_strcat(snaky_string *str, const char *s)
{
	if(!str || !s)
		return 0;

	size_t s_len = strlen(s);

	if(str->len + s_len + 1 >= str->size)
	{
		if(!snaky_realloc_str(str, str->size + s_len))
			return 0;
	}

	strcat(str->data, s);
	str->len = strlen(str->data);

	return 1;
}
int snaky_strequals(const snaky_string *str1, const snaky_string *str2)
{
	if(!str1 || !str2)
		return 0;

	return strcmp(str1->data, str2->data) == 0;
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
static const char *find_end_of_group_str(const char *start, const char *open, const char *close)
{
	// same logic as find_end_of_group(...):

	size_t depth = 0;

	size_t open_len = strlen(open);
	size_t close_len = strlen(close);

	for(const char *p = start; *p; ++p)
	{
		if(strncmp(p, open, open_len) == 0)
			depth++;
		else if(strncmp(p, close, close_len) == 0)
		{
			depth--;

			if(depth == 0)
				return p + close_len;
		}
	}

	return NULL;
}
static const char *find_end_of_str(const char *start)
{
	// the first char should be " or ', now move until the matching string marker is found
	bool search_for_double_quotes = false, search_for_single_quote = false;
	if(*start == '"')
		search_for_double_quotes = true;
	else if(*start == '\'')
		search_for_single_quote = true;
	else
		return NULL;

	for(const char *p = start; *p; ++p)
	{
		// skip any instances of '\"' or '\'' depending on current string marker
		if(*p == '\\')
		{
			if(*(p + 1) == '"' || *(p + 1) == '\'')
			{
				// skip the entire '\"' or '\''string
				p += 2;
				continue;
			}
		}
		else if((search_for_double_quotes && *p == '"') || (search_for_single_quote && *p == '\''))
		{
			// p will point to char directly after the matching closing character
			return p + 1;
		}
	}

	return NULL;
}
static const char *find_end_of_array(const char *start)
{
	return find_end_of_group(start, ARR_OPENING, ARR_CLOSING);
}
static const char *find_end_of_attribs(const char *start)
{
	return find_end_of_group_str(start, ATTRIB_STRING_OPENING, ATTRIB_STRING_CLOSING);
}

// skip all comments in a string until a valid char is reached
static void skip_comment(const char **str, size_t *out_num_spaces)
{
	// after moving to next char, check for a comment
	const char *comment_origin = *str;

	while(**str && strncmp(*str, COMMENT_CLOSING, strlen(COMMENT_CLOSING)) != 0)
	{
		(*str)++;

		if(out_num_spaces)
			*out_num_spaces += 1;
	}

	if(!**str)
	{
		vl_log(VL_ERROR, "Expected '*/' to close off a comment in string: '%s'!\n", comment_origin);
		return;
	}

	// when the '*/' is found, skip those last 2 chars
	(*str) += 2;
	if(out_num_spaces)
		*out_num_spaces += 2;
}
// skip all whitespace in a string until a valid char is reached
static void skip_whitespace(const char **str, size_t *out_num_spaces)
{
	// reset num spaces to 0
	if(out_num_spaces)
		*out_num_spaces = 0;

	// skip all comments found
	while(strncmp(*str, COMMENT_OPENING, strlen(COMMENT_OPENING)) == 0)
		skip_comment(str, out_num_spaces);

	// now skip all spaces, and comments found after those spaces
	while(**str && isspace(**str))
	{
		if(out_num_spaces)
			*out_num_spaces += 1;
		(*str)++;

		// skip any comments found
		while(strncmp(*str, COMMENT_OPENING, strlen(COMMENT_OPENING)) == 0)
			skip_comment(str, out_num_spaces);
	}
}

/*
   - 'start' should point to the first char in an attribute

   example: start = "attrib = VALUE;"

   add to total_len for every character until the next available
   attribute is found
*/
static void obtain_total_attrib_len(const char *start, snaky_attrib_info *out_info)
{
	if(out_info)
	{
		// move forward until the end of the string or end of the attribute string
		while(*start && strncmp(start, ATTRIB_STRING_CLOSING, strlen(ATTRIB_STRING_CLOSING)) != 0)
		{
			start++;
			out_info->total_len++;

			// if a ';' is found, now move forward until the next valid char, and break
			if(*start == ATTRIB_DELIM)
			{
				// skip the ';'
				start++;
				out_info->total_len++;

				// skip all whitespace
				size_t spaces = 0;
				skip_whitespace(&start, &spaces);
				// add to total len then break
				out_info->total_len += spaces;
				break;
			}
		}
	}
}
/*
   - 'start' should point to the character directly after the ':' character
   - 'out_jump' is how many characters are in between the last char in the attrib name and the '=' sign
   - return -1 on complete failure, 0 on successful call but no data type resolved, 1 on complete success
*/
static int resolve_data_type(const char *start, size_t *out_jump, snaky_attrib_info *out_info)
{
	if(!out_info)
		return 0;
	// default to invalid value
	out_info->data_type = SNAKY_INVALID;

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
		while(*start && *start != DATA_TYPE_ASSIGNMENT)
			start++;
	}
	// if now on the ':', skip it
	if(*start == DATA_TYPE_ASSIGNMENT)
		start++;

	// skip all whitespace after the ':'
	skip_whitespace(&start, &num_spaces);
	if(out_jump)
		*out_jump += num_spaces;

	// now find which data type the user provided
	if(strncmp(start, "char", 4) == 0)
	{
		out_info->data_type = SNAKY_CHAR;
		if(out_jump)
			*out_jump += 4;
	}
	else if(strncmp(start, "bool", 4) == 0)
	{
		out_info->data_type = SNAKY_BOOL;
		if(out_jump)
			*out_jump += 4;
	}
	else if(strncmp(start, "int", 3) == 0)
	{
		out_info->data_type = SNAKY_INT;
		if(out_jump)
			*out_jump += 3;
	}
	else if(strncmp(start, "float", 5) == 0)
	{
		out_info->data_type = SNAKY_FLOAT;
		if(out_jump)
			*out_jump += 5;
	}
	else if(strncmp(start, "double", 6) == 0)
	{
		out_info->data_type = SNAKY_DOUBLE;
		if(out_jump)
			*out_jump += 6;
	}
	else if(strncmp(start, "array", 5) == 0)
	{
		out_info->data_type = SNAKY_ARRAY;
		if(out_jump)
			*out_jump += 5;
	}
	else if(strncmp(start, "attribs", 6) == 0)
	{
		out_info->data_type = SNAKY_ATTRIB_STRING;
		if(out_jump)
			*out_jump += 6;
	}
	else if(strncmp(start, "string", 6) == 0)
	{
		out_info->data_type = SNAKY_STRING;
		if(out_jump)
			*out_jump += 6;
	}
	else
	{
		// default to the string type when no type is given
		out_info->data_type = SNAKY_STRING;
		return 1;
	}

	return -1;
}

static void trim_trailing_whitespace_from_value(snaky_string *sbuffer, char *buffer)
{
	// only trim trailing whitespace when the value is not a string (example of a string: "..., str = "some string!!    ""):
	char *buf = sbuffer ? sbuffer->data : buffer;
	char *start_of_str = buf;
	char *end_of_str = buf + strlen(buf) - 1;

	// if a valid string, do not trim
	if((*buf == STR_MARKER_1 || *buf == STR_MARKER_2) && find_end_of_str(buf) != NULL)
		return;

	// otherwise, trim:

	// walk from end to the start, tracking all whitespace:
	while(end_of_str != start_of_str && isspace(*end_of_str))
		end_of_str--;

	// if whitespace was found, the pointers will not be equal
	if(end_of_str != start_of_str)
		*(end_of_str + 1) = '\0';
}
static void return_next_start_pos(const char *attribs_start, snaky_attrib_info *out_info)
{
	if(out_info)
	{
		while(*attribs_start && *attribs_start != ATTRIB_DELIM && strncmp(attribs_start, ATTRIB_STRING_CLOSING, strlen(ATTRIB_STRING_CLOSING)) != 0)
			attribs_start++;

		if(!*attribs_start)
			out_info->next_start_pos = NULL;
		else
		{
			skip_whitespace(&attribs_start, NULL);
			out_info->next_start_pos = attribs_start;
		}
	}
}
static int parse_value(const char *str_origin, const char *value_start, snaky_string *sbuffer, char *buffer, size_t buffer_size, const char *(*end_pos_fn)(const char*), snaky_attrib_info *out_info)
{
	const char *end_pos = NULL;
	if(end_pos_fn)
		end_pos = end_pos_fn(value_start);
	else
	{
		const char *value_start_copy_origin = value_start;
		const char *value_start_copy = value_start;

		while(*value_start_copy && strncmp(value_start_copy, ATTRIB_STRING_CLOSING, strlen(ATTRIB_STRING_CLOSING)) != 0 && *value_start_copy != ATTRIB_DELIM)
			// move end_pos pointer
			end_pos = value_start_copy++;

		// make sure to skip to the actual ending char (',' or '>')
		end_pos++;
		if(!*value_start_copy)
		{
			vl_log(VL_ERROR, "Unexpected termination of string while parsing value: '%s'\n", value_start_copy_origin);
			return 0;
		}
	}

	if(out_info)
		out_info->value_start_pos = value_start;

	size_t i = 0;
	while(*value_start && value_start != end_pos)
	{
		if(sbuffer)
		{
			if(i + 1 >= sbuffer->size)
			{
				if(!snaky_realloc_str(sbuffer, sbuffer->size + STR_CAPACITY_INCREASE))
					return 0;
			}
		}
		else if(buffer && buffer_size > 0)
			if(i + 1 >= buffer_size)
				break;

		char next = *value_start++;

		if(sbuffer)
			sbuffer->data[i++] = next;
		else if(buffer && buffer_size > 0)
			buffer[i++] = next;
	}

	if(sbuffer)
		sbuffer->data[i] = '\0';
	else if(buffer && buffer_size > 0)
		buffer[i] = '\0';

	// see if trailing whitespace should be removed from value (example: "..., end_value = 827    >")
	trim_trailing_whitespace_from_value(sbuffer, buffer);

	// if next start pos queried, return it
	return_next_start_pos(value_start, out_info);

	return 1;
}
static int find_array_element(const char *array_start, const char *array_end, const int *indices, size_t num_indices, size_t depth, const char **out_elem_start)
{
	// this function tracks depth levels between nested arrays and finds the desired element:

	if(depth >= num_indices)
		return 0;

	const char *p = array_start + 1;
	size_t curr_index = 0;

	while(p < array_end)
	{
		skip_whitespace(&p, NULL);

		if(p >= array_end)
			break;

		const char *elem_start = p;
		const char *elem_end = NULL;

		// when a new array is opened:
		if(*p == ARR_OPENING)
		{
			size_t depth = 0;

			do
			{
				// track nested arrays within that first array:
				if(*p == ARR_OPENING)
					depth++;
				else if(*p == ARR_CLOSING)
					depth--;

				p++;
			}
			while(p < array_end && depth > 0);

			// if the depth is mismatched, error out
			if(depth != 0)
				return 0;

			// new elem_end is end of that last parsed array
			elem_end = p - 1;
		}
		// when the current char is not the start of a new array:
		else
		{
			// go to the end of the next full element (counts nested arrays too)
			while(p < array_end && *p != ARR_ELEM_DELIM && *p != ARR_CLOSING)
				p++;

			elem_end = p - 1;

			// trim trailing whitespace on elem_end
			while(elem_end >= elem_start && isspace(*elem_end))
				elem_end--;
		}

		// do not allow negative indices
		if(indices[depth] < 0)
			return 0;

		// when the indices match:
		if(curr_index == (size_t) indices[depth])
		{
			// if depth matches the exact number of indices, a single element is being parsed
			if(depth + 1 == num_indices)
			{
				*out_elem_start = elem_start;
				return 1;
			}

			// otherwise, a nested array will be parsed:
			if(*elem_start != ARR_OPENING)
				return 0;

			// recursively re-find the desired element or nested array:
			return find_array_element(elem_start, elem_end, indices, num_indices, depth + 1, out_elem_start);
		}

		curr_index++;

		p = elem_end + 1;

		skip_whitespace(&p, NULL);

		if(p < array_end && *p == ARR_ELEM_DELIM)
			p++;
	}

	return 0;
}
static int parse_array_element(const char *str_origin, const char *array_start, const char *array_end, const char *elem_start, snaky_string *sbuffer, char *buffer, size_t buffer_size, snaky_attrib_info *out_info)
{
	// save original pos of array
	const char *array_origin = array_start;

	// push array_end back until a non ']' char is found
	array_end--;
	while(*array_end && *array_end == ARR_CLOSING)
		array_end--;
	// push end pointer one slot forward so last element is fully parsed
	array_end++;

	// see if the array is empty (user declared like '[]' or with whitespace: '[   ]')
	array_start++;
	skip_whitespace(&array_start, NULL);
	if(array_start == array_end)
	{
		vl_log(VL_ERROR, "An empty array was encountered, cannot parse elements! String: '%s'!\n", str_origin);
		return 0;
	}

	// current value start of an array is always the opening '['
	if(out_info)
		out_info->value_start_pos = array_start;

	// move array_start to the position of the element within the array
	array_start = elem_start;

	/*
	   if the beginning of the array is another array, see if the
	   parsed element should be a nested array
	*/
	if(*array_start == ARR_OPENING)
	{
		// copy the entire nested array into the chosen buffer
		const char *nested_array_end = find_end_of_array(array_start);
		if(!nested_array_end)
		{
			vl_log(VL_ERROR, "No end of array found: '%s'!\n", array_start);
			return 0;
		}

		size_t i = 0;
		while(*array_start && array_start != nested_array_end)
		{
			if(sbuffer)
			{
				if(i + 1 >= sbuffer->size)
				{
					if(!snaky_realloc_str(sbuffer, sbuffer->size + STR_CAPACITY_INCREASE))
						return 0;
				}
			}
			else if(buffer && buffer_size > 0)
				if(i + 1 >= buffer_size)
					break;

			char next = *array_start++;

			if(sbuffer)
				sbuffer->data[i++] = next;
			else if(buffer && buffer_size > 0)
				buffer[i++] = next;
		}

		if(sbuffer)
			sbuffer->data[i] = '\0';
		else if(buffer && buffer_size > 0)
			buffer[i] = '\0';

		return_next_start_pos(array_origin, out_info);

		return 1;
	}

	// parse current value until the next array elem delimiter is found
	size_t i = 0;
	while(*array_start && array_start != array_end && *array_start != ARR_ELEM_DELIM && *array_start != ARR_CLOSING)
	{
		// copy all the contents in this subregion of the array
		if(sbuffer)
		{
			if(i + 1 >= sbuffer->size)
			{
				if(!snaky_realloc_str(sbuffer, sbuffer->size + STR_CAPACITY_INCREASE))
					return 0;
			}
		}
		else if(buffer && buffer_size > 0)
			if(i + 1 >= buffer_size)
				break;

		// get next char in original string
		char next = *array_start++;

		if(sbuffer)
			sbuffer->data[i++] = next;
		else if(buffer && buffer_size > 0)
			buffer[i++] = next;
	}

	if(sbuffer)
		sbuffer->data[i] = '\0';
	else if(buffer && buffer_size > 0)
		buffer[i] = '\0';

	// obtain next attribute pos (note that this is not the next element pos within the array, but the next attribute)
	return_next_start_pos(array_origin, out_info);

	return 1;
}

// if using a snaky_string, 'sbuffer' won't be NULL, but if using a normal stack-allocated buffer, it will be NULL
static int parse_target_attrib(const char *str_origin, const char *attribs_start, snaky_string *sbuffer, char *buffer, size_t buffer_size, const char *attrib_name, size_t attrib_len, snaky_attrib_info *out_info)
{
	skip_whitespace(&attribs_start, NULL);

	while(*attribs_start && *attribs_start != '=' && *attribs_start != ATTRIB_DELIM)
		attribs_start++;

	if(!*attribs_start)
	{
		vl_log(VL_ERROR, "Expected '=' or '%c' after attribute: '%s'!\n", ATTRIB_DELIM, attrib_name);
		return 0;
	}

	// if no value found, use UNDEF for undefined
	if(*attribs_start == ATTRIB_DELIM)
	{
		if(sbuffer)
			snaky_strcpy(sbuffer, "UNDEF");
		else if(buffer && buffer_size > 0)
			snprintf(buffer, buffer_size, "UNDEF");
		return_next_start_pos(attribs_start, out_info);
		return 1;
	}

	// go to the character after the '='
	attribs_start++;
	skip_whitespace(&attribs_start, NULL);

	if(!*attribs_start)
	{
		vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", str_origin);
		return 0;
	}

	// determine if a nested attribute or string is given
	if(strncmp(attribs_start, ATTRIB_STRING_OPENING, 2) == 0)
	{
		if(out_info)
			out_info->data_type = SNAKY_ATTRIB_STRING;
		return parse_value(str_origin, attribs_start, sbuffer, buffer, buffer_size, find_end_of_attribs, out_info);
	}
	else if(*attribs_start == ARR_OPENING)
	{
		if(out_info)
			out_info->data_type = SNAKY_ARRAY;
		return parse_value(str_origin, attribs_start, sbuffer, buffer, buffer_size, find_end_of_array, out_info);
	}

	// if not a nested string, copy the attrib value normally (no end_pos_fn for a normal value)
	return parse_value(str_origin, attribs_start, sbuffer, buffer, buffer_size, NULL, out_info);
}
static int parse_dimensions(const char *str_origin, const char *attrib_name, size_t attrib_name_len, size_t *out_dimensions, size_t *out_num_dimensions)
{
	*out_num_dimensions = 0;

	// track nested array info
	size_t comma_counts[SNAKY_MAX_ARR_DIMENSIONS] = {0};
	int has_elements[SNAKY_MAX_ARR_DIMENSIONS] = {0};

	// track dimension info
	size_t dimension_counts[SNAKY_MAX_ARR_DIMENSIONS] = {0};
	int dimension_seen[SNAKY_MAX_ARR_DIMENSIONS] = {0};

	// depth is used to track the current nested array
	size_t depth = 0;
	// max_depth tracks the most amount of nested arrays currently found
	size_t max_depth = 0;

	// TODO add empty element validation (ex.: [0,,1] should fail)?

	const char *array_start = NULL;
	for(const char *p = str_origin; *p; ++p)
	{
		// find the name of the array
		if(strncmp(p, attrib_name, attrib_name_len) == 0)
		{
			array_start = p;

			// jump to end of array name
			p += attrib_name_len;

			skip_whitespace(&p, NULL);

			// move until the first '[' is found
			while(*p && *p != ARR_OPENING)
				p++;

			skip_whitespace(&p, NULL);

			// now p should point to the first '['
			if(!*p)
			{
				vl_log(VL_ERROR, "Expected '%c' in array: '%s'!\n", ARR_OPENING, array_start);
				return 0;
			}

			// parse dimensions and commas:
			while(*p)
			{
				skip_whitespace(&p, NULL);

				if(!*p)
				{
					vl_log(VL_ERROR, "Unexpected termination of array string: '%s'!\n", array_start);
					return 0;
				}

				// when a new array is opened:
				if(*p == ARR_OPENING)
				{
					// make sure the max amount of dimensions is not crossed
					if(depth >= SNAKY_MAX_ARR_DIMENSIONS)
					{
						vl_log(VL_ERROR, "Max number of array dimensions reached: '%s'!\n", array_start);
						return 0;
					}

					// when depth is at least 1, this array has elements
					if(depth > 0)
						has_elements[depth - 1] = 1;

					// reset counts and statuses for the new array
					comma_counts[depth] = 0;
					has_elements[depth] = 0;

					// a new array means depth must be incremented
					depth++;

					// if max depth is reached, make that depth the new max depth
					if(depth > max_depth)
						max_depth = depth;

					p++;
					continue;
				}

				// when a comma is encountered:
				if(*p == ARR_ELEM_DELIM)
				{
					// if no arrays have been found yet, error out
					if(depth == 0)
					{
						vl_log(VL_ERROR, "Empty array encountered: '%s'!\n", array_start);
						return 0;
					}

					// the array at this depth gains a comma count
					comma_counts[depth - 1]++;

					p++;
					continue;
				}

				// when a ']' is encountered:
				if(*p == ARR_CLOSING)
				{
					// if no arrays have been found yet, error out
					if(depth == 0)
					{
						vl_log(VL_ERROR, "Empty array encountered: '%s'!\n", array_start);
						return 0;
					}

					// go back one level
					depth--;

					// count amount of elements in the array just parsed:
					size_t elem_count = 0;

					// if it had any elements:
					if(has_elements[depth])
						// the amount of elements is the amount of commas + 1
						elem_count = comma_counts[depth] + 1;

					if(dimension_seen[depth])
					{
						// enforce that every array has the same amount of elements:
						if(dimension_counts[depth] != elem_count)
						{
							vl_log(VL_ERROR, "Array size does not match other dimensions: '%s'!\n", array_start);
							return 0;
						}
					}
					else
					{
						// save this dimension info for the parsed nested array
						dimension_counts[depth] = elem_count;
						dimension_seen[depth] = 1;
					}

					p++;

					// at the end of parsing, depth of 0 means all arrays were successfully parsed
					if(depth == 0)
					{
						// now copy the dimensions info into the output args:
						for(size_t i = 0; i < max_depth; ++i)
							out_dimensions[i] = dimension_counts[i];

						*out_num_dimensions = max_depth;

						return 1;
					}

					continue;
				}

				// on any other character, a depth of 0 means an element was encountered
				if(depth == 0)
					return 0;

				has_elements[depth - 1] = 1;

				p++;
			}

			vl_log(VL_ERROR, "Unexpected termination of array string: '%s'!\n", array_start);
			return 0;
		}
	}

	if(array_start)
		vl_log(VL_ERROR, "Failed to parse dimensions in array: '%s'!\n", array_start);
	return 0;
}
static size_t parse_index(const char *str_origin, const char **index_values, size_t num_indices, size_t *dimensions, size_t num_dimensions, int *out_success)
{
	// default to failure status
	*out_success = 0;

	// make sure invalid array elements won't be read later
	if(num_indices == 0 || num_indices > num_dimensions)
		return 0;

	// parse each individual index and add to final_index
	size_t final_index = 0;
	for(size_t i = 0; i < num_indices; ++i)
	{
		int idx = snaky_parse_int(str_origin, index_values[i], out_success);
		if(*out_success == 0)
			return 0;

		if(idx < 0 || (size_t) idx >= dimensions[i])
		{
			*out_success = 0;
			return 0;
		}

		if(dimensions[i] == 0)
		{
			*out_success = 0;
			return 0;
		}

		if(final_index > (SIZE_MAX - (size_t) idx) / dimensions[i])
		{
			*out_success = 0;
			return 0;
		}

		final_index = final_index * dimensions[i] + (size_t) idx;
	}

	*out_success = 1;
	return final_index;
}
static int prepare_parse_target_attrib(const char *str, const char *attrib_name, snaky_string *sbuffer, char *buffer, size_t buffer_size, snaky_attrib_info *out_info)
{
	const char *str_origin = str;

	size_t attrib_len = attrib_name ? strlen(attrib_name) : 0;

	if(!str || strlen(str) == 0 || !attrib_name || attrib_len == 0)
		return 0;

	// see if resolved attrib name is invalid (starts with number of symbol)
	if(!isalpha(*attrib_name) && !snaky_is_string(attrib_name))
		return 0;

	// see if user is trying to find a nested attribute :
	char substrings[SNAKY_MAX_SUBSTRINGS][SNAKY_BUF_SIZE] = {0};
	size_t num_substrings = 0;

	char substring[SNAKY_BUF_SIZE] = {0};
	size_t substring_iter = 0;
	bool at_least_one_subfield_found = false;
	for(const char *p = attrib_name; *p; ++p)
	{
		if(substring_iter + 1 >= sizeof(substring))
			break;

		// if a space is found, stop parsing substrings (substrings must be chained together with no whitespace)
		if(isspace(*p))
		{
			// reset all substring data just in case some were already parsed
			at_least_one_subfield_found = false;
			num_substrings = 0;
			break;
		}

		// check for a '.' which means the current substring has ended
		if(*p == OBJ_FIELD_DELIM)
		{
			// if no more rows, error out
			if(num_substrings >= SNAKY_MAX_SUBSTRINGS)
			{
				vl_log(VL_ERROR, "Max number of substrings allowed in targeted nested attribute string: '%s'\n", attrib_name);
				return 0;
			}

			// end the current substring
			substring[substring_iter] = '\0';

			// otherwise, add the new substring to the array
			strcpy(substrings[num_substrings++], substring);

			// reset data for next substring
			substring_iter = 0;

			// signal that at least one period was found
			at_least_one_subfield_found = true;

			continue;
		}

		// if not on a '.' yet, add the current char to the current substring array
		substring[substring_iter++] = *p;

		// if the next char is the null terminator and at least one other field was found, end the last substring
		if(*(p + 1) == '\0' && at_least_one_subfield_found)
		{
			// same logic as for when the '.' is found:

			if(num_substrings >= SNAKY_MAX_SUBSTRINGS)
			{
				vl_log(VL_ERROR, "Max number of substrings allowed in targeted nested attribute string: '%s'\n", attrib_name);
				return 0;
			}

			substring[substring_iter] = '\0';

			strcpy(substrings[num_substrings++], substring);

			substring_iter = 0;

			continue;
		}
	}

	/*
	   now iterate over substring array, and parse each substring as an individual target attribute

	   also, every time one of the substring attributes is parsed, move the string pointer to that
	   attribute's position
	*/
	snaky_attrib_info pos_info = {0};
	for(size_t s = 0; s < num_substrings; ++s)
	{
		if(sbuffer)
		{
			if(!snaky_parse_target_attrib_str(pos_info.value_start_pos, substrings[s], sbuffer, &pos_info))
			{
				vl_log(VL_ERROR, "Failed to parse substring attribute: '%s' in string '%s'\n", substrings[s], str);
				return 0;
			}
		}
		else if(buffer && buffer_size > 0)
		{
			if(!snaky_parse_target_attrib_buf(pos_info.value_start_pos, substrings[s], buffer, buffer_size, &pos_info))
			{
				vl_log(VL_ERROR, "Failed to parse substring attribute: '%s' in string '%s'\n", substrings[s], str);
				return 0;
			}
		}
	}

	/*
	   if num_substrings is > 0 (the for loop above did execute), and the function hasn't returned yet,
	   then the last nested attribute was parsed successfully
	   and its value was inserted into the correct buffer (sbuffer or buffer),
	   which means the function has succeeded, return 1
	*/
	if(num_substrings > 0)
	{
		// see if 'out_curr_start_pos' needs to use 'pos'
		if(out_info)
			out_info->value_start_pos = pos_info.value_start_pos;
		return 1;
	}

	// see if user searching for a value within an array (requires '[N]' in 'attrib_name')
	char index_values[SNAKY_MAX_ARR_DIMENSIONS][SNAKY_BUF_SIZE] = {0};
	size_t num_indices = 0;

	char index[SNAKY_BUF_SIZE] = {0};
	size_t index_iter = 0;
	for(const char *p = attrib_name; *p; ++p)
	{
		if(*p == ARR_OPENING)
		{
			p++;

			while(*p && *p != ARR_CLOSING)
			{
				if(index_iter + 1 >= sizeof(index))
					break;
				index[index_iter++] = *p++;
			}

			index[index_iter] = '\0';

			strcpy(index_values[num_indices++], index);

			index_iter = 0;
		}
	}

	// if num_indices exceeds 0, user provided at least one index
	if(num_indices > 0)
	{
		// because the attrib name contains a '[N]...' part, the actual length of the array's name in the string does not include these characters
		size_t actual_array_name_len = 0;
		for(const char *a = attrib_name; *a; ++a)
		{
			if(*a == ARR_OPENING)
				break;
			actual_array_name_len++;
		}

		// now parse the index values:
		const char *index_values_ptr[num_indices];
		for(size_t i = 0; i < num_indices; ++i)
			index_values_ptr[i] = index_values[i];

		/*
		   dimensions array represents the size of the array ([[0, 1, 2], [3, 4, 5]] would be represented as { 2, 3 })

		   this means the dimensions array is always going to store 2 numbers
		*/
		size_t dimensions[SNAKY_MAX_ARR_DIMENSIONS] = {0};
		size_t num_dimensions = 0;
		if(!parse_dimensions(str_origin, attrib_name, actual_array_name_len, dimensions, &num_dimensions))
			return 0;

		int s = 0;
		int index = parse_index(str_origin, index_values_ptr, num_indices, dimensions, num_dimensions, &s);
		if(s == 0)
		{
			vl_log(VL_ERROR, "Invalid index or failed parsing: '%s'!\n", attrib_name);
			return 0;
		}

		// validate index
		if(index < 0)
		{
			vl_log(VL_ERROR, "Cannot have a negative index: '%s'!\n", attrib_name);
			return 0;
		}

		// the final index represents the final linear index so go to the comma in the array matching that number
		for(const char *p = str; *p; ++p)
		{
			if(strncmp(p, attrib_name, actual_array_name_len) == 0)
			{
				while(*p && *p != ARR_OPENING && strncmp(p, ATTRIB_STRING_OPENING, strlen(ATTRIB_STRING_OPENING)) != 0)
					p++;
				if(!*p)
				{
					vl_log(VL_ERROR, "Unexpected termination of string: '%s'!\n", str);
					return 0;
				}
				// see if a nested attribute string was found, meaning the array element cannot be parsed directly
				if(strncmp(p, ATTRIB_STRING_OPENING, strlen(ATTRIB_STRING_OPENING)) == 0)
				{
					vl_log(VL_ERROR, "Array elements cannot be parsed yet, a nested attribute string was encountered: '%s'!\n", p);
					return 0;
				}

				const char *array_start = p;

				const char *array_end = find_end_of_array(array_start);
				if(!array_end)
				{
					vl_log(VL_ERROR, "No end of array found: '%s'!\n", array_start);
					return 0;
				}

				// convert index strings to ints in an array
				int real_indices[num_indices];
				for(size_t i = 0; i < num_indices; ++i)
				{
					int s = 0;
					real_indices[i] = snaky_parse_int(str_origin, index_values[i], &s);
					if(s == 0 || real_indices[i] < 0)
					{
						vl_log(VL_ERROR, "Failed to parse index: '%s'!\n", index_values[i]);
						return 0;
					}
				}

				// when the array is found, go to the specific index in the array
				const char *elem_start = NULL;
				if(!find_array_element(array_start, array_end, real_indices, num_indices, 0, &elem_start))
				{
					vl_log(VL_ERROR, "Failed to locate array element: '%s'!\n", attrib_name);
					return 0;
				}

				return parse_array_element(str_origin, array_start, array_end, elem_start, sbuffer, buffer, buffer_size, out_info);
			}
		}

		return 0;
	}

	bool in_attrib_string = false;

	for(const char *p = str; *p; ++p)
	{
		// get char
		char c = *p;

		// exit an attrib string when '}>' is found
		if(strncmp(p, ATTRIB_STRING_CLOSING, strlen(ATTRIB_STRING_CLOSING)) == 0 && in_attrib_string)
			in_attrib_string = false;

		/*
		   also exit the current attrib string when an
		   object is encountered

		   this way, when the next '<' is found for the
		   object, its fields can be parsed as well
		*/
		if(strncmp(p, ATTRIB_STRING_OPENING, strlen(ATTRIB_STRING_OPENING)) == 0 && in_attrib_string)
			in_attrib_string = false;

		// when the first '<' or ',' is encountered, compare attrib to the target arg
		if(strncmp(p, ATTRIB_STRING_OPENING, strlen(ATTRIB_STRING_OPENING)) == 0 && !in_attrib_string)
		{
			// now the parser is in an attribute string
			in_attrib_string = true;

			// compare the attribute with the target attribute
			const char *attribs_start = p + strlen(ATTRIB_STRING_OPENING);

			skip_whitespace(&attribs_start, NULL);

			if(!*attribs_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", str);
				return 0;
			}

			if(strncmp(attribs_start, attrib_name, attrib_len) == 0)
			{
				obtain_total_attrib_len(attribs_start, out_info);
				resolve_data_type(attribs_start, NULL, out_info);
				if(out_info)
					out_info->name_start_pos = attribs_start;
				return parse_target_attrib(str_origin, attribs_start, sbuffer, buffer, buffer_size, attrib_name, attrib_len, out_info);
			}

			continue;
		}

		// parse all subsequent attributes in an attrib string
		if(c == ATTRIB_DELIM && in_attrib_string)
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
				obtain_total_attrib_len(attribs_start, out_info);
				resolve_data_type(attribs_start, NULL, out_info);
				if(out_info)
					out_info->name_start_pos = attribs_start;
				return parse_target_attrib(str_origin, attribs_start, sbuffer, buffer, buffer_size, attrib_name, attrib_len, out_info);
			}

			continue;
		}
	}

	return 0;
}
int snaky_parse_target_attrib_buf(const char *str, const char *attrib_name, char *buffer, size_t buffer_size, snaky_attrib_info *out_info)
{
	return prepare_parse_target_attrib(str, attrib_name, NULL, buffer, buffer_size, out_info);
}
int snaky_parse_target_attrib_str(const char *str, const char *attrib_name, snaky_string *buffer, snaky_attrib_info *out_info)
{
	return prepare_parse_target_attrib(str, attrib_name, buffer, NULL, 0, out_info);
}

static int parse_attrib(const char *str_origin, const char *attribs_start, snaky_string *name_str, snaky_string *value_str, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, snaky_attrib_info *out_info)
{
	skip_whitespace(&attribs_start, NULL);

	obtain_total_attrib_len(attribs_start, out_info);

	if(out_info)
		out_info->name_start_pos = attribs_start;

	// 'attribs_start' points to the first char in the attrib name
	size_t i = 0;
	bool data_type_resolved = false;
	while(*attribs_start && *attribs_start != '=' && *attribs_start != ATTRIB_DELIM && strncmp(attribs_start, ATTRIB_STRING_CLOSING, strlen(ATTRIB_STRING_CLOSING)) != 0)
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

		// see if a ':' is found
		if(next == DATA_TYPE_ASSIGNMENT)
		{
			size_t jump = 0;
			resolve_data_type(attribs_start, &jump, out_info);
			attribs_start += jump;
			data_type_resolved = true;
			continue;
		}

		if(name_str)
			name_str->data[i++] = next;
		else if(name_buffer && name_buffer_size > 0)
			name_buffer[i++] = next;
	}

	if(name_str)
		name_str->data[i] = '\0';
	else if(name_buffer && name_buffer_size > 0)
		name_buffer[i] = '\0';

	// if no value found, just ';', use UNDEF for undefined value
	if(*attribs_start == ATTRIB_DELIM)
	{
		if(value_str)
			snaky_strcpy(value_str, "UNDEF");
		else if(value_buffer && value_buffer_size > 0)
			snprintf(value_buffer, value_buffer_size, "UNDEF");
		return_next_start_pos(attribs_start, out_info);
		return 1;
	}

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

	if(!*attribs_start)
	{
		vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", str_origin);
		return 0;
	}

	// out_curr_start_pos should point to first char in the value string
	if(out_info)
		out_info->value_start_pos = attribs_start;

	if(*attribs_start == ARR_OPENING)
	{
		if(out_info)
			out_info->data_type = SNAKY_ARRAY;
		return parse_value(str_origin, attribs_start, value_str, value_buffer, value_buffer_size, find_end_of_array, out_info);
	}
	else if(*attribs_start == STR_MARKER_1 || *attribs_start == STR_MARKER_2)
	{
		if(out_info)
			out_info->data_type = SNAKY_STRING;
		return parse_value(str_origin, attribs_start, value_str, value_buffer, value_buffer_size, find_end_of_str, out_info);
	}

	// if not a nested string, copy the attrib value normally
	i = 0;
	while(*attribs_start && strncmp(attribs_start, ATTRIB_STRING_CLOSING, strlen(ATTRIB_STRING_CLOSING)) != 0 && *attribs_start != ATTRIB_DELIM)
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

		if(value_buffer)
			value_buffer[i++] = next;
		else if(value_str)
			value_str->data[i++] = next;
	}

	if(value_buffer)
		value_buffer[i] = '\0';
	else if(value_str)
		value_str->data[i] = '\0';

	// return next start pos
	return_next_start_pos(attribs_start, out_info);

	// default to string here if no other data type resolved
	if(!data_type_resolved && out_info)
		out_info->data_type = SNAKY_STRING;

	return 1;
}
static int prepare_parse_attrib(const char *str, snaky_string *name_str, snaky_string *value_str, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, snaky_attrib_info *out_info)
{
	const char *str_origin = str;

	if(!str || strlen(str) == 0)
		return 0;

	bool in_attrib_string = false;

	for(const char *p = str; *p; ++p)
	{
		// get char
		char c = *p;

		// see if the first char in the string is just a letter
		if(isalpha(c))
			return parse_attrib(str_origin, p, name_str, value_str, name_buffer, name_buffer_size, value_buffer, value_buffer_size, out_info);

		// if the first char is a string marker, skip the string entirely
		if(c == STR_MARKER_1 || c == STR_MARKER_2)
		{
			const char *start_of_str = p;

			const char *end_of_str = find_end_of_str(p);
			if(!end_of_str)
			{
				vl_log(VL_ERROR, "Object was never closed in string: '%s'!\n", start_of_str);
				return 0;
			}

			while(*p && p != end_of_str)
				++p;

			continue;
		}

		// when the first '<' or ',' is encountered, see what the very next attrib is
		if((strncmp(p, ATTRIB_STRING_OPENING, strlen(ATTRIB_STRING_OPENING)) == 0 || c == ATTRIB_DELIM) && !in_attrib_string)
		{
			in_attrib_string = true;

			const char *attribs_start = p + strlen(ATTRIB_STRING_OPENING);

			skip_whitespace(&attribs_start, NULL);

			if(!*attribs_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", str);
				return 0;
			}

			return parse_attrib(str_origin, attribs_start, name_str, value_str, name_buffer, name_buffer_size, value_buffer, value_buffer_size, out_info);
		}

		// parse the very next attrib found
		if(c == ATTRIB_DELIM && in_attrib_string)
		{
			const char *attribs_start = p + 1;

			skip_whitespace(&attribs_start, NULL);

			if(*attribs_start)
			{
				vl_log(VL_ERROR, "Unexpected termination of attribute string: '%s'!\n", str);
				return 0;
			}

			return parse_attrib(str_origin, attribs_start, name_str, value_str, name_buffer, name_buffer_size, value_buffer, value_buffer_size, out_info);
		}

		// nested attributes are skipped
		if(strncmp(p, ATTRIB_STRING_OPENING, strlen(ATTRIB_STRING_OPENING)) == 0 && in_attrib_string)
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
		if(strncmp(p, ATTRIB_STRING_CLOSING, strlen(ATTRIB_STRING_CLOSING)) == 0 && in_attrib_string)
			in_attrib_string = false;
	}

	return 0;
}
int snaky_parse_attrib_buf(const char *str, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, snaky_attrib_info *out_info)
{
	return prepare_parse_attrib(str, NULL, NULL, name_buffer, name_buffer_size, value_buffer, value_buffer_size, out_info);
}
int snaky_parse_attrib_str(const char *str, snaky_string *name_str, snaky_string *value_str, snaky_attrib_info *out_info)
{
	return prepare_parse_attrib(str, name_str, value_str, NULL, 0, NULL, 0, out_info);
}

int snaky_remove_attrib_buf(char *str, const char *attrib_name)
{
	size_t attrib_name_len = attrib_name ? strlen(attrib_name) : 0;

	if(!str || strlen(str) == 0 || !attrib_name || attrib_name_len == 0)
		return 0;

	// see if the attrib is found
	snaky_attrib_info info = {0};
	char attrib_value[SNAKY_BUF_SIZE];
	if(!snaky_parse_target_attrib_buf(str, attrib_name, attrib_value, sizeof(attrib_value), &info))
	{
		vl_log(VL_ERROR, "The '%s' attribute was not found in this string, unable to remove: '%s'!\n", attrib_name, str);
		return 0;
	}

	// the attrib is present in the string so it can be removed:

	/*
	   the start_pos pointer points to the first character of the attrib value,
	   so it can be moved backwards to obtain the total length of the sub-string

	   + 2 for the '=' and attribute separator
	*/
	size_t sub_str_len = info.total_len;

	// now sub_str_len represents the total amount of characters that need to be removed

	// now get a pointer to the first character separating this attribute
	char *sub_str_start_pos = (char*) info.name_start_pos;

	memmove(sub_str_start_pos, sub_str_start_pos + sub_str_len, strlen(sub_str_start_pos + sub_str_len) + 1);

	// see if that was the last attribute in the string, and if so, remove trailing ';' and replace with '}>'
	size_t len = strlen(str);

	return 1;
}
int snaky_remove_attrib_str(snaky_string *str, const char *attrib_name)
{
	int s = snaky_remove_attrib_buf(str->data, attrib_name);
	str->len = strlen(str->data);
	return s;
}
int snaky_remove_attribs_buf(char *str, const char *attribs)
{
	size_t attribs_len = attribs ? strlen(attribs) : 0;

	if(!str || strlen(str) == 0 || !attribs || attribs_len == 0)
		return 0;

	// generate an attrib map (remember the values of these attributes shoud be "UNDEF")
	snaky_attrib_map map = {0};
	dynmaps_init(&map);
	if(map.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for attribute map!\n");
		return 0;
	}

	if(snaky_generate_attrib_map(attribs, &map))
	{
		// go through map and remove each individual entry
		for(size_t i = 0; i < map.size; ++i)
		{
			const char *name = map.keys[i];

			// remove it
			if(!snaky_remove_attrib_buf(str, name))
			{
				dynmaps_free_strkeyval(&map);
				return 0;
			}
		}

		dynmaps_free_strkeyval(&map);
		return 1;
	}

	vl_log(VL_ERROR, "Failed to remove attributes: original string: '%s', target attributes string: '%s'!\n", str, attribs);
	return 0;
}
int snaky_remove_attribs_str(snaky_string *str, const char *attribs)
{
	return snaky_remove_attribs_buf(str->data, attribs);
}

static void insert_char_buf(char *buffer, char c)
{
	// move all chars at pos one slot over to the right
	memmove(buffer + 1, buffer, strlen(buffer) + 1);
	*(buffer++) = c;
}
static const char *insert_str_buf(char *buffer, const char *str)
{
	size_t str_len = strlen(str);

	for(size_t i = 0; i < str_len; ++i)
		insert_char_buf(buffer + i, *(str + i));

	return buffer + str_len;
}
int snaky_set_attrib_buf(char *buf, size_t buffer_size, const char *attrib_name, const char *new_attrib_value)
{
	size_t str_len = strlen(buf);

	if(!buf || str_len >= buffer_size || buffer_size == 0 || !attrib_name || strlen(attrib_name) == 0 || !new_attrib_value || strlen(new_attrib_value) == 0)
		return 0;

	char *edit_pos = NULL;
	size_t curr_len = 0;
	bool attrib_found = false;

	snaky_attrib_info info = {0};
	char attrib_value[SNAKY_BUF_SIZE];
	if(snaky_parse_target_attrib_buf(buf, attrib_name, attrib_value, sizeof(attrib_value), &info))
	{
		edit_pos = (char*) info.value_start_pos;
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
		// new_len must contain '; attrib_name=attrib_value'
		new_len = strlen(attrib_name) + strlen(new_attrib_value) + 3;

		size_t new_total_len = str_len + new_len;

		if(new_total_len + 1 > buffer_size)
			return 0;
	}

	if(!attrib_found)
	{
		/*
		   add '; attrib_name = new_attrib_value' to string,
		   unless the attribute string is empty, then just add
		   'attrib_name=new_attrib_value' to it
		*/

		// walk forward in string until the next '>' is hit, but preserve the string's origin ptr
		const char *origin = buf;
		while(*buf && strncmp(buf, ATTRIB_STRING_CLOSING, strlen(ATTRIB_STRING_CLOSING)) != 0)
		{
			if(*buf == STR_MARKER_1 || *buf == STR_MARKER_2)
			{
				const char *end_of_str = find_end_of_str(buf);
				if(!end_of_str)
				{
					vl_log(VL_ERROR, "Object was never closed in string: '%s'!\n", buf);
					return 0;
				}

				// jump to end of string
				buf = (char*) end_of_str;
				continue;
			}

			buf++;
		}

		// use origin here instead of the current pointer
		bool empty = strlen(origin) == 0;
		size_t num_attribs = empty ? 0 : snaky_count_attribs(origin);

		if(!empty && num_attribs > 0)
		{
			insert_char_buf(buf++, ATTRIB_DELIM);
			insert_char_buf(buf++, ' ');
		}
		else if(empty)
			// insert '{<'
			buf = (char*) insert_str_buf(buf++, ATTRIB_STRING_OPENING);

		// now insert attrib name
		buf = (char*) insert_str_buf(buf++, attrib_name);

		// now insert the ' = '
		insert_char_buf(buf++, '=');

		// now insert the new attrib value
		buf = (char*) insert_str_buf(buf++, new_attrib_value);

		// close attrib string if necessary
		if(empty || num_attribs == 0)
			buf = (char*) insert_str_buf(buf++, ATTRIB_STRING_CLOSING);
	}
	else
	{
		// modify attrib value directly within string:

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

	snaky_attrib_info info = {0};
	char attrib_value[SNAKY_BUF_SIZE];
	if(snaky_parse_target_attrib_buf(str->data, attrib_name, attrib_value, sizeof(attrib_value), &info))
	{
		edit_pos = (char*) info.value_start_pos;
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

	int s = snaky_set_attrib_buf(str->data, str->size, attrib_name, new_attrib_value);

	// re-calculate string length
	str->len = s ? strlen(str->data) : 0;

	return s;
}
int snaky_set_attribs_buf(char *str, size_t buffer_size, const char *attribs)
{
	if(!str || strlen(str) >= buffer_size || buffer_size == 0 || !attribs || strlen(attribs) == 0)
		return 0;

	// because 'attribs' should be its own attribute string, get the attribute data from it
	snaky_attrib_map map = {0};
	dynmaps_init(&map);
	if(map.alloc_failure)
	{
		vl_log(VL_ERROR, "Failed to allocate memory for attribute map!\n");
		return 0;
	}

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

	vl_log(VL_ERROR, "Failed to set attributes: original string: '%s', target attributes string: '%s'!\n", str, attribs);
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

	vl_log(VL_ERROR, "Failed to set attributes: original string: '%s', target attributes string: '%s'!\n", str->data, attribs);
	return 0;
}

size_t snaky_count_attribs(const char *str)
{
	if(!str || strlen(str) == 0)
		return 0;

	snaky_attrib_map map = {0};
	dynmaps_init(&map);
	if(map.alloc_failure)
		return 0;

	snaky_generate_attrib_map(str, &map);
	size_t count = map.size;
	dynmaps_free_strkeyval(&map);

	return count;
}

bool snaky_is_string(const char *str)
{
	if(!str)
		return false;

	// make sure string starts with '"'
	bool valid_start = *str && (*str == STR_MARKER_1 || *str == STR_MARKER_2);

	// find the matching '}'
	const char *end_pos = find_end_of_str(str);

	// if all those are true, then it's a string
	return valid_start && end_pos != NULL;
}
bool snaky_is_attrib_string(const char *str)
{
	if(!str || strncmp(str, ATTRIB_STRING_OPENING, strlen(ATTRIB_STRING_OPENING)) != 0)
		return false;

	// find the matching '>'
	const char *end_pos = find_end_of_attribs(str);

	// if a matching '>' was found, then it is an attribute string
	return end_pos != NULL;
}

void snaky_goto_attrib(const char *str, const char *attrib_name, const char **out_attrib_pos)
{
	if(!out_attrib_pos)
		return;

	*out_attrib_pos = NULL;

	size_t attrib_name_len = attrib_name ? strlen(attrib_name) : 0;

	if(!str || strlen(str) == 0 || !attrib_name || attrib_name_len == 0)
		return;

	while(*str)
	{
		if(strncmp(str, attrib_name, attrib_name_len) == 0)
		{
			*out_attrib_pos = str;
			return;
		}

		str++;
	}

	vl_log(VL_ERROR, "No attribute with this name '%s' found!\n", attrib_name);
}
void snaky_goto_label(const char *str, const char *label_name, const char **out_pos)
{
	if(!out_pos)
		return;

	*out_pos = NULL;

	size_t label_name_len = label_name ? strlen(label_name) : 0;

	if(!str || strlen(str) == 0 || !label_name || label_name_len == 0)
		return;

	while(*str)
	{
		// first, find the "/[" characters
		if(strncmp(str, LABEL_OPENING, strlen(LABEL_OPENING)) == 0)
		{
			const char *label_origin = str;

			// move past the "/["
			str += strlen(LABEL_OPENING);

			skip_whitespace(&str, NULL);

			// now see if the name matches
			if(strncmp(str, label_name, label_name_len) == 0)
			{
				// if the names match, now move to the first valid char after the closing "]/" characters
				while(*str)
				{
					if(strncmp(str, LABEL_CLOSING, strlen(LABEL_CLOSING)) == 0)
					{
						// move past the "]/"
						str += strlen(LABEL_CLOSING);

						// skip all whitespace again
						skip_whitespace(&str, NULL);

						// see if a valid char exists there
						if(!*str)
						{
							vl_log(VL_ERROR, "No valid characters found after label: '%s'!\n", label_origin);
						}

						// now return the pos
						*out_pos = str;
						return;
					}

					str++;
				}
			}
		}

		str++;
	}

	vl_log(VL_ERROR, "No label with this name '%s' found!\n", label_name);
}

int snaky_generate_attrib_map(const char *str, snaky_attrib_map *map)
{
	if(!str || strlen(str) == 0 || !map || !map->keys || !map->values)
		return 0;

	snaky_string name = {0};
	if(!snaky_prepare_str(&name, SNAKY_BUF_SIZE))
		return 0;

	snaky_string value = {0};
	if(!snaky_prepare_str(&value, SNAKY_BUF_SIZE))
	{
		snaky_free_str(&name);
		return 0;
	}

	snaky_attrib_info info = {0};
	info.next_start_pos = str;
	while(info.next_start_pos && *info.next_start_pos && snaky_parse_attrib_str(info.next_start_pos, &name, &value, &info))
	{
		if(!info.next_start_pos || !*info.next_start_pos)
		{
			snaky_free_str(&name);
			snaky_free_str(&value);

			// IMPORTANT: 1 is returned here because a NULL next start pos just means there are no more attributes to parse
			return 1;
		}
		else
			skip_whitespace(&info.next_start_pos, NULL);

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
	if(!snaky_prepare_str(&name, SNAKY_BUF_SIZE))
		return 0;

	snaky_string value = {0};
	if(!snaky_prepare_str(&value, SNAKY_BUF_SIZE))
	{
		snaky_free_str(&name);
		return 0;
	}

	snaky_attrib_info info = {0};
	info.next_start_pos = str;
	while(info.next_start_pos && snaky_parse_attrib_str(info.next_start_pos, &name, &value, &info))
	{
		snaky_attrib_entry entry = {0};
		if(!snaky_prepare_str(&entry.name, SNAKY_BUF_SIZE))
		{
			snaky_free_str(&name);
			snaky_free_str(&value);
			return 0;
		}
		if(!snaky_prepare_str(&entry.value, SNAKY_BUF_SIZE))
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

		entry.data_type = info.data_type;
		snaky_parse_data_type(info.data_type, entry.data_type_str, sizeof(entry.data_type_str));

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
		entry->data_type = SNAKY_INVALID;
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
	else if(strncmp(str, "1", 1) == 0)
	{
		if(out_success)
			*out_success = 1;
		return true;
	}
	else if(strncmp(str, "0", 1) == 0)
	{
		if(out_success)
			*out_success = 1;
		return false;
	}
	else
		return false;
}

// returns the current char the pointer is on
static char peek_next_char(const char **str)
{
	skip_whitespace(str, NULL);
	return **str;
}
// moves pointer one slot forward and returns the char in that new slot
static char get_next_char(const char **str)
{
	skip_whitespace(str, NULL);
	return *(*str)++;
}
static float parse_term(const char*, const char**, int*);
static float parse_bitwise(const char *origin, const char **str, int *out_success)
{
	int s = 0;
	// parse the first term
	float f = parse_term(origin, str, &s);
	*out_success = s;
	if(s == 0)
		return 0.0f;

	while(peek_next_char(str) == '|' || peek_next_char(str) == '&' || peek_next_char(str) == '^' ||
			peek_next_char(str) == '<' || peek_next_char(str) == '>')
	{
		char op = get_next_char(str);

		// see if the operator is actually a "<<" or ">>" shift
		char bitshift[3] = {0};
		if(op == '<')
			strcpy(bitshift, "<<");
		else if(op == '>')
			strcpy(bitshift, ">>");
		size_t bitshift_len = strlen(bitshift);
		if(bitshift_len > 0)
			get_next_char(str);

		// parse the second term
		float next_val = parse_term(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;

		int fi = (int) f;
		int next_val_i = (int) next_val;
		// determine which operators to use
		if(bitshift_len > 0)
		{
			if(strcmp(bitshift, "<<") == 0)
				fi <<= next_val_i;
			else if(strcmp(bitshift, ">>") == 0)
				fi >>= next_val_i;
		}
		else
		{
			if(op == '|')
				fi |= next_val_i;
			else if(op == '&')
				fi &= next_val_i;
			else if(op == '^')
				fi ^= next_val_i;
		}

		f = fi;
		*out_success = 1;
	}

	return f;
}
/*
   the first function call for an expression:

   order is lowest precedence to highest

   expression,
   bitwise,
   term,
   factor
*/
static float parse_expression(const char *origin, const char **str, int *out_success)
{
	int s = 0;
	// tries to parse a bitwise operation which is made up of terms
	float f = parse_bitwise(origin, str, &s);
	*out_success = s;
	if(s == 0)
		return 0.0f;
	// after parsing the first term, check for lowest precedence operators like + and -
	while(peek_next_char(str) == '+' || peek_next_char(str) == '-')
	{
		char op = get_next_char(str);

		// parse second term and calculate result
		float next_val = parse_bitwise(origin, str, &s);
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
	// skip any string chars, '{' and whitespace
	if(snaky_is_string(*str))
	{
		// skip the '{'
		*str += 1;
		skip_whitespace(str, NULL);
	}

	// find expression wrapped around '()'
	char c = peek_next_char(str);
	if(c == EXPR_OPENING)
	{
		char next = get_next_char(str);
		int s = 0;
		float f = parse_expression(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;
		if(peek_next_char(str) == EXPR_CLOSING)
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
		char attrib_name[SNAKY_BUF_SIZE];
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
			if(**str == ARR_OPENING)
				checking_only_alpha = false;

			// re-enable alpha-only rule
			if(**str == ARR_CLOSING)
				checking_only_alpha = true;

			if(checking_only_alpha && !isalpha(**str))
				break;

			attrib_name[i++] = *(*str)++;
		}

		attrib_name[i] = '\0';

		// see what the value is:

		// try to parse as a target attribute:
		snaky_attrib_info info = {0};
		char attrib[SNAKY_BUF_SIZE];
		if(snaky_parse_target_attrib_buf(origin, attrib_name, attrib, sizeof(attrib), &info))
		{
			// if the attrib is an invalid type, no value can be parsed
			if(info.data_type == SNAKY_ARRAY)
			{
				vl_log(VL_ERROR, "The parsed attribute '%s' is an array, cannot parse a value!\n", attrib_name);
				*out_success = 0;
				return 0.0f;
			}
			else if(info.data_type == SNAKY_STRING)
			{
				vl_log(VL_ERROR, "The parsed attribute '%s' is a string, cannot parse a value!\n", attrib_name);
				*out_success = 0;
				return 0.0f;
			}
			else if(info.data_type == SNAKY_ATTRIB_STRING)
			{
				vl_log(VL_ERROR, "The parsed attribute '%s' is an attribute string, cannot parse a value!\n", attrib_name);
				*out_success = 0;
				return 0.0f;
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
				if(peek_next_char(str) == EXPR_OPENING)
				{
					char next = get_next_char(str);
					int s = 0;
					float f = parse_expression(origin, str, &s);
					*out_success = s;
					if(s == 0)
						return 0.0f;
					if(peek_next_char(str) == EXPR_CLOSING)
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
				if(peek_next_char(str) == EXPR_OPENING)
				{
					const char *start_paren = *str;

					char next = get_next_char(str);

					// get the string being parsed here:
					char fn_args[SNAKY_MAX_ARGS][SNAKY_BUF_SIZE] = {0};
					size_t num_args = 0;

					char fn_arg[SNAKY_BUF_SIZE] = {0};
					size_t j = 0;

					// see where the matching ')' is
					const char *end_paren = find_end_of_group(start_paren, EXPR_OPENING, EXPR_CLOSING);
					if(!end_paren)
					{
						vl_log(VL_ERROR, "Expected matching ')' in string: '%s'!\n", start_paren);
						*out_success = 0;
						return 0.0f;
					}

					// move str to the next ')', while adding to fn_arg buffer
					while(**str && *str != end_paren)
					{
						// get next char but do not move pointer yet
						char next = peek_next_char(str);

						// when an argument is going to end, finish building the current arg string and add it to the total array:
						if(next == ATTRIB_DELIM || next == EXPR_CLOSING)
						{
							if(num_args >= SNAKY_MAX_ARGS)
							{
								vl_log(VL_WARNING, "Max number of arguments reached for function call: '%s', number of arguments: %zu!\n", functions.keys[i], num_args);
								break;
							}

							// when the next char is a ',' then a new argument should be added:
							fn_arg[j] = '\0';
							strcpy(fn_args[num_args], fn_arg);

							// reset char counter for fn_arg buffer
							j = 0;

							// add to number of arguments
							num_args++;

							// if the char is a ',' go to next char and continue loop
							if(next == ATTRIB_DELIM)
							{
								(*str)++;
								continue;
							}
							// but if the next char is a ')' break loop
							if(next == EXPR_CLOSING)
								break;
						}

						// otherwise, add contents to current fn arg buffer (move pointer forward to next char)
						fn_arg[j++] = *(*str)++;
					}

					if(peek_next_char(str) == EXPR_CLOSING)
					{
						get_next_char(str);
						*out_success = 1;

						// parse func data
						function func_data = functions.values[i];

						// make sure args match:
						if(num_args != func_data.num_args)
						{
							vl_log(VL_ERROR, "The function '%s' expects %zu arguments but received %zu!\n", functions.keys[i], func_data.num_args, num_args);
							*out_success = 0;
							return 0.0f;
						}

						// convert fn_args to a const char* array
						const char *fn_args_ptr[num_args];

						// copy pointers over to new array
						for(size_t i = 0; i < num_args; ++i)
							fn_args_ptr[i] = fn_args[i];

						// now create a duplicate array holding floats instead of strings
						float floats[num_args];
						for(size_t i = 0; i < num_args; ++i)
						{
							int s = 0;
							floats[i] = parse_expression(origin, &fn_args_ptr[i], &s);
							if(s == 0)
							{
								vl_log(VL_ERROR, "Failed to parse value in string: '%s'!\n", fn_args_ptr[i]);
								*out_success = 0;
								return 0.0f;
							}
						}

						switch(func_data.return_type)
						{
							case SNAKY_CHAR:
							{
								char (*fn_ptr) (float*, size_t) = func_data.fn_ptr;
								return fn_ptr(floats, num_args);
							}
							case SNAKY_BOOL:
							{
								bool (*fn_ptr) (float*, size_t) = func_data.fn_ptr;
								return fn_ptr(floats, num_args);
							}
							case SNAKY_INT:
							{
								int (*fn_ptr) (float*, size_t) = func_data.fn_ptr;
								return fn_ptr(floats, num_args);
							}
							case SNAKY_FLOAT:
							{
								float (*fn_ptr) (float*, size_t) = func_data.fn_ptr;
								return fn_ptr(floats, num_args);
							}
							case SNAKY_DOUBLE:
							{
								double (*fn_ptr) (float*, size_t) = func_data.fn_ptr;
								return fn_ptr(floats, num_args);
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
static float parse_term(const char *origin, const char **str, int *out_success)
{
	int s = 0;
	// tries to parse a single factor
	float f = parse_factor(origin, str, &s);
	*out_success = s;
	if(s == 0)
		return 0.0f;

	while(peek_next_char(str) == '*' || peek_next_char(str) == '/' || peek_next_char(str) == '%')
	{
		char op = get_next_char(str);

		// parse the second factor
		float next_val = parse_factor(origin, str, &s);
		*out_success = s;
		if(s == 0)
			return 0.0f;
		if(op == '*')
			f *= next_val;
		else if(op == '/')
		{
			if(next_val == 0)
			{
				vl_log(VL_WARNING, "Dividing by 0 is undefined. Returning 0.0f.\n");
				return 0.0f;
			}
			f /= next_val;
		}
		else if(op == '%')
		{
			int fi = (int) f;
			fi %= (int) next_val;
			f = fi;
		}

		*out_success = 1;
	}

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
void snaky_parse_target_attrib_value(const char *str, const char *attrib_name, snaky_data_type target_type, void *out_value, snaky_attrib_info *out_info, int *out_success)
{
	if(!out_success)
	{
		vl_log(VL_ERROR, "snaky_parse_value(...) requires 'out_success' to be a valid pointer!\n");
		return;
	}
	if(!out_info)
	{
		vl_log(VL_ERROR, "snaky_parse_value(...) requires 'out_info' to be a valid pointer!\n");
		return;
	}

	// make sure it is 0 by default
	*out_success = 0;

	snaky_string attrib = {0};
	if(!snaky_prepare_str(&attrib, SNAKY_BUF_SIZE))
		return;

	if(snaky_parse_target_attrib_str(str, attrib_name, &attrib, out_info))
		snaky_parse_value(str, attrib.data, out_info->data_type, out_value, out_success);

	snaky_free_str(&attrib);

	if(*out_success == 0)
		vl_log(VL_ERROR, "Failed to parse target attrib value: string: '%s', attribute name: '%s'!\n", str, attrib_name);
}
void snaky_parse_target_attrib_value_buf(const char *str, const char *attrib_name, snaky_data_type target_type, void *out_value, char *buffer, size_t buffer_size, snaky_attrib_info *out_info, int *out_success)
{
	if(!out_info)
	{
		vl_log(VL_ERROR, "snaky_parse_value(...) requires 'out_info' to be a valid pointer!\n");
		return;
	}
	if(!out_success)
	{
		vl_log(VL_ERROR, "snaky_parse_value(...) requires 'out_success' to be a valid pointer!\n");
		return;
	}

	// make sure it is 0 by default
	*out_success = 0;

	if(snaky_parse_target_attrib_buf(str, attrib_name, buffer, buffer_size, out_info))
		snaky_parse_value(str, buffer, out_info->data_type, out_value, out_success);

	if(*out_success == 0)
		vl_log(VL_ERROR, "Failed to parse target attrib value: string: '%s', attribute name: '%s'!\n", str, attrib_name);
}
void snaky_parse_target_attrib_value_str(const char *str, const char *attrib_name, snaky_data_type target_type, void *out_value, snaky_string *buffer, snaky_attrib_info *out_info, int *out_success)
{
	if(!buffer || !buffer->data || buffer->len == 0 || buffer->size == 0)
	{
		vl_log(VL_ERROR, "Unable to parse target attribute value: invalid arguments given!\n");
		return;
	}

	if(!out_info)
	{
		vl_log(VL_ERROR, "snaky_parse_value(...) requires 'out_info' to be a valid pointer!\n");
		return;
	}

	if(!out_success)
	{
		vl_log(VL_ERROR, "snaky_parse_value(...) requires 'out_success' to be a valid pointer!\n");
		return;
	}

	// make sure it is 0 by default
	*out_success = 0;

	if(snaky_parse_target_attrib_str(str, attrib_name, buffer, out_info))
		snaky_parse_value(str, buffer->data, out_info->data_type, out_value, out_success);

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
		case SNAKY_STRING:
			snprintf(buffer, buffer_size, "string");
			break;
		case SNAKY_ATTRIB_STRING:
			snprintf(buffer, buffer_size, "attribs");
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

	if(mode == SNAKY_FWRITE_TEXT)
	{
		char read[SNAKY_MAX_LINE_LEN];
		size_t total_size = 0;
		while(fgets(read, sizeof(read), f))
		{
			total_size += strlen(read);

			if(total_size + 1 >= buffer_size)
				break;

			// append contents to user's buffer
			strcat(buffer, read);
		}
	}
	else if(mode == SNAKY_FWRITE_BINARY)
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

	if(mode == SNAKY_FWRITE_TEXT)
	{
		char read[SNAKY_MAX_LINE_LEN];
		size_t total_size = 0;
		while(fgets(read, sizeof(read), f))
		{
			total_size += strlen(read);

			// use strlen(read) * 2 to decrease number of reallocations
			if(total_size + 1 >= str->size && !snaky_realloc_str(str, str->size + (strlen(read) * 2)))
				return 0;

			// append contents to string
			snaky_strcat(str, read);
		}
	}
	else if(mode == SNAKY_FWRITE_BINARY)
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
	if(mode == SNAKY_FWRITE_TEXT)
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
	else if(mode == SNAKY_FWRITE_BINARY)
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
	if(mode == SNAKY_FWRITE_TEXT)
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
	else if(mode == SNAKY_FWRITE_BINARY)
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

	if(mode == SNAKY_FWRITE_TEXT)
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
	else if(mode == SNAKY_FWRITE_BINARY)
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
int snaky_define_function(const char *str, snaky_data_type return_type, void *func, size_t num_args)
{
	if(!func)
	{
		vl_log(VL_ERROR, "Cannot define a NULL function!\n");
		return 0;
	}

	function func_data = {0};
	func_data.fn_ptr = func;
	func_data.num_args = num_args;
	func_data.return_type = return_type;

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
