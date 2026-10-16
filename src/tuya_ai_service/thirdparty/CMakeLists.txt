# Register each library here; its directory owns configuration and build logic.
# Keep downloaded sources, local adaptations and tests inside that library.
add_subdirectory(opus)

# Forward the component objects and headers collected by the library builds.
set(COMPONENT_LIBS "${COMPONENT_LIBS}" PARENT_SCOPE)
set(COMPONENT_PUBINC "${COMPONENT_PUBINC}" PARENT_SCOPE)
