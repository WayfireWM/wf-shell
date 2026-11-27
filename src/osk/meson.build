sources = [
        'osk.cpp',
        'wayland-window.cpp',
        'virtual-keyboard.cpp',
        'layout.cpp',
        'display.cpp',
]

if enchant.found()
        sources += 'complete/complete-enchant.cpp'
endif

if llama.found()
        sources += 'complete/complete-tiny.cpp'
endif

executable(
        'wf-osk',
        sources,
        dependencies: [
                gtkmm,
                wf_protos,
                gtklayershell,
                xkb,
                xkbregistry,
                json,
                wayfire,
                libutil,
                enchant,
                llama,
        ],
        install: true,
)