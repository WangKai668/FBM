# Get all files in dir.
file(GLOB test_sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/[^.]*.cc)

#Return early if no sources in the dir.
list(LENGTH test_sources number_sources)
if(number_sources EQUAL 0)
  return()
endif()

set(test_src)
foreach(source_file ${test_sources})
  file(READ ${source_file} source_file_contents)
  string(REGEX MATCHALL "main[(| (]" main_position "${source_file_contents}")
  if(CMAKE_MATCH_0)
    set(test_src ${source_file})

    if(NOT test_src)
      return()
    endif()

  get_filename_component(src_name_we ${test_src} NAME_WE)
  get_filename_component(src_name ${test_src} NAME)

  build_example(
    NAME ${src_name_we}
    SOURCE_FILES
        ${src_name}
        ../helper/sim-helper.cc
        ../helper/star-sim-helper.cc
        ../helper/fullmesh-star-sim-helper.cc
        ../applications/fullmesh-application.cc
        ../applications/fullmesh-helper.cc
    HEADER_FILES
        ../helper/sim-helper.h
        ../helper/star-sim-helper.h
        ../helper/fullmesh-star-sim-helper.h
        ../applications/fullmesh-application.h
        ../applications/fullmesh-helper.h
    LIBRARIES_TO_LINK
        ${libcore}
        ${libinternet}
        ${libpoint-to-point}
        ${libflow-monitor}
        ${libapplications}
        ${libtraffic-control}
  )
  endif()
endforeach()
