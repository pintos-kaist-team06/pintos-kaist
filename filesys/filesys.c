#include "filesys/filesys.h"

#include <debug.h>
#include <stdio.h>
#include <string.h>

#include "devices/disk.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format(void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void filesys_init(bool format) {
    filesys_disk = disk_get(0, 1);
    if (filesys_disk == NULL)
        PANIC("hd0:1 (hdb) not present, file system initialization failed");

    inode_init();

#ifdef EFILESYS
    fat_init();

    if (format)
        do_format();

    fat_open();
#else
    /* Original FS */
    free_map_init();

    if (format)
        do_format();

    free_map_open();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void filesys_done(void) {
    /* Original FS */
#ifdef EFILESYS
    fat_close();
#else
    free_map_close();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool filesys_create(const char *name, off_t initial_size) {
    disk_sector_t inode_sector = 0;     // 새 파일이 저장될 disk
    struct dir *dir = dir_open_root();  // dir = root_dir임

    /*dir != NULL:      루트 디렉토리가 성공적으로 열렸는지 확인합니다.
    free_map_allocate:  새 파일의 inode를 위한 빈 디스크 섹터를 할당하려고 시도. 성공하면 inode_sector는 할당된 섹터 번호로 업데이트됨.
    inode_create:       할당된 섹터에 지정된 초기 크기로 inode를 생성
    dir_add:            새 파일에 대한 디렉토리 항목을 추가, 파일 이름과 inode 섹터를 디렉토리에 연결.*/
    bool success = (dir != NULL && free_map_allocate(1, &inode_sector) && inode_create(inode_sector, initial_size) && dir_add(dir, name, inode_sector));

    if (!success && inode_sector != 0)
        free_map_release(inode_sector, 1);

    dir_close(dir);  // dir 닫음. 파일 못만들었어도 닫음

    return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *filesys_open(const char *name) {
    struct dir *dir = dir_open_root();
    struct inode *inode = NULL;

    if (dir != NULL)
        dir_lookup(dir, name, &inode);
    dir_close(dir);

    return file_open(inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool filesys_remove(const char *name) {
    struct dir *dir = dir_open_root();
    bool success = dir != NULL && dir_remove(dir, name);
    dir_close(dir);

    return success;
}

/* Formats the file system. */
static void do_format(void) {
    printf("Formatting file system...");

#ifdef EFILESYS
    /* Create FAT and save it to the disk. */
    fat_create();
    fat_close();
#else
    free_map_create();
    if (!dir_create(ROOT_DIR_SECTOR, 16))
        PANIC("root directory creation failed");
    free_map_close();
#endif

    printf("done.\n");
}
