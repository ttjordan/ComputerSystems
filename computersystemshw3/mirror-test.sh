#!/bin/sh
# unique disk names
for i in 1 2 3 4; do
    disks="$disks /tmp/disk$i.img"
done

# use 'dd' to create 4 disk images, each with 256 512-byte blocks
for d in $disks; do
    # note that 2<&- suppresses dd info messages
    dd if=/dev/zero bs=512 count=256 of=$d 2<&- 
done

# run the test and clean up

./mirror-test $disks
rm -f $disks