# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/IDGcode_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/IDGcode_autogen.dir/ParseCache.txt"
  "IDGcode_autogen"
  )
endif()
