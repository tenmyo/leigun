# Leigun Coding Guidelines

Based on [LLVM Coding Standards](http://llvm.org/docs/CodingStandards.html).
And overwritten with the below.

## Naming
The LLVM are written by C++. But The Leigun are written by C, which has no classes and namespaces.
Therefore, The module name to prefix, Types to suffix. And case letters by scope.

### Case letters
Constants MUST BE "UPPER_CASE" (e.g. LIB_DEFAULT_CONF).
Global scope MUST be "UpperCamelCase" (e.g. Lib_Init()).
File(static) scope MUST be "lowerCamelCase" (e.g. Lib_onExit()).
Local(in function) scope MUST be "snake_case" (e.g. dir_path).


### Module
Module name MUST be nouns.
Module name MUST be "UpperCamelCase" (e.g. TextFileReader).

### File
MUST be The Module name to "lowercase".
If split C file, MUST be The Module name and "-subname".

For example:
- textfilereader.h
- time-utc.h

### Type
The Types to MUST be suffix.
- struct(_s): struct Time_TimeZone_s
- union(_u): union IO_AnyHandle_u
- enum(_e): enum Log_Level_e
- typedef without function pointer(_t): typedef struct Time_TimeZone_s Time_TimeZone_t
- typedef of function pointer for callback)(_cb): typedef void (*Time_TimerHadler_cb)(void *eventData)
