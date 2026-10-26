# SPDX-License-Identifier: GPL-2.0-or-later
test_env = environment()
test_env.set('G_TEST_SRCDIR', meson.current_source_dir())
test_env.set('G_TEST_BUILDDIR', meson.current_build_dir())
test_env.set('G_DEBUG', 'gc-friendly,fatal-criticals')
test_env.set('MALLOC_CHECK_', '2')
test_env.set('LC_ALL', 'C.UTF-8')
test_env.set('GIO_USE_VFS', 'local')
test_env.set('GSETTINGS_BACKEND', 'memory')

foreach t : ['model', 'config', 'desktop', 'coexist', 'launch']
  exe = executable('test-' + t, 'test-@0@.c'.format(t), dependencies: core_dep)
  test(t, exe, env: test_env, protocol: 'tap', args: ['--tap'])
endforeach
