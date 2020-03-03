#!/bin/bash
SCRIPT=$(realpath $0)
ORIG=$(dirname $SCRIPT)
DEST=$(realpath .)

if [ -f "$DEST/grub/grub-core/vmsnap.mod" ]; then
    echo "Copying to /boot/grub/i386-pc"
    cp $DEST/grub/grub-core/vmsnap.mod /boot/grub/i386-pc/
    if [ "$(cat /boot/grub/i386-pc/command.lst | grep vmsnap | wc -l)" -eq "0" ]; then
        echo "*vmsnap: vmsnap" >> /boot/grub/i386-pc/command.lst
    fi
    if [ -d "/usr/lib/grub/i386-pc" ]; then
	echo "Copying to /usr/lib/grub/i386-pc"
        cp $DEST/grub/grub-core/vmsnap.mod /usr/lib/grub/i386-pc/
        if [ "$(cat /usr/lib/grub/i386-pc/command.lst | grep vmsnap | wc -l)" -eq "0" ]; then
            echo "*vmsnap: vmsnap" >> /usr/lib/grub/i386-pc/command.lst
        fi
    fi
    if [ "$(cat /etc/grub.d/40_custom | grep vmsnap | wc -l)" -eq "0" ]; then
        echo -e "\ninsmod vmsnap" >> /etc/grub.d/40_custom
    fi
    update-grub
    echo "Module installed."
else
    echo "You need to build the module first. Run the script build.sh."
fi
