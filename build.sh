#!/bin/bash
SCRIPT=$(realpath $0)
ORIG=$(dirname $SCRIPT)
DEST=$(realpath .)
cd $DEST
#apt-get source grub-pc
if [[ ! -d $DEST/grub ]]; then
    echo "Downloading grub source code..."
    git clone git://git.savannah.gnu.org/grub.git
    cd grub
    
    echo "Building..."
    cp $ORIG/*.c ./grub-core/commands/
    cp $ORIG/*.h ./include/grub/
    if [ "$(cat ./grub-core/Makefile.core.def | grep vmware | wc -l)" -eq "0" ]; then
        cat $ORIG/Makefile.core.def >> ./grub-core/Makefile.core.def
    fi

    ./bootstrap
    ./linguas.sh
    echo "es" > po/LINGUAS
    ./autogen.sh
    # http://www.linuxfromscratch.org/lfs/view/development/chapter06/grub.html
    ./configure --prefix=/usr --sbindir=/sbin --sysconfdir=/etc --disable-werror
    cd ..
fi

cd grub
cp $ORIG/*.c ./grub-core/commands/
cp $ORIG/*.h ./include/grub/
if [ "$(cat ./grub-core/Makefile.core.def | grep vmware | wc -l)" -eq "0" ]; then
    cat $ORIG/Makefile.core.def >> ./grub-core/Makefile.core.def
fi

make -j8 | tee ../build.log
ls --color $DEST/grub/grub-core/vmsnap.mod