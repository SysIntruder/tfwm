tfwm - Tiling Floating Window Manager
=============================

Minimal window manager with tiling and floating support. Designed as small and simple
as possible while fulfilling author's needs. Heavily inspired by "https://github.com/i3/i3"
and "https://git.suckless.org/dwm".

REQUIREMENTS
------------

xcb-util-keysyms, terminus-font

NOTES
-----

If terminus not listed in xlsfonts
    $ sudo pacman -S terminus-font
    $ cd /usr/share/fonts/misc
    $ sudo mkfontscale
    $ sudo mkfontdir
    $ xset +fp /usr/share/fonts/misc

DISCLAIMER
----------

tfwm ("Tiling Floating Window Manager") is a project created for
learning and personal use. The author is using "https://github.com/mcpcpc/xwm"
as an initial resource for learning.
