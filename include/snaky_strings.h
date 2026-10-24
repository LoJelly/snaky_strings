#pragma once

#ifdef _WIN32
	#ifdef SNAKY_DLL
		#ifdef SNAKY_EXPORTS
			#define SNAKY_API __declspec(dllexport)
		#else
			#define SNAKY_API __declspec(dllimport)
		#endif
	#else
		#define SNAKY_API
	#endif
#else
	#define SNAKY_API
#endif

#include <stdio.h>
#include <stddef.h>
#include <string.h>

/**
  Define SNAKY_CUSTOM_BUF_SIZE to change the default
  buffer size in snaky_strings.

  The default buffer size is 64.
*/
#ifdef SNAKY_CUSTOM_BUF_SIZE
	#define SNAKY_BUF_SIZE SNAKY_CUSTOM_BUF_SIZE
#else
	/**
	  The default size of char buffers in snaky_strings.

	  To override this value, define SNAKY_CUSTOM_BUF_SIZE
	  and set it equal to your custom buffer size.
	*/
	#define SNAKY_BUF_SIZE 64
#endif

/**
  Define SNAKY_CUSTOM_MAX_LINE_LEN to change the
  default maximum line length of snaky_strings files.
*/
#ifdef SNAKY_CUSTOM_MAX_LINE_LEN
	#define SNAKY_MAX_LINE_LEN SNAKY_CUSTOM_MAX_LINE_LEN
#else
	/**
	  The default maximum line length in a snaky_strings file.
	*/
	#define SNAKY_MAX_LINE_LEN 256
#endif

/**
  Represents any kind of invalid value within snaky_strings.
*/
#define SNAKY_INVALID_VALUE -1

/**
  Contains string data.

  snaky_strings are primarily
  used for automatic-memory-allocation
  functions within snaky_strings.
  Automatic-memory-allocation functions
  use snaky_strings to automatically
  resize char arrays when necessary.
  These functions are suffixed with 'str.'
*/
typedef struct snaky_string
{
	/**
	  The char array.
	*/
	char *data;
	/**
	  The current amount of bytes
	  allocated for the string.
	*/
	size_t size;
	/**
	  The actual length of the string.
	*/
	size_t len;
} snaky_string;

/**
  Represents a map with attribute names as the keys
  and attribute values as the values.
*/
typedef struct snaky_attrib_map
{
	/**
	  The keys of the map are the attribute names in the string.
	*/
	char **keys;
	/**
	  The values of the map are the attribute values in the string.
	*/
	char **values;
	/**
	  The current size of the map.
	*/
	size_t size;
	/**
	  The capacity of the map.
	*/
	size_t capacity;
	/**
	  Whether or not any allocations failed for this map.
	*/
	bool alloc_failure;
} snaky_attrib_map;

/**
  The different data types in snaky_strings.

  @note snaky_strings does not use pointers or strings
  as data types. This means that custom expression
  functions should always return a numerical, character,
  or boolean result.
*/
typedef enum snaky_data_type
{
	/**
	  Represents the character type.
	*/
	SNAKY_CHAR,
	/**
	  Represents the boolean type.
	*/
	SNAKY_BOOL,
	/**
	  Represents the integer type.
	*/
	SNAKY_INT,
	/**
	  Represents the float type.
	*/
	SNAKY_FLOAT,
	/**
	  Represents the double type.
	*/
	SNAKY_DOUBLE,
	/**
	  Represents the array type.
	*/
	SNAKY_ARRAY,
	/**
	  Represents the object type.
	*/
	SNAKY_OBJECT,
	/**
	  Represents the string type.

	  @important When parsing attributes,
	  this becomes the default data type.
	*/
	SNAKY_STRING
} snaky_data_type;

/**
  Represents a single attribute entry
  within an attribute entry list.

  @see snaky_attrib_list
*/
typedef struct snaky_attrib_entry
{
	/**
	  The name of the attribute.
	*/
	snaky_string name;
	/**
	  The value of the attribute.
	*/
	snaky_string value;
	/**
	  The data type of the attribute in string form.
	*/
	char data_type_str[7];
	/**
	  The data type of the attribute.
	*/
	snaky_data_type data_type;
} snaky_attrib_entry;

/**
  Represents a list of attributes
  collected from an attribute string.

  @note Similar to snaky_attrib_maps,
  snaky_attrib_lists contain info
  about the attributes in a string. However,
  entry lists do not contain key:value pairs,
  and instead all the entries collected
  are stored in a sequential order. They also
  contain more information, such as the data
  type of the attribute.
*/
typedef struct snaky_attrib_list
{
	/**
	  The list contains attribute entries.
	*/
	snaky_attrib_entry *data;
	/**
	  The size of the list.
	*/
	size_t size;
	/**
	  The capacity of the list.
	*/
	size_t capacity;
	/**
	  Whether or not any allocations failed for this list.
	*/
	bool alloc_failure;
} snaky_attrib_list;

/**
  Determines how data should be written to a file.
*/
typedef enum snaky_fwrite_mode
{
	/**
	  The default write mode.

	  This results in all data being
	  written in binary format.
	*/
	SNAKY_FWRITE_BINARY,
	/**
	  Results in plain text being
	  written to the file.

	  @important When using snaky_write_file_data(...)
	  with SNAKY_FWRITE_TEXT,
	  each entry in the map is
	  written in its own attribute
	  string. For example,
	  if the map contains 2 entries,
	  they are written as 2 individual
	  attribute strings even if they
	  may be in the same string originally.
	  This is because snaky_attrib_maps
	  only know about the entries, and nothing
	  about the starting and ending positions
	  in attribute strings. If you want to
	  preserve the actual attribute strings,
	  use snaky_write_file_str(...) instead.
	*/
	SNAKY_FWRITE_TEXT,
	/**
	  Results in non-string values
	  being written in binary format,
	  and strings will be encrypted.
	*/
	SNAKY_FWRITE_ENCRYPT
} snaky_fwrite_mode;

/**
  Initializes the snaky_strings library.

  @important Make sure you call snaky_shutdown()
  later!

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_init(void);
/**
  Determines if the snaky_strings library has been initialized.
*/
SNAKY_API bool snaky_is_init(void);
/**
  Shuts down and frees any memory allocated
  by snaky_strings.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_shutdown(void);

/**
  Creates and initializes a snaky_string
  using a string literal or char buffer.

  @important This function duplicates
  the 'data' array given and uses
  that for the snaky_string's 'data'
  field. Because of this, you must
  free the string later with
  snaky_free_str(...).

  @return 1 on success, 0 on failure.

  @see snaky_free_str(snaky_string*)
*/
SNAKY_API int snaky_create_str(snaky_string *str, char *data);
/**
  Creates and initializes a snaky_string
  using a specified amount of bytes to allocate.

  @important You must free the string with snaky_free_str(...)
  later!

  @note Use snaky_strcpy(...) to populate the char array on the
  snaky_string!

  @return 1 on success, 0 on failure.

  @see snaky_free_str(snaky_string*)
  @see snaky_strcpy(snaky_string*, const char*)
*/
SNAKY_API int snaky_prepare_str(snaky_string *str, size_t bytes);
/**
  Frees all memory associated with the given snaky_string.
*/
SNAKY_API void snaky_free_str(snaky_string *str);
/**
  Copies a char array's data into a snaky_string's 'data' field.

  @note This function automatically allocates more memory
  if it is necessary. You will still have to use snaky_free_str(...)
  later on the snaky_string.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_strcpy(snaky_string *dest, const char *src);
/**
  Reallocates memory for a snaky_string based on a new size in bytes.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_realloc_str(snaky_string *str, size_t bytes);

/**
  Searches the given string for a specific attribute and tries to parse its value.

  Examples:

  The attribute string is: '<id=obj4,x=20,y=300>'

  The attribute list will consist of 'id,' 'x,' and 'y.'

  The value list will consist of 'obj4,' '20,' and '300.'

  Attributes must be separated by the ',' character,
  and they must be assigned using the '=' character.

  @important All attribute strings should be char arrays,
  unless you do not plan on modifying the string, then it
  can be a const char*.

  @note Whitespace is allowed in attribute strings. These
  two strings are equal when parsed: '<id=obj4,x=20,y=300>'
  and '< id = obj4, x = 20, y = 300 >'

  When attempting to place a nested attribute string inside
  of another attribute string, the nested attribute string
  MUST be surrounded by '\"' characters. So the proper way
  to write it would look like this:

  '<attrib_string="<...>">'

  Also, any attributes surrounded by the '\"' character are
  treated as string literals and are copied exactly as they are typed.
  If any strings in the attribute list contain nested attribute
  lists, they are not parsed. Strings are skipped during the
  parsing stage. There is an exception to this rule however:
  if you are specifically attempting to find a nested attribute,
  you can chain together the outer names to piece the path together.
  For example, in this string:

  '<player="<name=PLAYER1>">'

  The 'name' attribute can be accessed using 'player.name' as the
  target name. Note that this only works for nested attribute strings.
  Additionally, if multiple strings are nested, the names can
  be chained, like this 'v1.v2.v3...'

  @param str The string to search.
  @param buffer Where to place the value of the found attribute.
  If this is NULL, then the function will only return
  whether or not the attribute was found.
  @param buffer_size The size of 'buffer' in bytes.
  @param attrib_name The name of the attribute to search for.
  @param out_start_pos A pointer to a const char*. If a valid pointer
  is given, it will be equal to the position in the original
  string where the next attribute starts. Specifically, it will point
  to the first character in the next attribute name found. If no next
  attribute is found, then the pointer will be equal to NULL.
  @param out_data_type A pointer to a snaky_data_type. If a valid pointer
  is given, it will be equal to the data type of the parsed attribute.

  @return 1 if the attribute was successfully parsed/found, 0 if the
  function fails in any way.
*/
SNAKY_API int snaky_parse_target_attrib_buf(const char *str, char *buffer, size_t buffer_size, const char *attrib_name, const char **out_start_pos, snaky_data_type *out_data_type);
/**
  Same as snaky_parse_target_attrib(...) but uses a snaky_string instead
  of a char buffer.

  @return 1 on success, 0 on failure.

  @see snaky_parse_target_attrib(const char*, char*, size_t, const char*, const char**, snaky_data_type*)
*/
SNAKY_API int snaky_parse_target_attrib_str(const char *str, snaky_string *buffer, const char *attrib_name, const char **out_start_pos, snaky_data_type *out_data_type);
/**
  Searches the given string for the very next attribute and tries to parse its value.

  @note Because this function parses the next attribute found, two buffers are required;
  one for the name of the attribute parsed, as well as one for its value.

  To automatically walk a string and its attributes, pass a pointer into 'out_start_pos'
  and then on the next call use 'out_start_pos' as the 'str' attribute string.

  @important When data types are parsed within a string, it can go 2 routes. The first route
  is the explicit route: this means the user directly placed a ':DATA_TYPE' part into
  the name of the attribute. The second route is the implicit route. The function will try
  to determine the type of the value. Because of this, it will never assume the SNAKY_DOUBLE
  or SNAKY_CHAR types. It can only ever assume SNAKY_BOOL, SNAKY_INT, or SNAKY_FLOAT.

  @see snaky_parse_target_attrib(const char*, char*, size_t, const char*, const char**, snaky_data_type*)
*/
SNAKY_API int snaky_parse_attrib_buf(const char *str, char *name_buffer, size_t name_buffer_size, char *value_buffer, size_t value_buffer_size, const char **out_start_pos, snaky_data_type *out_data_type);
/**
  Same as snaky_parse_attrib(...) but uses snaky_strings for the name and value
  char buffers.

  @see snaky_parse_attrib(const char*, char*, size_t, char*, size_t, const char**, snaky_data_type*)
*/
SNAKY_API int snaky_parse_attrib_str(const char *str, snaky_string *name_str, snaky_string *value_str, const char **out_start_pos, snaky_data_type *out_data_type);

/**
  Searches the given string for a specific attribute and removes it entirely.

  @important All attribute strings should be char arrays.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_remove_attrib_buf(char *str, const char *attrib_name);
/**
  Same as snaky_remove_attrib(...) but uses a snaky_string instead.

  @see snaky_remove_attrib(char*, const char*)
*/
SNAKY_API int snaky_remove_attrib_str(snaky_string *str, const char *attrib_name);

/**
  Modifies an attribute value directly in a string.

  If the attribute is not found, it is automatically
  appended to the buffer, if possible.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_set_attrib_buf(char *buf, size_t buffer_size, const char *attrib_name, const char *new_attrib_value);
/**
  Same as snaky_set_attrib(...) but uses a snaky_string instead.

  If the attribute is not found, it is automatically
  appended to the string, if possible.

  @see snaky_set_attrib(char*, size_t, const char*, const char*)
*/
SNAKY_API int snaky_set_attrib_str(snaky_string *str, const char *attrib_name, const char *new_attrib_value);
/**
  Modifies multiple attribute values directly in a string.

  To set multiple attributes, the 'attribs' string must be formatted
  like this:

  "<attrib_name=new_attrib_value,attrib_name2=new_attrib_value,...>"

  For example, using this attribute string:

  "<x=100,y=200>"

  To set both the 'x' and 'y' attributes at the same time, the function
  would be called like this:

  'snaky_set_attribs(str, sizeof(str), "<x=300,y=150>");'

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_set_attribs_buf(char *str, size_t buffer_size, const char *attribs);
/**
  Same as snaky_set_attribs(...) but uses a snaky_string instead.

  @return 1 on success, 0 on failure.

  @see snaky_set_attribs(char*, size_t, const char*)
*/
SNAKY_API int snaky_set_attribs_str(snaky_string *str, const char *attribs);

/**
  Counts the number of attributes within an attribute string.
*/
SNAKY_API size_t snaky_count_attribs(const char *str);

/**
  Determines if an attribute list is an object/group.
*/
SNAKY_API bool snaky_is_object(const char *str);

/**
  Finds the next instance of an attribute within
  a string and returns a pointer to the start of
  the attribute name in the original string.

  If 'out_attrib_pos' is NULL, the function will not
  do anything. Otherwise, it will be equal to
  the pointer to the first character in the
  matching attribute name in the original string,
  or NULL if the attribute is not found.
*/
SNAKY_API void snaky_goto_attrib(const char *str, const char *attrib_name, const char **out_attrib_pos);
/**
  Finds the given label within an attribute string
  and returns the position of it.

  Labels are used to organize and navigate large
  files more easily.

  Labels are expected to be on a single line, with no
  characters before or after the label's name.
  A label is created like this:

  "/[ LABEL NAME ]/"

  A full example of a label looks like this:

  "
  /[ Generic Options ]/
  <x = y>
  "

  @note For the 'label_name' parameter, you do not have to include
  the "/[" or "]/" characters, just the name of the label. In the
  example above, the label name string would just be "Generic Options."

  When using this function to go to the 'Generic Options' label,
  the returned character is the '<.'

  If 'out_pos' is NULL, the function will not do anything.
  Othwerise, it will be equal to the pointer to the
  first character after the label,
  or NULL if the label was not found.
*/
SNAKY_API void snaky_goto_label(const char *str, const char *label_name, const char **out_pos);

/**
  Obtains all the data within an attribute string.

  @note This function will not automatically initialize
  the map for you.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_generate_attrib_map(const char *str, snaky_attrib_map *map);
/**
  Obtains all the data within an attribute string,
  and generates a list of the entries.

  @note This function will not automatically initialize
  the list for you.

  @important When you are done with the attribute list later, use
  snaky_destroy_attrib_list(...).

  @return 1 on success, 0 on failure.

  @see snaky_destroy_attrib_list(snaky_attrib_list*)
*/
SNAKY_API int snaky_generate_attrib_list(const char *str, snaky_attrib_list *list);
/**
  Destroys and frees any memory associated with the given
  attribute list.

  @see snaky_generate_attrib_list(const char*, snaky_attrib_list*)
  @see snaky_attrib_list
*/
SNAKY_API int snaky_destroy_attrib_list(snaky_attrib_list *list);

/**
  Reads a character attribute value and obtains the actual
  char equivalent of it.

  @param str The string holding the character attribute value. This is
  not an attribute string.
  @param out_success A pointer to an int that indicates whether or
  not the function succeeded. It will be equal to 1 if it succeeded,
  and 0 if it failed.
*/
SNAKY_API char snaky_parse_char(const char *str, int *out_success);
/**
  Reads a boolean attribute value and obtains
  the actual bool equivalent of it.

  @note The "OPPOSITE" attribute value is not accepted
  here.

  @note Valid boolean attribute include: 'TRUE,' 'ON,' 'FALSE,' and 'OFF.'

  @param str The string holding the boolean attribute value. This is not
  an attribute string.
  @param out_success A pointer to an int that indicates whether
  or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API bool snaky_parse_bool(const char *str, int *out_success);
/**
  Reads an integer attribute value and obtains the
  actual int equivalent of it.

  @param origin The start of the whole attribute string.
  @param str The string holding the integer attribute value. This is not
  an attribute string.
  @param out_success A pointer to an int that indicates
  whether or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API int snaky_parse_int(const char *origin, const char *str, int *out_success);
/**
  Reads a float attribute value and obtains the
  actual float equivalent of it.

  @param origin The start of the whole attribute string.
  @param str The string holding the float attribute value. This is not
  an attribute string.
  @param out_success A pointer to an int that indicates whether
  or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API float snaky_parse_float(const char *origin, const char *str, int *out_success);
/**
  Reads a double attribute value and obtains the
  actual double equivalent of it.

  @param origin The start of the whole attribute string.
  @param str The string holding the double attribute value. This is not
  an attribute string.
  @param out_success A pointer to an int that indicates whether
  or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.
*/
SNAKY_API double snaky_parse_double(const char *origin, const char *str, int *out_success);
/**
  Similarly to other snaky_parse_[type] functions, this function
  parses a generic value based on a target data type. More specifically,
  this function is intended for numerical expressions. The
  SNAKY_OBJECT, SNAKY_ARRAY, and SNAKY_STRING data types
  are not supported in this function. For these 3 types, you
  should use the snaky_parse_target_attrib...(...) functions instead.

  @important Because this function is generic and
  needs to be able to determine if parsing failed or not,
  the 'out_success' pointer cannot be NULL.

  @param origin The start of the whole attribute string. This used
  for resolving attribute names in expressions throughout the string.
  @param str The string holding the generic value. This is not
  an attribute string.
  @param target_type The data type to try to parse
  the value as. For example, using SNAKY_INT
  indicates the value should be parsed as an integer.
  @param out_value A pointer to the actual variable
  that will hold the final parsed result.
  @param out_success A pointer to an int that indicates whether
  or not the function succeeded. It will be equal to 1 if it
  succeeded, and 0 if it failed.

  @see snaky_parse_target_attrib_value(const char*, const char*, snaky_data_type, void*, const char**, int*)
*/
SNAKY_API void snaky_parse_value(const char *origin, const char *str, snaky_data_type target_type, void *out_value, int *out_success);
/**
  Parses a generic value just like snaky_parse_value(...) but obtains
  the value from an attribute from an attribute string.

  @note The SNAKY_AUTO data type works in this function.

  @see snaky_parse_value(const char*, snaky_data_type, void*, int*)
  @see snaky_parse_target_attrib(const char*, char*, size_t, const char*, const char**)
*/
SNAKY_API void snaky_parse_target_attrib_value(const char *str, const char *attrib_name, snaky_data_type target_type, void *out_value, const char **out_start_pos, int *out_success);
/**
  Parses a data type into a string.

  For example, if SNAKY_CHAR is used,
  the buffer given will be equal to
  "char." This also means the maximum
  required buffer size is only 7, since
  "double" is the longest possible data
  type parsed.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_parse_data_type(snaky_data_type data_type, char *buffer, size_t buffer_size);

/**
  Determines if a file exists.
*/
SNAKY_API bool snaky_file_exists(const char *file_path);
/**
  Obtains the size of a file in bytes.
*/
SNAKY_API long snaky_get_file_size(const char *file_path);
/**
  Opens a file for reading attribute lists from.

  @param file_path The file path.
  @param buffer Where the file's contents should be placed.
  @param buffer_size The size of 'buffer' in bytes.
  @param mode The file write mode to use.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_read_file_buf(const char *file_path, char *buffer, size_t buffer_size, snaky_fwrite_mode mode);
/**
  Same as snaky_read_file(...)
  but uses a snaky_string instead.

  @see snaky_read_file(const char*, char*, size_t, snaky_fwrite_mode)
*/
SNAKY_API int snaky_read_file_str(const char *file_path, snaky_string *str, snaky_fwrite_mode mode);
/**
  Reads a file's contents and inserts it all into
  a snaky_attrib_map.

  @note This function will not automatically initialize
  the map for you.

  @return 1 on success, 0 on failure.

  @see snaky_attrib_maps
*/
SNAKY_API int snaky_read_file_map(const char *file_path, snaky_attrib_map *map, snaky_fwrite_mode mode);
/**
  Opens a file and writes an attribute list to it.

  @note If using SNAKY_FWRITE_TEXT, the entries in the map
  are automatically written to the file in their own
  attribute strings.

  @param file_path The file path.
  @param data The attribute data to write.

  @return 1 on success, 0 on failure.

  @see SNAKY_FWRITE_TEXT
*/
SNAKY_API int snaky_write_file_map(const char *file_path, snaky_attrib_map *map, snaky_fwrite_mode mode);
/**
  Opens a file and writes attribute strings to it.

  @note If using SNAKY_FWRITE_BINARY or SNAKY_FWRITE_ENCRYPT
  the function behaves the same as snaky_write_file_data(...).
  However, SNAKY_FWRITE_TEXT behaves differently, as it will
  write the string exactly as it is presented.

  @param file_path The file path.
  @param buffer The attribute string to write.
  @param mode The file write mode to use.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_write_file_buf(const char *file_path, const char *buffer, snaky_fwrite_mode mode);
/**
  Obtains the next line in a buffer that contains the lines from
  a read file.

  @param cursor A pointer to the string containing ALL of the file's lines.
  This is called the 'cursor' because as the file is read, this pointer
  is moved forward. Because of this, if you are using malloc() to create
  the char*, do not use that pointer here. Instead create another pointer and
  use that. Moving the malloc()-ed pointer forward will lead to undefined
  behavior.
  @param buffer Where the next line should be placed.
  @param buffer_size The size of 'buffer' in bytes.

  @return 1 on success, 0 on failure.

  @see snaky_read_file(const char*, char*, size_t)
*/
SNAKY_API int snaky_get_next_line(char **cursor, char *buffer, size_t buffer_size);

/**
  Single-argument functions that are evaluated in attribute
  strings.

  They should take in a single float value, and return a
  single float value.
*/
typedef float (*snaky_eval_func) (float);

/**
  Registers a constant that can be used in attribute strings later.

  Constants are single string values, such as 'PI' and should
  be equal to a single value.

  @note It is recommended to make all constants uppercase
  to keep them clearly separated from other values or functions.

  @param str The constant to define.
  @param value The value of the constant.

  @return 1 on success, 0 on failure.
*/
SNAKY_API int snaky_define_constant(const char *str, float value);
/**
  Registers an evaluation function that can be used in mathematical
  expressions in attribute strings later.

  Evaluation functions are single string values, such as 'sin' or 'round' and should
  take in a single value, and return a single result.

  When using functions in attribute strings, you must use the
  function name, then an opening '(' followed by the value, and then a closing
  ').'

  @param str The name of the function that will be used in the attribute string.
  @param func A pointer to the function to use.

  @return 1 on success, 0 on failure.

  @see snaky_eval_func
*/
SNAKY_API int snaky_define_eval_function(const char *str, snaky_eval_func func);
/**
  Registers a normal function that can be used in attribute strings later.

  A normal function can be customized more than an evaluation
  function. The return type, as well as a generic pointer to a list
  or structure of arguments is required.

  @note The internal function should take in a const char*, which will be equal
  to the string value of the attribute parsed inside of the function call.

  @important A mismatch in return type or argument type may result
  in undefined behavior.

  @param str The name of the function that will be used in the attribute string.
  @param return_type The data type that is returned by the function.
  @param fn_ptr A pointer to the internal function.

  @see snaky_define_eval_function(const char*, snaky_eval_func)
*/
SNAKY_API int snaky_define_function(const char *str, snaky_data_type return_type, void *fn_ptr);

/**
  Sets the key used when encrypting strings.
*/
SNAKY_API void snaky_set_encryption_key(int k);
