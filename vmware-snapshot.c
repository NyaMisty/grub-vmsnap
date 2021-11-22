/* rdmsr.c - Read CPU model-specific registers. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2019  Free Software Foundation, Inc.
 *  Based on gcc/gcc/config/i386/driver-i386.c
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/file.h>
#include <grub/disk.h>
#include <grub/term.h>
#include <grub/misc.h>
#include <grub/extcmd.h>
#include <grub/normal.h>
#include <grub/mm.h>
#include <grub/env.h>
#include <grub/i18n.h>
#include <grub/charset.h>

//#include <grub/vmsnap.h>

#define MAX_VMFILE_SIZE 0x4000
#define MAX_RET_BUF 0x400
#define MAX_SNAPSHOT_NUM 20

GRUB_MOD_LICENSE("GPLv3+");

static grub_extcmd_t cmd_vmsnap;

static const struct grub_arg_option options[] =
{
    {"set", 0, 0, N_("Save read value into variable VARNAME."),
    N_("VARNAME"), ARG_TYPE_STRING},
    {0, 0, 0, 0, 0, 0}
};

static void _handle_return(grub_extcmd_context_t ctxt, char *retstr){
    if (ctxt->state[0].set)
        grub_env_set (ctxt->state[0].arg, retstr);
    else
        grub_printf ("%s\n", retstr);
}
static char *FIND(char *buf, const char* str) {
    char *temp = grub_strstr(buf, str);
    return temp ? temp + grub_strlen(str) : temp;
}
//#define FIND(buf, str) (strstr(buf, str) + grub_strlen(grub_strlen(str)))
#define CMPHEAD(buf, head) (grub_strncmp(buf, head, grub_strlen(head)))

static char *extract_config_val(char *buf, const char *startstr, const char endchar) {
    char *beg = FIND(buf, startstr);
    if (!beg)
        return NULL;
    char *end = grub_strrchr(buf, endchar);
    if (!end)
        return NULL;
    *end = 0;
    return beg;
}

/*static int extract_config_line(char *buf, char *prop, char *val) {
    char *propend = grub_strstr(buf, " = \"");
    if (!propend)
        return 0;
    
    char *beg = propend + sizeof(" = \"") - 1;
    char *end = grub_strrchr(buf, '"');
    if (!end)
        return 0;
    *propend = 0;
    grub_strcpy(prop, buf);
    
    *end = 0;
    grub_strcpy(val, beg);

    return 1;
}*/

/* Read a line from the file FILE.  */
char *
grub_file_getline_mod (grub_file_t file)
{
  char c;
  grub_size_t pos = 0;
  char *cmdline;
  int have_newline = 0;
  grub_size_t max_len = 64;

  /* Initially locate some space.  */
  cmdline = grub_malloc (max_len);
  if (! cmdline)
    return 0;

  while (1)
    {
      if (grub_file_read (file, &c, 1) != 1)
        break;

      /* Skip all carriage returns.  */
      if (c == '\r')
        continue;
      
      if (c == 0)
        break;

      if (pos + 1 >= max_len)
        {
          char *old_cmdline = cmdline;
          max_len = max_len * 2;
          cmdline = grub_realloc (cmdline, max_len);
          if (! cmdline)
            {
              grub_free (old_cmdline);
              return 0;
            }
        }

      if (c == '\n')
        {
          have_newline = 1;
          break;
        }

      cmdline[pos++] = c;
    }

  cmdline[pos] = '\0';

  /* If the buffer is empty, don't return anything at all.  */
  if (pos == 0 && !have_newline)
    {
      grub_free (cmdline);
      cmdline = 0;
    }

  return cmdline;
}


#define DEBUG(...) grub_dprintf("vmsd", __VA_ARGS__)
//#define DEBUG_REFRESH() do{ void *cmdl = grub_cmdline_get("Continue?"); grub_free(cmdl); grub_refresh(); } while(0)
#define DEBUG_REFRESH() grub_refresh()
static grub_err_t
grub_cmd_vmsnap (grub_extcmd_context_t ctxt, int argc, char **argv)
{

    if (argc != 1)
        return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

    grub_errno = GRUB_ERR_NONE;
    
    char vmxpath[256]; char vmxdir[256]; char temppath[256];
    
    {
        grub_strcpy(vmxpath, argv[0]);
        DEBUG("Got vmxpath: %s\n", vmxpath);
        {
            grub_strcpy(vmxdir, vmxpath);
            
            char *ret = grub_strrchr(vmxdir, '/');
            if (!ret)
                return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid vmx path");
            
            *(ret+1) = 0;
        }
        grub_strcpy(temppath, vmxdir);
        DEBUG("Got vmxdir: %s\n", temppath);
    }
    DEBUG_REFRESH();

    grub_errno = GRUB_ERR_NONE;

    DEBUG("Opening vmx file\n");
    DEBUG_REFRESH();

    char curdisk[256] = { 0 };
    {
        grub_file_t vmxfile = grub_file_open(vmxpath, GRUB_FILE_TYPE_NONE | GRUB_FILE_TYPE_NO_DECOMPRESS);
        if (!vmxfile){
            grub_err_t temp = grub_errno;
            grub_errno = GRUB_ERR_NONE;
            return grub_error (temp, "Failed to open vmx file");
        }
    
        DEBUG("Successfully opened vmx: %s\n", vmxpath);
        DEBUG_REFRESH();
    
        grub_errno = GRUB_ERR_NONE;

        char *buf = NULL;
        while (grub_free (buf), (buf = grub_file_getline (vmxfile))) {
            DEBUG("Got line: %s\n", buf);
            DEBUG_REFRESH();
            char *lineStart = buf;
            if (!FIND(buf, ".fileName = \"")) {
                continue;
            }
            if ( !(!CMPHEAD(lineStart, "ide") || !CMPHEAD(lineStart, "sata") || !CMPHEAD(lineStart, "scsi") || !CMPHEAD(lineStart, "nvme")) ) {
                continue;
            }
            char *filename = extract_config_val(lineStart, " = \"", '"');
            if (!grub_memcmp(filename + grub_strlen(filename) - 3, "iso", 3) 
                || !grub_memcmp(filename + grub_strlen(filename) - 3, "flp", 3) ) {
                continue;
            }
            grub_strcpy(curdisk, filename);
            break;
        }
        grub_errno = GRUB_ERR_NONE;
        grub_file_close(vmxfile);
        DEBUG("Closed vmxfile\n");
    }

    if (!grub_strlen(curdisk))
        return grub_error (1, N_("invalid vmx contains no disk"));  

    grub_printf("Got disk in vmx file: %s\n", curdisk);
    DEBUG_REFRESH();
    //return GRUB_ERR_NONE;

    grub_errno = GRUB_ERR_NONE;
    

    char disks[20][256] = { 0 };
    int disk_num = 0;
    char cursubdisk[256]; grub_strcpy(cursubdisk, curdisk);
    {
        for (disk_num = 0; disk_num < 20; disk_num++) {
            grub_strcpy(temppath + grub_strlen(vmxdir), cursubdisk);
            grub_printf("processing disk file %s\n", temppath);
            DEBUG_REFRESH();
            grub_file_t curdiskfile = grub_file_open(temppath, GRUB_FILE_TYPE_NONE | GRUB_FILE_TYPE_NO_DECOMPRESS);
            if (!curdiskfile) {
                grub_printf("can't find disk!\n");
                grub_error (1, N_("can't find disk!"));
                return GRUB_ERR_NONE;
            }
            grub_file_seek(curdiskfile, 0x200);
            char *buf = NULL;
            char parentdisk[256] = { 0 };
            int ln = 0;
            while (grub_free (buf), (buf = grub_file_getline_mod (curdiskfile))) {
                DEBUG("Got disk info line: %s\n", buf);
                DEBUG_REFRESH();
                ln++;
                char *lineStart = buf;
                if (!!CMPHEAD(lineStart, "parentFileNameHint=")) {
                    continue;
                }
                char *filename = extract_config_val(lineStart, "=\"", '"');
                grub_strcpy(parentdisk, filename);
                DEBUG("Got new disk on ln %d: %s\n", ln, parentdisk);
                break;
            }
            
            grub_errno = GRUB_ERR_NONE;
            grub_file_close(curdiskfile);
    
            grub_strcpy(disks[disk_num], cursubdisk);
            grub_strcpy(cursubdisk, parentdisk);
        
            if (!grub_strlen(parentdisk))
                break;
        }
        if (disk_num == 20) {
            grub_printf("Toooo many disks (> 20)\n");
            return grub_error (1, "Toooo many disks (> 20)");
        }
        disk_num++;
    }
    
    DEBUG("Finished processing disks\n");
    DEBUG_REFRESH();

    grub_errno = GRUB_ERR_NONE;
    
    grub_printf("Found %d disks: ", disk_num);
    // debug printting
    for (int i = 0; i < disk_num; i++) {
        grub_printf("%s ", disks[i]);
        DEBUG_REFRESH();
    }
    grub_printf("\n");
    DEBUG_REFRESH();
    
    // go through the chain
    char diskret[MAX_RET_BUF]; char *diskretend = diskret + sizeof(diskret);
    
    DEBUG_REFRESH();
    //for (int i = disk_num - 1; i >= 0; i--) {
    for (int i = 0; i < disk_num; i++) {
        DEBUG("Outputting disks: %s\n", disks[i]);
        DEBUG_REFRESH();
        int len = grub_strlen(disks[i]);
        diskretend -= len + 1;
        grub_memcpy(diskretend, disks[i], len);
        diskretend[len] = ' ';
    }
    DEBUG_REFRESH();
    diskret[sizeof(diskret) - 1] = 0;
    _handle_return(ctxt, diskretend);
    return GRUB_ERR_NONE;
}

GRUB_MOD_INIT(vmsnap)
{
    cmd_vmsnap = grub_register_extcmd ("vmsnap", grub_cmd_vmsnap, 0, N_("VMXPATH"),
                     N_("Parse vmx and vmsd to get snapshot disks"),
                     options);
}

GRUB_MOD_FINI(vmsnap)
{
    grub_unregister_extcmd (cmd_vmsnap);
}
