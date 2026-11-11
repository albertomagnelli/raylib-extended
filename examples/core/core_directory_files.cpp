/*******************************************************************************************
*
*   raylib [core] example - directory files
*
*   Example complexity rating: [★☆☆☆] 1/4
*
*   Example originally created with raylib 5.5, last time updated with raylib 5.6
*
*   Example contributed by Hugo ARNAL (@hugoarnal) and reviewed by Ramon Santamaria (@raysan5)
*
*   Example licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2025 Hugo ARNAL (@hugoarnal)
*
********************************************************************************************/

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"                 // Required for GUI controls

#define MAX_FILEPATH_SIZE       1024
#define FILE_FILTER             "DIRS*;.png;.c"

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
i32 main()
{
    // Initialization
    //--------------------------------------------------------------------------------------
    constexpr i32 screenWidth = 800;
    constexpr i32 screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - directory files");

    std::string directory = GetWorkingDirectory();

    // Load file-paths on current working directory
    // NOTE: LoadDirectoryFiles() loads files and directories by default,
    // use LoadDirectoryFilesEx() for custom filters and recursive directories loading
    //FilePathList files = LoadDirectoryFiles(directory);
    FilePathList files = LoadDirectoryFilesEx(directory, FILE_FILTER, false);

    i32 btnBackPressed = false;

    i32 listScrollIndex = 0;
    i32 listItemActive = -1;
    i32 listItemFocused = -1;

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        if (btnBackPressed)
        {
            directory = GetPrevDirectoryPath(directory);
            files = LoadDirectoryFilesEx(directory, FILE_FILTER, false);

            listScrollIndex = 0;
            listItemActive = -1;
            listItemFocused = -1;
        }

        if ((listItemActive >= 0) && (listItemActive < static_cast<i32>(files.paths.size())) && DirectoryExists(files.paths[listItemActive]))
        {
            directory = files.paths[listItemActive];
            files = LoadDirectoryFilesEx(directory, FILE_FILTER, false);

            listScrollIndex = 0;
            listItemActive = -1;
            listItemFocused = -1;
        }
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);

            btnBackPressed = GuiButton(Rectangle{ 40.0f, 10.0f, 48, 28 }, "<");

            GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize*2);
            GuiLabel(Rectangle{ 40 + 48 + 10, 10, 700, 28 }, directory.c_str());
            GuiSetStyle(DEFAULT, TEXT_SIZE, GuiGetFont().baseSize);

            GuiSetStyle(LISTVIEW, TEXT_ALIGNMENT, TEXT_ALIGN_LEFT);
            GuiSetStyle(LISTVIEW, TEXT_PADDING, 40);
            std::vector<char *> filePathPtrs;
            filePathPtrs.reserve(files.paths.size());
            for (std::string &path : files.paths) filePathPtrs.push_back(path.data());

            GuiListViewEx(Rectangle{ 0, 50, static_cast<f32>(GetScreenWidth()), static_cast<f32>(GetScreenHeight()) - 50 },
                filePathPtrs.data(), static_cast<i32>(filePathPtrs.size()), &listScrollIndex, &listItemActive, &listItemFocused);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
