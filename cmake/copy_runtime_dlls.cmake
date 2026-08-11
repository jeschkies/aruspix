# Copy runtime DLLs into the target output directory. Invoked as a
# POST_BUILD step for the aruspix Windows target. Split into its own
# script so we can no-op cleanly when the DLL list is empty (cmake -E
# copy_if_different errors on a destination-only invocation).

if(NOT DLLS OR DLLS STREQUAL "")
    return()
endif()

file(COPY ${DLLS} DESTINATION ${DEST})
