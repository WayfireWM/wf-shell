widget_sources = ['widgets/battery.cpp',
                  'widgets/clock.cpp',
                  'widgets/launchers.cpp']

executable('wf-panel', ['panel.cpp'] + widget_sources,
        dependencies: [gtkmm, wayland_client, libutil, wf_protos, wfconfig],
        install: true)
