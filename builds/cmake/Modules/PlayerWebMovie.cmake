# A separate WASM module so legacy movie support costs nothing at startup.
find_program(PLAYER_MOVIE_BASH bash REQUIRED)
get_filename_component(PLAYER_EMSCRIPTEN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
add_custom_command(
	OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/movie-decoder.js"
		"${CMAKE_CURRENT_BINARY_DIR}/movie-decoder.wasm"
		"${CMAKE_CURRENT_BINARY_DIR}/movie-decoder.LICENSE.txt"
	COMMAND "${CMAKE_COMMAND}" -E env "PATH=${PLAYER_EMSCRIPTEN_DIR}:$ENV{PATH}"
		"${PLAYER_MOVIE_BASH}" "${CMAKE_CURRENT_SOURCE_DIR}/builds/emscripten/build-movie-decoder.sh"
		"${CMAKE_CURRENT_BINARY_DIR}"
	DEPENDS src/platform/emscripten/movie_decoder.c builds/emscripten/build-movie-decoder.sh
	VERBATIM)
add_custom_target(player-movie-decoder ALL DEPENDS
	"${CMAKE_CURRENT_BINARY_DIR}/movie-decoder.js"
	"${CMAKE_CURRENT_BINARY_DIR}/movie-decoder.wasm"
	"${CMAKE_CURRENT_BINARY_DIR}/movie-decoder.LICENSE.txt")
add_dependencies(${EXE_NAME} player-movie-decoder)
