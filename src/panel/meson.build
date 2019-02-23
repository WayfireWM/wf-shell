widget_sources = ['widgets/battery.cpp',
                  'widgets/menu.cpp',
                  'widgets/clock.cpp',
                  'widgets/launchers.cpp',
                  'widgets/network.cpp',
                  'widgets/spacing.cpp',
                  'widgets/wf-popover.cpp',
                  'widgets/window-list.cpp',
                  'widgets/toplevel.cpp',
                  'widgets/toplevel-icon.cpp']

executable('wf-panel', ['panel.cpp'] + widget_sources,
        dependencies: [gtkmm, wayland_client, libutil, wf_protos, wfconfig],
        install: true)
