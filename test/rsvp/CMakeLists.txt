add_executable(RsvpTest RsvpTest.cpp)
target_link_libraries(RsvpTest PRIVATE crosspoint_test_common GTest::gtest_main)
target_include_directories(RsvpTest PRIVATE ${REPO_ROOT}/lib/Utf8)
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

add_executable(SmartTimingHarness SmartTimingHarness.cpp)
target_include_directories(SmartTimingHarness PRIVATE ${REPO_ROOT}/lib/Rsvp ${REPO_ROOT}/lib/Utf8)
add_test(NAME SmartTimingHarness COMMAND SmartTimingHarness)

add_executable(ContextHarness ContextHarness.cpp
  ${REPO_ROOT}/lib/EpdFont/EpdFont.cpp ${REPO_ROOT}/lib/Utf8/Utf8.cpp)
target_include_directories(ContextHarness PRIVATE ${REPO_ROOT}/lib/Rsvp ${REPO_ROOT}/lib/EpdFont ${REPO_ROOT}/lib/Utf8)
add_test(NAME ContextHarness COMMAND ContextHarness)

if(EXISTS "${REPO_ROOT}/.pio/libdeps/default/ArduinoJson/src/ArduinoJson.h")
  add_executable(PreferencesHarness PreferencesHarness.cpp)
  target_include_directories(PreferencesHarness PRIVATE ${REPO_ROOT}/lib/Rsvp ${REPO_ROOT}/lib/Utf8 ${REPO_ROOT}/src
    ${REPO_ROOT}/.pio/libdeps/default/ArduinoJson/src)
  add_test(NAME PreferencesHarness COMMAND PreferencesHarness)
endif()
