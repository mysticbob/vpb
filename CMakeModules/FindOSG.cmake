# This module defines

# OSG_LIBRARY
# OSG_FOUND, if false, do not try to link to osg
# OSG_INCLUDE_DIRS, where to find the headers
# OSG_INCLUDE_DIR, where to find the source headers
# OSG_GEN_INCLUDE_DIR, where to find the generated headers

# to use this module, set variables to point to the osg build
# directory, and source directory, respectively
# OSGDIR or OSG_SOURCE_DIR: osg source directory, typically OpenSceneGraph
# OSG_DIR or OSG_BUILD_DIR: osg build directory, place in which you've
#    built osg via cmake 

# Header files are presumed to be included like
# #include <osg/PositionAttitudeTransform>
# #include <osgUtil/SceneView>

###### headers ######

MACRO( FIND_OSG_INCLUDE THIS_OSG_INCLUDE_DIR THIS_OSG_INCLUDE_FILE )

FIND_PATH( ${THIS_OSG_INCLUDE_DIR} ${THIS_OSG_INCLUDE_FILE}
    PATHS
        $ENV{OSG_SOURCE_DIR}
        $ENV{OSGDIR}
        $ENV{OSG_DIR}
        /usr/local/
        /usr/
        /sw/ # Fink
        /opt/local/ # DarwinPorts
        /opt/csw/ # Blastwave
        /opt/
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment;OSG_ROOT]/
        ~/Library/Frameworks
        /Library/Frameworks
    PATH_SUFFIXES
        /include/
)

ENDMACRO( FIND_OSG_INCLUDE THIS_OSG_INCLUDE_DIR THIS_OSG_INCLUDE_FILE )

FIND_OSG_INCLUDE( OSG_GEN_INCLUDE_DIR   osg/Config )
FIND_OSG_INCLUDE( OSG_INCLUDE_DIR       osg/Node )

###### libraries ######

MACRO( FIND_OSG_LIBRARY MYLIBRARY MYLIBRARYNAME )

FIND_LIBRARY(${MYLIBRARY}
    NAMES
        ${MYLIBRARYNAME}
    PATHS
        $ENV{OSG_BUILD_DIR}
        $ENV{OSG_DIR}
        $ENV{OSGDIR}
        $ENV{OSG_ROOT}
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local
        /usr
        /sw
        /opt/local
        /opt/csw
        /opt
        [HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session\ Manager\\Environment;OSG_ROOT]/lib
        /usr/freeware
    PATH_SUFFIXES
        /lib/
        /lib64/
        /build/lib/
        /build/lib64/
        /Build/lib/
        /Build/lib64/
     )

ENDMACRO(FIND_OSG_LIBRARY LIBRARY LIBRARYNAME)

FIND_OSG_LIBRARY( OSG_LIBRARY osg )
FIND_OSG_LIBRARY( OSGUTIL_LIBRARY osgUtil )
FIND_OSG_LIBRARY( OSGDB_LIBRARY osgDB )
FIND_OSG_LIBRARY( OSGTEXT_LIBRARY osgText )
FIND_OSG_LIBRARY( OSGTERRAIN_LIBRARY osgTerrain )
FIND_OSG_LIBRARY( OSGFX_LIBRARY osgFX )
FIND_OSG_LIBRARY( OSGSIM_LIBRARY osgSim )
FIND_OSG_LIBRARY( OSGVIEWER_LIBRARY osgViewer )
FIND_OSG_LIBRARY( OSGGA_LIBRARY osgGA)
FIND_OSG_LIBRARY( OPENTHREADS_LIBRARY OpenThreads)

SET( OSG_FOUND FALSE )
IF( OSG_LIBRARY AND OSG_INCLUDE_DIR )

    SET( OSG_FOUND TRUE )

    EXEC_PROGRAM(osgversion ARGS --major-number OUTPUT_VARIABLE OPENSCENEGRAPH_MAJOR_VERSION)
    EXEC_PROGRAM(osgversion ARGS --minor-number OUTPUT_VARIABLE OPENSCENEGRAPH_MINOR_VERSION)
    EXEC_PROGRAM(osgversion ARGS --patch-number OUTPUT_VARIABLE OPENSCENEGRAPH_PATCH_VERSION)
    EXEC_PROGRAM(osgversion ARGS --so-number OUTPUT_VARIABLE OPENSCENEGRAPH_SOVERSION)

    set(OPENSCENEGRAPH_VERSION "${OPENSCENEGRAPH_MAJOR_VERSION}.${OPENSCENEGRAPH_MINOR_VERSION}.${OPENSCENEGRAPH_PATCH_VERSION}"
                                    CACHE INTERNAL "The version of OSG which was detected")

    if(OSG_FIND_VERSION)
        if(OSG_FIND_VERSION_EXACT)
            if(NOT OPENSCENEGRAPH_VERSION VERSION_EQUAL ${OSG_FIND_VERSION})
                message(
                    "ERROR: Version ${OSG_FIND_VERSION} of the OpenSceneGraph is required "
                    "(exactly), version ${OPENSCENEGRAPH_VERSION} was found.")

                SET(OSG_FOUND "NO")
                set(OSG_LIBRARY)
                set(OSG_INCLUDE_DIR)

            endif()
        else()
            # version is too low
            if(NOT OPENSCENEGRAPH_VERSION VERSION_EQUAL ${OSG_FIND_VERSION} AND
                    NOT OPENSCENEGRAPH_VERSION VERSION_GREATER ${OSG_FIND_VERSION})

                SET(OSG_FOUND "NO")
                set(OSG_LIBRARY)
                set(OSG_INCLUDE_DIR)

                message(
                    "ERROR: Version ${OSG_FIND_VERSION} or higher of the OpenSceneGraph "
                    "is required.  Version ${OPENSCENEGRAPH_VERSION} was found.")

            endif()
        endif()
    endif()

ENDIF()

IF( OSG_FOUND )
    
    IF (${OSG_INCLUDE_DIR} STREQUAL ${OSG_GEN_INCLUDE_DIR})
        SET( OSG_INCLUDE_DIRS ${OSG_INCLUDE_DIR})
    ELSE (${OSG_INCLUDE_DIR} STREQUAL ${OSG_GEN_INCLUDE_DIR})
        SET( OSG_INCLUDE_DIRS ${OSG_INCLUDE_DIR} ${OSG_GEN_INCLUDE_DIR} )
    ENDIF (${OSG_INCLUDE_DIR} STREQUAL ${OSG_GEN_INCLUDE_DIR})
    
    GET_FILENAME_COMPONENT( OSG_LIBRARIES_DIR ${OSG_LIBRARY} PATH )
ENDIF( OSG_FOUND )


