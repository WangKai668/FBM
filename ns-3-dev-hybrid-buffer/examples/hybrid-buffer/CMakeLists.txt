build_example(
  NAME hybrid-buffer-example
  SOURCE_FILES
    hybrid-buffer-example.cc
    helper/sim-helper.cc
    helper/star-sim-helper.cc
  HEADER_FILES
    helper/sim-helper.h
    helper/star-sim-helper.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libflow-monitor}
    ${libapplications}
    ${libtraffic-control}
)

build_example(
  NAME fullmesh-example
  SOURCE_FILES
    fullmesh-example.cc
    helper/sim-helper.cc
    helper/star-sim-helper.cc
    helper/fullmesh-star-sim-helper.cc
    applications/fullmesh-application.cc
    applications/fullmesh-helper.cc
  HEADER_FILES
    helper/sim-helper.h
    helper/star-sim-helper.h
    helper/fullmesh-star-sim-helper.h
    applications/fullmesh-application.h
    applications/fullmesh-helper.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libinternet}
    ${libpoint-to-point}
    ${libflow-monitor}
    ${libapplications}
    ${libtraffic-control}
)

# Scan *.cc files in subdirectories
file(
  GLOB_RECURSE subdirectories
  CONFIGURE_DEPENDS
  LIST_DIRECTORIES true
  ${CMAKE_CURRENT_SOURCE_DIR}/**
)
# Filter out files
foreach(entry ${subdirectories})
  if(NOT (IS_DIRECTORY ${entry}))
    list(REMOVE_ITEM subdirectories ${entry})
  endif()
endforeach()

foreach(subdir ${subdirectories})
  if(EXISTS ${subdir}/CMakeLists.txt)
    add_subdirectory(${subdir})
  endif()
endforeach()
