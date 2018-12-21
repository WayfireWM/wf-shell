widget_sources = ['widgets/battery.cpp',
                  'widgets/menu.cpp',
                  'widgets/clock.cpp',
                  'widgets/launchers.cpp',
                  'widgets/network.cpp',
                  'widgets/spacing.cpp']

executable('wf-panel', ['panel.cpp'] + widget_sources,
        dependencies: [gtkmm, wayland_client, libutil, wf_protos, wfconfig],
        install: true)
