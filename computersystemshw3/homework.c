/*
 * file:        homework.c
 * description: skeleton code for CS 5600 Homework 3
 *
 * Peter Desnoyers, Northeastern Computer Science, 2011
 * $Id: homework.c 410 2011-11-07 18:42:45Z pjd $
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "blkdev.h"

#define MIRROR_SIZE 2   /* mirror size of 2*/ 

/********** MIRRORING ***************/

/* example state for mirror device. See mirror_create for how to
 * initialize a struct blkdev with this.
 */
struct mirror_dev {
    struct blkdev *disks[2];    /* flag bad disk by setting to NULL */
    int nblks;
};
    
static int mirror_num_blocks(struct blkdev *dev)
{
    /* your code here */
    return ((struct mirror_dev *)dev->private)->nblks;
}

/* read from one of the sides of the mirror. (if one side has failed,
 * it had better be the other one...) If both sides have failed,
 * return an error.
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should close the
 * device and flag it (e.g. as a null pointer) so you won't try to use
 * it again. 
 */
static int mirror_read(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf) {
    /* your code here */
    struct mirror_dev *mdev = ((struct mirror_dev *)dev->private);

    int result = E_UNAVAIL;
    struct blkdev *d0 = mdev->disks[0];
    struct blkdev *d1 = mdev->disks[1];
    if(d0 != NULL) {
        //attempt to read first disk
        result = d0->ops->read(d0, first_blk, num_blks, buf);
        if(result == SUCCESS ) {
            return result;
        } else if (result == E_UNAVAIL) {
            //close the device and flag it
            d0->ops->close(d0);
            ((struct mirror_dev *)dev->private)->disks[0] = NULL;
        }
    }
    if(d1 != NULL) {
        // attempt to read a second disk
        result = d1->ops->read(d1, first_blk, num_blks, buf);
        if(result == SUCCESS ) {
            return result;
        } else if (result == E_UNAVAIL) {
            //close the device and flag it
            d1->ops->close(d1);
            ((struct mirror_dev *)dev->private)->disks[1] = NULL;
        }
    }
    return result;
}

/* write to both sides of the mirror, or the remaining side if one has
 * failed. If both sides have failed, return an error.
 * Note that a write operation may indicate that the underlying device
 * has failed, in which case you should close the device and flag it
 * (e.g. as a null pointer) so you won't try to use it again.
 */
static int mirror_write(struct blkdev * dev, int first_blk, int num_blks, void *buf) {
    /* your code here */
    struct mirror_dev *mdev = ((struct mirror_dev *)dev->private);

    int result_first_write, result_second_write, error_result = E_UNAVAIL;

    struct blkdev *d0 = mdev->disks[0];
    struct blkdev *d1 = mdev->disks[1];

    //attempt to write to first disk
    if(d0 != NULL) {
        result_first_write = d0->ops->write(d0, first_blk, num_blks, buf);
        if(result_first_write != SUCCESS) {
            error_result = result_first_write;
            if(error_result == E_UNAVAIL) {
                d0->ops->close(d0);
                ((struct mirror_dev *)dev->private)->disks[0] = NULL;
            }
        }
    }
    //attempt to write to second disk
    if(d1 != NULL) {
        result_second_write = d1->ops->write(d1, first_blk, num_blks, buf);
        if(result_second_write != SUCCESS) {
            error_result = result_second_write;
            if(error_result == E_UNAVAIL) {
                d1->ops->close(d1);
                ((struct mirror_dev *)dev->private)->disks[1] = NULL;
            }
        }
    }
    //if both writes failed, return error
    if( (result_first_write != SUCCESS) && (result_second_write !=SUCCESS) ) {
        return error_result;
    }

    //SUCCESS
    return SUCCESS;
}

/* clean up, including: close any open (i.e. non-failed) devices, and
 * free any data structures you allocated in mirror_create.
 */
static void mirror_close(struct blkdev *dev)
{
    /* your code here */
    struct mirror_dev *mdev = (struct mirror_dev *)dev->private;
    struct blkdev *d0 = mdev->disks[0];
    struct blkdev *d1 = mdev->disks[1];

    d0->ops->close(d0);
    d1->ops->close(d1);

    free(mdev);
    free(dev);
}

struct blkdev_ops mirror_ops = {
    .num_blocks = mirror_num_blocks,
    .read = mirror_read,
    .write = mirror_write,
    .close = mirror_close
};

/* create a mirrored volume from two disks. Do not write to the disks
 * in this function - you should assume that they contain identical
 * contents. 
 */
struct blkdev *mirror_create(struct blkdev *disks[2])
{ 
    struct blkdev *dev = malloc(sizeof(*dev));
    struct mirror_dev *mdev = malloc(sizeof(*mdev));

    struct blkdev *d0 = disks[0];
    struct blkdev *d1 = disks[1];

    //verify that the two disks are of the same size

    if(d0->ops->num_blocks(d0) != d1->ops->num_blocks(d1)) {
        printf("Disks are of different sizes, mirror creation failed!");
        return NULL;
    }

    /* your code here */
    mdev->nblks = d0->ops->num_blocks(d0);
    mdev->disks[0] = d0;
    mdev->disks[1] = d1;
    
    dev->private = mdev;
    dev->ops = &mirror_ops;

    return dev;
}

/* replace failed device 'i' (0 or 1) in a mirror. Note that we assume
 * the upper layer knows which device failed. You will need to
 * replicate content from the other underlying device before returning
 * from this call.
 */
int mirror_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{

    struct mirror_dev *mdev = (struct mirror_dev *)volume->private;

    struct blkdev *dsk = mdev->disks[(i + 1) % MIRROR_SIZE];

    //check if old disk and new disk are of the same size
    if(dsk->ops->num_blocks(dsk) != newdisk->ops->num_blocks(newdisk)) {
        return E_SIZE;
    }

    mdev->disks[i] = newdisk;

    int readlen = BLOCK_SIZE * mdev->nblks;
    char dst[readlen];

    int rwresult = E_UNAVAIL;
    rwresult =  mdev->disks[(i + 1) % MIRROR_SIZE]->ops->read( mdev->disks[(i + 1) % MIRROR_SIZE], 
        0, mdev->nblks, dst);

    if(rwresult == E_UNAVAIL) {
        return rwresult;
    }
    rwresult = mdev->disks[i]->ops->write(mdev->disks[i],0, mdev->nblks, dst);
    if(rwresult == E_UNAVAIL) {
        return rwresult;
    }

    return SUCCESS;
}

/**********  STRIPING ***************/


struct stripe_dev {
     struct blkdev **disks; /*flag bad disk by setting to NULL*/
     int nblks; 
     int num_disks;
     int unit;
};


int stripe_num_blocks(struct blkdev *dev) {
    return ((struct stripe_dev *)dev->private)->nblks;
}


int min(int a, int b) {
    return(a < b) ? a : b;
}

/* read blocks from a striped volume. 
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should (a) close the
 * device and (b) return an error on this and all subsequent read or
 * write operations. 
 */
static int stripe_read(struct blkdev * dev, int first_blk, int num_blks, void *buf) {

    struct stripe_dev *sdev = ((struct stripe_dev *)dev->private);

    int num_disks = sdev->num_disks;

    int stripe_num_in_volume = first_blk / sdev->unit;
    int offset_in_stripe = first_blk % sdev->unit;
    int disk_number = stripe_num_in_volume % num_disks;
    int stripe_num_in_disk = stripe_num_in_volume / num_disks;

    int start_block = stripe_num_in_disk*sdev->unit + offset_in_stripe;

    int read_result = E_UNAVAIL;

    while(num_blks > 0) {
        if(sdev->disks[disk_number] != NULL) {
            //determine how many blocks to read
            int num_blks_to_read = min(num_blks, sdev->unit);
            read_result = sdev->disks[disk_number]->ops->read(sdev->disks[disk_number], start_block, num_blks_to_read, buf);
  

            if(read_result == SUCCESS) {
                // advance buffer by number of bytes read
                buf = buf + num_blks_to_read * BLOCK_SIZE;
                // update how many blocks to read are left
                num_blks = num_blks - num_blks_to_read;
                disk_number++;
                //if no more disks, go back to first disk and update start block to new level of stripe
                if(disk_number == num_disks) {
                    disk_number = 0;
                    start_block = start_block + num_blks_to_read;
                }
            } else {
                if(read_result == E_UNAVAIL) {
                    sdev->disks[disk_number]->ops->close(sdev->disks[disk_number]);
                    ((struct stripe_dev *)dev->private)->disks[disk_number] = NULL;
                    return E_UNAVAIL;
                }
                break;
            }
        } else {
            printf("disk is null\n");
            break;
        }
    }

    if(num_blks == 0) {
        return SUCCESS;
    } else {
        return E_UNAVAIL;
    }
}


/* write blocks to a striped volume.
 * Again if an underlying device fails you should close it and return
 * an error for this and all subsequent read or write operations.
 */
static int stripe_write(struct blkdev * dev, int first_blk,
                        int num_blks, void *buf) {

    struct stripe_dev *sdev = ((struct stripe_dev *)dev->private);

    int num_disks = sdev->num_disks;

    int stripe_num_in_volume = first_blk / sdev->unit;
    int offset_in_stripe = first_blk % sdev->unit;
    int disk_number = stripe_num_in_volume % num_disks;
    int stripe_num_in_disk = stripe_num_in_volume / num_disks;

    int start_block = stripe_num_in_disk*sdev->unit + offset_in_stripe;

    int write_result = E_UNAVAIL;

    while(num_blks > 0) {
        if(sdev->disks[disk_number] != NULL) {
            //determine how many blocks to write
            int num_blks_to_write = min(num_blks, sdev->unit);

            write_result = sdev->disks[disk_number]->ops->write(sdev->disks[disk_number], 
                start_block, num_blks_to_write, buf);

            if(write_result == SUCCESS) {
                // move buffer pointer by the number of bytes that has been written
                buf = buf + num_blks_to_write * BLOCK_SIZE;
                // update how many blocks are left to write
                num_blks = num_blks - num_blks_to_write;
                disk_number++;
                //if no more disks, go back to first disk and update start block to new level of stripe
                if(disk_number == num_disks) {
                    disk_number = 0;
                    start_block = start_block + num_blks_to_write;
                }
            } else {
                if(write_result == E_UNAVAIL) {
                    sdev->disks[disk_number]->ops->close(sdev->disks[disk_number]);
                    ((struct stripe_dev *)dev->private)->disks[disk_number] = NULL;
                    return E_UNAVAIL;
                }
                break;
            }
        } else {
            break;
        }
    }

    if(num_blks == 0) {
        return SUCCESS;
    } else {
        return E_UNAVAIL;
    }
}
/* clean up, including: c

*/

/* clean up, including: close all devices and free any data structures
 * you allocated in stripe_create. 
 */
static void stripe_close(struct blkdev *dev) {

    /* your code here */
    struct stripe_dev *sdev = (struct stripe_dev *)dev->private;

    struct blkdev **disks = sdev->disks;
    int num_disks = sdev->num_disks;

    int x = 0;
    for ( ; x < num_disks; x++) {
        disks[x]->ops->close(disks[x]);
    }

    free(sdev);
    free(dev);
}

struct blkdev_ops stripe_ops = {
    .num_blocks = stripe_num_blocks,
    .read = stripe_read,
    .write = stripe_write,
    .close = stripe_close
};

/* create a striped volume across N disks, with a stripe size of
 * 'unit'. (i.e. if 'unit' is 4, then blocks 0..3 will be on disks[0],
 * 4..7 on disks[1], etc.)
 * Check the size of the disks to compute the final volume size, and
 * fail (return NULL) if they aren't all the same.
 * Do not write to the disks in this function.
 */
struct blkdev *striped_create(int N, struct blkdev *disks[], int unit)
{
    struct blkdev *dev = malloc(sizeof(*dev));
    struct stripe_dev *sdev = malloc(sizeof(*sdev));
    //verify that all disks have the same size
    int disk_size = disks[0]->ops->num_blocks(disks[0]);
    int x = 1;
    for ( ; x < N; x++) {
        if(disk_size != disks[x]->ops->num_blocks(disks[x])) {
            return NULL;
        }
    }
    sdev->nblks = disk_size*N - (disk_size*N % unit);
    sdev->disks = disks;
    sdev->num_disks = N;
    sdev->unit = unit;
    
    dev->private = sdev;
    dev->ops = &stripe_ops;

    return dev;
}


/**********   RAID 4  ***************/

/* helper function - compute parity function across two blocks of
 * 'len' bytes and put it in a third block. Note that 'dst' can be the
 * same as either 'src1' or 'src2', so to compute parity across N
 * blocks you can do: 
 *
 *     void **block[i] - array of pointers to blocks
 *     dst = <zeros[len]>
 *     for (i = 0; i < N; i++)
 *        parity(block[i], dst, dst);
 *
 * Yes, it's slow.
 */
void parity (int len, unsigned char *src1, unsigned char *src2, unsigned char *dst)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i;
    for (i = 0; i < len; i++)
        d[i] = s1[i] ^ s2[i];
}


/* read blocks from a RAID 4 volume.
 * If the volume is in a degraded state you may need to reconstruct
 * data from the other stripes of the stripe set plus parity.
 * If a drive fails during a read and all other drives are
 * operational, close that drive and continue in degraded state.
 * If a drive fails and the volume is already in a degraded state,
 * close the drive and return an error.
 */
static int raid4_read(struct blkdev * dev, int first_blk,
                      int num_blks, void *buf)  {
    struct raid4_dev *r4dev = ((struct raid4_dev *)dev->private);

    int num_disks = r4dev->num_disks;

    int stripe_num_in_volume = first_blk / r4dev->unit;
    int offset_in_stripe = first_blk % r4dev->unit;
    int disk_number = stripe_num_in_volume % num_disks;
    int stripe_num_in_disk = stripe_num_in_volume / num_disks;

    int start_block = stripe_num_in_disk*r4dev->unit + offset_in_stripe;

    int read_result = E_UNAVAIL;

    while(num_blks > 0) {
        if(r4dev->disks[disk_number] != NULL) {
            //determine how many blocks to read
            int num_blks_to_read = min(num_blks, r4dev->unit);
            read_result = r4dev->disks[disk_number]->ops->read(r4dev->disks[disk_number], start_block, num_blks_to_read, buf);
  
            if(read_result == SUCCESS) {
                // advance buffer by number of bytes read
                buf = buf + num_blks_to_read * BLOCK_SIZE;
                // update how many blocks to read are left
                num_blks = num_blks - num_blks_to_read;
                disk_number++;
                //if no more disks, go back to first disk and update start block to new level of stripe
                if(disk_number == num_disks) {
                    disk_number = 0;
                    start_block = start_block + num_blks_to_read;
                }
            } else {
                if(read_result == E_UNAVAIL) {
                    if(r4dev->degraded_state == true) {
                        r4dev->disks[disk_number]->ops->close(r4dev->disks[disk_number]);
                        ((struct stripe_dev *)dev->private)->disks[disk_number] = NULL;
                        return E_UNAVAIL;
                    }
                } else {
                    ((struct raid4_dev *)dev->private)->degraded_state = true;
                    ((struct raid4_dev *)dev->private)->degraded_disk_index = disk_number;
                    //recover data
                    !!!
                }
            }
        } else { //if some disk is null then drive must be degraded
            if(r4dev->degraded_disk_index == disk_number) {
                //recover data
                //void **block[i] - array of pointers to blocks
                int disk_size = disks[0]->ops->num_blocks(disks[0]);
                int x = 0;
                char *recovery_blocks = malloc(N * r4dev->unit * BLOCK_SIZE);
                int error_result = E_UNAVAIL;
                int num_blocks_written = 0;
                for ( ; x < N; x++) {
                    if(x == disk_number) {
                        continue;
                    }
                    error_result = r4dev->disks[disk_number]->ops->read(r4dev->disks[x], 
                        start_block, r4dev->unit, recovery_blocks + num_blocks_written* r4dev->unit * BLOCK_SIZE);
                    num_blocks_written = num_blocks_written + r4dev->unit;
                    if(error_result != SUCCESS) {
                        // more than one disk failed
                        return E_UNAVAIL;
                    }
                }

                char dst[BLOCK_SIZE] = {0};
                for (i = 0; i < N; i++) {
                    parity(r4dev->unit*BLOCK_SIZE, recovery_blocks[i], dst, dst);
                }

            } else { //more than one disks are failed
                return E_UNAVAIL;
            }


            //recover data

        }
    }

    if(num_blks == 0) {
        return SUCCESS;
    } else {
        return E_UNAVAIL;
    }
}

/* write blocks to a RAID 4 volume.
 * Note that you must handle short writes - i.e. less than a full
 * stripe set. You may either use the optimized algorithm (for N>3
 * read old data, parity, write new data, new parity) or you can read
 * the entire stripe set, modify it, and re-write it. Your code will
 * be graded on correctness, not speed.
 * If an underlying device fails you should close it and complete the
 * write in the degraded state. If a drive fails in the degraded
 * state, close it and return an error.
 * In the degraded state perform all writes to non-failed drives, and
 * forget about the failed one. (parity will handle it)
 */
static int raid4_write(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf) {
    return 0;
}

/* clean up, including: close all devices and free any data structures
 * you allocated in raid4_create. 
 */
static void raid4_close(struct blkdev *dev) {
    /* your code here */
    struct raid4_dev *r4dev = (struct raid4_dev *)dev->private;

    struct blkdev **disks = r4dev->disks;
    int num_disks = r4dev->num_disks;

    int x = 0;
    for ( ; x < num_disks; x++) {
        disks[x]->ops->close(disks[x]);
    }

    free(r4dev);
    free(dev);
}

/* Initialize a RAID 4 volume with stripe size 'unit', using
 * disks[N-1] as the parity drive. Do not write to the disks - assume
 * that they are properly initialized with correct parity. (warning -
 * some of the grading scripts may fail if you modify data on the
 * drives in this function)
 */
struct blkdev *raid4_create(int N, struct blkdev *disks[], int unit) {
    struct blkdev *dev = malloc(sizeof(*dev));
    struct raid4_dev *r4dev = malloc(sizeof(*r4dev));

    //verify that all disks have the same size
    int disk_size = disks[0]->ops->num_blocks(disks[0]);
    int x = 1;
    for ( ; x < N; x++) {
        if(disk_size != disks[x]->ops->num_blocks(disks[x])) {
            return NULL;
        }
    }

    r4dev->nblks = disk_size*N - (disk_size*N % unit);
    r4dev->disks = disks;
    r4dev->num_disks = N;
    r4dev->unit = unit;
    r4dev->degraded_state = false;
    r4dev->degraded_disk_index = -1;//no degraded disks
    
    dev->private = r4dev;
    dev->ops = &raid4_ops;

    return dev;
}

/* replace failed device 'i' in a RAID 4. Note that we assume
 * the upper layer knows which device failed. You will need to
 * reconstruct content from data and parity before returning
 * from this call.
 */
int raid4_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    return SUCCESS;
}

struct raid4_dev {
     struct blkdev **disks; /*flag bad disk by setting to NULL*/
     int nblks; 
     int num_disks;
     int unit;
     bool degraded_state;
     int degraded_disk_index;
};

struct raid4_ops raid4_ops = {
    .num_blocks = stripe_num_blocks,
    .read = raid4_read,
    .write = raid4_write,
    .close = raid4_close
};