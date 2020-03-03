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

static char *extract_config_val(char *buf) {
	char *beg = FIND(buf, " = \"");
	if (!beg)
		return NULL;
	char *end = grub_strrchr(buf, '"');
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

#define DEBUG(...) grub_dprintf("vmsd", __VA_ARGS__)
//#define DEBUG_REFRESH() do{ void *cmdl = grub_cmdline_get("Continue?"); grub_free(cmdl); grub_refresh(); } while(0)
#define DEBUG_REFRESH() grub_refresh()
static grub_err_t
grub_cmd_vmsnap (grub_extcmd_context_t ctxt, int argc, char **argv)
{

	if (argc != 1)
		return grub_error (GRUB_ERR_BAD_ARGUMENT, N_("filename expected"));

	grub_errno = GRUB_ERR_NONE;
	
	char vmxpath[256]; grub_strcpy(vmxpath, argv[0]);
	DEBUG("Got vmxpath: %s\n", vmxpath);
	char vmsdpath[256];
    {
        grub_strcpy(vmsdpath, vmxpath);
        char *ret = grub_strrchr(vmsdpath, '.');
        if (!ret)
            return grub_error (GRUB_ERR_BAD_ARGUMENT, "invalid vmx path");
        
        *ret = 0;
        grub_strcpy(vmsdpath + grub_strlen(vmsdpath), ".vmsd");
    }
	DEBUG("Got vmsdpath: %s\n", vmsdpath);
	DEBUG_REFRESH();

	grub_errno = GRUB_ERR_NONE;

	DEBUG("Opening vmx file\n");
	DEBUG_REFRESH();
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
    char curdisk[256] = { 0 };
    while (grub_free (buf), (buf = grub_file_getline (vmxfile))) {
		DEBUG("Got line: %s\n", buf);
        char *lineStart = buf;
        if (!FIND(buf, ".fileName = \"")) {
            continue;
        }
        if ( !(!CMPHEAD(lineStart, "ide") || !CMPHEAD(lineStart, "sata") || !CMPHEAD(lineStart, "scsi") || !CMPHEAD(lineStart, "nvme")) ) {
            continue;
        }
        char *filename = extract_config_val(lineStart);
        if (!grub_memcmp(filename + grub_strlen(filename) - 3, "iso", 3) 
            || !grub_memcmp(filename + grub_strlen(filename) - 3, "flp", 3) ) {
            continue;
        }
        grub_strcpy(curdisk, filename);
        break;
    }

	if (!grub_strlen(curdisk))
		return grub_error (1, N_("invalid vmx contains no disk"));  

	grub_errno = GRUB_ERR_NONE;
	grub_file_close(vmxfile);
	DEBUG("Closed vmxfile");

    grub_printf("Got disk in vmx file: %s\n", curdisk);
	DEBUG_REFRESH();

	grub_errno = GRUB_ERR_NONE;

	grub_file_t vmsdfile = grub_file_open(vmsdpath, GRUB_FILE_TYPE_NONE | GRUB_FILE_TYPE_NO_DECOMPRESS);
    if (!vmsdfile) {
		_handle_return(ctxt, curdisk);
		return GRUB_ERR_NONE;
	}
	DEBUG("Successfully opened vmsd: %s\n", vmsdpath);
	DEBUG_REFRESH();

	// has vmsd file
	struct SNAPSHOT {
		char entry[20];
		char uid[8];
		char diskname[256];
		char displayname[256];
		char parent[8];
	} snapshots[MAX_SNAPSHOT_NUM] = { 0 };
	int cur_index = -1;
	char currentUid[8] = { 0 };
	while (grub_free (buf), (buf = grub_file_getline (vmsdfile))) {
        if (!CMPHEAD(buf, "snapshot.")) { // general configs
			if (!!CMPHEAD(buf, "snapshot.current")) {
				continue;
			}
			char *ret = extract_config_val(buf);
			if (ret){
				grub_strcpy(currentUid, extract_config_val(buf));
			}
            DEBUG("Found snapshot.current: %s\n", currentUid);
			continue;
		}
		else if (!CMPHEAD(buf, "snapshot") && grub_isdigit(buf[8])) { // things like snapshotXX.XXX
			//puts(buf);
            char *pDot = grub_strchr(buf, '.');
			*pDot = 0;
			if (cur_index == -1 || !!grub_strcmp(buf, snapshots[cur_index].entry)) {
				cur_index++;
				grub_strcpy(snapshots[cur_index].entry, buf);
                DEBUG("Enter new snapshot entry: %s\n", buf);
				DEBUG_REFRESH();
			}
			
			char *propStart = pDot + 1;
			//char propName[20] = { 0 };

			char *ret = extract_config_val(propStart);
            if (!ret)
                continue;
			
			if (!CMPHEAD(propStart, "disk0.fileName = ")) {
				grub_strcpy(snapshots[cur_index].diskname, ret);
			} else if (!CMPHEAD(propStart, "uid = ")) {
				grub_strcpy(snapshots[cur_index].uid, ret);
			} else if (!CMPHEAD(propStart, "displayName = ")) {
				grub_strcpy(snapshots[cur_index].displayname, ret);
			} else if (!CMPHEAD(propStart, "parent = ")) {
				grub_strcpy(snapshots[cur_index].parent, ret);
			} else {
				continue;
			}
		} else {
			continue;
		}
	}
	DEBUG_REFRESH();
	grub_errno = GRUB_ERR_NONE;
	grub_file_close(vmsdfile);
	DEBUG("Closed vmxfile");


	grub_errno = GRUB_ERR_NONE;
	
    // debug printting
    for (int i = 0; i <= cur_index; i++) {
        grub_printf("Found node entry: %s uid: %s, parent: %s, name: %s, disk: %s\n",
                    snapshots[i].entry, snapshots[i].uid, snapshots[i].parent, snapshots[i].displayname, snapshots[i].diskname);
    }
	DEBUG_REFRESH();
    
    // go through the chain
    char disks[MAX_RET_BUF]; char *disksend = disks + sizeof(disks);
    disksend -= grub_strlen(curdisk) + 1;
    grub_strcpy(disksend, curdisk);
    
	DEBUG_REFRESH();
    char cur[8]; grub_strcpy(cur, currentUid);
    while (1) {
        int i;
        for (i = 0; i <= cur_index; i++) {
            if (!!grub_strcmp(snapshots[i].uid, cur)) 
                continue;
            break;
        }
        if (i == cur_index + 1) {
            return grub_error (1, N_("can't find target snapshot in chain"));
        }
        // found current node
        grub_printf("Cur nodes uid: %s, name: %s, disk: %s\n", cur, snapshots[i].displayname, snapshots[i].diskname);
        int len = grub_strlen(snapshots[i].diskname);
        disksend -= len + 1;
        grub_memcpy(disksend, snapshots[i].diskname, len);
        disksend[len] = ' ';
        grub_strcpy(cur, snapshots[i].parent);
        if (!grub_strcmp(snapshots[i].parent, ""))
            // completed
            break;
    }
	DEBUG_REFRESH();
    disks[sizeof(disks) - 1] = 0;
    _handle_return(ctxt, disksend);
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
