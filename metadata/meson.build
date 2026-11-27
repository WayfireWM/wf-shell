configure_file(input: 'background.xml.in', output: 'background.xml',
    configuration: {
        'wallpaper': join_paths(resource_dir, 'backgrounds')
        },
    install: true,
    install_dir: metadata_dir)

install_data('osk.xml', install_dir: metadata_dir)
install_data('dock.xml', install_dir: metadata_dir)
install_data('panel.xml', install_dir: metadata_dir)
install_data('locker.xml', install_dir: metadata_dir)
