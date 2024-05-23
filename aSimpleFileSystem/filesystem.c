/*
Wrote by Ian Shaneyfelt / William Duke
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "filesystem.h"
#include "softwaredisk.h"

#define MAX_FILES 512 					//from the inode_idx being a 16 bit int
#define MAX_FILE_SIZE 8437760
/*
block = 4096 bytes
pointer = 2bytes
direct = 12 blocks
indirect = block/pointer
4096/2 = 2048 block pointers
indirect + direct = max file size
2048 + 12 = 2060 blocks * 4096 bytes =  8,437,760
*/
#define DATA_BITMAP_BLOCK 0
#define INODE_BITMAP_BLOCK 1
#define FIRST_INODE_BLOCK 2
#define LAST_INODE_BLOCK 5
#define TOTAL_INODES 512
#define FIRST_DIR_ENTRY_BLOCK 6
#define LAST_DIR_ENTRY_BLOCK 69
#define FIRST_DATA_BLOCK 70
#define LAST_DATA_BLOCK 4095

#define DIR_ENTRY_PER_BLOCK 8
#define INODES_PER_BLOCK 128
#define NUM_DIRECT_BLOCKS 12
#define MAX_NAME_SIZE 507
#define SOFTWARE_DISK_BLOCK_SIZE 4096

//protypes
unsigned long read_direct_block(File file, char *buf, unsigned long numbytes, uint16_t offset_bytes);
unsigned long read_indirect_block(File file, char *buf, unsigned long numbytes, unsigned long bytes_read, uint16_t offset_bytes);
unsigned long write_to_direct_block(File file, char *data, unsigned long numbytes, uint16_t offset_bytes);
unsigned long write_to_indirect_block(File file, char *data, unsigned long numbytes, uint16_t offset_bytes);
void zero_block_at(int block);
int file_exists(char *name);
int is_file_open(char *name);
int locate_and_fill_free_data_bitmap();
int locate_and_fill_free_inode_bitmap();
void free_data_block(int block);
void free_inode_block(int block);

FSError fserror;

typedef struct IndirectBlock_Block{
    uint16_t indirectblock_array[SOFTWARE_DISK_BLOCK_SIZE/sizeof(uint16_t)];
}IndirectBlock_Block;

typedef struct DirEntry{
	uint8_t file_is_open;		// file is open
	int16_t inode_idx;		// inode #
	char name[MAX_NAME_SIZE]; 	// null terminated ascci filename
}DirEntry;

typedef struct Inode{
	uint32_t size;				//file size
	uint16_t direct_block_num[12];		//direct blocks +1 for idirect node
	uint16_t indirect_block_num;
}Inode;



typedef struct InodeBlock{
	Inode inode_array[SOFTWARE_DISK_BLOCK_SIZE / sizeof(Inode)];		// ensure no remainder that is the main thing
									
}InodeBlock;

typedef struct DirBlock{
	DirEntry dir_array[SOFTWARE_DISK_BLOCK_SIZE / sizeof(DirEntry)];		// ensure no remainder that is the main thing
									
}DirBlock;

typedef struct FileInternals
{
    uint32_t position;
    FileMode mode;
    Inode *inode;
    DirEntry *d;

}FileInternals;

void fs_print_error()
{
    switch(fserror)
    {
        case FS_NONE:
            printf("FS: no error\n");
            break;
        case FS_FILE_NOT_FOUND:
            printf("FS: File Not Found\n");
            break;
        case FS_FILE_READ_ONLY:
            printf("FS: File Read Only \n");
            break;
        case FS_FILE_ALREADY_EXISTS:
            printf("FS: Already Exists \n");
            break;
        case FS_OUT_OF_SPACE:
            printf("FS: Out Of Space or reached max file limit \n");
            break;
        case FS_FILE_OPEN:
            printf("FS: File Open\n");
            break;
        case FS_EXCEEDS_MAX_FILE_SIZE:
            printf("FS: File Exceeds Max File Size \n");
            break;
        case FS_ILLEGAL_FILENAME:
            printf("FS: Illegal Filename \n");
            break;
        case FS_FILE_NOT_OPEN:
            printf("FS: File Not Open \n");
            break;
        case FS_IO_ERROR:
            printf("FS: IO Error");
            break;
        default:
            printf("FS: Unknown error code %d.\n", fserror);
    }    
}

//implement modes
File open_file(char *name, FileMode mode)  
{
    fserror = FS_NONE;                      //set fs error
    if(!file_exists(name))
    {
        fserror = FS_FILE_NOT_FOUND;
        return;
    }
    if(is_file_open(name))
    {
        fserror = FS_FILE_OPEN;
        return;
    }

    File file;                             //holders
    DirBlock holder;
    DirEntry entry_holder;
    InodeBlock inodeBlock_holder;
    Inode inode_holder;

    for (int i = FIRST_DIR_ENTRY_BLOCK; i <= LAST_DIR_ENTRY_BLOCK; i++)     //find file dir
    {
        read_sd_block(&holder, i);
        for (int j = 0; j < DIR_ENTRY_PER_BLOCK; j++)
        {
            entry_holder = holder.dir_array[j];
            if(! strncmp(entry_holder.name, name, MAX_NAME_SIZE))
            {
            file->d = &entry_holder;
            break;
            }
        }
    }
    
    int inode_block_location = (entry_holder.inode_idx / INODES_PER_BLOCK)+FIRST_INODE_BLOCK;                        //find inode
    int inode_true_location = (entry_holder.inode_idx % INODES_PER_BLOCK);
    read_sd_block(&inodeBlock_holder, inode_block_location);
    inode_holder = inodeBlock_holder.inode_array[inode_true_location];
    file->inode = &inode_holder;
    file->position = 0;
    file->d->file_is_open = 1;
    file->mode = mode;

    return file;
}

File create_file(char *name)
{
    fserror = FS_NONE;
   
    if(file_exists(name))
    {
        fserror = FS_FILE_ALREADY_EXISTS;
        return;
    }
    if(sizeof(name) > MAX_NAME_SIZE || name[0] == '\0')
    {
        fserror = FS_ILLEGAL_FILENAME;
        return;
    }
    File file;
    DirEntry entry_holder;
    Inode inode_holder;
    file->d = &entry_holder; 
    file->inode = &inode_holder;
    //file->mode = ;
    
    DirBlock holder;
    InodeBlock inode_block_holder;
    int i, j;
    for (i = FIRST_DIR_ENTRY_BLOCK; i <= LAST_DIR_ENTRY_BLOCK; i++)
    {
        read_sd_block(&holder, i);
        for (j = 0; j < DIR_ENTRY_PER_BLOCK; j++)
        {
            entry_holder = holder.dir_array[j];
            if(entry_holder.name == 0)
            {
            break;
            }
        }
    }
    
    if(i == SOFTWARE_DISK_BLOCK_SIZE)
    {
        fserror = FS_OUT_OF_SPACE;
        return;
    }
    //intitalize entry
    printf("1\n");
    entry_holder = holder.dir_array[j];
    entry_holder.file_is_open = 0;
    entry_holder.name;
    entry_holder.inode_idx = locate_and_fill_free_data_bitmap();
    printf("2\n");
    strncpy(entry_holder.name, name, MAX_NAME_SIZE);
    printf("3\n");
    holder.dir_array[j] = entry_holder;
    printf("4\n");
    memcpy(file->d,&entry_holder,sizeof(DirEntry));
    printf("5\n");
    file->d->inode_idx = entry_holder.inode_idx;
    
    write_sd_block(&holder, i);
    printf("6\n");

    //init inode
    inode_holder.direct_block_num;
    inode_holder.indirect_block_num;
    inode_holder.size = 0;
    file->inode = &inode_holder;

    file->position = 0;
    
    printf("8\n");
    return file;
}

void close_file(File file)
{
    fserror = FS_NONE;
    if(!file_exists(file->d->name))
    {
        fserror = FS_FILE_NOT_FOUND;
    }
    else if (! is_file_open(file->d->name))
    {
        fserror = FS_FILE_NOT_OPEN;
    }
    else
    {
        free(file);
    }
    return 0;
}

unsigned long read_direct_block(File file, char *buf, unsigned long numbytes, uint16_t offset_bytes){
    unsigned long bytes_read =0;
    unsigned long remaining_bytes = numbytes;
    char data_to_read[SOFTWARE_DISK_BLOCK_SIZE];
    unsigned long bytes_to_read;
    while (file->d->inode_idx < NUM_DIRECT_BLOCKS && bytes_read < numbytes)
{
        bytes_to_read = remaining_bytes < SOFTWARE_DISK_BLOCK_SIZE - offset_bytes? remaining_bytes : SOFTWARE_DISK_BLOCK_SIZE - offset_bytes;
        bzero(data_to_read, SOFTWARE_DISK_BLOCK_SIZE);
        if(file->inode->direct_block_num[file->d->inode_idx])
    {
            read_sd_block(data_to_read, file->inode->direct_block_num[file->d->inode_idx]);
        }
        memcpy((buf + bytes_read), (data_to_read + offset_bytes), bytes_to_read);
        file->position = file->position + bytes_to_read;
        bytes_read = bytes_read + bytes_to_read;
        remaining_bytes = remaining_bytes - bytes_to_read;
        file->d->inode_idx++;
        offset_bytes = 0;
    }
    return bytes_read;
}

unsigned long read_indirect_block(File file, char *buf, unsigned long numbytes, unsigned long bytes_read, uint16_t offset_bytes)
{
    unsigned long remaining_bytes = numbytes;
        char data_to_read[SOFTWARE_DISK_BLOCK_SIZE];
        unsigned long bytes_to_read;
    IndirectBlock_Block Indirectblock;
    read_sd_block(Indirectblock.indirectblock_array, file->inode->indirect_block_num);
    while(file->d->inode_idx < sizeof(Indirectblock.indirectblock_array) && bytes_read < numbytes)
        {
        bytes_to_read = remaining_bytes < SOFTWARE_DISK_BLOCK_SIZE - offset_bytes? remaining_bytes : SOFTWARE_DISK_BLOCK_SIZE - offset_bytes;
            bzero(data_to_read, SOFTWARE_DISK_BLOCK_SIZE);
        if(Indirectblock.indirectblock_array[file->d->inode_idx])
            {
            read_sd_block(data_to_read, Indirectblock.indirectblock_array[file->d->inode_idx]);
            }
        memcpy(buf + bytes_read, data_to_read + offset_bytes, bytes_to_read);
        file->position = file->position + bytes_to_read;
        bytes_read = bytes_read + bytes_to_read;
        remaining_bytes = remaining_bytes - bytes_to_read;
        file->d->inode_idx++;
        offset_bytes = 0;
        }
    return bytes_read;
    }

unsigned long read_file(File file, void *buf, unsigned long numbytes)
{
    fserror = FS_NONE;
    unsigned long bytes_read = 0;
    file->d->inode_idx = file->position % SOFTWARE_DISK_BLOCK_SIZE;
    int starting_at_direct = file->d->inode_idx < NUM_DIRECT_BLOCKS;
    uint16_t offset_bytes = file->position / SOFTWARE_DISK_BLOCK_SIZE;
    if(starting_at_direct && 0 <  numbytes)
    {
        bytes_read += read_direct_block(file, buf, numbytes, offset_bytes);
        offset_bytes = 0;
        file->d->inode_idx = file->position/SOFTWARE_DISK_BLOCK_SIZE;        
    }
    if (!starting_at_direct || 0 < numbytes - bytes_read)
    {
        bytes_read = read_indirect_block(file, buf, numbytes, bytes_read, offset_bytes);
    }
    return bytes_read;
}
/*
unsigned long write_to_direct_block(File file, char *data, unsigned long numbytes, uint16_t offset_bytes){
    uint16_t block_number;
    uint64_t bytes_written = 0;
    numbytes = strlen(data)  < numbytes ? strlen(data) : numbytes;
    uint64_t remaining_data = numbytes;
    uint64_t dataForBlock = 0;
    block_number = file->inode->direct_block_num[file->d->inode_idx];
    char data_to_write[SOFTWARE_DISK_BLOCK_SIZE];
    while( file->d->inode_idx < NUM_DIRECT_BLOCKS && bytes_written < numbytes)
    {
        bzero(data_to_write, SOFTWARE_DISK_BLOCK_SIZE * sizeof(char));
        dataForBlock = (remaining_data < SOFTWARE_DISK_BLOCK_SIZE - offset_bytes? remaining_data : SOFTWARE_DISK_BLOCK_SIZE- offset_bytes);
        if(!file->inode->direct_block_num[file->d->inode_idx])
        {
            memcpy(data_to_write + offset_bytes, data + bytes_written, dataForBlock);
            block_number = locate_and_fill_free_data_bitmap();
            file->inode->direct_block_num[file->d->inode_idx] = block_number;
            if(block_number)
            {
                file->inode->size += dataForBlock;
            }
        }
        else
        {
            block_number = file->inode->direct_block_num[file->d->inode_idx];
            read_sd_block(data_to_write, block_number);
            file->inode->size += dataForBlock <= strlen(data_to_write) ? 0 : dataForBlock - strlen(data_to_write);
            memcpy(data_to_write  + offset_bytes, data + bytes_written, dataForBlock);
            write_sd_block(data_to_write, block_number);
        }
        if(! block_number)
        {
            fserror = FS_OUT_OF_SPACE;
            InodeBlock inodeBlock;
            read_sd_block(inodeBlock.inode_array, 1);
            inodeBlock.inode_array[file->d->inode_idx] = file->inode; // needs to be fixed
            write_sd_block(inodeBlock.inode_array, 1);
            return bytes_written;
        }
        if(dataForBlock + bytes_written == numbytes)
        {
            InodeBlock inodeBlock;
            read_sd_block(inodeBlock.inode_array, 1);
            inodeBlock.inode_array[file->d->inode_idx] = file->inode;
            write_sd_block(inodeBlock.inode_array, 1);
            file->position += dataForBlock;
            return bytes_written + dataForBlock;
        }
        bytes_written += dataForBlock;
        file->position += dataForBlock;
        file->d->inode_idx++;
        remaining_data -= dataForBlock;
        offset_bytes = 0;
    }
    InodeBlock inodeBlock;
    read_sd_block(inodeBlock.inode_array, 1);
    inodeBlock.inode_array[file->d->inode_idx] = file->inode;
    write_sd_block(inodeBlock.inode_array, 1);
    return bytes_written;
}

unsigned long write_to_indirect_block(File file, char *data, unsigned long numbytes, uint16_t offset_bytes)
{
    unsigned long bytes_written = 0;
    if(! file->inode->indirect_block_num)
    {
        file->inode->indirect_block_num = locate_and_fill_free_data_bitmap();
        InodeBlock inodeBlock;
        read_sd_block(inodeBlock.inode_array, 1);
        inodeBlock.inode_array[file->d->inode_idx] = file->inode;
        write_sd_block(inodeBlock.inode_array, 1);
    }
    IndirectBlock_Block IndirectBlock;
    unsigned long remaining_data = numbytes;
    read_sd_block(IndirectBlock.indirectblock_array, file->inode->indirect_block_num);
    char data_to_write[SOFTWARE_DISK_BLOCK_SIZE];
    uint16_t dataForBlock = 0;
    uint16_t block_number;
    while(file->d->inode_idx < sizeof(IndirectBlock.indirectblock_array) && bytes_written < numbytes)
    {
        bzero(data_to_write, SOFTWARE_DISK_BLOCK_SIZE *sizeof(char));
        dataForBlock = remaining_data < SOFTWARE_DISK_BLOCK_SIZE - offset_bytes? remaining_data : SOFTWARE_DISK_BLOCK_SIZE- offset_bytes;
        if(!IndirectBlock.indirectblock_array[file->inode->indirect_block_num])
        {
            memcpy(data_to_write + offset_bytes, data + bytes_written, dataForBlock);
            block_number = locate_and_fill_free_data_bitmap();
            IndirectBlock.indirectblock_array[file->inode->indirect_block_num] = block_number;
            if (block_number)
            {
                file->inode->size += dataForBlock;
            }
            else
            {
                block_number = IndirectBlock.indirectblock_array[file->inode->indirect_block_num];
                read_sd_block(data_to_write, block_number);
                file->inode->size += dataForBlock <= strlen(data_to_write) ? 0 : dataForBlock - strlen(data_to_write);
                memcpy(data_to_write  + offset_bytes, data + bytes_written, dataForBlock);
                write_sd_block(data_to_write, block_number);
            }
            if (!block_number)
            {
                fserror = FS_OUT_OF_SPACE;
                InodeBlock inodeBlock;
                read_sd_block(inodeBlock.inode_array, 1);
                inodeBlock.inode_array[file->d->inode_idx] = file->inode;
                write_sd_block(inodeBlock.inode_array, 1);
                write_sd_block(IndirectBlock.indirectblock_array, file->inode->indirect_block_num);
                file->position += dataForBlock;
                return bytes_written;
            }
            if (dataForBlock + bytes_written == numbytes)
            {
                InodeBlock inodeBlock;
                read_sd_block(inodeBlock.inode_array, 1);
                inodeBlock.inode_array[file->d->inode_idx] = file->inode;
                write_sd_block(inodeBlock.inode_array, 1);
                write_sd_block(IndirectBlock.indirectblock_array, file->inode->indirect_block_num);
                file->position += dataForBlock;
                return bytes_written + dataForBlock;
            }
            bytes_written += dataForBlock;
            file->position += dataForBlock;
            file->d->inode_idx++;
            remaining_data -= dataForBlock;
            offset_bytes = 0;
        }
        write_sd_block(IndirectBlock.indirectblock_array, file->inode->indirect_block_num);
        fserror = FS_EXCEEDS_MAX_FILE_SIZE;
        return bytes_written;
    }
}

unsigned long write_file(File file, void *buf, unsigned long numbytes)
{
    fserror = FS_NONE;
    char *data = (char *) buf;
    if(file->mode == READ_ONLY)
    {
        fserror = FS_FILE_READ_ONLY;
        return 0;
    }
    if(file->position >= MAX_FILE_SIZE)
    {
        fserror = FS_EXCEEDS_MAX_FILE_SIZE;
        return 0;
    }
    unsigned long bytes_written = 0;
    file->d->inode_idx = file->position % SOFTWARE_DISK_BLOCK_SIZE;
    int starting_at_direct = file->d->inode_idx < NUM_DIRECT_BLOCKS;
    uint16_t offset_bytes = file->position / SOFTWARE_DISK_BLOCK_SIZE;
    if(starting_at_direct && 0 < numbytes)
    {
        bytes_written += write_to_direct_block(file, data, numbytes, offset_bytes);
        offset_bytes = 0;
        file->d->inode_idx = file->position / SOFTWARE_DISK_BLOCK_SIZE;
        numbytes -= bytes_written;
    }
    if(!starting_at_direct || 0 < numbytes)
    {
        bytes_written += write_to_indirect_block(file, data + bytes_written, numbytes, offset_bytes);
    }
    return bytes_written;
}
*/
int seek_file(File file, unsigned long bytepos)
{
    fserror = FS_NONE;
    if(bytepos >= file_length(file)){
        fserror = FS_IO_ERROR;
        return 0;
    }
    if(bytepos >= MAX_FILE_SIZE)
    {
        fserror = FS_EXCEEDS_MAX_FILE_SIZE;
        return 0;
    }
    file->position = bytepos;
    fserror = FS_NONE;
    return 1;
}

unsigned long file_length(File file)
{
    fserror = FS_NONE;
    if (file_exists(file->d->name))
    {
        //count number of full blocks in inode ie #1's 
        //could do a while loop going through all blocks
        //until you hit the end ie MAX_FILE_SIZE, gonna
        //need some bit manipulation
    }
    fserror = FS_FILE_NOT_FOUND;
    return 0;
}

int delete_file(char *name)
{
    
    fserror = FS_NONE;              //fs error
    if(!file_exists(name))
    {
        fserror = FS_FILE_NOT_FOUND;
        return 0;
    }
    if(is_file_open(name))
    {
        fserror = FS_FILE_OPEN;
        return 0;
    }

    DirBlock dirblock_holder;               //temp 
    InodeBlock inodeBlockHolder;
    File file;
    DirEntry direntry_holder; 
    Inode inode_holder;
    IndirectBlock_Block indirectblock_holder;
    int dirblock_num;
    int dir_num;
    for (int i = FIRST_DIR_ENTRY_BLOCK; i <= LAST_DIR_ENTRY_BLOCK; i++) //find dirblock and dirEntry position in block
    {
        read_sd_block(&dirblock_holder, i);
        for (int j = 0; j <= DIR_ENTRY_PER_BLOCK; j++)
        {
            if(name == dirblock_holder.dir_array[j].name)
            {
                dirblock_num = i;
                dir_num = j;
                break;
            }
        }
    }
    
    direntry_holder = dirblock_holder.dir_array[dir_num];                                            //find the inode
    file->d = &direntry_holder;
    int block_to_copy = ((file->d->inode_idx * 512) / SOFTWARE_DISK_BLOCK_SIZE) + 2 ;
    int inode_spot = (file->d->inode_idx * 512) % SOFTWARE_DISK_BLOCK_SIZE;
    read_sd_block(&inodeBlockHolder, block_to_copy);
    inode_holder = inodeBlockHolder.inode_array[inode_spot];
    file->inode = &inode_holder;
    
    read_sd_block(&indirectblock_holder, inode_holder.indirect_block_num);                //clear indirect
    for(int i = 0; i < sizeof(indirectblock_holder); i++)
    {
        if(indirectblock_holder.indirectblock_array[i] == 0){
            continue;
        }else{
            zero_block_at(indirectblock_holder.indirectblock_array[i]);
            free_data_block(indirectblock_holder.indirectblock_array[i]);
        }
    }
    
    for(int i = 0; i < sizeof(inode_holder.direct_block_num); i++){           //clear direct
        if(file->inode->direct_block_num[i] == 0){
            continue;
        }else{
            zero_block_at(file->inode->direct_block_num[i]);
            free_data_block(indirectblock_holder.indirectblock_array[i]);
        }
    }
    
    bzero(&inodeBlockHolder.inode_array[inode_spot], sizeof(char[32]));      //clear inode
    write_sd_block(&inodeBlockHolder, block_to_copy);
    free_inode_block((int)file->d->inode_idx);
    
    bzero(&dirblock_holder.dir_array[dirblock_num], sizeof(char[512]));                 //clear direntry
    write_sd_block(&dirblock_holder,dirblock_num);
    return 1;
}

void zero_block_at(int block)
{
    char zeros [SOFTWARE_DISK_BLOCK_SIZE];
    bzero(zeros,SOFTWARE_DISK_BLOCK_SIZE);
    write_sd_block(zeros, block);
}

int file_exists(char *name)
{
    DirBlock holder;
    for (int i = FIRST_DIR_ENTRY_BLOCK; i <= LAST_DIR_ENTRY_BLOCK; i++)
    {
        read_sd_block(&holder, i);
        for (int j = 0; j <= DIR_ENTRY_PER_BLOCK; j++)
        {
            if(name == holder.dir_array[j].name)
            {
                return 1;
            }
        }
    }
    return 0;
}

int is_file_open(char *name)
{
    //can assume that file exsits
    File file;
    DirBlock holder;
    DirEntry entry_holder;
    for (int i = FIRST_DIR_ENTRY_BLOCK; i <= LAST_DIR_ENTRY_BLOCK; i++)
    {
        read_sd_block(&holder, i);
        for (int j = 0; j <= DIR_ENTRY_PER_BLOCK; j++)
        {
            entry_holder = holder.dir_array[j]; 
            if(! strncmp(entry_holder.name , name, MAX_NAME_SIZE))
            {   
                file->d = &entry_holder;
                break;
            }
        }
    }
    if (file->d->file_is_open)
    {
        return 1;
    }
    return 0;
}

//unsure so far???
//change to meet our specifications
int check_structure_alignment(void)
{
    printf("Expecting sizeof(Inode) = 32, actual = %lu\n", sizeof(Inode));
    printf("Expecting sizeof(IndirectBlock = %d, actual %lu\n", SOFTWARE_DISK_BLOCK_SIZE, sizeof(IndirectBlock_Block));
    printf("Expecting sizeof(InodeBlock = %d, actual %lu\n", SOFTWARE_DISK_BLOCK_SIZE, sizeof(InodeBlock));
    printf("Expecting sizeof(DirEntry) = 512, actual = %lu\n", sizeof(DirEntry));
    printf("Expecting sizeof(DirEntryBlock = %d, actual %lu\n", SOFTWARE_DISK_BLOCK_SIZE, sizeof(DirBlock));
    //printf("Expecting sizeof(FreeBitmap = %d, actual %lu\n", SOFTWARE_DISK_BLOCK_SIZE, sizeof(FreeBitmap));

    if ((int)sizeof(Inode) != (int)32 ||
        (int)sizeof(IndirectBlock_Block) != (int)SOFTWARE_DISK_BLOCK_SIZE ||
        (int)sizeof(InodeBlock) != (int)SOFTWARE_DISK_BLOCK_SIZE ||
        (int)sizeof(DirEntry) != (int)512 ||
        (int)sizeof(DirBlock) != (int)SOFTWARE_DISK_BLOCK_SIZE){ //|| 
        //sizeof(FreeBitmap) != SOFTWARE_DISK_BLOCK_SIZE) {
            fserror = FS_IO_ERROR;
            return 0;
    }
    else {
        return 1;
    }
    
}


int locate_and_fill_free_data_bitmap() // returns the first free data block. (70-4095)
{
    long long data_bitmap[SOFTWARE_DISK_BLOCK_SIZE/sizeof(long long)];
    read_sd_block(data_bitmap, DATA_BITMAP_BLOCK);
    int location = 0;
    long long max = 0xFFFFFFFFFFFFFFFF;
    while(location < 4096){
        for(int i = 0; i < sizeof(data_bitmap) - 1; i++)
        {
            if(data_bitmap[i] == max)
            {
                location += sizeof(long long) * 8;
            }else
            {
                long long shift = 1;
                for(int j = 0; j < sizeof(long long) * 8; j++)
                {
                    if(! (data_bitmap[i] & shift))
                    {
                        data_bitmap[i] = data_bitmap[i] | shift;
                        write_sd_block(data_bitmap, DATA_BITMAP_BLOCK);
                        return location + FIRST_DATA_BLOCK;
                    }else
                    {
                        location++;
                        shift = shift<<1;
                    }
                }
            }
        }
    }
    return 0;
}

int locate_and_fill_free_inode_bitmap() // returns the first free inode spot 0-511
{
    long long inode_bitmap[TOTAL_INODES/sizeof(long long)];
    int location = 0;
    long long max = 0xFFFFFFFFFFFFFFFF;

    
    for(int i = 0; i <= TOTAL_INODES/sizeof(long long) - 1; i++)
    {
        if(inode_bitmap[i] == max)
        {
            location += sizeof(long long) * 8;
        }else
        {
            long long shift = 1;
            for(int j = 0; j < sizeof(long long) * 8; j++)
            {
                if(! (inode_bitmap[i] & shift))
                {
                    inode_bitmap[i] = inode_bitmap[i] | shift;
                    write_sd_block(inode_bitmap, INODE_BITMAP_BLOCK);
                    return location;
                }else
                {
                    location++;
                    shift = shift<<1;
                }
            }
        }
    }

    return 0;
}
void free_data_block(int block) // flips bit in data bitmap to show that it is free
{
    //block = block + FIRST_DATA_BLOCK;
    int long_location = block/sizeof(long long);
    int bit_location = block%sizeof(long long);
    long long bit_convert = 1;

    long long data_bitmap[SOFTWARE_DISK_BLOCK_SIZE/sizeof(long long)];
    read_sd_block(data_bitmap, DATA_BITMAP_BLOCK);

    bit_convert = bit_convert<< bit_location;
    data_bitmap[block] = data_bitmap[block] &(~bit_convert);
    write_sd_block(data_bitmap,DATA_BITMAP_BLOCK);
}

void free_inode_block(int block) // flips bit in inode bitmap to show that it is free
{
    //block = block + FIRST_INODE_BLOCK;
    int long_location = block/sizeof(long long);
    int bit_location = block%sizeof(long long);
    long long bit_convert = 1;

    long long inode_bitmap[SOFTWARE_DISK_BLOCK_SIZE/sizeof(long long)];
    read_sd_block(inode_bitmap, INODE_BITMAP_BLOCK);

    bit_convert = bit_convert<< bit_location;
    inode_bitmap[block] = inode_bitmap[block] &(~bit_convert);
    write_sd_block(inode_bitmap,INODE_BITMAP_BLOCK);
}
