gtest_dep = dependency('gtest', static: false)
gtest_main_dep = dependency('gtest_main', required: true)

test_sources = files('CreateRaceTests.cpp')

poco_foundation = cpp.find_library('PocoFoundation', required: true)
poco_util = cpp.find_library('PocoUtil', required: true)
poco_json = cpp.find_library('PocoJSON', required: true)
poco_net = cpp.find_library('PocoNet', required: true)
poco_data = cpp.find_library('PocoData', required: false) 
poco_data_postgresql = cpp.find_library('PocoDataPostgreSQL', required: false)

devkit_dep = dependency('rgt-devkit',
  fallback: ['rgt-devkit', 'devkit_dep'],
  required: true
)

rgt_management_tests = executable(
  'rgt-management-tests',
  test_sources,
  dependencies: [gtest_dep, 
    gtest_main_dep, 
    poco_foundation, 
    poco_util, 
    poco_json, 
    poco_net, 
    poco_data, 
    poco_data_postgresql,
    devkit_dep],  
    include_directories: include_directories('../src/platform', '.'),
  install: false,
  link_args: [
    '-Wl,-rpath,@executable_path/../lib',
    '-Wl,-rpath,/opt/homebrew/lib',
    '-Wl,-rpath,/usr/local/lib',
  ],
  cpp_args: ['-DPROJECT_NAME="rgt-management"']
)

test(
  'rgt-management-tests',
  rgt_management_tests,
  args: [],
  should_fail: false,
  timeout: 30
)
