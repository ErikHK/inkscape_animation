Quick instructions:
===================

1) install MacPorts from source into $MP_PREFIX (e.g. "/opt/local-x11")
   <https://www.macports.org/install.php>

2) add MacPorts to your PATH environement variable, for example:

$ export PATH="$MP_PREFIX/bin:$MP_PREFIX/sbin:$PATH"

3) add 'ports/' subdirectory as local portfile repository:

$ sudo sed -e '/^rsync:/i\'$'\n'"file://$(pwd)/ports" -i "" "$MP_PREFIX/etc/macports/sources.conf"

4) index the new local portfile repository: 

$ (cd ports && portindex)

5) add default variants for x11-based package to MacPorts' global variants:

$ sudo sed -e '$a\'$'\n''+x11 -quartz -no_x11 +rsvg +Pillow -tkinter +gnome_vfs' -i "" "$MP_PREFIX/etc/macports/variants.conf"

6) optional: to force an i386 build on x86_64 machine:

$ sudo sed -e '/^#build_arch/i\'$'\nbuild_arch i386' -i "" "$MP_PREFIX/etc/macports/macports.conf"

7) install required dependencies: 

$ sudo port install inkscape-packaging

8) compile inkscape, create app bundle and DMG (if building for i386 on x86_64 machine, add at the beginning ARCH="i386"):

$ LIBPREFIX="$MP_PREFIX" ./osx-build.sh a c b -j 5 i p -s d

9) upload the DMG.
