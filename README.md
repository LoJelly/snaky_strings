# snaky_strings

**snaky_strings** is a simple C library used to create attribute lists that describe objects or collections of data.

An example of a snaky_string looks like this:

```
<x = 20, y = 50>
<z = y - x>

/[ This is a Label! ]/
<key = value>

/* functions like cos and rads can be used! */
<degs = 60, angle = cos(rads(degs))>

/* arrays can be created like so: */
<arr = [2, 4, 6]>

/* objects can be created like so: */
<obj = {< x-pos = 30, y-pos = 45, color = white >}>

/* those values above can also be referenced like so: */
<value1 = arr[1], value2 = obj.x-pos>
```

## Features

- Creating attribute strings holding key=value pairs
- Using and defining custom mathematical expressions, functions, and constants
- Functions for parsing certain types (bool, int, float, etc.)
- Functions for moving within a file or string
- Functions for bundling up snaky_strings into a map or list
- Dynamically and directly modifying attribute strings
- Reading and writing to files using differing modes (text, binary, encrypt)
- Using arrays and objects to hold collections of data
- Creating comments and labels within a file holding snaky_string data

> [!NOTE]
> snaky_strings is not thread-safe.

## Documentation and Information

Please refer to the snaky_strings documentation <a href="docs/html">here</a>.
