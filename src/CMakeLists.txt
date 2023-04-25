set(SOURCE_FILES
        tairgis.c
        spatial.c
        util.c
        spatial/geom.c
        spatial/grisu3.c
        spatial/rtree.c
        spatial/geoutil.c
        spatial/poly.c
        spatial/polyinside.c
        spatial/polyintersects.c
        spatial/polyraycast.c
        spatial/hash.c
        spatial/bing.c
        spatial/json.c)

set(CMAKE_MACOSX_RPATH 1)
add_library(tairgis SHARED ${SOURCE_FILES})