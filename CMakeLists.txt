cmake_minimum_required(VERSION 3.10)
project(Crafters)

# Ustawiamy standard C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- 1. Definiowanie plików źródłowych ---

# Dodajemy plik .c od GLAD do kompilacji razem z naszym projektem
set(GLAD_SRC ${PROJECT_SOURCE_DIR}/lib/src/glad.c)

# Definiujemy nasz główny plik wykonywalny
add_executable(Crafters main.cpp ${GLAD_SRC})


# --- 2. Informowanie CMake o naszych bibliotekach ---

# Mówimy CMake, gdzie ma szukać plików nagłówkowych (.h)
target_include_directories(Crafters PRIVATE
        ${PROJECT_SOURCE_DIR}/lib/include
)

# Mówimy linkerowi, gdzie ma szukać plików bibliotek (.lib)
target_link_directories(Crafters PRIVATE
        ${PROJECT_SOURCE_DIR}/lib/lib
)


# --- 3. Linkowanie bibliotek ---

# Łączymy nasz program z bibliotekami
# UWAGA: Kolejność ma znaczenie!

# Dla Windows (najczęszy przypadek)
if(WIN32)
    target_link_libraries(Crafters PRIVATE
            glfw3       # Biblioteka GLFW (plik glfw3.lib)
            opengl32    # Standardowa biblioteka OpenGL Windows
            gdi32       # Wymagane przez GLFW na Windows
            user32      # Wymagane przez GLFW na Windows
            shell32     # Wymagane przez GLFW na Windows
            stdc++fs
    )
    # Dla macOS
elseif(APPLE)
    # Na macOS trzeba inaczej znaleźć pakiety
    find_package(OpenGL REQUIRED)
    find_package(GLFW 3.3 REQUIRED)

    target_link_libraries(Crafters PRIVATE
            ${OPENGL_LIBRARIES}
            ${GLFW_LIBRARIES}
    )
    # Dla Linux
else()
    find_package(OpenGL REQUIRED)
    find_package(GLFW 3.3 REQUIRED)

    target_link_libraries(Crafters PRIVATE
            ${OPENGL_LIBRARIES}
            ${GLFW_LIBRARIES}
            pthread # Wymagane na Linux
    )
endif()
# --- 4. Kopiowanie plików (np. shadery) do folderu wyjściowego ---
# Upewnij się, że pliki .vert i .frag są w tym samym folderze co CMakeLists.txt

set(ASSET_FILES
        ${PROJECT_SOURCE_DIR}/shader.vert
        ${PROJECT_SOURCE_DIR}/shader.frag
        ${PROJECT_SOURCE_DIR}/atlas.png
        ${PROJECT_SOURCE_DIR}/crosshair.vert # <<< NOWOŚĆ
        ${PROJECT_SOURCE_DIR}/crosshair.frag # <<< NOWOŚĆ
        ${PROJECT_SOURCE_DIR}/highlighter.vert # <<< NOWOŚĆ
        ${PROJECT_SOURCE_DIR}/highlighter.frag # <<< NOWOŚĆ
        ${PROJECT_SOURCE_DIR}/skybox.frag
        ${PROJECT_SOURCE_DIR}/skybox.vert
        ${PROJECT_SOURCE_DIR}/right.png
        ${PROJECT_SOURCE_DIR}/left.png
        ${PROJECT_SOURCE_DIR}/top.png
        ${PROJECT_SOURCE_DIR}/bottom.png
        ${PROJECT_SOURCE_DIR}/front.png
        ${PROJECT_SOURCE_DIR}/back.png
        ${PROJECT_SOURCE_DIR}/ui.vert
        ${PROJECT_SOURCE_DIR}/ui.frag
        # NOWOŚĆ: Tekstury UI
        ${PROJECT_SOURCE_DIR}/ui_slot.png
        ${PROJECT_SOURCE_DIR}/ui_selector.png
        ${PROJECT_SOURCE_DIR}/ui_icons.png
        ${PROJECT_SOURCE_DIR}/loading.png
        ${PROJECT_SOURCE_DIR}/button_quit.png
        ${PROJECT_SOURCE_DIR}/button_load_world.png
        ${PROJECT_SOURCE_DIR}/button_new_world.png
        ${PROJECT_SOURCE_DIR}/button_back.png
        ${PROJECT_SOURCE_DIR}/button_start.png
        ${PROJECT_SOURCE_DIR}/font.png
        ${PROJECT_SOURCE_DIR}/button_save_menu.png
        ${PROJECT_SOURCE_DIR}/button_resume.png
        ${PROJECT_SOURCE_DIR}/overlay.png
        ${PROJECT_SOURCE_DIR}/sun.png
        ${PROJECT_SOURCE_DIR}/depth.frag
        ${PROJECT_SOURCE_DIR}/depth.vert
        ${PROJECT_SOURCE_DIR}/break.mp3
        ${PROJECT_SOURCE_DIR}/step.mp3
        ${PROJECT_SOURCE_DIR}/place.mp3
        ${PROJECT_SOURCE_DIR}/pop.mp3
        ${PROJECT_SOURCE_DIR}/clouds.frag
        ${PROJECT_SOURCE_DIR}/clouds.vert
)

# Kopiuj te pliki do folderu docelowego (gdzie jest .exe)
add_custom_command(TARGET Crafters POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${ASSET_FILES}
        $<TARGET_FILE_DIR:Crafters>
)