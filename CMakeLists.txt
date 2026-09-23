cmake_minimum_required(VERSION 3.22)
project(World11 LANGUAGES CXX)

find_package(glfw3 CONFIG REQUIRED)
set(GLFW_TARGET "")
if (TARGET glfw)
  set(GLFW_TARGET glfw)
elseif (TARGET glfw3)
  set(GLFW_TARGET glfw3)
elseif (TARGET glfw::glfw)
  set(GLFW_TARGET glfw::glfw)
else()
  message(FATAL_ERROR "Не найден импортируемый таргет GLFW из find_package(glfw3)")
endif()

if (UNIX AND NOT WIN32)
  find_package(OpenGL REQUIRED COMPONENTS OpenGL)
endif()

set(HG_CPP_HEADERS
  cpp/hg_console_renderer.hpp
  cpp/hg_camera_movement_applicator.hpp
  cpp/hg_camera_tuning.hpp
  cpp/hg_engine.hpp
  cpp/hg_frame_title_controller.hpp
  cpp/hg_runtime_api.h
  cpp/hg_runtime_bridge.hpp
  cpp/hg_gamepad_controller.hpp
  cpp/hg_game_app.hpp
  cpp/hg_glfw_runtime_window.hpp
  cpp/hg_gl33_runtime_state.hpp
  cpp/hg_interfaces.hpp
  cpp/hg_keyboard_movement_controller.hpp
  cpp/hg_mouse_look_controller.hpp
  cpp/hg_runtime_input.hpp
  cpp/hg_runtime_camera_state.hpp
  cpp/hg_runtime_frame_state.hpp
  cpp/hg_runtime_input_snapshot.hpp
  cpp/hg_runtime_universe.hpp
  cpp/hg_runtime_world_flags.hpp
  cpp/render/gl33/Gl33Api.hpp
  cpp/render/gl33/Gl33Buffer.hpp
  cpp/render/gl33/Gl33HudRenderer.hpp
  cpp/render/gl33/Gl33Mesh.hpp
  cpp/render/gl33/Gl33Renderer.hpp
  cpp/render/gl33/Gl33Skybox.hpp
  cpp/render/gl33/Gl33Texture.hpp
  cpp/render/gl33/Gl33WorldRenderer.hpp
  cpp/render/gl33/Gl33World11DecorRenderer.hpp
  cpp/render/gl33/Gl33World11FishRenderer.hpp
  cpp/render/gl33/World11CoralGeometry.hpp
  cpp/render/gl33/World11WaterWaves.hpp
  cpp/render/gl33/World11Environment.hpp
  cpp/render/gl33/Gl33ShaderProgram.hpp
  cpp/render/gl33/Gl33VertexArray.hpp
  cpp/third_party/stb_image.h
  cpp/third_party/stb_easy_font.h
  cpp/world/BaseWorld.hpp
  cpp/world/HudViewModel.hpp
  cpp/world/World11Seabed.hpp
  cpp/world/World11WaterSurface.hpp
  cpp/world/World11Landmarks.hpp
  cpp/world/World11DecorGenerator.hpp
  cpp/world/World11FishTrajectory.hpp
   cpp/world/PortalController.hpp
   cpp/world/CppWorldGl33.hpp
  cpp/world/VerticalMotionController.hpp
  cpp/world/WorldBuilder.hpp
  cpp/world/WorldCatalog.hpp
  cpp/world/WorldDefinition.hpp
  cpp/world/WorldManager.hpp
  cpp/world/WorldHost.hpp
  cpp/world/WorldRegistry.hpp
  cpp/world/collision/CollisionTypes.hpp
  cpp/world/collision/CollisionWorld.hpp
  cpp/world/collision/CollisionResolver.hpp
)

set(HG_CPP_TEMPLATES
  cpp/world/CppWorld_template.hpp
  cpp/world/CppWorld_template.cpp
)
set(HG_CPP_SOURCES
  cpp/main.cpp
  cpp/hg_camera_movement_applicator.cpp
  cpp/hg_engine.cpp
  cpp/hg_frame_title_controller.cpp
  cpp/hg_gamepad_controller.cpp
  cpp/hg_game_app.cpp
  cpp/hg_glfw_runtime_window.cpp
  cpp/hg_gl33_runtime_state.cpp
  cpp/hg_keyboard_movement_controller.cpp
  cpp/hg_mouse_look_controller.cpp
  cpp/hg_runtime_camera_state.cpp
  cpp/hg_runtime_engine.cpp
  cpp/hg_runtime_frame_state.cpp
  cpp/hg_runtime_input_snapshot.cpp
  cpp/hg_runtime_universe.cpp
  cpp/hg_runtime_world_flags.cpp
  cpp/hg_console_renderer.cpp
  cpp/hg_runtime_input.cpp
  cpp/hg_runtime_bridge.cpp
  cpp/render/gl33/Gl33Api.cpp
  cpp/render/gl33/Gl33Buffer.cpp
  cpp/render/gl33/Gl33HudRenderer.cpp
  cpp/render/gl33/Gl33Mesh.cpp
  cpp/render/gl33/Gl33Renderer.cpp
  cpp/render/gl33/Gl33Skybox.cpp
  cpp/render/gl33/Gl33Texture.cpp
  cpp/render/gl33/Gl33WorldRenderer.cpp
  cpp/render/gl33/Gl33World11DecorRenderer.cpp
  cpp/render/gl33/Gl33World11FishRenderer.cpp
  cpp/render/gl33/World11CoralGeometry.cpp
  cpp/render/gl33/Gl33ShaderProgram.cpp
  cpp/render/gl33/Gl33VertexArray.cpp
  cpp/world/WorldBuilder.cpp
  cpp/world/WorldRegistry.cpp
  cpp/world/WorldManager.cpp
  cpp/world/WorldHost.cpp
  cpp/world/WorldCatalog.cpp
  cpp/world/collision/CollisionWorld.cpp
  cpp/world/collision/CollisionResolver.cpp
  cpp/world/BaseWorld.cpp
  cpp/world/HudViewModel.cpp
  cpp/world/World11DecorGenerator.cpp
  cpp/world/World11FishTrajectory.cpp
   cpp/world/PortalController.cpp
   cpp/world/CppWorldGl33.cpp
  cpp/world/VerticalMotionController.cpp
)

add_executable(horizongates_cpp
  ${HG_CPP_SOURCES}
  ${HG_CPP_HEADERS}
  ${HG_CPP_TEMPLATES}
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/cpp PREFIX "Include" FILES ${HG_CPP_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/cpp PREFIX "Source" FILES ${HG_CPP_SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/cpp PREFIX "Template" FILES ${HG_CPP_TEMPLATES})
target_include_directories(horizongates_cpp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
set_property(TARGET horizongates_cpp PROPERTY CXX_STANDARD 17)
set_property(TARGET horizongates_cpp PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET horizongates_cpp PROPERTY CXX_EXTENSIONS OFF)

if (WIN32)
  target_link_libraries(horizongates_cpp PRIVATE
    ${GLFW_TARGET} opengl32 gdi32 user32 kernel32)
else()
  target_link_libraries(horizongates_cpp PRIVATE
    ${GLFW_TARGET} OpenGL::GL)
endif()

# <<< ВАЖНО: стартовый проект в Visual Studio
 if (MSVC)
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT horizongates_cpp)
 endif()
# <<< ВАЖНО: рабочая папка дебаггера = папка exe
set_property(TARGET horizongates_cpp PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:horizongates_cpp>")

# Если GLFW динамический — положим DLL рядом с exe
get_target_property(GLFW_TYPE ${GLFW_TARGET} TYPE)
if (GLFW_TYPE STREQUAL "SHARED_LIBRARY" OR GLFW_TYPE STREQUAL "MODULE_LIBRARY" OR GLFW_TYPE STREQUAL "EXECUTABLE")
  add_custom_command(TARGET horizongates_cpp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${GLFW_TARGET}>
            $<TARGET_FILE_DIR:horizongates_cpp>)
endif()


# Копируем всю папку datasets в корень билда (vsbuild)
add_custom_command(TARGET horizongates_cpp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/datasets
          ${CMAKE_BINARY_DIR}/datasets
  COMMENT "Copy datasets directory в корень билда (vsbuild)"
)
add_custom_command(TARGET horizongates_cpp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/datasets
          $<TARGET_FILE_DIR:horizongates_cpp>/datasets
  COMMENT "Copy datasets directory рядом с exe (Debug/Release output)"
)
include(GNUInstallDirs)
install(TARGETS horizongates_cpp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/datasets DESTINATION ${CMAKE_INSTALL_BINDIR})

include(CTest)
if (BUILD_TESTING)
  add_executable(world11_decor_generator_tests
    tests/World11DecorGeneratorTests.cpp
    cpp/world/World11DecorGenerator.cpp
    cpp/world/World11DecorGenerator.hpp
    cpp/world/World11Seabed.hpp
  )
  target_include_directories(world11_decor_generator_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
  set_property(TARGET world11_decor_generator_tests PROPERTY CXX_STANDARD 17)
  set_property(TARGET world11_decor_generator_tests PROPERTY CXX_STANDARD_REQUIRED ON)
  set_property(TARGET world11_decor_generator_tests PROPERTY CXX_EXTENSIONS OFF)
  add_test(NAME world11_decor_generator COMMAND world11_decor_generator_tests)

  add_executable(world11_coral_geometry_tests
    tests/World11CoralGeometryTests.cpp
    cpp/render/gl33/World11CoralGeometry.cpp
    cpp/render/gl33/World11CoralGeometry.hpp
    cpp/world/World11DecorGenerator.cpp
    cpp/world/World11DecorGenerator.hpp
    cpp/world/World11Seabed.hpp
  )
  target_include_directories(world11_coral_geometry_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
  set_property(TARGET world11_coral_geometry_tests PROPERTY CXX_STANDARD 17)
  set_property(TARGET world11_coral_geometry_tests PROPERTY CXX_STANDARD_REQUIRED ON)
  set_property(TARGET world11_coral_geometry_tests PROPERTY CXX_EXTENSIONS OFF)
  add_test(NAME world11_coral_geometry COMMAND world11_coral_geometry_tests)

  add_executable(world11_fish_trajectory_tests
    tests/World11FishTrajectoryTests.cpp
    cpp/world/World11FishTrajectory.cpp
    cpp/world/World11FishTrajectory.hpp
    cpp/world/World11Seabed.hpp
    cpp/world/World11WaterSurface.hpp
  )
  target_include_directories(world11_fish_trajectory_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
  set_property(TARGET world11_fish_trajectory_tests PROPERTY CXX_STANDARD 17)
  set_property(TARGET world11_fish_trajectory_tests PROPERTY CXX_STANDARD_REQUIRED ON)
  set_property(TARGET world11_fish_trajectory_tests PROPERTY CXX_EXTENSIONS OFF)
  add_test(NAME world11_fish_trajectory COMMAND world11_fish_trajectory_tests)
endif()

#   Сборка на Linux:
#   cd /path/to/horizongates
#   mkdir build-linux
#   cd build-linux
#   cmake -G Ninja ..
#   ninja

# Explicit manual GPU probe; excluded from default builds and CTest.
if (BUILD_TESTING)
  set(HG_PROBE_SOURCES ${HG_CPP_SOURCES})
  list(FILTER HG_PROBE_SOURCES EXCLUDE REGEX "/main\\.cpp$")
  add_executable(world11_scene_probe EXCLUDE_FROM_ALL
    tests/World11SceneProbe.cpp ${HG_PROBE_SOURCES})
  target_include_directories(world11_scene_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)
  target_compile_features(world11_scene_probe PRIVATE cxx_std_17)
  add_custom_command(TARGET world11_scene_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      ${CMAKE_CURRENT_SOURCE_DIR}/datasets $<TARGET_FILE_DIR:world11_scene_probe>/datasets)
  if (GLFW_TYPE STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET world11_scene_probe POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${GLFW_TARGET}> $<TARGET_FILE_DIR:world11_scene_probe>)
  endif()
  if (WIN32)
    target_link_libraries(world11_scene_probe PRIVATE ${GLFW_TARGET} opengl32 gdi32 user32 kernel32)
  else()
    target_link_libraries(world11_scene_probe PRIVATE ${GLFW_TARGET} OpenGL::GL)
  endif()
endif()

# Use the runtime belonging to this compiler, not a stale DLL from another build.
if (MINGW)
  get_filename_component(HG_COMPILER_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  foreach(HG_TARGET horizongates_cpp world11_decor_generator_tests
      world11_coral_geometry_tests world11_fish_trajectory_tests world11_scene_probe)
    if (TARGET ${HG_TARGET})
      foreach(HG_DLL libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libc++.dll libunwind.dll)
        if (EXISTS "${HG_COMPILER_BIN}/${HG_DLL}")
          add_custom_command(TARGET ${HG_TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${HG_COMPILER_BIN}/${HG_DLL}" "$<TARGET_FILE_DIR:${HG_TARGET}>")
        endif()
      endforeach()
    endif()
  endforeach()
endif()
