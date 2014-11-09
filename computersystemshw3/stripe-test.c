#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "blkdev.h"

int main(int argc, char **argv)
{
    /* This code reads arguments <stripesize> d1 d2 d3 ...
     * Note that you don't need any extra images, as there's no way to
     * repair a striped volume after failure.
     */
    struct blkdev *disks[10];
    int i, ndisks, stripesize = atoi(argv[1]);


    for (i = 2, ndisks = 0; i < argc; i++)
        disks[ndisks++] = image_create(argv[i]);


    /* and creates a striped volume with the specified stripe size
     */

    struct blkdev *striped = striped_create(ndisks, disks, stripesize);
    assert(striped != NULL);

    /* your tests here */

    /* from pdf:
       - reports the correct size
       - reads data from the right disks and locations. (prepare disks
         with known data at various locations and make sure you can read it back) 
       - overwrites the correct locations. i.e. write to your prepared disks
         and check the results (using something other than your stripe
         code) to check that the right sections got modified. 
       - fail a disk and verify that the volume fails.
       - large (> 1 stripe set), small, unaligned reads and writes
         (i.e. starting, ending in the middle of a stripe), as well as small
         writes wrapping around the end of a stripe. 
       - Passes all your other tests with different stripe sizes (e.g. 2,
         4, 7, and 32 sectors) and different numbers of disks.  
     */
    printf("0\n");
    int nblks = disks[0]->ops->num_blocks(disks[0]);
    nblks = nblks - (nblks % stripesize);
    assert(striped->ops->num_blocks(striped) == ndisks*nblks);
    int one_chunk = stripesize * BLOCK_SIZE;
    char *buf = malloc(ndisks * one_chunk);
    for (i = 0; i < ndisks; i++) {
      memset(buf + i * one_chunk, 'A'+i, one_chunk);
    }
    printf("1\n");
    int result, j;
    for (i = 0; i < 16; i++) {
      result = striped->ops->write(striped, i*ndisks*stripesize, ndisks*stripesize, buf);
      assert(result == SUCCESS);
    }
    char *buf2 = malloc(ndisks * one_chunk);
    printf("2\n");
    for (i = 0; i < 16; i++) {
      result = striped->ops->read(striped, i*ndisks*stripesize, ndisks*stripesize, buf2);
      
      assert(result == SUCCESS);
      assert(memcmp(buf, buf2, ndisks * one_chunk) == 0);
    }
    printf("3\n");

    for (i = 0; i < ndisks; i++) {
      result = disks[i]->ops->read(disks[i], i*stripesize, stripesize, buf2);
      assert(result == SUCCESS);
      assert(memcmp(buf + i*one_chunk, buf2, one_chunk) == 0);
    }
    printf("4\n");
    for (i = 0; i < ndisks; i++) {
      memset(buf + i * one_chunk, 'a'+i, one_chunk);
    }
    printf("5\n");
    for (i = 0; i < 8; i++) {
      for (j = 0; j < ndisks*stripesize; j ++) {
        result = striped->ops->write(striped, i*ndisks*stripesize + j, 1, buf + j*BLOCK_SIZE);
        assert(result == SUCCESS);
      }
    }
    printf("6\n");
    for (i = 0; i < 8; i++) {
      result = striped->ops->read(striped, i*ndisks*stripesize, ndisks*stripesize, buf2);
      assert(result == SUCCESS);
      assert(memcmp(buf, buf2, ndisks * one_chunk) == 0);
    }
    printf("7\n");
 
    //Failing a disk
    image_fail(disks[0]); 
    assert(striped->ops->read(striped, 0, 2, buf) == E_UNAVAIL);

    
    printf("Striping Test: SUCCESS\n");
    return 0;
}
