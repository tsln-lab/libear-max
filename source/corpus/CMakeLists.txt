# The ADM corpus checker: reads every BW64 file of a corpus (the EBU's ADM
# test materials, see README "Testing against the EBU's ADM test files"),
# runs the metadata through libear's gain calculators and the document
# builder's round trip, and records or checks a manifest of what it found.
# Not a Max external: a plain executable next to the unit tests.

cmake_minimum_required(VERSION 3.19)

add_executable(adm_corpus adm_corpus.cpp)
target_include_directories(adm_corpus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../projects/shared)
target_link_libraries(adm_corpus PRIVATE ear adm bw64)
set_property(TARGET adm_corpus PROPERTY CXX_STANDARD 17)
set_property(TARGET adm_corpus PROPERTY CXX_STANDARD_REQUIRED ON)
set_target_properties(adm_corpus PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/tests"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/tests"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/tests")

# ctest runs the check against the committed manifest when EARMAX_ADM_CORPUS
# names the corpus directory, and reports the test as skipped otherwise
add_test(NAME adm_corpus COMMAND adm_corpus --check "${CMAKE_CURRENT_SOURCE_DIR}/manifest.txt")
set_tests_properties(adm_corpus PROPERTIES SKIP_RETURN_CODE 77)
