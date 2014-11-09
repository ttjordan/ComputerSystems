#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "blkdev.h"

/* example main() function that takes several disk image filenames as
 * arguments on the command line.
 * Note that you'll need three images to test recovery after a failure.
 */
int main(int argc, char **argv)
{
    printf("start\n");
    /* create the underlying blkdevs from the images
     */

    struct blkdev *d1 = image_create(argv[1]);
    struct blkdev *d2 = image_create(argv[2]);
    /* ... */
    printf("created the underlying blkdevs from the images\n");

    /* create the mirror
     */
    struct blkdev *disks[] = {d1, d2};
    struct blkdev *mirror = mirror_create(disks);
    printf("created the mirror\n");
    /* two asserts - that the mirror create worked, and that the size
     * is correct. 
    */
    assert(mirror != NULL);
    printf("mirror create worked\n");

    assert(mirror->ops->num_blocks(mirror) == d1->ops->num_blocks(d1));
    printf("size is correct\n");

    /* put your test code here. Errors should exit via either 'assert'
     * or 'exit(1)' - that way the shell script knows that the test failed.
     */

    /* suggested test codes (from the assignment PDF)
         You should test that your code:
          - creates a volume properly
          - returns the correct length
          - can handle reads and writes of different sizes, and
            returns the same data as was written 
          - reads data from the proper location in the images, and
            doesn't overwrite incorrect locations on write. 
          - continues to read and write correctly after one of the
            disks fails. (call image_fail() on the image blkdev to
            force it to fail) 
          - continues to read and write (correctly returning data
            written before the failure) after the disk is replaced. 
          - reads and writes (returning data written before the first
            failure) after the other disk fails
    */

    char src[BLOCK_SIZE * 16];
    FILE *fp = fopen("/dev/urandom", "r");
    assert(fread(src, sizeof(src), 1, fp) == 1);
    fclose(fp);
    printf("5\n");

    printf("first test\n");
    int i, result;
    for (i = 0; i < 128; i += 16) {
        result = mirror->ops->write(mirror, i, 16, src);
        assert(result == SUCCESS);
    }

    printf("second test\n");
    char dst[BLOCK_SIZE * 16];
    for (i = 0; i < 128; i += 16) {
        result = mirror->ops->read(mirror, i, 16, dst);
        assert(result == SUCCESS);
        assert(memcmp(src, dst, 16*BLOCK_SIZE) == 0);
    }
    printf("third test\n");
    image_fail(d1);
    for (i = 0; i < 128; i += 16) {
        result = mirror->ops->read(mirror, i, 16, dst);
        assert(result == SUCCESS);
        assert(memcmp(src, dst, 16*BLOCK_SIZE) == 0);
    }
    printf("fourth test\n");
    struct blkdev *d3 = image_create(argv[3]);
    assert(mirror_replace(mirror, 0, d3) == SUCCESS);
    for (i = 0; i < 128; i += 16) {
        result = mirror->ops->read(mirror, i, 16, dst);
        assert(result == SUCCESS);
        assert(memcmp(src, dst, 16*BLOCK_SIZE) == 0);
    }

  
    printf("Mirror Test: SUCCESS\n");
    return 0;
}
