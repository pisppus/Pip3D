set(srcs
    "Core/Jobs.cpp"
    "Debug/Logging.cpp"
    "Debug/Gizmos.cpp"
    "Geometry/Mesh.cpp"
    "Physics/Collision/Raycast.cpp"
    "Physics/World.cpp"
    "Rendering/Effects.cpp"
    "Rendering/Effects/Exposure.cpp"
    "Rendering/Lighting/Shadow.cpp"
    "Rendering/Pipeline/DrawCache.cpp"
    "Rendering/Pipeline/Pipeline.cpp"
    "Rendering/Pipeline/Telemetry.cpp"
    "Rendering/Renderer.cpp"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "."
    REQUIRES
        pipcore
        esp_timer
    PRIV_REQUIRES
        esp_hw_support
        freertos
)

set(pip3d_compile_opts
    -O3
    -funroll-loops
    -fno-math-errno
    -fno-trapping-math
    -ffp-contract=fast
    -fstrict-aliasing
    -Wno-error=attributes
)
if(IDF_TARGET STREQUAL "esp32" OR IDF_TARGET STREQUAL "esp32s2" OR IDF_TARGET STREQUAL "esp32s3")
    list(APPEND pip3d_compile_opts -mtext-section-literals)
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE ${pip3d_compile_opts})
target_compile_options(${COMPONENT_LIB} PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++2b>
)

set(PIP3D_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
if(EXISTS "${PIP3D_PROJECT_ROOT}/Tools/Prebuild.py")
    set(_pip3d_python "${PYTHON}")
    if(NOT _pip3d_python)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        set(_pip3d_python "${Python3_EXECUTABLE}")
    endif()
    add_custom_target(pip3d_assets
        COMMAND ${_pip3d_python} "${PIP3D_PROJECT_ROOT}/Tools/Prebuild.py"
                --project-dir "${PIP3D_PROJECT_ROOT}"
        WORKING_DIRECTORY "${PIP3D_PROJECT_ROOT}"
        COMMENT "Pip3D: generating assets (models, textures, clouds, sounds)"
        VERBATIM
    )
    add_dependencies(${COMPONENT_LIB} pip3d_assets)
endif()
