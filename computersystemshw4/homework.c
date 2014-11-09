/*
 * file:        homework.c
 * description: skeleton file for CS 5600 homework 4
 *
 * CS 5600, Computer Systems, Northeastern CCIS
 * Peter Desnoyers, updated April 2012
 * $Id: homework.c 452 2011-11-28 22:25:31Z pjd $
 */




#define FUSE_USE_VERSION 27
#define FS_DS_RATIO 2
#define FS_SECTOR_SIZE (FS_DS_RATIO * SECTOR_SIZE)  
#define DIRENT_MAX_LENGTH 43
#define MAX_DIR_ENTRIES 16

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "cs5600fs.h"
#include "blkdev.h"

/* 
 * disk access - the global variable 'disk' points to a blkdev
 * structure which has been initialized to access the image file.
 *
 * NOTE - blkdev access is in terms of 512-byte SECTORS, while the
 * file system uses 1024-byte BLOCKS. Remember to multiply everything
 * by 2.
 */
 //file system to disk sector ratio - here is it 2 due to description above


extern struct blkdev *disk;

struct cs5600fs_super superblock;
struct cs5600fs_entry *fat_table;
struct cs5600fs_dirent dirent;

struct cs5600fs_dirent directory[MAX_DIR_ENTRIES];
struct cs5600fs_dirent dirent;

//////////////////////////////////////

/* find the next word starting at 's', delimited by characters
 * in the string 'delim', and store up to 'len' bytes into *buf
 * returns pointer to immediately after the word, or NULL if done.
 */
char *strwrd(char *s, char *buf, size_t len, char *delim)
{
    s += strspn(s, delim);
    int n = strcspn(s, delim);  /* count the span (spn) of bytes in */
    if (len-1 < n)              /* the complement (c) of *delim */
        n = len-1;
    memcpy(buf, s, n);
    buf[n] = 0;
    s += n;
    return (*s == 0) ? NULL : s;
}
 
 /*
  * Parses path into directory entries
  * Returns number of directory entries
  * 
  */
int parsepath(char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH], char * path) {
    char* entry = path;

    int direntries_counter = 0;
    for(;direntries_counter < MAX_DIR_ENTRIES; direntries_counter++) {
        entry = strwrd( entry, direntries[direntries_counter], DIRENT_MAX_LENGTH, "/");
        if(entry == NULL) {
            break;
        }
    }
    return direntries_counter;
 }

 /*
  * Loads directory from disk
  * Returns error or position in directory of the direntry
  * direntry parent is where direntry is located
  */
int load_directory(int num_of_direntries, 
    char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH], int* direntry_parent) {

    int directory_start = superblock.root_dirent.start;
    int i = 0, j = 0;
    int previous = superblock.root_dirent.start;

    for(; i <= num_of_direntries; i++) {
        //read the directory to memory
        disk->ops->read(disk, directory_start * 2, 2, (void *) directory);
        for(j = 0; j < MAX_DIR_ENTRIES; j++) {
            // make sure the path is correct - dir entry exists on disk
            if(strcmp(direntries[i], directory[j].name) == 0) {
                // make sure it is not a file in the middle of the path
                if(directory[j].isDir != 1 && i < num_of_direntries) {
                    return -ENOTDIR;
                }
                previous = directory_start;
                directory_start = directory[j].start;
                // copy last direntry
                memcpy(&dirent, &directory[j], sizeof(dirent));
                break;
            }
        }

        // directory entry was not found
        if(j == MAX_DIR_ENTRIES) {
            return -ENOENT;
        } 
    }
    *direntry_parent = previous;

    return j;
}

void set_stats(struct stat *sb, struct cs5600fs_dirent dirent) {
    sb->st_dev     = 0;
    sb->st_ino     = 0;
    sb->st_rdev    = 0;
    sb->st_nlink   = 1;
    sb->st_uid     = dirent.uid;
    sb->st_gid     = dirent.gid;
    sb->st_mode    = dirent.mode | (dirent.isDir ? S_IFDIR : S_IFREG);
    sb->st_size    = dirent.length;
    sb->st_mtime   = dirent.mtime;
    sb->st_atime   = dirent.mtime;
    sb->st_ctime   = dirent.mtime;
    sb->st_blocks  = (dirent.isDir ? 0 : (sb->st_size + FS_SECTOR_SIZE - 1) / (FS_SECTOR_SIZE)) ;
    sb->st_blksize = FS_SECTOR_SIZE;
 }


//returns index of first unused block in fat
// if fat is full returns negative number
int find_unused_block_in_fat() {
    int free_block_idx = 0;
    for(; free_block_idx < superblock.fs_size; free_block_idx++) {
        if(fat_table[free_block_idx].inUse == 0) {
            fat_table[free_block_idx].inUse = 1;
            fat_table[free_block_idx].eof = 1;
            return free_block_idx;
        }
    }
    return -ENOMEM;
}



int create_direntry(const char *path, mode_t mode, struct fuse_file_info* fi,
 unsigned short isDir) {
    //cant create root dir
    if(strcmp("/", path) == 0) {
        return -EEXIST;
    }
    char new_dir_name[DIRENT_MAX_LENGTH + 1];
    //extract name of the new directory
    char * last_slash_location = strrchr(path, '/');
    strcpy(new_dir_name, last_slash_location + 1);
    int read_start = 0;

    char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];

    int num_of_direntries = parsepath( direntries, (char *) path);
    int dummy = 0;
    int result = load_directory(num_of_direntries, direntries, &dummy);
        
    // check if dir already exists
    if(result >= 0) {
        return result;
    }

    if(last_slash_location == path) {
        // creating dir in root directory
        read_start = superblock.root_dirent.start;
    } else {
        if(dirent.isDir != 1) {
            return -ENOTDIR;
        }
        read_start = dirent.start;
    }
    disk->ops->read(disk, read_start * 2, 2, (void *) directory);


    // find first invalid unused direntry for new directory
    int new_dir_idx = 0;
    for(; new_dir_idx< MAX_DIR_ENTRIES; new_dir_idx++) {
        if(directory[new_dir_idx].valid == 0) {
            break;
        }
    }
    if(new_dir_idx == MAX_DIR_ENTRIES) {
        return -ENOSPC;
    }
    // find first unused block in fat table
    int free_block_idx = find_unused_block_in_fat();
    if(free_block_idx < 0) {
        return free_block_idx;
    }

    directory[new_dir_idx].valid = 1;
    directory[new_dir_idx].isDir = isDir;
    directory[new_dir_idx].pad = 6;
    directory[new_dir_idx].uid = getuid();
    directory[new_dir_idx].gid = getgid();
    directory[new_dir_idx].mode = mode & 01777;
    directory[new_dir_idx].mtime = time(NULL);
    directory[new_dir_idx].start = free_block_idx;
    directory[new_dir_idx].length = 0;
    strcpy(directory[new_dir_idx].name, new_dir_name);
    disk->ops->write(disk, read_start * 2, 2, (void *) directory);

    if(isDir == 1) {
        disk->ops->read(disk, free_block_idx * 2, 2, (void *) directory);
        int y = 0;
        for(; y < MAX_DIR_ENTRIES; y++) {
            directory[y].valid = 0;
        }
        disk->ops->write(disk, free_block_idx * 2, 2, (void *) directory);
    }
    //update fat table
    disk->ops->write(disk, 2, FS_DS_RATIO * superblock.fat_len, (char*)fat_table);

    return 0;
}


////////////////////////

/* init - this is called once by the FUSE framework at startup.
 * This might be a good place to read in the super-block and set up
 * any global variables you need. You don't need to worry about the
 * argument or the return value.
 */
void* hw4_init(struct fuse_conn_info *conn)
{
    //read in the superblock data
    char buffer[FS_SECTOR_SIZE];

    disk->ops->read(disk, 0, 2, buffer);

    // initialize superblock with the data read
    superblock.magic       = *((unsigned int *) buffer);
    superblock.blk_size    = *((unsigned int *) (buffer +               sizeof(uint32_t)));
    superblock.fs_size     = *((unsigned int *) (buffer +           2 * sizeof(uint32_t)));
    superblock.fat_len     = *((unsigned int *) (buffer +           3 * sizeof(uint32_t)));
    superblock.root_dirent = *((struct cs5600fs_dirent *) (buffer + 4 * sizeof(uint32_t)));

    // read fat table
    char * fat_table_buffer = (char *) malloc(FS_SECTOR_SIZE * superblock.fat_len);

    disk->ops->read(disk, 2, FS_DS_RATIO * superblock.fat_len, fat_table_buffer);
    fat_table = (struct cs5600fs_entry*) fat_table_buffer;

    return NULL;
}


/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path is not present.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - fields not provided in CS5600fs are:
 *    st_nlink - always set to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * errors - path translation, ENOENT
 */
static int hw4_getattr(const char *path, struct stat *sb)
{
    //  root directory 
    if(strcmp("/", path) == 0) { 
         set_stats(sb, superblock.root_dirent);
         return 0;
    } 

    char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];

    int num_of_direntries = parsepath( direntries, (char *) path); 
    int dummy = 0;
    int result = load_directory(num_of_direntries, direntries, &dummy);

    //check for any errors
    if(result < 0) {
        return result;
    }
    set_stats(sb, dirent);

    return 0;
}

/* readdir - get directory contents.
 *
 * for each entry in the directory, invoke the 'filler' function,
 * which is passed as a function pointer, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a struct stat, just like in getattr.
 *
 * Errors - path resolution, ENOTDIR, ENOENT
 */
static int hw4_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{ 
    char *name;
    struct stat sb;

    int read_start = 0;

    if(strcmp("/", path) != 0) {
        char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];

        int num_of_direntries = parsepath( direntries, (char *) path);
        int dummy = 0;
        int result = load_directory(num_of_direntries, direntries, &dummy);
        // read dirent directory
        read_start = dirent.start;

        if(result < 0) {
            return result;
        }
    } else {
        read_start = superblock.root_dirent.start;
    } 

    disk->ops->read(disk, read_start * 2, 2, (void *) directory);


    /* Example code - you have to iterate over all the files in a
     * directory and invoke the 'filler' function for each.
     */
    memset(&sb, 0, sizeof(sb));
    
    int x = 0;
    for (; x < MAX_DIR_ENTRIES; x++) {
        if(directory[x].valid == 0) {
            continue;
        } 
	    set_stats(&sb, directory[x]);
	    name = directory[x].name;
        filler(buf, name, &sb, 0); /* invoke callback function */ 
    }
    return 0;
}

/* create - create a new file with permissions (mode & 01777)
 *
 * Errors - path resolution, EEXIST
 *
 * If a file or directory of this name already exists, return -EEXIST.
 */
static int hw4_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi)
{
    return create_direntry(path, mode, NULL, 0);
}

/* mkdir - create a directory with the given mode.
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create.
 */
static int hw4_mkdir(const char *path, mode_t mode)
{
    return create_direntry(path, mode, NULL, 1);
}

/* unlink - delete a file
 *  Errors - path resolution, ENOENT, EISDIR
 */
static int hw4_unlink(const char *path)
{
    //check if you try to unlink root dir
    if(strcmp("/", path) == 0) {
        return -EISDIR;
    }

    char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];
    int parent = 0;
    int num_of_direntries = parsepath( direntries, (char *) path);
    int result = load_directory(num_of_direntries, direntries, &parent);

    if(result < 0) {
        return result;
    }

    if(dirent.isDir == 1) {
        return -EISDIR;
    }
    //result holds the index of the direntry in directory
    directory[result].valid = 0;

    int start_block = dirent.start;
   
    //unlink all blocks in the file
    while(1) {
        fat_table[start_block].inUse = 0;
        if(fat_table[start_block].eof == 1) {
            break;
        }
        start_block = fat_table[start_block].next;
    }
    
    //update fat table
    disk->ops->write(disk, 2, FS_DS_RATIO * superblock.fat_len, (char*)fat_table);
    disk->ops->write(disk, parent* 2, 2, (void *) directory);
   
    return 0;
}

/* rmdir - remove a directory
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
static int hw4_rmdir(const char *path)
{
    if(strcmp("/", path) == 0) {
        return -ENOENT;
    }

    char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];
    int parent = 0;
    int num_of_direntries = parsepath( direntries, (char *) path);
    int result = load_directory(num_of_direntries, direntries, &parent);

    if(result < 0) {
        return result;
    }
    if(dirent.isDir != 1) {
        return -ENOTDIR;
    }

    struct cs5600fs_dirent dir_buffer[MAX_DIR_ENTRIES];
    //read the directory to check if it is empty
    disk->ops->read(disk, dirent.start * 2, 2, (void*) dir_buffer);

    int x = 0;
    for(; x < MAX_DIR_ENTRIES; x++) {
        if(dir_buffer[x].valid == 1) {
            break;
        }
    }
    if(x < MAX_DIR_ENTRIES) {
        return -ENOTEMPTY;
    }

    fat_table[dirent.start].inUse = 0;
    directory[result].valid = 0;
    disk->ops->write(disk, parent * 2, 2, (void *) directory);
    //update fat table
    disk->ops->write(disk, 2, FS_DS_RATIO * superblock.fat_len, (char*)fat_table);

    return 0;
}

/* rename - rename a file or directory
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
static int hw4_rename(const char *src_path, const char *dst_path)
{
    if(strcmp("/", src_path) == 0) {
        return -EOPNOTSUPP;
    }

    char src_direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];
    char dst_direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];

    int num_of_src_direntries = parsepath( src_direntries, (char *) src_path);
    int num_of_dst_direntries = parsepath( dst_direntries, (char *) dst_path);

    int src_parent = 0;
    int dst_parent = 0;

    int dst_result = load_directory(num_of_dst_direntries, dst_direntries, &dst_parent);

    if(dst_result > 0) {
        return -EEXIST;
    }

    int src_result = load_directory(num_of_src_direntries, src_direntries, &src_parent);

    if(src_result < 0) {
        return src_result;
    }

    //check if in different directories
    char * last_slash_location_src = strrchr(src_path, '/');
    char * last_slash_location_dst = strrchr(dst_path, '/');

    int last_slash_location_src_idx = last_slash_location_src - src_path;
    int last_slash_location_dst_idx = last_slash_location_dst - dst_path;
    if(last_slash_location_dst_idx != last_slash_location_src_idx) {
        return -EOPNOTSUPP;
    }
    int x = 0;
    for(; x < last_slash_location_src_idx; x++) {
        if(src_path[x] != dst_path[x]) {
            return -EOPNOTSUPP;
        }
    }

    //extract name of the new directory
    strcpy(directory[src_result].name, last_slash_location_dst + 1);

    disk->ops->write(disk, src_parent * 2, 2, (void*) directory);
    return 0;


}

/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * Errors - path resolution, ENOENT.
 */
static int hw4_chmod(const char *path, mode_t mode)
{
    if(strcmp("/", path) == 0) {
        return -EOPNOTSUPP;
    }

    char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];

    int num_of_direntries = parsepath( direntries, (char *) path);
    int parent = 0;
    int result = load_directory(num_of_direntries, direntries, &parent);

    if(result < 0) {
        return result;
    }

    directory[result].mode = mode;
    disk->ops->write(disk, parent * 2, 2, (void*)directory);

    return 0;
}
int hw4_utime(const char *path, struct utimbuf *ut)
{
    if(strcmp("/", path) == 0) {
        return -EOPNOTSUPP;
    }

    char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];

    int num_of_direntries = parsepath( direntries, (char *) path);
    int parent = 0;
    int result = load_directory(num_of_direntries, direntries, &parent);

    if(result < 0) {
        return result;
    }

    directory[result].mtime = ut->modtime;
    disk->ops->write(disk, parent * 2, 2, (void*)directory);
    return 0;
}

/* truncate - truncate file to exactly 'len' bytes
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
static int hw4_truncate(const char *path, off_t len)
{
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0) {
	   return -EINVAL;
    }		/* invalid argument */
    
    if(strcmp("/", path) == 0) {
        return -EOPNOTSUPP;
    }

    char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];

    int num_of_direntries = parsepath( direntries, (char *) path);
    int parent = 0;
    int result = load_directory(num_of_direntries, direntries, &parent);

    if(result < 0) {
        return result;
    }

    if(dirent.isDir == 1) {
        return -EISDIR;
    }

    directory[result].length = 0;
    disk->ops->write(disk, parent * 2, 2, (void *) directory);

    int start_block = dirent.start; 
    //unlink all blocks in the file
    while(1) {
        if(fat_table[start_block].eof == 1) {
            break;
        }
        start_block = fat_table[start_block].next;
        fat_table[start_block].inUse = 0;

    }
    
    //update fat table
    disk->ops->write(disk, 2, FS_DS_RATIO * superblock.fat_len, (char*)fat_table);
    return 0;
}

/* read - read data from an open file.
 * should return exactly the number of bytes requested, except:
 *   - if offset >= len, return 0
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
static int hw4_read(const char *path, char *buf, size_t len, off_t offset,
		    struct fuse_file_info *fi)
{
    //check if we are trying to read a root directory
    if(strcmp("/", path) == 0) {
        return -EISDIR;
    }
    
    char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];

    int num_of_direntries = parsepath( direntries, (char *) path);
    int dummy = 0;
    int result = load_directory(num_of_direntries, direntries, &dummy);

    if(result < 0) {
        return result;
    }

    if(dirent.isDir == 1) {
        return -EISDIR;
    }

    if(offset >= dirent.length) {
        return 0;
    }

    //find block start in fat table 
    int start_block = dirent.start;
    int block_num = offset / FS_SECTOR_SIZE;
    int block_offset = offset % FS_SECTOR_SIZE;

    int x = 0;
    for(; x < block_num; x++) {
        start_block = fat_table[start_block].next;
    }

    char file_buffer[FS_SECTOR_SIZE];

    int number_bytes_to_read = len;
    if( len + offset > dirent.length) {
        number_bytes_to_read = dirent.length - offset;
    }

    int num_bytes_to_copy = 0;
    int num_bytes_read = 0;

    while(number_bytes_to_read > 0 ) {
        //read from disk
        disk->ops->read(disk, start_block * 2, 2, file_buffer);
        //check if copy the full block or less
        if (number_bytes_to_read > (FS_SECTOR_SIZE - block_offset)) {
            //copy whole block minus offset 
            num_bytes_to_copy = FS_SECTOR_SIZE - block_offset;

        } else {
            // copy part of the block
            num_bytes_to_copy = number_bytes_to_read;
        }
        memcpy(buf, file_buffer + block_offset, num_bytes_to_copy);
        block_offset = 0;
        buf = buf + num_bytes_to_copy;
        number_bytes_to_read -= num_bytes_to_copy;
        num_bytes_read += num_bytes_to_copy;

        start_block = fat_table[start_block].next;

    }

    //return how many bytes requested
    return num_bytes_read;
}

/* write - write data to a file
 * It should return exactly the number of bytes requested, except on
 * error.
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 */
static int hw4_write(const char *path, const char *buf, size_t len,
		     off_t offset, struct fuse_file_info *fi)
{
    //cant write root
    if(strcmp("/", path) == 0) {
        return -EISDIR;
    }

    char direntries[MAX_DIR_ENTRIES][DIRENT_MAX_LENGTH];

    int num_of_direntries = parsepath( direntries, (char *) path);
    int parent = 0;
    int result = load_directory(num_of_direntries, direntries, &parent);
    if(result < 0) {
        return result;
    }

    if(dirent.isDir == 1) {
        return -EISDIR;
    }

    if(offset > dirent.length) {
        return -EINVAL;
    }

    int start_block = dirent.start;
    int block_offset = offset % FS_SECTOR_SIZE;
    int block_num = offset / FS_SECTOR_SIZE;

    //find start block
    int x = 0;
    for(; x < block_num; x++) {
        if(fat_table[start_block].eof == 1) {
            int new_block = find_unused_block_in_fat();
            if(new_block < 0) {
                return new_block;
            }
            fat_table[start_block].eof = 0;
            fat_table[start_block].next = new_block;
            start_block = new_block;
        } else {
            start_block = fat_table[start_block].next;
        }
    }
    char file_buffer[FS_SECTOR_SIZE];

    int number_bytes_to_write = len;
    if( len + offset > dirent.length) {
        directory[result].length = offset + len;
        disk->ops->write(disk, parent * 2, 2, (void *) directory);
    }

    int num_bytes_to_copy = 0;
    int num_bytes_written = 0;
    int num_bytes_written_current_block = 0;

    while(number_bytes_to_write > 0 ) {
        //read from disk
        disk->ops->read(disk, start_block * 2, 2, file_buffer);
        //check if copy the full block or less
        if (number_bytes_to_write > (FS_SECTOR_SIZE - block_offset)) {
            //copy whole block minus offset 
            num_bytes_to_copy = FS_SECTOR_SIZE - block_offset;

        } else {
            // copy part of the block
            num_bytes_to_copy = number_bytes_to_write;
        }
        memcpy(file_buffer + block_offset, buf + num_bytes_written, num_bytes_to_copy);
        disk->ops->write(disk, start_block * 2, 2, file_buffer);
        //buf = buf + num_bytes_to_copy;
        number_bytes_to_write -= num_bytes_to_copy;
        num_bytes_written_current_block += num_bytes_to_copy;
        num_bytes_written += num_bytes_to_copy;

        if(number_bytes_to_write > 0 
            && ((num_bytes_written_current_block + block_offset) == FS_SECTOR_SIZE )) {
            block_offset = 0;
            num_bytes_written_current_block = 0;
            if(fat_table[start_block].eof == 1) {
                //need to find new block
                int new_block = find_unused_block_in_fat();
                if(new_block < 0) {
                    return new_block;
                }
                fat_table[start_block].eof = 0;
                fat_table[start_block].next = new_block;
                start_block = new_block;
            } else {
                start_block = fat_table[start_block].next;
            }
        }
    }

    //update fat table
    disk->ops->write(disk, 2, FS_DS_RATIO * superblock.fat_len, (char*)fat_table);

    //return how many bytes written
    return num_bytes_written;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
static int hw4_statfs(const char *path, struct statvfs *st)
{
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - (superblock + FAT)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namelen = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */

     st->f_bsize   = superblock.blk_size;
     st->f_frsize  = 0;
     st->f_blocks  = superblock.fs_size - (1 + superblock.fat_len);

     unsigned int in_use_counter = 0;
     unsigned int x = 0;
     for(; x < superblock.fs_size; x++) {
        if(fat_table[x].inUse == 1) {
            in_use_counter++;
        }
     }

     st->f_bfree   = st->f_blocks - in_use_counter;
     st->f_bavail  = st->f_bfree;
     st->f_files   = 0;
     st->f_ffree   = 0;
     st->f_favail  = 0;
     st->f_fsid    = 0;
     st->f_flag    = 0;
     st->f_namemax = DIRENT_MAX_LENGTH;

    return 0;
}

/* operations vector. Please don't rename it, as the skeleton code in
 * misc.c assumes it is named 'hw4_ops'.
 */
struct fuse_operations hw4_ops = {
    .init = hw4_init,
    .getattr = hw4_getattr,
    .readdir = hw4_readdir,
    .create = hw4_create,
    .mkdir = hw4_mkdir,
    .unlink = hw4_unlink,
    .rmdir = hw4_rmdir,
    .rename = hw4_rename,
    .chmod = hw4_chmod,
    .utime = hw4_utime,
    .truncate = hw4_truncate,
    .read = hw4_read,
    .write = hw4_write,
    .statfs = hw4_statfs,
};

