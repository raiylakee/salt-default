project('salt_default',
        'cpp',
        version: '0.0.1',
        default_options: ['cpp_std=c++26', 'warning_level=3'])

gtkmm_dep = dependency('gtkmm-4.0')

subdir('src')
subdir('data')
subdir('tests')

executable('salt_default',
           sources,
           dependencies: [gtkmm_dep],
           install: true,
           install_dir: get_option('bindir'),
            )
