set(MC_SOURCES
    main.cpp

    # Core
    core/DatabaseManager.cpp
    core/UserProfile.cpp

    # Scanner
    scanner/FfprobeScanner.cpp
    scanner/ScanWorker.cpp

    # UI (Phase 1 implemented)
    ui/McMainWindow.cpp
    ui/McTrackTableModel.cpp

    # Phase 2+ stubs (will be replaced with full implementations per phase)
    Phase2Stubs.cpp
)

set(MC_HEADERS
    # Core
    ../include/core/DatabaseManager.h
    ../include/core/AppSettings.h
    ../include/core/UserProfile.h
    ../include/core/ExternalTools.h

    # Scanner
    ../include/scanner/FfprobeScanner.h
    ../include/scanner/ScanWorker.h
    ../include/scanner/OriginalLanguageDetector.h

    # Engine
    ../include/engine/RuleEngine.h
    ../include/engine/ActionEngine.h
    ../include/engine/JobQueue.h
    ../include/engine/RemuxJob.h
    ../include/engine/TrackDecision.h

    # Classifier
    ../include/classifier/TrackClassifier.h
    ../include/classifier/RegexClassifier.h

    # UI
    ../include/ui/McMainWindow.h
    ../include/ui/McTrackTableModel.h
    ../include/ui/McLibraryPanel.h
    ../include/ui/McFilterPanel.h
    ../include/ui/McJobPanel.h
    ../include/ui/McPreviewDialog.h
    ../include/ui/McSettingsDialog.h
)

qt_add_executable(MediaCurator
    WIN32    # Windows: no console window in release
    ${MC_SOURCES}
    ${MC_HEADERS}
)

target_include_directories(MediaCurator PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/include"
)

target_link_libraries(MediaCurator PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
    nlohmann_json::nlohmann_json
)

# Copy tools/ directory next to the executable after build
add_custom_command(TARGET MediaCurator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/tools"
        "$<TARGET_FILE_DIR:MediaCurator>/tools"
    COMMENT "Copying bundled tools (ffprobe, mkvmerge) to output directory"
)

# Copy data/ directory (classifier rules, etc.)
add_custom_command(TARGET MediaCurator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:MediaCurator>/data"
    COMMENT "Copying data files to output directory"
)

# Windows: deploy Qt DLLs automatically (requires windeployqt in PATH)
if(WIN32)
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET MediaCurator POST_BUILD
            COMMAND "${WINDEPLOYQT}"
                --no-translations
                --no-system-d3d-compiler
                "$<TARGET_FILE:MediaCurator>"
            COMMENT "Running windeployqt to copy Qt DLLs"
        )
    endif()
endif()

install(TARGETS MediaCurator
    RUNTIME DESTINATION bin
)
