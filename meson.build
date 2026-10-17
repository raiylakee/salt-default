project('salt_default',
        'cpp',
        version: '0.0.1',
        default_options: ['cpp_std=c++26'])

gtkmm_dep = dependency('gtkmm-4.0')

subdir('src')

executable('salt_default',
           sources,
           dependencies: [gtkmm_dep]
            )
