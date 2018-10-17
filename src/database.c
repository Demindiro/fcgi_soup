#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "util/string.h"
#include "../include/database.h"


// 1U << 30 == 1 Gigabyte
// This should be sufficient for most usecases (larger databases are probably best
// stored inside multiple files)
#define MMAP_SIZE (1U << 30)



static size_t get_offset(database *db, uint8_t field)
{
	size_t offset = 0;
	for (size_t i = 0; i < field; i++)
		offset += db->field_lengths[i];
	return offset;
}


static char *get_entry_ptr(database *db, uint8_t keyfield, const char *key)
{
	size_t f_offset = get_offset(db, keyfield);
	for (size_t i = 0; i < *db->count; i++) {
		char *entry = db->data + (i * db->entry_length);
		if (memcmp(entry + f_offset, key, db->field_lengths[i]) == 0)
			return entry;
	}
	return NULL;
}


static int get_map_name(database *db, uint8_t field, char *buf, size_t size)
{
	size_t strl = strlen(db->name);
	if (strl > size - 5)
		return -1;
	memcpy(buf, db->name, strl);
	buf[strl++] = 'm';
	buf[strl++] = 'a';
	buf[strl++] = 'p';
	buf[strl++] = field < 10 ? field + '0' : (field - 10) + 'A';
	buf[strl  ] =  0;
	return 0;
}



int database_create(database *db, const char *file, uint8_t field_count, uint16_t *field_lengths)
{
	int fd = open(file, O_RDWR | O_CREAT, 0644);
	if (fd < 0)
		return -1;
	// TODO this should be done automatically
	if (ftruncate(fd, 4096 << 3) < 0) {
		close(fd);
		return -1;
	}
	char *map = db->mapptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (db->mapptr == NULL)
		return -1;
	
	db->count = (uint32_t *)map;
	*db->count = 0;
	map += sizeof(db->count);
	
	*((uint8_t  *)map) = db->field_count  = field_count;
	map += sizeof(db->field_count);
	db->entry_length = 0;
	for (size_t i = 0; i < field_count; i++) {
		*((uint16_t *)map) = db->field_lengths[i] = field_lengths[i];
		map += sizeof(*db->field_lengths);
		db->entry_length  += field_lengths[i];
	}
	
	memset(db->maps, 0, sizeof(db->maps));
	for (size_t i = 0; i < field_count; i++) {

	}

	db->data = map;
	msync(db->mapptr, map - (char *)db->mapptr, MS_ASYNC);
	close(fd);
	return 0;
}


int database_load(database *db, const char *file)
{
	int fd = open(file, O_RDWR);
	if (fd < 0)
		return -1;
	char *map = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == NULL)
		return -1;
	db->mapptr = map;
	db->count = (uint32_t *)map;
	map += sizeof(db->count);
	db->field_count = *((uint8_t *)map);
	map += sizeof(db->field_count);
	db->entry_length = 0;
	for (size_t i = 0; i < db->field_count; i++) {
		db->field_lengths[i] = *((uint16_t *)map);
		db->entry_length    += db->field_lengths[i];
		map += sizeof(*db->field_lengths);
	}
	for (size_t i = 0; i < db->field_count; i++) {
		char name[256];
		if (get_map_name(db, i, name, sizeof(name)) < 0)
			continue; // TODO
		int mfd = open(name, O_RDWR);
		if (mfd < 0) {
			db->maps[i].data = NULL;
		} else {
			char *kmap = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			if (map == NULL) {
				/* TODO */
				continue;
			}
			db->maps[i].count  = (uint32_t *)kmap;
			db->maps[i].data   = kmap + sizeof(*db->maps[i].count);
		}
	}
	db->data = map;
	return 0;
}


void database_free(database *db)
{
	for (size_t i = 0; i < db->field_count; i++) {
		if (db->maps[i].data != NULL)
			munmap(db->maps[i].data - sizeof(*db->maps[i].count), MMAP_SIZE);
	}
	munmap(db->mapptr, MMAP_SIZE);
}


int database_create_map(database *db, uint8_t field)
{
	char name[256];
	if (field >= db->field_count ||
	    db->maps[field].data != NULL ||
	    get_map_name(db, field, name, sizeof(name)) < 0)
		return -1;
	int fd = open(name, O_RDWR | O_CREAT);
	if (fd < 0)
		return -1;
	// TODO
	if (ftruncate(fd, 4096 << 3) < 0) {
		close(fd);
		return -1;
	}
	char *map = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (map == NULL) {
		close(fd);
		return -1;
	}
	db->maps[field].count = (uint32_t *)map;
	db->maps[field].data  = map + sizeof(*db->maps[field].count);
	return 0;	
}


int database_get(database *db, char *buf, uint8_t keyfield, const char *key)
{
	const char *entry = get_entry_ptr(db, keyfield, key);
	if (entry == NULL)
		return -1;
	memcpy(buf, entry, db->entry_length);
	return 0;
}


int database_add(database *db, const char *entry)
{
	memcpy(db->data + (*db->count * db->entry_length), entry, db->entry_length);
	(*db->count)++;
	msync(db->mapptr, MMAP_SIZE, MS_ASYNC);
	return 0;
}


int database_del(database *db, uint8_t keyfield, const char *key)
{
	char *entry = get_entry_ptr(db, keyfield, key);
	if (entry == NULL)
		return -1;
	memset(entry, 0, db->entry_length);
	return 0;
}


int database_get_field(database *db, char *buf, const char *entry, uint8_t field)
{
	size_t f_offset = get_offset(db, field);
	memcpy(buf, entry + f_offset, db->field_lengths[field]);
	return 0;
}


int database_set_field(database *db, char *entry, uint8_t field, const void *val)
{
	size_t f_offset = get_offset(db, field);
	memcpy(entry + f_offset, val, db->field_lengths[field]);
	return 0;
}


uint32_t database_get_range(database *db, const char ***entries, uint8_t field, const void *key0, const void *key1)
{
	if (field >= db->field_count)
		return -1;

	database_map map = db->maps[field];
	if (map.data != NULL) {
		size_t i = 0, start, end;
		size_t flen = db->field_lengths[field], elen = flen + sizeof(*db->count);
		for ( ; i < *map.count; i++) {
			char *entry = map.data + (i * elen);
			if (memcmp(key0, entry, flen) <= 0) {
				start = i;
				goto found_start;
			}
		}
		*entries = NULL;
		return 0;
	found_start:
		for ( ; i < *map.count; i++) {
			char *entry = map.data + (i * elen);
			if (memcmp(entry, key1, flen) < 0) {
				end = i - 1;
				goto found_end;
			}
		}
		end = *map.count;
	found_end:;
		size_t count = end - start;
		*entries = malloc(count * sizeof(char *));
		for (size_t j = 0, i = start; i < end; i++, j++) {
			size_t index = *((uint32_t *)map.data + (i * elen) + flen);
			(*entries)[j] = db->data + (index * db->entry_length);
		}
		return count;
	} else {
		size_t size = 16, len = 0, count = 0;
		size_t f_offset = get_offset(db, field);
		char *buf = malloc(size);
		for (size_t i = 0; i < *db->count; i++) {
			char *entry = db->data + (i * db->entry_length);
			if (memcmp(key0, entry + f_offset, db->entry_length) <= 0 &&
			    memcmp(key0, entry + f_offset, db->entry_length) <= 0) {
				if (buf_write(&buf, &len, &size, entry, sizeof(entry))) {
					free(buf);
					return -1;
				}
				count++;
			}
		}
		*entries = realloc(buf, len);
		return count;
	}
}
