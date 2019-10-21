/*
	FUSE: Filesystem in Userspace
	Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

	This program can be distributed under the terms of the GNU GPL.
	See the file COPYING.
*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

//size of a disk block
#define	BLOCK_SIZE 512
#define BITMAP_BITS 10237
#define BITMAP_INTS 320

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct cs1550_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct cs1550_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct cs1550_file_directory) - sizeof(int)];
} ;

typedef struct cs1550_root_directory cs1550_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct cs1550_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct cs1550_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct cs1550_directory) - sizeof(int)];
} ;


typedef struct cs1550_directory_entry cs1550_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE)

#define BITMAP_START_ADDRESS (-1*3*BLOCK_SIZE)

struct cs1550_disk_block
{
	//All of the space in the block can be used for actual data
	//storage.
	char data[MAX_DATA_IN_BLOCK];
};

typedef struct cs1550_disk_block cs1550_disk_block;

static int parse_file(const char* path, char* directory, char* filename, char* extension){
	// printf("path: %s\n", path);
	// char* dir_name = strtok((char*)path, "/");
	// char* full_file_name = strtok(NULL, "/");
	// char* file_name = strtok(full_file_name, ".");
	// char* file_extension = strtok(NULL, ".");
	// directory = dir_name;
	// filename = file_name;
	// extension = file_extension;

	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	// printf("dir: %s\n", directory);
	// printf("fname: %s\n", filename);
	// printf("fname: %s\n", extension);

	directory[MAX_FILENAME + 1] = '\0';
	filename[MAX_FILENAME + 1] = '\0';
	extension[MAX_EXTENSION + 1] = '\0';

	int type = -1;
	if(strcmp(directory,"\0") != 0) type = 1;
	if(strcmp(filename,"\0") != 0) type = 2;
	// printf("directory: %s\n", directory);
	// printf("filename: %s\n", filename);
	// printf("extension: %s\n", extension);
	// printf("type: %d\n", type);
	return type;
}

static void load_root(cs1550_root_directory *root){
	//printf("loading the root\n");
	FILE *disk = fopen(".disk", "r+b");
	if(disk!=NULL){
		fseek(disk, 0, SEEK_SET);
		fread(root, sizeof(struct cs1550_root_directory), 1, disk);
		fclose(disk);
	}
	// printf("loaded the root\n");
	// printf("directories in the root atm -> %d\n", (int)root->nDirectories);
}

static void load_directory(char* directory, cs1550_directory_entry *dir){
	// printf("loading directory\n");
	cs1550_root_directory root;
	int i = 0;
	long ret_dir_start = 0;
	load_root(&root);

	for(i = 0; root.nDirectories; i++){
		if(strcmp(directory, root.directories[i].dname) == 0){
			ret_dir_start = root.directories[i].nStartBlock;
			FILE *disk = fopen(".disk", "r+b");
			if(disk!=NULL){
				fseek(disk, ret_dir_start, SEEK_SET);
				fread(dir, sizeof(struct cs1550_directory_entry), 1, disk);
				fclose(disk);
			}
			break;
		}
	}
	// printf("finished loading\n");
}

static void write_directory(char* directory, cs1550_directory_entry *dir){
	// printf("loading directory\n");
	cs1550_root_directory root;
	int i = 0;
	long ret_dir_start = 0;
	load_root(&root);

	for(i = 0; root.nDirectories; i++){
		if(strcmp(directory, root.directories[i].dname) == 0){
			ret_dir_start = root.directories[i].nStartBlock;
			FILE *disk = fopen(".disk", "r+b");
			if(disk!=NULL){
				fseek(disk, ret_dir_start, SEEK_SET);
				fwrite(dir, sizeof(struct cs1550_directory_entry), 1, disk);
				fclose(disk);
			}
			break;
		}
	}
	// printf("finished loading\n");
}

static size_t file_info(char* directory, char* filename, char* extension){
	// printf("getting file info\n");
	int i = 0;
	cs1550_directory_entry dir;
	load_directory(directory, &dir);
	size_t fsize_file = -1;

	for(i = 0; i < dir.nFiles; i++){
		if(strcmp(filename, dir.files[i].fname) == 0){
			fsize_file = (size_t) dir.files[i].fsize;
			if(strcmp(extension, "\0") != 0 && strcmp(extension, dir.files[i].fext) != 0){
				fsize_file = -1;
			}
		}
	}
	// printf("the file size for %s is %d\n", directory, (int)fsize_file);
	return fsize_file;
}

static void write_bitmap(int idx, int bit){
	//printf("writing on bitmap, idx = %d, bit = %d\n", idx, bit);
	int i = 0;
	uint32_t* bitmap = (uint32_t*) malloc(BITMAP_INTS*sizeof(uint32_t));
	FILE *disk = fopen(".disk", "r+b");

	fseek(disk, BITMAP_START_ADDRESS, SEEK_END);
	fread(bitmap, BITMAP_INTS*sizeof(uint32_t), 1, disk);


	// for(i = 0; i<BITMAP_INTS; i++){
	// 	printf("bits before: %" PRIu32 "\n",bitmap[i]);
	// }
	int offset = idx%31;
	int bitmap_index = idx/32;
	// printf("index: %d\n",bitmap_index);
	// printf("offset: %d\n",offset);
	uint32_t map = 0x80000000;
	// printf("map before: %" PRIu32 "\n",map);
	for(i = 0; i<offset; i++){
		map = map >> 1;
	}
	// printf("map after: %" PRIu32 "\n",map);
	// printf("bit before: %" PRIu32 "\n",bitmap[bitmap_index]);
	if(bit == 1){
		bitmap[bitmap_index]|=map;
	}else{
		bitmap[bitmap_index]^=map;
	}
	// printf("bit after: %" PRIu32 "\n",bitmap[bitmap_index]);

	fseek(disk, BITMAP_START_ADDRESS, SEEK_END);
	fwrite(bitmap, sizeof(uint32_t)*BITMAP_INTS, 1, disk);
	fclose(disk);


	// for(i = 0; i<BITMAP_INTS; i++){
	// 	printf("bits after: %" PRIu32 "\n",bitmap[i]);
	// }

	free(bitmap);
	// printf("finished writing on bitmap\n");
}

static int next_block(void){
	write_bitmap(0, 1);
	// printf("finding next free block\n");
	int i = 0;
	int block_num = -1;
	uint32_t* bitmap = (uint32_t*) malloc(BITMAP_INTS*sizeof(uint32_t));
	FILE *disk = fopen(".disk", "r+b");
	int counter = 0;
	if(disk!=NULL){
		fseek(disk, BITMAP_START_ADDRESS, SEEK_END);
		fread(bitmap, BITMAP_INTS*sizeof(uint32_t), 1, disk);
		fclose(disk);
	}
	// for(i = 0; i<BITMAP_INTS; i++){
	// 	printf("bits to find From: %" PRIu32 "\n",bitmap[i]);
	// }
	for(i = 0; i<BITMAP_INTS; i++){
		uint32_t curr_int = bitmap[i];
		if(curr_int == 0xFFFFFFFF){
			// printf("int at i = %d is full", i);
			continue;
		}
		// printf("curr_int: %" PRIu32 "\n",curr_int);
		// printf("comparison: %" PRIu32 "\n",(curr_int & 0x80000000));
		while(((curr_int & 0x80000000) == 0x80000000) && counter<32){
			// printf("encountered 1 bit at i= %d index = %d\n", i, counter);
			curr_int = curr_int << 1;
			counter++;
		}
		if(i==0){
			block_num = counter;
		}else{
			block_num = (i-1)*32 + counter;
		}
		break;
	}
	free(bitmap);
	// printf("found next free block %d\n", block_num);
	return block_num;
}
// static int write(){
//
// }
//
// static int read(){
//
// }
//
// static int write_dir(){
//
// }
//
// static int read_dir(){
//
// }
/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int cs1550_getattr(const char *path, struct stat *stbuf)
{
	int ret = 0;
	// printf("root getattr");

	memset(stbuf, 0, sizeof(struct stat));

	int i = 0;
	int found = 0;
	char dir_name[MAX_FILENAME + 1];
	char fname[MAX_FILENAME + 1];
	char ext[MAX_EXTENSION + 1];

	dir_name[0] = '\0';
	fname[0] = '\0';
	ext[0] = '\0';

	//is path the root dir?
	//ROOT DIRECTORY
	if (strcmp(path, "/") == 0) {
		// printf("root getattr");
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		int ftype = parse_file(path, dir_name, fname, ext);
		// printf("directory: %s", directory);
		//Check if name is subdirectory
		//printf("%d\n", ftype);
		if (ftype == 1){
			cs1550_root_directory root;
			load_root(&root);
			for(i = 0; i < root.nDirectories; i++){
				if(strcmp(dir_name, root.directories[i].dname) == 0){
					/*
						//Might want to return a structure with these fields
						stbuf->st_mode = S_IFDIR | 0755;
						stbuf->st_nlink = 2;
						res = 0; //no error
					*/
					stbuf->st_mode = S_IFDIR | 0755;
					stbuf->st_nlink = 2;
					ret = 0;
					found = 1;
					break;
				}
			}
			if(found == 0){
				ret = - ENOENT;
			}
		}else if(ftype == 2){
			size_t fs = -1;
			fs = file_info(dir_name, fname, ext);
			if(fs == -1){
				ret = - ENOENT;
			}else{

				//Check if name is a regular file
				/*
					//regular file, probably want to be read and write
					stbuf->st_mode = S_IFREG | 0666;
					stbuf->st_nlink = 1; //file links
					stbuf->st_size = 0; //file size - make sure you replace with real size!
					res = 0; // no error
				*/
				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 1; //file links
				stbuf->st_size = fs; //file size - make sure you replace with real size!
				ret = 0; // no error
			}
		}else{
			//Else return that path doesn't exist
			//printf("Path doesn't exist.");
			ret = -ENOENT;
		}
	}
	return ret;
}

/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	int i = 0;
	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;

	// printf("readdir\n");
	//This line assumes we have no subdirectories, need to change
	if (strcmp(path, "/") != 0)
	return -ENOENT;

	//the filler function allows us to add entries to the listing
	//read the fuse.h file for a description (in the ../include dir)
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	//This is the root folder contents
	if(strcmp(path, "/") == 0){
		cs1550_root_directory root;
		load_root(&root);
		for(i = 0; i<root.nDirectories; i++){
			filler(buf, root.directories[i].dname, NULL, 0);
		}
	}else{
		int ftype = 0;
		char parent_dir[MAX_FILENAME + 1];
		char parent_fname[MAX_FILENAME + 1];
		char parent_ext[MAX_EXTENSION + 1];
		parent_dir[0] = '\0';
		parent_fname[0] = '\0';
		parent_ext[0] = '\0';
		ftype = parse_file(path, parent_dir, parent_fname, parent_ext);
		// printf("type = %d\n",type);
		if(ftype == 1){
			// printf("This is a directory in readdir.\n");
			cs1550_directory_entry dir;
			load_directory(parent_dir, &dir);
			for(i = 0; i<dir.nFiles; i++){
				char curr_file[MAX_FILENAME + MAX_EXTENSION + 2];
				strcpy(curr_file, dir.files[i].fname);
				if(strcmp(dir.files[i].fext, "\0") != 0){
					strcat(curr_file, ".");
					strcat(curr_file, dir.files[i].fext);
				}
				curr_file[strlen(dir.files[i].fname) + strlen(dir.files[i].fext) + 2] = '\0';
				filler(buf, curr_file, NULL, 0);
			}
		}else{
			return -ENOENT;
		}
	}
	/*
	//add the user stuff (subdirs or files)
	//the +1 skips the leading '/' on the filenames
	filler(buf, newpath + 1, NULL, 0);
	*/
	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	// printf("making directory\n");
	(void) mode;
	int i = 0;
	char new_dir[MAX_FILENAME + 1];
	char new_fname[MAX_FILENAME + 1];
	char new_ext[MAX_FILENAME + 1];
	new_dir[0] = '\0';
	new_fname[0] = '\0';
	new_ext[0] = '\0';
 	cs1550_root_directory root;
	load_root(&root);
	if(strcmp(path, "/") == 0){
		// printf("Can't add empty filename as a directory.\n");
	}else{
		int ftype = parse_file(path, new_dir, new_fname, new_ext);
		// printf("parsed file\n");
		if(ftype == 1){

			if(strlen(new_dir) > MAX_FILENAME){
				return -ENAMETOOLONG;
			}

			if(root.nDirectories < MAX_DIRS_IN_ROOT){
				// printf("safe to add, need to check if already exists\n");
				for(i = 0; i<root.nDirectories; i++){
					if(strcmp(root.directories[i].dname, new_dir) == 0){
						// printf("A directory with this name already exists.\n");
						return -EEXIST;
					}
				}
				int block_to_assign = -1;
				block_to_assign = next_block();
				//printf("%d\n",block_to_assign);
				//printf("block to assign next: %d\n", block_to_assign);
				if(block_to_assign > 0){
					write_bitmap(block_to_assign, 1);
					strcpy(root.directories[root.nDirectories].dname, new_dir);
					root.directories[root.nDirectories].nStartBlock = BLOCK_SIZE * block_to_assign;
					root.nDirectories++;
				}
				FILE* disk = fopen(".disk", "r+b");
				if(disk != NULL){
					fwrite(&root, sizeof(struct cs1550_root_directory), 1, disk);
					fclose(disk);
				}
			}else{
				return -EPERM;
			}
		}
	}
	return 0;
}

/*
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void) path;
    return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) path;
	(void) mode;
	(void) dev;
	int i = 0;
	int ftype = 0;
	cs1550_directory_entry parent;
	char new_dir[MAX_FILENAME + 1];
	char new_fname[MAX_FILENAME + 1];
	char new_ext[MAX_FILENAME + 1];
	new_dir[0] = '\0';
	new_fname[0] = '\0';
	new_ext[0] = '\0';

	ftype = parse_file(path, new_dir, new_fname, new_ext);
	load_directory(new_dir, &parent);

	if(strcmp(path, "/") == 0 || ftype < 2){
		return -EPERM;
	}else{
		if(strlen(new_fname) > MAX_FILENAME || strlen(new_ext) > MAX_EXTENSION){
			return -ENAMETOOLONG;
		}
		if(parent.nFiles < MAX_FILES_IN_DIR){
			printf("%d\n", parent.nFiles);
			for(i = 0; i < parent.nFiles; i++){
				if(strcmp(parent.files[i].fname, new_fname) == 0 && strcmp(parent.files[i].fext, new_ext) == 0){
					return -EEXIST;
				}
			}
			int block_to_assign = -1;
			block_to_assign = next_block();
			if(block_to_assign > 0){
				write_bitmap(block_to_assign, 1);
				strcpy(parent.files[parent.nFiles].fname, new_fname);
				strcpy(parent.files[parent.nFiles].fext, new_ext);
				parent.files[parent.nFiles].nStartBlock = BLOCK_SIZE * block_to_assign;
				parent.nFiles = parent.nFiles + 1;
			}
			write_directory(new_dir, &parent);
		}
	}
	return 0;
}

/*
 * Deletes a file
 */
static int cs1550_unlink(const char *path)
{
    (void) path;

    return 0;
}

/*
 * Read size bytes from file into buf starting from offset
 *
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//read in data
	//set size and return, or error

	size = 0;

	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;

	//check to make sure path exists
	//check that size is > 0
	//check that offset is <= to the file size
	//write data
	//set size (should be same as input) and return, or error

	return size;
}

/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}


/*
 * Called when we open a file
 *
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations hello_oper = {
    .getattr	= cs1550_getattr,
    .readdir	= cs1550_readdir,
    .mkdir	= cs1550_mkdir,
	.rmdir = cs1550_rmdir,
    .read	= cs1550_read,
    .write	= cs1550_write,
	.mknod	= cs1550_mknod,
	.unlink = cs1550_unlink,
	.truncate = cs1550_truncate,
	.flush = cs1550_flush,
	.open	= cs1550_open,
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &hello_oper, NULL);
}
