add_executable(RsvpTest RsvpTest.cpp)
target_link_libraries(RsvpTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(RsvpTest)

add_executable(RsvpTypographyTest
  RsvpTypographyTest.cpp
  ${REPO_ROOT}/lib/Rsvp/RsvpTypography.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFont.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)
target_include_directories(RsvpTypographyTest PRIVATE
  ${REPO_ROOT}/lib/Rsvp
  ${REPO_ROOT}/lib/EpdFont
  ${REPO_ROOT}/lib/Utf8
)
target_link_libraries(RsvpTypographyTest PRIVATE crosspoint_test_common GTest::gtest_main)
gtest_discover_tests(RsvpTypographyTest)
