# Backend only: no GTK widgets, so the test binary can link it without a
# window.
backend_sources = files(
    'mime_service.cpp',
    'mime_service.h',
    'log.h',
    'app_id.h',
)

sources = files(
    'main.cpp',
    'app_window.cpp',
    'rows.cpp',
)

sources += backend_sources
