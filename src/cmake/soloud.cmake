include(FetchContent)

FetchContent_Declare(
        soloud
        GIT_REPOSITORY "https://github.com/jarikomppa/soloud"
        GIT_TAG "RELEASE_20200207"
)
FetchContent_MakeAvailable(soloud)

# Core sources
file(GLOB_RECURSE SOLOUD_CORE_SOURCES
        "${soloud_SOURCE_DIR}/src/core/*.cpp"
        "${soloud_SOURCE_DIR}/src/core/*.c"
)

# Audio sources
file(GLOB_RECURSE SOLOUD_WAV_SOURCES
        "${soloud_SOURCE_DIR}/src/audiosource/wav/*.cpp"
        "${soloud_SOURCE_DIR}/src/audiosource/wav/*.c"
)

# Backend
file(GLOB_RECURSE SOLOUD_BACKEND_SOURCES
        "${soloud_SOURCE_DIR}/src/backend/miniaudio/*.cpp"
        "${soloud_SOURCE_DIR}/src/backend/miniaudio/*.c"
)

# Filters
file(GLOB_RECURSE SOLOUD_FILTER_SOURCES
        "${soloud_SOURCE_DIR}/src/filter/*.cpp"
)

add_library(soloud STATIC
        ${SOLOUD_CORE_SOURCES}
        ${SOLOUD_WAV_SOURCES}
        ${SOLOUD_BACKEND_SOURCES}
        ${SOLOUD_FILTER_SOURCES}
)

target_include_directories(soloud PUBLIC
        "${soloud_SOURCE_DIR}/include"
)

