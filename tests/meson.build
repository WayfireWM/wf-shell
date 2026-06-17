gtest_dep = dependency('gtest_main')
gtkmm_dep = dependency('gtkmm-4.0')

# Headers live in src/util; add that to the include path for tests.
test_includes = include_directories('../src/util')

test_exec_power = executable(
    'power-controller-test',
    'power-controller-test.cpp',
    dependencies: [gtest_dep, gtkmm_dep, libutil],
    include_directories: test_includes,
    link_with: util,
)

test(
    'power-controller-test',
    test_exec_power,
    suite: 'unit',
)

test_exec_network = executable(
    'network-backend-test',
    'network-backend-test.cpp',
    dependencies: [gtest_dep, gtkmm_dep, libutil],
    include_directories: test_includes,
    link_with: util,
)

test(
    'network-backend-test',
    test_exec_network,
    suite: 'unit',
)
