/*
 *************************************************************************************************
 *
 * Load Elf files 
 *
 * Status: Untested 
 *
 * Copyright 2015 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Jochen Karrer ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of Jochen Karrer.
 *
 *************************************************************************************************
 */
#include "compiler_extensions.h"
#include "elfloader.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include "byteorder.h"

/* 32-bit ELF base types. */
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t Elf32_Sword;
typedef uint32_t Elf32_Word;

/* 64-bit ELF base types. */
typedef uint64_t Elf64_Addr;
typedef uint16_t Elf64_Half;
typedef int16_t Elf64_SHalf;
typedef uint64_t Elf64_Off;
typedef int32_t Elf64_Sword;
typedef uint32_t Elf64_Word;
typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;

#define EI_MAG0     (0)
#define EI_MAG1     (1)
#define EI_MAG2     (2)
#define EI_MAG3     (3)
#define EI_CLASS    (4)
#define EI_DATA     (5)
#define EI_VERSION  (6)
#define EI_PAD      (7)
#define EI_NIDENT   (16)

#define ELFCLASSNONE    (0)
#define ELFCLASS32      (1)
#define ELFCLASS64      (2)

#define ELFDATANONE (0)
#define ELFDATA2LSB (1)
#define ELFDATA2MSB (2)

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half e_type;
    Elf32_Half e_machine;
    Elf32_Word e_version;
    Elf32_Addr e_entry;         /* Entry point */
    Elf32_Off e_phoff;
    Elf32_Off e_shoff;
    Elf32_Word e_flags;
    Elf32_Half e_ehsize;
    Elf32_Half e_phentsize;
    Elf32_Half e_phnum;
    Elf32_Half e_shentsize;
    Elf32_Half e_shnum;
    Elf32_Half e_shstrndx;
} Elf32_Ehdr;

typedef struct elf32_shdr {
    Elf32_Word sh_name;
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off sh_offset;
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
} Elf32_Shdr;

#define PT_NULL     (0)
#define PT_LOAD     (1)
#define PT_DYNAMIC  (2)
#define PT_INTERP   (3)
#define PT_NOTE     (4)
#define PT_SHLIB    (5)
#define PT_PHDR     (6)

typedef struct {
    Elf32_Word p_type;
    Elf32_Off p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
} Elf32_Phdr;

typedef struct {
    unsigned char e_ident[EI_NIDENT];   /* ELF "magic number" */
    Elf64_Half e_type;
    Elf64_Half e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;         /* Entry point virtual address */
    Elf64_Off e_phoff;          /* Program header table file offset */
    Elf64_Off e_shoff;          /* Section header table file offset */
    Elf64_Word e_flags;
    Elf64_Half e_ehsize;
    Elf64_Half e_phentsize;
    Elf64_Half e_phnum;
    Elf64_Half e_shentsize;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    Elf64_Word sh_name;         /* Section name, index in string tbl */
    Elf64_Word sh_type;         /* Type of section */
    Elf64_Xword sh_flags;       /* Miscellaneous section attributes */
    Elf64_Addr sh_addr;         /* Section virtual addr at execution */
    Elf64_Off sh_offset;        /* Section file offset */
    Elf64_Xword sh_size;        /* Size of section in bytes */
    Elf64_Word sh_link;         /* Index of another section */
    Elf64_Word sh_info;         /* Additional section information */
    Elf64_Xword sh_addralign;   /* Section alignment */
    Elf64_Xword sh_entsize;     /* Entry size if section holds table */
} Elf64_Shdr;

typedef struct {
    Elf64_Word p_type;          /* Type of segment */
    Elf64_Word p_flags;         /* Segment attributes */
    Elf64_Off p_offset;         /* Offset in file */
    Elf64_Addr p_vaddr;         /* Virtual address in memory */
    Elf64_Addr p_paddr;         /* Reserved */
    Elf64_Xword p_filesz;       /* Size of segment in file */
    Elf64_Xword p_memsz;        /* Size of segment in memory */
    Elf64_Xword p_align;        /* Alignment of segment */
} Elf64_Phdr;

/*
 **********************************************************************
 * \fn bool Elf_CheckElf(const char *filename)
 * Check if a file is in elf Format by reading the magic.
 **********************************************************************
 */
bool
Elf_CheckElf(const char *filename)
{
    FILE *file = NULL;
    bool retval = true;
    Elf32_Ehdr elf32Hdr;
    if ((file = fopen(filename, "r")) == NULL) {
        perror("[E] Error opening file:");
        exit(1);
    }
    fseek(file, 0, SEEK_SET);
    if (fread(&elf32Hdr, 1, sizeof(Elf32_Ehdr), file) == 0) {
        retval = false; 
    }  
    fclose(file);

    if ((elf32Hdr.e_ident[EI_MAG0] == 0x7f) &&
        (elf32Hdr.e_ident[EI_MAG1] == 'E') &&
        (elf32Hdr.e_ident[EI_MAG2] == 'L') && (elf32Hdr.e_ident[EI_MAG3] == 'F')) {
        return retval;
    }
    return false;
}

/**
 **********************************************************************************
 * \fn static void Elf32_HeaderLittleEndianToHost(Elf32_Ehdr * elf32Hdr)
 * In place conversion of Little Endian ELF header to host bytorder
 **********************************************************************************
 */
static void
Elf32_HeaderLittleEndianToHost(Elf32_Ehdr * elf32Hdr)
{
    elf32Hdr->e_type = le16_to_host(elf32Hdr->e_type);
    elf32Hdr->e_machine = le16_to_host(elf32Hdr->e_machine);
    elf32Hdr->e_version = le32_to_host(elf32Hdr->e_version);
    elf32Hdr->e_entry = le32_to_host(elf32Hdr->e_entry);
    elf32Hdr->e_phoff = le32_to_host(elf32Hdr->e_phoff);
    elf32Hdr->e_shoff = le32_to_host(elf32Hdr->e_shoff);
    elf32Hdr->e_flags = le32_to_host(elf32Hdr->e_flags);
    elf32Hdr->e_ehsize = le16_to_host(elf32Hdr->e_ehsize);
    elf32Hdr->e_phentsize = le16_to_host(elf32Hdr->e_phentsize);
    elf32Hdr->e_phnum = le16_to_host(elf32Hdr->e_phnum);
    elf32Hdr->e_shentsize = le16_to_host(elf32Hdr->e_shentsize);
    elf32Hdr->e_shnum = le16_to_host(elf32Hdr->e_shnum);
    elf32Hdr->e_shstrndx = le16_to_host(elf32Hdr->e_shstrndx);
}

/**
 **************************************************************************
 * \fn static void Elf32_HeaderBigEndianToHost(Elf32_Ehdr * elf32Hdr)
 * In place conversion of Big Endian Elf header to Host byte order.
 **************************************************************************
 */
static void
Elf32_HeaderBigEndianToHost(Elf32_Ehdr * elf32Hdr)
{
    elf32Hdr->e_type = be16_to_host(elf32Hdr->e_type);
    elf32Hdr->e_machine = be16_to_host(elf32Hdr->e_machine);
    elf32Hdr->e_version = be32_to_host(elf32Hdr->e_version);
    elf32Hdr->e_entry = be32_to_host(elf32Hdr->e_entry);
    elf32Hdr->e_phoff = be32_to_host(elf32Hdr->e_phoff);
    elf32Hdr->e_shoff = be32_to_host(elf32Hdr->e_shoff);
    elf32Hdr->e_flags = be32_to_host(elf32Hdr->e_flags);
    elf32Hdr->e_ehsize = be16_to_host(elf32Hdr->e_ehsize);
    elf32Hdr->e_phentsize = be16_to_host(elf32Hdr->e_phentsize);
    elf32Hdr->e_phnum = be16_to_host(elf32Hdr->e_phnum);
    elf32Hdr->e_shentsize = be16_to_host(elf32Hdr->e_shentsize);
    elf32Hdr->e_shnum = be16_to_host(elf32Hdr->e_shnum);
    elf32Hdr->e_shstrndx = be16_to_host(elf32Hdr->e_shstrndx);
}

static void 
Elf64_HeaderLittleEndianToHost(Elf64_Ehdr *elf64Hdr)
{
    elf64Hdr->e_type = le16_to_host(elf64Hdr->e_type);
    elf64Hdr->e_machine = le16_to_host(elf64Hdr->e_machine);
    elf64Hdr->e_version = le32_to_host(elf64Hdr->e_version);
    elf64Hdr->e_entry = le64_to_host(elf64Hdr->e_entry);
    elf64Hdr->e_phoff = le64_to_host(elf64Hdr->e_phoff);
    elf64Hdr->e_shoff = le64_to_host(elf64Hdr->e_shoff);
    elf64Hdr->e_flags = le32_to_host(elf64Hdr->e_flags);
    elf64Hdr->e_ehsize = le16_to_host(elf64Hdr->e_ehsize),
    elf64Hdr->e_phentsize = le16_to_host(elf64Hdr->e_phentsize);
    elf64Hdr->e_phnum = le16_to_host(elf64Hdr->e_phnum);
    elf64Hdr->e_shentsize = le16_to_host(elf64Hdr->e_shentsize);
    elf64Hdr->e_shnum = le16_to_host(elf64Hdr->e_shnum);
    elf64Hdr->e_shstrndx = le16_to_host(elf64Hdr->e_shstrndx);
}

static void 
Elf64_HeaderBigEndianToHost(Elf64_Ehdr *elf64Hdr)
{
    elf64Hdr->e_type = be16_to_host(elf64Hdr->e_type);
    elf64Hdr->e_machine = be16_to_host(elf64Hdr->e_machine);
    elf64Hdr->e_version = be32_to_host(elf64Hdr->e_version);
    elf64Hdr->e_entry = be64_to_host(elf64Hdr->e_entry);
    elf64Hdr->e_phoff = be64_to_host(elf64Hdr->e_phoff);
    elf64Hdr->e_shoff = be64_to_host(elf64Hdr->e_shoff);
    elf64Hdr->e_flags = be32_to_host(elf64Hdr->e_flags);
    elf64Hdr->e_ehsize = be16_to_host(elf64Hdr->e_ehsize),
    elf64Hdr->e_phentsize = be16_to_host(elf64Hdr->e_phentsize);
    elf64Hdr->e_phnum = be16_to_host(elf64Hdr->e_phnum);
    elf64Hdr->e_shentsize = be16_to_host(elf64Hdr->e_shentsize);
    elf64Hdr->e_shnum = be16_to_host(elf64Hdr->e_shnum);
    elf64Hdr->e_shstrndx = be16_to_host(elf64Hdr->e_shstrndx);
}

/**
 *****************************************************************************
 * \fn void Elf32_PHeaderLittleEndianToHost(Elf32_Phdr * elf32PHdr)
 * In place conversion of Little Endian PHeader to Host byte order
 *****************************************************************************
 */
static void
Elf32_PHeaderLittleEndianToHost(Elf32_Phdr * elf32PHdr)
{
    elf32PHdr->p_type = le32_to_host(elf32PHdr->p_type);
    elf32PHdr->p_offset = le32_to_host(elf32PHdr->p_offset);
    elf32PHdr->p_vaddr = le32_to_host(elf32PHdr->p_vaddr);
    elf32PHdr->p_paddr = le32_to_host(elf32PHdr->p_paddr);
    elf32PHdr->p_filesz = le32_to_host(elf32PHdr->p_filesz);
    elf32PHdr->p_memsz = le32_to_host(elf32PHdr->p_memsz);
    elf32PHdr->p_flags = le32_to_host(elf32PHdr->p_flags);
    elf32PHdr->p_align = le32_to_host(elf32PHdr->p_align);
}

/**
 ***********************************************************************
 * \fn void Elf32_PHeaderBigEndianToHost(Elf32_Phdr * elf32PHdr)
 * In place conversion of Big endian PHeader to Host byte order
 ***********************************************************************
 */
static void
Elf32_PHeaderBigEndianToHost(Elf32_Phdr * elf32PHdr)
{
    elf32PHdr->p_type = be32_to_host(elf32PHdr->p_type);
    elf32PHdr->p_offset = be32_to_host(elf32PHdr->p_offset);
    elf32PHdr->p_vaddr = be32_to_host(elf32PHdr->p_vaddr);
    elf32PHdr->p_paddr = be32_to_host(elf32PHdr->p_paddr);
    elf32PHdr->p_filesz = be32_to_host(elf32PHdr->p_filesz);
    elf32PHdr->p_memsz = be32_to_host(elf32PHdr->p_memsz);
    elf32PHdr->p_flags = be32_to_host(elf32PHdr->p_flags);
    elf32PHdr->p_align = be32_to_host(elf32PHdr->p_align);
}


static void
Elf64_PHeaderLittleEndianToHost(Elf64_Phdr * elf64PHdr)
{
    elf64PHdr->p_type = le32_to_host(elf64PHdr->p_type);
    elf64PHdr->p_flags = le32_to_host(elf64PHdr->p_flags);
    elf64PHdr->p_offset = le64_to_host(elf64PHdr->p_offset); 
    elf64PHdr->p_vaddr = le64_to_host(elf64PHdr->p_vaddr);
    elf64PHdr->p_paddr = le64_to_host(elf64PHdr->p_paddr);
    elf64PHdr->p_filesz = le64_to_host(elf64PHdr->p_filesz);
    elf64PHdr->p_memsz = le64_to_host(elf64PHdr->p_memsz);
    elf64PHdr->p_align = le64_to_host(elf64PHdr->p_align);
}

static void
Elf64_PHeaderBigEndianToHost(Elf64_Phdr * elf64PHdr)
{
    elf64PHdr->p_type = be32_to_host(elf64PHdr->p_type);
    elf64PHdr->p_flags = be32_to_host(elf64PHdr->p_flags);
    elf64PHdr->p_offset = be64_to_host(elf64PHdr->p_offset); 
    elf64PHdr->p_vaddr = be64_to_host(elf64PHdr->p_vaddr);
    elf64PHdr->p_paddr = be64_to_host(elf64PHdr->p_paddr);
    elf64PHdr->p_filesz = be64_to_host(elf64PHdr->p_filesz);
    elf64PHdr->p_memsz = be64_to_host(elf64PHdr->p_memsz);
    elf64PHdr->p_align = be64_to_host(elf64PHdr->p_align);
}

/**
 **************************************************************************************
 * \fn static void Elf32_SHeaderLittleEndianToHost(Elf32_Shdr *elf32Shdr);
 * In place conversion of little endian section header to host byte order.
 **************************************************************************************
 */
__UNUSED__ static void
Elf32_SHeaderLittleEndianToHost(Elf32_Shdr *elf32Shdr) {
    elf32Shdr->sh_name = le32_to_host(elf32Shdr->sh_name);
    elf32Shdr->sh_type = le32_to_host(elf32Shdr->sh_type);
    elf32Shdr->sh_flags = le32_to_host(elf32Shdr->sh_flags);
    elf32Shdr->sh_addr = le32_to_host(elf32Shdr->sh_addr);
    elf32Shdr->sh_offset = le32_to_host(elf32Shdr->sh_offset);
    elf32Shdr->sh_size = le32_to_host(elf32Shdr->sh_size);
    elf32Shdr->sh_link = le32_to_host(elf32Shdr->sh_link);
    elf32Shdr->sh_info = le32_to_host(elf32Shdr->sh_info);
    elf32Shdr->sh_addralign = le32_to_host(elf32Shdr->sh_addralign);
    elf32Shdr->sh_entsize = le32_to_host(elf32Shdr->sh_entsize);
}

/**
 **************************************************************************************
 * static void Elf32_SHeaderBigEndianToHost(Elf32_Shdr *elf32Shdr); 
 * In place conversion of Big endian Section header to host byte order
 **************************************************************************************
 */
__UNUSED__ static void
Elf32_SHeaderBigEndianToHost(Elf32_Shdr *elf32Shdr) 
{
    elf32Shdr->sh_name = be32_to_host(elf32Shdr->sh_name);
    elf32Shdr->sh_type = be32_to_host(elf32Shdr->sh_type);
    elf32Shdr->sh_flags = be32_to_host(elf32Shdr->sh_flags);
    elf32Shdr->sh_addr = be32_to_host(elf32Shdr->sh_addr);
    elf32Shdr->sh_offset = be32_to_host(elf32Shdr->sh_offset);
    elf32Shdr->sh_size = be32_to_host(elf32Shdr->sh_size);
    elf32Shdr->sh_link = be32_to_host(elf32Shdr->sh_link);
    elf32Shdr->sh_info = be32_to_host(elf32Shdr->sh_info);
    elf32Shdr->sh_addralign = be32_to_host(elf32Shdr->sh_addralign);
    elf32Shdr->sh_entsize = be32_to_host(elf32Shdr->sh_entsize);
}

__UNUSED__ static void
Elf64_SHeaderLittleEndianToHost(Elf64_Shdr *elf64Shdr) 
{
    elf64Shdr->sh_name = le32_to_host(elf64Shdr->sh_name);    
    elf64Shdr->sh_type = le32_to_host(elf64Shdr->sh_type);
    elf64Shdr->sh_flags = le64_to_host(elf64Shdr->sh_flags);
    elf64Shdr->sh_addr = le64_to_host(elf64Shdr->sh_addr);
    elf64Shdr->sh_offset = le64_to_host(elf64Shdr->sh_offset);
    elf64Shdr->sh_size = le64_to_host(elf64Shdr->sh_size);
    elf64Shdr->sh_link = le32_to_host(elf64Shdr->sh_link);
    elf64Shdr->sh_info = le32_to_host(elf64Shdr->sh_info);
    elf64Shdr->sh_addralign = le64_to_host(elf64Shdr->sh_addralign);
    elf64Shdr->sh_entsize = le64_to_host(elf64Shdr->sh_entsize);
}

__UNUSED__ static void
Elf64_SHeaderBigEndianToHost(Elf64_Shdr *elf64Shdr) 
{
    elf64Shdr->sh_name = be32_to_host(elf64Shdr->sh_name);    
    elf64Shdr->sh_type = be32_to_host(elf64Shdr->sh_type);
    elf64Shdr->sh_flags = be64_to_host(elf64Shdr->sh_flags);
    elf64Shdr->sh_addr = be64_to_host(elf64Shdr->sh_addr);
    elf64Shdr->sh_offset = be64_to_host(elf64Shdr->sh_offset);
    elf64Shdr->sh_size = be64_to_host(elf64Shdr->sh_size);
    elf64Shdr->sh_link = be32_to_host(elf64Shdr->sh_link);
    elf64Shdr->sh_info = be32_to_host(elf64Shdr->sh_info);
    elf64Shdr->sh_addralign = be64_to_host(elf64Shdr->sh_addralign);
    elf64Shdr->sh_entsize = be64_to_host(elf64Shdr->sh_entsize);
}

static void
Elf32_ReadHeader(FILE * file, Elf32_Ehdr * elf32Hdr)
{
    fseek(file, 0, SEEK_SET);
    if (fread(elf32Hdr, 1, sizeof(Elf32_Ehdr), file) <= 0) {
        fprintf(stderr,"Read Elf32_Ehdr failed\n");
        exit(1);
    }
    if (elf32Hdr->e_ident[EI_DATA] == ELFDATA2LSB) {
        Elf32_HeaderLittleEndianToHost(elf32Hdr);
        return;
    } else if (elf32Hdr->e_ident[EI_DATA] == ELFDATA2MSB) {
        Elf32_HeaderBigEndianToHost(elf32Hdr);
        return;
    } else {
        fprintf(stderr, "Unknown data encoding in elf file\n");
        exit(1);
    }
}

static void
Elf32_ReadPHeader(FILE * file, long offset, Elf32_Phdr * elf32pHdr, const Elf32_Ehdr * elf32Hdr)
{
    fseek(file, offset, SEEK_SET);
    if (fread(elf32pHdr, 1, sizeof(Elf32_Phdr), file) <= 0) {
        fprintf(stderr, "Read Elf32PHeader failed\n");
            exit(1);
    }
    if (elf32Hdr->e_ident[EI_DATA] == ELFDATA2LSB) {
        Elf32_PHeaderLittleEndianToHost(elf32pHdr);
        return;
    } else if (elf32Hdr->e_ident[EI_DATA] == ELFDATA2MSB) {
        Elf32_PHeaderBigEndianToHost(elf32pHdr);
        return;
    } else {
        fprintf(stderr, "Unknown data encoding in elf file\n");
        exit(1);
    }
}

static void
Elf64_ReadHeader(FILE * file, Elf64_Ehdr * elf64Hdr)
{
    fseek(file, 0, SEEK_SET);
    if (fread(elf64Hdr, 1, sizeof(Elf64_Ehdr), file) <= 0) {
        fprintf(stderr,"Read Elf64_Ehdr failed\n");
        exit(1);
    }
    if (elf64Hdr->e_ident[EI_DATA] == ELFDATA2LSB) {
        Elf64_HeaderLittleEndianToHost(elf64Hdr);
        return;
    } else if (elf64Hdr->e_ident[EI_DATA] == ELFDATA2MSB) {
        Elf64_HeaderBigEndianToHost(elf64Hdr);
        return;
    } else {
        fprintf(stderr, "Unknown data encoding in elf file\n");
        exit(1);
    }
}

static void
Elf64_ReadPHeader(FILE * file, off_t offset, Elf64_Phdr * elf64pHdr, const Elf64_Ehdr * elf64Hdr)
{
    fseeko(file, offset, SEEK_SET);
    if (fread(elf64pHdr, 1, sizeof(Elf64_Phdr), file) <= 0) {
        fprintf(stderr, "Read Elf32PHeader failed\n");
            exit(1);
    }
    if (elf64Hdr->e_ident[EI_DATA] == ELFDATA2LSB) {
        Elf64_PHeaderLittleEndianToHost(elf64pHdr);
        return;
    } else if (elf64Hdr->e_ident[EI_DATA] == ELFDATA2MSB) {
        Elf64_PHeaderBigEndianToHost(elf64pHdr);
        return;
    } else {
        fprintf(stderr, "Unknown data encoding in elf file\n");
        exit(1);
    }
}

int64_t
Elf64_LoadFile(FILE *file, Elf_LoadCallback * cbProc, void *cbData)
{
    Elf64_Ehdr elf64Hdr;
    uint32_t idx;
    uint8_t buf[256];
    int64_t cnt;
    int64_t totalCnt = 0;

    /* read ELF header, first thing in the file */
    Elf64_ReadHeader(file, &elf64Hdr);
    if (elf64Hdr.e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "Only 64 Bit ELF is supported currently\n");
        exit(1);
    }
    for (idx = 0; idx < elf64Hdr.e_phnum; idx++) {
        Elf64_Phdr pHdr;
        Elf64_ReadPHeader(file, elf64Hdr.e_phoff + idx * sizeof(Elf64_Phdr), &pHdr, &elf64Hdr);
        if (pHdr.p_type != PT_LOAD) {
            continue;
        }
        if (pHdr.p_filesz == 0) {
            continue;
        }
        fprintf(stderr, "%02u), tp %u, of %016"PRIx64", va %016"PRIx64", pa %016"PRIx64", fsz %"PRIu64
           ", msz %"PRIu64" flags %08x\n",
               idx, pHdr.p_type, pHdr.p_offset, pHdr.p_vaddr, pHdr.p_paddr, pHdr.p_filesz,
               pHdr.p_memsz, pHdr.p_flags);
        /* Now load the segment */
        if (fseeko(file, pHdr.p_offset, SEEK_SET) != 0) {
            fprintf(stderr, "fseeko in ELF file failed\n");
            exit(1);
        }
        for (cnt = 0; cnt < pHdr.p_filesz;) {
            size_t readsz = pHdr.p_filesz - cnt;
            size_t result;
            if (readsz == 0) {
                break;
            } else if (readsz > sizeof(buf)) {
                readsz = sizeof(buf);
            }
            //fprintf(stderr, "Reading %u at %08lx\n", readsz, cnt);
            result = fread(buf, 1, readsz, file);
            if (result <= 0) {
                fprintf(stderr, "Read data from ELF file failed\n");
                exit(1);
            } else {
                cbProc(pHdr.p_paddr + cnt, buf, result, cbData);
            }
            cnt += result;
        }
        totalCnt += cnt;
    }
    return totalCnt;
}
/**
 *********************************************************************************************
 * \fn int64_t Elf32_LoadFile(const char *filename, Elf_LoadCallback * cbProc, void *cbData)
 * Load an 32 Bit ELF file. The data are written to a callback routine
 *********************************************************************************************
 */

static int64_t
Elf32_LoadFile(FILE *file, Elf_LoadCallback * cbProc, void *cbData)
{
    Elf32_Ehdr elf32Hdr;
    uint32_t idx;
    uint8_t buf[256];
    int64_t cnt;
    int64_t totalCnt = 0;

    /* read ELF header, first thing in the file */
    Elf32_ReadHeader(file, &elf32Hdr);
    if (elf32Hdr.e_ident[EI_CLASS] != ELFCLASS32) {
        fprintf(stderr, "Elf32 Load file is loading non 32 Bit elf\n");
        exit(1);
    }
    for (idx = 0; idx < elf32Hdr.e_phnum; idx++) {
        Elf32_Phdr pHdr;
        Elf32_ReadPHeader(file, elf32Hdr.e_phoff + idx * sizeof(Elf32_Phdr), &pHdr, &elf32Hdr);
        if (pHdr.p_type != PT_LOAD) {
            continue;
        }
        if (pHdr.p_filesz == 0) {
            continue;
        }
        fprintf(stderr, "%02u), tp %u, of %08x, va %08x, pa %08x, " 
            "fsz %u, msz %u flags %08x\n",
               idx, pHdr.p_type, pHdr.p_offset, pHdr.p_vaddr, pHdr.p_paddr, pHdr.p_filesz,
               pHdr.p_memsz, pHdr.p_flags);
        /* Now load the segment */
        if (fseek(file, pHdr.p_offset, SEEK_SET) != 0) {
            fprintf(stderr, "fseek in ELF file failed\n");
            exit(1);
        }
        for (cnt = 0; cnt < pHdr.p_filesz;) {
            size_t readsz = pHdr.p_filesz - cnt;
            size_t result;
            if (readsz == 0) {
                break;
            } else if (readsz > sizeof(buf)) {
                readsz = sizeof(buf);
            }
            //fprintf(stderr, "Reading %u at %08lx\n", readsz, cnt);
            result = fread(buf, 1, readsz, file);
            if (result <= 0) {
                fprintf(stderr, "Read data from ELF file failed\n");
                exit(1);
            } else {
                cbProc(pHdr.p_paddr + cnt, buf, result, cbData);
            }
            cnt += result;
        }
        totalCnt += cnt;
    }
    return totalCnt;
}

int64_t
Elf_LoadFile(const char *filename, Elf_LoadCallback * cbProc, void *cbData)
{
    FILE *file = NULL;
    Elf32_Ehdr elf32Hdr;
    int64_t totalCnt;

    if (Elf_CheckElf(filename) == false) {
        fprintf(stderr, "Not an elf file: \"%s\"\n", filename);
        exit(1);
    }
    if ((file = fopen(filename, "r")) == NULL) {
        perror("[E] Error opening file:");
        exit(1);
    }
    Elf32_ReadHeader(file, &elf32Hdr);
    if (elf32Hdr.e_ident[EI_CLASS] == ELFCLASS32) {
        totalCnt = Elf32_LoadFile(file, cbProc, cbData);
    } else if (elf32Hdr.e_ident[EI_CLASS] == ELFCLASS64) {
        totalCnt = Elf64_LoadFile(file, cbProc, cbData);
    } else {
        fprintf(stderr, "Only 32 Bit and 64 Bit ELF is supported currently\n");
        exit(1);
    }
    fclose(file);
    return totalCnt;
}
